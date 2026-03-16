#!/usr/bin/env bash
set -euo pipefail

WM_BIN="${1:-./cupidwm}"
CTL_BIN="${2:-./cupidwmctl}"
HOST_DISPLAY="${DISPLAY:-}"
DISPLAY_NUM="${DISPLAY_NUM:-119}"
TEST_DISPLAY=""

fail() {
	if [ -n "${LOG_DIR:-}" ]; then
		printf 'ipc test failed: %s\n' "$*" >"${LOG_DIR}/failure.log" 2>/dev/null || true
	fi
	echo "ipc test failed: $*" >&2
	exit 1
}

need_cmd() {
	if ! command -v "$1" >/dev/null 2>&1; then
		fail "missing required command: $1"
	fi
}

[ -n "$HOST_DISPLAY" ] || fail "host DISPLAY is not set"

for cmd in Xephyr xdpyinfo xprop mktemp grep sed touch; do
	need_cmd "$cmd"
done

LOG_DIR="$(mktemp -d "${TMPDIR:-/tmp}/cupidwm-ipc.XXXXXX")"
xephyr_pid=""
wm_pid=""

cleanup() {
	if [ -n "$wm_pid" ] && kill -0 "$wm_pid" 2>/dev/null; then
		kill "$wm_pid" 2>/dev/null || true
		wait "$wm_pid" 2>/dev/null || true
	fi
	if [ -n "$xephyr_pid" ] && kill -0 "$xephyr_pid" 2>/dev/null; then
		kill "$xephyr_pid" 2>/dev/null || true
		wait "$xephyr_pid" 2>/dev/null || true
	fi
}
trap cleanup EXIT INT TERM

choose_test_display() {
	local num="$DISPLAY_NUM"
	for _ in $(seq 1 40); do
		if [ ! -e "/tmp/.X${num}-lock" ] && ! DISPLAY=":${num}" xdpyinfo >/dev/null 2>&1; then
			TEST_DISPLAY=":${num}"
			export DISPLAY="$TEST_DISPLAY"
			return 0
		fi
		num=$((num + 1))
	done
	return 1
}

wait_display() {
	for _ in $(seq 1 100); do
		if xdpyinfo >/dev/null 2>&1; then
			return 0
		fi
		if [ -n "$xephyr_pid" ] && ! kill -0 "$xephyr_pid" 2>/dev/null; then
			return 1
		fi
		sleep 0.1
	done
	return 1
}

wait_socket() {
	local path="$1"
	for _ in $(seq 1 80); do
		if [ -S "$path" ]; then
			return 0
		fi
		sleep 0.1
	done
	return 1
}

wait_process_exit() {
	local pid="$1"
	for _ in $(seq 1 80); do
		if ! kill -0 "$pid" 2>/dev/null; then
			return 0
		fi
		sleep 0.1
	done
	return 1
}

wait_root_prop_contains() {
	local prop="$1"
	local expect="$2"
	for _ in $(seq 1 80); do
		if xprop -root "$prop" 2>/dev/null | grep -Fq "$expect"; then
			return 0
		fi
		sleep 0.1
	done
	return 1
}

wait_ctl_reply() {
	local expect="$1"
	shift
	for _ in $(seq 1 80); do
		local out=""
		out="$($CTL_BIN "$@" 2>"${LOG_DIR}/ctl.err" || true)"
		if echo "$out" | grep -Eq "$expect"; then
			echo "$out"
			return 0
		fi
		sleep 0.1
	done
	return 1
}

choose_test_display || fail "could not find free test display"
DISPLAY="$HOST_DISPLAY" Xephyr "$TEST_DISPLAY" -screen 1280x720 -ac -nolisten tcp >"${LOG_DIR}/xephyr.log" 2>&1 &
xephyr_pid=$!
wait_display || fail "Xephyr did not become ready"

expected_socket="$($CTL_BIN --print-socket)"
[ -n "$expected_socket" ] || fail "failed to resolve expected socket path"

"$WM_BIN" >"${LOG_DIR}/wm.log" 2>&1 &
wm_pid=$!
sleep 1
kill -0 "$wm_pid" 2>/dev/null || fail "window manager exited early"

wait_socket "$expected_socket" || fail "socket was not created at $expected_socket"
wait_root_prop_contains _CUPIDWM_IPC_SOCKET "$expected_socket" || fail "root property did not publish active socket path"

if command -v python3 >/dev/null 2>&1; then
	partial_timeout_out="$(
		SOCK_PATH="$expected_socket" python3 - <<'PY'
import os
import socket
import time

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.settimeout(2.0)
sock.connect(os.environ["SOCK_PATH"])
sock.sendall(b"status")
time.sleep(0.6)
try:
    data = sock.recv(4096)
except socket.timeout:
    data = b"timeout"
sock.close()
print(data.decode("utf-8", "replace"), end="")
PY
	)"
	echo "$partial_timeout_out" | grep -Fq 'error read timeout' || fail "partial newline-free command did not time out"

	oversize_out="$(
		SOCK_PATH="$expected_socket" python3 - <<'PY'
import os
import socket

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.settimeout(2.0)
sock.connect(os.environ["SOCK_PATH"])
sock.sendall((b"x" * 800) + b"\n")
try:
    data = sock.recv(4096)
except socket.timeout:
    data = b"timeout"
sock.close()
print(data.decode("utf-8", "replace"), end="")
PY
	)"
	echo "$oversize_out" | grep -Fq 'error command too long' || fail "oversized command was not rejected"
fi

ping_out="$(wait_ctl_reply '^ok pong$' ping || true)"
[ -n "$ping_out" ] || fail "cupidwmctl ping did not succeed"

status_out="$(wait_ctl_reply '^ok workspace=[0-9]+ monitor=' status || true)"
[ -n "$status_out" ] || fail "cupidwmctl status did not succeed"
echo "$status_out" | grep -Eq 'layout=' || fail "status output did not include layout details"

bar_set_out="$(wait_ctl_reply '^ok$' 'bar status set IPC-Status-Test' || true)"
[ -n "$bar_set_out" ] || fail "bar status set did not succeed"

bar_query_out="$(wait_ctl_reply '^ok show=' 'query bar' || true)"
[ -n "$bar_query_out" ] || fail "query bar did not succeed"
echo "$bar_query_out" | grep -Fq 'status text=IPC-Status-Test' || fail "query bar did not include pushed status text"

special_status='Q "A" \ B'
bar_set_special_out="$(wait_ctl_reply '^ok$' bar status set "$special_status" || true)"
[ -n "$bar_set_special_out" ] || fail "bar status set with special characters did not succeed"

bar_query_json_out="$(wait_ctl_reply '^\{\"ok\":true,\"bar\":\{' --json query bar || true)"
[ -n "$bar_query_json_out" ] || fail "JSON query bar did not succeed"
echo "$bar_query_json_out" | grep -Fq '"status":"Q \"A\" \\ B"' || fail "JSON status text was not escaped correctly"

bar_restore_out="$(wait_ctl_reply '^ok$' 'bar status set IPC-Status-Test' || true)"
[ -n "$bar_restore_out" ] || fail "bar status restore did not succeed"

click_marker="${LOG_DIR}/status-click.marker"
rm -f "$click_marker"

bar_action_set_out="$(wait_ctl_reply '^ok$' "bar action set 1 left touch $click_marker" || true)"
[ -n "$bar_action_set_out" ] || fail "bar action set did not succeed"

bar_click_out="$(wait_ctl_reply '^ok$' 'bar click 1 left' || true)"
[ -n "$bar_click_out" ] || fail "bar click did not succeed"

for _ in $(seq 1 20); do
	[ -f "$click_marker" ] && break
	sleep 0.1
done
[ -f "$click_marker" ] || fail "bar click did not dispatch action command"

bar_status_clear_out="$(wait_ctl_reply '^ok$' 'bar status clear' || true)"
[ -n "$bar_status_clear_out" ] || fail "bar status clear did not succeed"

ws_out="$(wait_ctl_reply '^ok$' workspace 3 || true)"
[ -n "$ws_out" ] || fail "workspace switch command did not succeed"

layout_out="$(wait_ctl_reply '^ok$' layout monocle || true)"
[ -n "$layout_out" ] || fail "layout change command did not succeed"

verify_ws_out="$(wait_ctl_reply '^ok workspace=3 monitor=' status || true)"
[ -n "$verify_ws_out" ] || fail "workspace did not switch to 3 before reload"

verify_layout_out="$(wait_ctl_reply 'workspace id=3 .*layout=monocle' query workspaces || true)"
[ -n "$verify_layout_out" ] || fail "workspace 3 layout was not monocle before reload"

reload_out="$(wait_ctl_reply '^ok$' reload || true)"
[ -n "$reload_out" ] || fail "cupidwmctl reload did not succeed"

wait_socket "$expected_socket" || fail "socket not available after reload"
wait_root_prop_contains _CUPIDWM_IPC_SOCKET "$expected_socket" || fail "root socket property missing after reload"

post_reload_ws_out="$(wait_ctl_reply '^ok workspace=3 monitor=' status || true)"
[ -n "$post_reload_ws_out" ] || fail "workspace state did not persist across reload"

post_reload_layout_out="$(wait_ctl_reply 'workspace id=3 .*layout=monocle' query workspaces || true)"
[ -n "$post_reload_layout_out" ] || fail "layout state did not persist across reload"

quit_out="$(wait_ctl_reply '^ok$' quit || true)"
[ -n "$quit_out" ] || fail "cupidwmctl quit did not succeed"

wait_process_exit "$wm_pid" || fail "window manager did not exit after quit command"
wm_pid=""

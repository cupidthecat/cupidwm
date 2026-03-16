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

for cmd in Xephyr xdpyinfo xprop mktemp grep sed; do
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
		out="$("$CTL_BIN" "$@" 2>"${LOG_DIR}/ctl.err" || true)"
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

expected_socket="$("$CTL_BIN" --print-socket)"
[ -n "$expected_socket" ] || fail "failed to resolve expected socket path"

"$WM_BIN" >"${LOG_DIR}/wm.log" 2>&1 &
wm_pid=$!
sleep 1
kill -0 "$wm_pid" 2>/dev/null || fail "window manager exited early"

wait_socket "$expected_socket" || fail "socket was not created at $expected_socket"
wait_root_prop_contains _CUPIDWM_IPC_SOCKET "$expected_socket" || fail "root property did not publish active socket path"

ping_out="$(wait_ctl_reply '^ok pong$' ping || true)"
[ -n "$ping_out" ] || fail "cupidwmctl ping did not succeed"

status_out="$(wait_ctl_reply '^ok workspace=1 monitor=' status || true)"
[ -n "$status_out" ] || fail "cupidwmctl status did not succeed"

echo "$status_out" | grep -Eq 'layout=' || fail "status output did not include layout details"

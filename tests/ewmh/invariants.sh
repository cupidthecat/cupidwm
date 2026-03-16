#!/usr/bin/env bash
set -euo pipefail

WM_BIN="${1:-./cupidwm}"
HOST_DISPLAY="${DISPLAY:-}"
DISPLAY_NUM="${DISPLAY_NUM:-109}"
TEST_DISPLAY=""

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
# shellcheck source=tests/ewmh/assert.sh
source "${SCRIPT_DIR}/assert.sh"

[ -n "$HOST_DISPLAY" ] || fail "host DISPLAY is not set"

for cmd in Xephyr xdpyinfo xprop xdotool xterm xwininfo awk sed grep mktemp; do
	need_cmd "$cmd"
done

LOG_DIR="$(mktemp -d "${TMPDIR:-/tmp}/cupidwm-ewmh.XXXXXX")"
xephyr_pid=""
wm_pid=""
declare -a app_pids=()

cleanup() {
	for pid in "${app_pids[@]}"; do
		if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
			kill "$pid" 2>/dev/null || true
		fi
	done
	if [ -n "$wm_pid" ] && kill -0 "$wm_pid" 2>/dev/null; then
		kill "$wm_pid" 2>/dev/null || true
	fi
	if [ -n "$xephyr_pid" ] && kill -0 "$xephyr_pid" 2>/dev/null; then
		kill "$xephyr_pid" 2>/dev/null || true
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
	for _ in $(seq 1 80); do
		if xdpyinfo >/dev/null 2>&1; then
			return 0
		fi
		sleep 0.1
	done
	return 1
}

wait_visible_window_by_name() {
	local pattern="$1"
	local wid=""
	for _ in $(seq 1 80); do
		wid="$(xdotool search --onlyvisible --name "$pattern" 2>/dev/null | head -n1 || true)"
		if [ -n "$wid" ]; then
			echo "$wid"
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

"$WM_BIN" >"${LOG_DIR}/wm.log" 2>&1 &
wm_pid=$!
sleep 1
kill -0 "$wm_pid" 2>/dev/null || fail "window manager exited early"

# _NET_SUPPORTED core atoms
for atom in _NET_ACTIVE_WINDOW _NET_CURRENT_DESKTOP _NET_NUMBER_OF_DESKTOPS _NET_DESKTOP_NAMES _NET_CLIENT_LIST _NET_CLIENT_LIST_STACKING _NET_WORKAREA; do
	assert_prop_contains root _NET_SUPPORTED "$atom"
done

num_desktops="$(root_prop_int _NET_NUMBER_OF_DESKTOPS)"
assert_int_eq "$num_desktops" "9" "_NET_NUMBER_OF_DESKTOPS mismatch"

cur_ws="$(root_prop_int _NET_CURRENT_DESKTOP)"
assert_int_eq "$cur_ws" "0" "initial current desktop should be 0"

xterm -title "ewmh-a" -class st -geometry 80x24+60+60 >"${LOG_DIR}/xterm-a.log" 2>&1 &
app_pids+=("$!")
wid_a="$(wait_visible_window_by_name '^ewmh-a$' || true)"
[ -n "$wid_a" ] || fail "failed to open first test window"

xterm -title "ewmh-b" -class st -geometry 80x24+280+60 >"${LOG_DIR}/xterm-b.log" 2>&1 &
app_pids+=("$!")
wid_b="$(wait_visible_window_by_name '^ewmh-b$' || true)"
[ -n "$wid_b" ] || fail "failed to open second test window"

# client lists should include both windows
assert_prop_contains root _NET_CLIENT_LIST "$wid_a"
assert_prop_contains root _NET_CLIENT_LIST "$wid_b"
assert_prop_contains root _NET_CLIENT_LIST_STACKING "$wid_a"
assert_prop_contains root _NET_CLIENT_LIST_STACKING "$wid_b"

# active window should track focus changes
xdotool windowactivate "$wid_a" >/dev/null 2>&1 || true
sleep 0.2
assert_prop_contains root _NET_ACTIVE_WINDOW "0x$(printf '%x' "$wid_a")"

xdotool windowactivate "$wid_b" >/dev/null 2>&1 || true
sleep 0.2
assert_prop_contains root _NET_ACTIVE_WINDOW "0x$(printf '%x' "$wid_b")"

# move workspace via keybinding and verify root property
xdotool key --clearmodifiers super+2 >/dev/null 2>&1 || true
sleep 0.3
cur_ws="$(root_prop_int _NET_CURRENT_DESKTOP)"
assert_int_eq "$cur_ws" "1" "workspace switch to 2 failed"

xdotool key --clearmodifiers super+1 >/dev/null 2>&1 || true
sleep 0.3
cur_ws="$(root_prop_int _NET_CURRENT_DESKTOP)"
assert_int_eq "$cur_ws" "0" "workspace switch to 1 failed"

# strut/workarea invariant: changing dock strut should change workarea
workarea_before="$(xprop -root _NET_WORKAREA 2>/dev/null | sed -n 's/.*= //p' | head -n1)"

xprop -id "$wid_b" -f _NET_WM_WINDOW_TYPE 32a -set _NET_WM_WINDOW_TYPE _NET_WM_WINDOW_TYPE_DOCK >/dev/null 2>&1 || true
xprop -id "$wid_b" -f _NET_WM_STRUT_PARTIAL 32c -set _NET_WM_STRUT_PARTIAL "0,0,48,0,0,0,0,0,0,1279,0,0" >/dev/null 2>&1 || true
sleep 0.4

workarea_after="$(xprop -root _NET_WORKAREA 2>/dev/null | sed -n 's/.*= //p' | head -n1)"
[ -n "$workarea_before" ] || fail "could not read _NET_WORKAREA baseline"
[ -n "$workarea_after" ] || fail "could not read _NET_WORKAREA updated value"
[ "$workarea_before" != "$workarea_after" ] || fail "_NET_WORKAREA did not change after dock strut update"

echo "ewmh invariants passed"

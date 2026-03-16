#!/usr/bin/env bash
set -euo pipefail

WM_BIN="${1:-./cupidwm}"
HOST_DISPLAY="${DISPLAY:-}"
DISPLAY_NUM="${DISPLAY_NUM:-109}"
TEST_DISPLAY=""

SCRIPT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
# shellcheck source=tests/ewmh/assert.sh
source "${SCRIPT_DIR}/assert.sh"

[ -n "$HOST_DISPLAY" ] || fail "host DISPLAY is not set"

for cmd in Xephyr xdpyinfo xprop xdotool xterm xwininfo awk sed grep mktemp; do
	need_cmd "$cmd"
done

LOG_DIR="$(mktemp -d "${TMPDIR:-/tmp}/cupidwm-ewmh.XXXXXX")"
MSG_TOOL="${LOG_DIR}/send-client-message"
xephyr_pid=""
wm_pid=""
last_spawn_pid=""
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

wait_visible_window_by_pid() {
	local pid="$1"
	local wid=""
	for _ in $(seq 1 120); do
		if ! kill -0 "$pid" 2>/dev/null; then
			return 1
		fi
		wid="$(xdotool search --onlyvisible --pid "$pid" 2>/dev/null | head -n1 || true)"
		if [ -n "$wid" ]; then
			echo "$wid"
			return 0
		fi
		sleep 0.1
	done
	return 1
}

wait_window_gone() {
	local wid="$1"
	for _ in $(seq 1 80); do
		if ! xwininfo -id "$wid" >/dev/null 2>&1; then
			return 0
		fi
		sleep 0.1
	done
	return 1
}

wait_window_map_state() {
	local wid="$1"
	local expect="$2"
	for _ in $(seq 1 80); do
		local state=""
		state="$(xwininfo -id "$wid" 2>/dev/null | awk -F': ' '/Map State:/ {print $2; exit}')"
		if [ "$state" = "$expect" ]; then
			return 0
		fi
		sleep 0.1
	done
	return 1
}

wait_window_geometry() {
	local wid="$1"
	local want_x="$2"
	local want_y="$3"
	local want_w="$4"
	local want_h="$5"
	for _ in $(seq 1 80); do
		local got_x got_y got_w got_h
		got_x="$(xwininfo -id "$wid" 2>/dev/null | awk -F': ' '/Absolute upper-left X:/ {print $2; exit}' | tr -d '[:space:]')"
		got_y="$(xwininfo -id "$wid" 2>/dev/null | awk -F': ' '/Absolute upper-left Y:/ {print $2; exit}' | tr -d '[:space:]')"
		got_w="$(xwininfo -id "$wid" 2>/dev/null | awk -F': ' '/Width:/ {print $2; exit}' | tr -d '[:space:]')"
		got_h="$(xwininfo -id "$wid" 2>/dev/null | awk -F': ' '/Height:/ {print $2; exit}' | tr -d '[:space:]')"
		if [ "$got_x" = "$want_x" ] && [ "$got_y" = "$want_y" ] && [ "$got_w" = "$want_w" ] && [ "$got_h" = "$want_h" ]; then
			return 0
		fi
		sleep 0.1
	done
	return 1
}

wait_prop_order() {
	local target="$1"
	local prop="$2"
	local first="$3"
	local second="$4"
	for _ in $(seq 1 80); do
		if prop_order_matches "$target" "$prop" "$first" "$second"; then
			return 0
		fi
		sleep 0.1
	done
	return 1
}

spawn_xterm() {
	local tag="$1"
	shift
	xterm "$@" >"${LOG_DIR}/xterm-${tag}.log" 2>&1 &
	last_spawn_pid="$!"
	app_pids+=("$last_spawn_pid")
}

choose_test_display || fail "could not find free test display"
DISPLAY="$HOST_DISPLAY" Xephyr "$TEST_DISPLAY" -screen 1280x720 -ac -nolisten tcp >"${LOG_DIR}/xephyr.log" 2>&1 &
xephyr_pid=$!
wait_display || fail "Xephyr did not become ready"

"$WM_BIN" >"${LOG_DIR}/wm.log" 2>&1 &
wm_pid=$!
sleep 1
kill -0 "$wm_pid" 2>/dev/null || fail "window manager exited early"

cc -std=c99 -Wall -Wextra "${SCRIPT_DIR}/send-client-message.c" -o "${MSG_TOOL}" -lX11 >/dev/null 2>&1 \
	|| fail "failed to build send-client-message helper"

# _NET_SUPPORTED core atoms
for atom in _NET_ACTIVE_WINDOW _NET_CURRENT_DESKTOP _NET_NUMBER_OF_DESKTOPS _NET_DESKTOP_NAMES _NET_CLIENT_LIST _NET_CLIENT_LIST_STACKING _NET_WORKAREA _NET_CLOSE_WINDOW _NET_WM_STATE_MODAL _NET_WM_STATE_ABOVE _NET_WM_STATE_BELOW _NET_WM_STATE_STICKY _NET_WM_STATE_MAXIMIZED_VERT _NET_WM_STATE_MAXIMIZED_HORZ _NET_WM_STATE_SHADED _NET_WM_STATE_SKIP_TASKBAR _NET_WM_STATE_SKIP_PAGER _NET_WM_STATE_HIDDEN _NET_WM_STATE_DEMANDS_ATTENTION _NET_WM_STATE_FOCUSED _NET_MOVERESIZE_WINDOW _NET_REQUEST_FRAME_EXTENTS _NET_RESTACK_WINDOW _NET_WM_ALLOWED_ACTIONS _NET_WM_ACTION_MOVE _NET_WM_ACTION_RESIZE _NET_WM_ACTION_MINIMIZE _NET_WM_ACTION_SHADE _NET_WM_ACTION_STICK _NET_WM_ACTION_MAXIMIZE_HORZ _NET_WM_ACTION_MAXIMIZE_VERT _NET_WM_ACTION_FULLSCREEN _NET_WM_ACTION_CHANGE_DESKTOP _NET_WM_ACTION_CLOSE; do
	assert_prop_contains root _NET_SUPPORTED "$atom"
done

num_desktops="$(root_prop_int _NET_NUMBER_OF_DESKTOPS)"
assert_int_eq "$num_desktops" "9" "_NET_NUMBER_OF_DESKTOPS mismatch"

cur_ws="$(root_prop_int _NET_CURRENT_DESKTOP)"
assert_int_eq "$cur_ws" "0" "initial current desktop should be 0"

spawn_xterm "a" -title "ewmh-a" -class st -geometry 80x24+60+60
pid_a="$last_spawn_pid"
wid_a="$(wait_visible_window_by_pid "$pid_a" || wait_visible_window_by_name '^ewmh-a$' || true)"
[ -n "$wid_a" ] || fail "failed to open first test window"

spawn_xterm "b" -title "ewmh-b" -class st -geometry 80x24+280+60
pid_b="$last_spawn_pid"
wid_b="$(wait_visible_window_by_pid "$pid_b" || wait_visible_window_by_name '^ewmh-b$' || true)"
[ -n "$wid_b" ] || fail "failed to open second test window"

hex_a="0x$(printf '%x' "$wid_a")"
hex_b="0x$(printf '%x' "$wid_b")"

# client lists should include both windows
assert_prop_contains root _NET_CLIENT_LIST "$hex_a"
assert_prop_contains root _NET_CLIENT_LIST "$hex_b"
assert_prop_contains root _NET_CLIENT_LIST_STACKING "$hex_a"
assert_prop_contains root _NET_CLIENT_LIST_STACKING "$hex_b"
wait_prop_order root _NET_CLIENT_LIST "$hex_a" "$hex_b" || fail "_NET_CLIENT_LIST did not preserve initial mapping order"
wait_prop_order root _NET_CLIENT_LIST_STACKING "$hex_a" "$hex_b" || fail "_NET_CLIENT_LIST_STACKING did not reflect initial stacking order"

# _NET_RESTACK_WINDOW explicit ordering
"${MSG_TOOL}" "$wid_a" _NET_RESTACK_WINDOW 0 "$wid_b" 1 0 0 || fail "failed to send _NET_RESTACK_WINDOW below"
sleep 0.2
wait_prop_order root _NET_CLIENT_LIST_STACKING "$hex_a" "$hex_b" || fail "restack below did not update stacking order"

"${MSG_TOOL}" "$wid_a" _NET_RESTACK_WINDOW 0 "$wid_b" 0 0 0 || fail "failed to send _NET_RESTACK_WINDOW above"
sleep 0.2
wait_prop_order root _NET_CLIENT_LIST_STACKING "$hex_b" "$hex_a" || fail "restack above did not update stacking order"

"${MSG_TOOL}" "$wid_a" _NET_WM_STATE 1 _NET_WM_STATE_FULLSCREEN 0 1 || fail "failed to send fullscreen-add client message"
sleep 0.3
wait_prop_order root _NET_CLIENT_LIST "$hex_a" "$hex_b" || fail "_NET_CLIENT_LIST changed after raising a window"
wait_prop_order root _NET_CLIENT_LIST_STACKING "$hex_b" "$hex_a" || fail "_NET_CLIENT_LIST_STACKING did not update after fullscreen raise"

"${MSG_TOOL}" "$wid_a" _NET_WM_STATE 0 _NET_WM_STATE_FULLSCREEN 0 1 || fail "failed to send fullscreen-remove client message"
sleep 0.3

# active window should track focus changes
xdotool windowactivate "$wid_a" >/dev/null 2>&1 || true
sleep 0.2
assert_prop_contains root _NET_ACTIVE_WINDOW "$hex_a"
assert_prop_contains "$wid_a" _NET_WM_STATE _NET_WM_STATE_FOCUSED

# _NET_REQUEST_FRAME_EXTENTS + _NET_WM_ALLOWED_ACTIONS
"${MSG_TOOL}" "$wid_a" _NET_REQUEST_FRAME_EXTENTS 0 0 0 0 0 || fail "failed to send _NET_REQUEST_FRAME_EXTENTS"
sleep 0.2
assert_prop_contains "$wid_a" _NET_FRAME_EXTENTS ","
assert_prop_contains "$wid_a" _NET_WM_ALLOWED_ACTIONS _NET_WM_ACTION_MOVE
assert_prop_contains "$wid_a" _NET_WM_ALLOWED_ACTIONS _NET_WM_ACTION_FULLSCREEN

# _NET_MOVERESIZE_WINDOW should move/resize geometry
"${MSG_TOOL}" "$wid_a" _NET_MOVERESIZE_WINDOW 3840 120 140 640 360 || fail "failed to send _NET_MOVERESIZE_WINDOW"
wait_window_geometry "$wid_a" 120 140 640 360 || fail "_NET_MOVERESIZE_WINDOW did not apply requested geometry"

# demands-attention should persist while unfocused and clear on focus
xdotool windowactivate "$wid_b" >/dev/null 2>&1 || true
sleep 0.2
"${MSG_TOOL}" "$wid_a" _NET_WM_STATE 1 _NET_WM_STATE_DEMANDS_ATTENTION 0 1 || fail "failed to add demands-attention"
sleep 0.2
assert_prop_contains "$wid_a" _NET_WM_STATE _NET_WM_STATE_DEMANDS_ATTENTION
xdotool windowactivate "$wid_a" >/dev/null 2>&1 || true
sleep 0.2
assert_prop_not_contains "$wid_a" _NET_WM_STATE _NET_WM_STATE_DEMANDS_ATTENTION

xdotool windowactivate "$wid_b" >/dev/null 2>&1 || true
sleep 0.2
assert_prop_contains root _NET_ACTIVE_WINDOW "$hex_b"
assert_prop_contains "$wid_b" _NET_WM_STATE _NET_WM_STATE_FOCUSED
assert_prop_not_contains "$wid_a" _NET_WM_STATE _NET_WM_STATE_FOCUSED

# move workspace via keybinding and verify root property
xdotool key --clearmodifiers super+2 >/dev/null 2>&1 || true
sleep 0.3
cur_ws="$(root_prop_int _NET_CURRENT_DESKTOP)"
assert_int_eq "$cur_ws" "1" "workspace switch to 2 failed"

xdotool key --clearmodifiers super+1 >/dev/null 2>&1 || true
sleep 0.3
cur_ws="$(root_prop_int _NET_CURRENT_DESKTOP)"
assert_int_eq "$cur_ws" "0" "workspace switch to 1 failed"
xdotool windowmap "$wid_b" >/dev/null 2>&1 || true
sleep 0.2
wait_window_map_state "$wid_b" "IsViewable" || fail "second test window is not viewable before strut/workarea checks"

# strut/workarea invariant: changing dock strut should change workarea
workarea_before="$(xprop -root _NET_WORKAREA 2>/dev/null | sed -n 's/.*= //p' | head -n1)"

xprop -id "$wid_b" -f _NET_WM_WINDOW_TYPE 32a -set _NET_WM_WINDOW_TYPE _NET_WM_WINDOW_TYPE_DOCK >/dev/null 2>&1 || true
xprop -id "$wid_b" -f _NET_WM_STRUT_PARTIAL 32c -set _NET_WM_STRUT_PARTIAL "0,0,48,0,0,0,0,0,0,1279,0,0" >/dev/null 2>&1 || true
sleep 0.4
assert_prop_contains "$wid_b" _NET_WM_WINDOW_TYPE _NET_WM_WINDOW_TYPE_DOCK
assert_prop_contains "$wid_b" _NET_WM_STRUT_PARTIAL "48"
xdotool key --clearmodifiers super+2 >/dev/null 2>&1 || true
sleep 0.2
xdotool key --clearmodifiers super+1 >/dev/null 2>&1 || true
sleep 0.2
wait_window_map_state "$wid_b" "IsViewable" || fail "dock test window is not viewable after workspace toggle"

workarea_after="$(xprop -root _NET_WORKAREA 2>/dev/null | sed -n 's/.*= //p' | head -n1)"
[ -n "$workarea_before" ] || fail "could not read _NET_WORKAREA baseline"
[ -n "$workarea_after" ] || fail "could not read _NET_WORKAREA updated value"
[ "$workarea_before" != "$workarea_after" ] || fail "_NET_WORKAREA did not change after dock strut update"

# _NET_WM_STATE modal add/remove handling
spawn_xterm "modal" -title "ewmh-modal" -class st -geometry 80x24+500+100
pid_modal="$last_spawn_pid"
wid_modal="$(wait_visible_window_by_pid "$pid_modal" || wait_visible_window_by_name '^ewmh-modal$' || true)"
[ -n "$wid_modal" ] || fail "failed to open modal test window"

"${MSG_TOOL}" "$wid_modal" _NET_WM_STATE 1 _NET_WM_STATE_MODAL 0 1 || fail "failed to send modal-add client message"
sleep 0.3
assert_prop_contains "$wid_modal" _NET_WM_STATE _NET_WM_STATE_MODAL

"${MSG_TOOL}" "$wid_modal" _NET_WM_STATE 0 _NET_WM_STATE_MODAL 0 1 || fail "failed to send modal-remove client message"
sleep 0.3
assert_prop_not_contains "$wid_modal" _NET_WM_STATE _NET_WM_STATE_MODAL

# additional _NET_WM_STATE coverage: maximize
spawn_xterm "max" -title "ewmh-max" -class st -geometry 80x24+540+140
pid_max="$last_spawn_pid"
wid_max="$(wait_visible_window_by_pid "$pid_max" || wait_visible_window_by_name '^ewmh-max$' || true)"
[ -n "$wid_max" ] || fail "failed to open maximize test window"

"${MSG_TOOL}" "$wid_max" _NET_WM_STATE 1 _NET_WM_STATE_MAXIMIZED_HORZ _NET_WM_STATE_MAXIMIZED_VERT 1 || fail "failed to send maximize-add client message"
sleep 0.3
assert_prop_contains "$wid_max" _NET_WM_STATE _NET_WM_STATE_MAXIMIZED_HORZ
assert_prop_contains "$wid_max" _NET_WM_STATE _NET_WM_STATE_MAXIMIZED_VERT

"${MSG_TOOL}" "$wid_max" _NET_WM_STATE 0 _NET_WM_STATE_MAXIMIZED_HORZ _NET_WM_STATE_MAXIMIZED_VERT 1 || fail "failed to send maximize-remove client message"
sleep 0.3
assert_prop_not_contains "$wid_max" _NET_WM_STATE _NET_WM_STATE_MAXIMIZED_HORZ
assert_prop_not_contains "$wid_max" _NET_WM_STATE _NET_WM_STATE_MAXIMIZED_VERT

# additional _NET_WM_STATE coverage: hidden
spawn_xterm "hidden" -title "ewmh-hidden" -class st -geometry 80x24+700+160
pid_hidden="$last_spawn_pid"
wid_hidden="$(wait_visible_window_by_pid "$pid_hidden" || wait_visible_window_by_name '^ewmh-hidden$' || true)"
[ -n "$wid_hidden" ] || fail "failed to open hidden-state test window"

"${MSG_TOOL}" "$wid_hidden" _NET_WM_STATE 1 _NET_WM_STATE_HIDDEN 0 1 || fail "failed to send hidden-add client message"
sleep 0.3
assert_prop_contains "$wid_hidden" _NET_WM_STATE _NET_WM_STATE_HIDDEN
wait_window_map_state "$wid_hidden" "IsUnMapped" || fail "_NET_WM_STATE_HIDDEN did not unmap the target window"

"${MSG_TOOL}" "$wid_hidden" _NET_WM_STATE 0 _NET_WM_STATE_HIDDEN 0 1 || fail "failed to send hidden-remove client message"
sleep 0.3
assert_prop_not_contains "$wid_hidden" _NET_WM_STATE _NET_WM_STATE_HIDDEN
wait_window_map_state "$wid_hidden" "IsViewable" || fail "hidden window did not remap after removing _NET_WM_STATE_HIDDEN"

# advisory states should be toggled in _NET_WM_STATE
"${MSG_TOOL}" "$wid_modal" _NET_WM_STATE 1 _NET_WM_STATE_SKIP_TASKBAR 0 1 || fail "failed to send skip-taskbar-add client message"
sleep 0.2
assert_prop_contains "$wid_modal" _NET_WM_STATE _NET_WM_STATE_SKIP_TASKBAR
"${MSG_TOOL}" "$wid_modal" _NET_WM_STATE 0 _NET_WM_STATE_SKIP_TASKBAR 0 1 || fail "failed to send skip-taskbar-remove client message"
sleep 0.2
assert_prop_not_contains "$wid_modal" _NET_WM_STATE _NET_WM_STATE_SKIP_TASKBAR

# _NET_WM_DESKTOP all-desktops request should sync sticky state
spawn_xterm "sticky" -title "ewmh-sticky" -class st -geometry 80x24+760+180
pid_sticky="$last_spawn_pid"
wid_sticky="$(wait_visible_window_by_pid "$pid_sticky" || wait_visible_window_by_name '^ewmh-sticky$' || true)"
[ -n "$wid_sticky" ] || fail "failed to open sticky-state test window"

"${MSG_TOOL}" "$wid_sticky" _NET_WM_DESKTOP 0xffffffff 0 0 1 || fail "failed to send _NET_WM_DESKTOP all-desktops request"
sleep 0.3
assert_prop_contains "$wid_sticky" _NET_WM_STATE _NET_WM_STATE_STICKY

"${MSG_TOOL}" "$wid_sticky" _NET_WM_DESKTOP 0 0 0 1 || fail "failed to clear sticky desktop assignment"
sleep 0.3
assert_prop_not_contains "$wid_sticky" _NET_WM_STATE _NET_WM_STATE_STICKY

# _NET_CLOSE_WINDOW handling
spawn_xterm "close" -title "ewmh-close" -class st -geometry 80x24+620+120
pid_close="$last_spawn_pid"
wid_close="$(wait_visible_window_by_pid "$pid_close" || wait_visible_window_by_name '^ewmh-close$' || true)"
[ -n "$wid_close" ] || fail "failed to open close-window test window"

"${MSG_TOOL}" "$wid_close" _NET_CLOSE_WINDOW 0 0 0 0 0 || fail "failed to send _NET_CLOSE_WINDOW"
wait_window_gone "$wid_close" || fail "_NET_CLOSE_WINDOW did not close the target window"

echo "ewmh invariants passed"

#!/usr/bin/env bash
set -euo pipefail

WM_BIN="${1:-./cupidwm}"
HOST_DISPLAY="${DISPLAY:-}"
DISPLAY_NUM="${DISPLAY_NUM:-99}"
TEST_DISPLAY=""

if [ -z "${HOST_DISPLAY}" ]; then
	echo "smoke test failed: host DISPLAY is not set (run from an X session, or preserve DISPLAY when using sudo)" >&2
	exit 1
fi

if [ -n "${LOG_DIR:-}" ]; then
	mkdir -p "${LOG_DIR}" 2>/dev/null || {
		echo "smoke test failed: unable to create LOG_DIR=${LOG_DIR}" >&2
		exit 1
	}
else
	LOG_DIR="$(mktemp -d "${TMPDIR:-/tmp}/cupidwm-smoke.XXXXXX")"
fi

if [ ! -w "${LOG_DIR}" ]; then
	echo "smoke test failed: log directory is not writable: ${LOG_DIR}" >&2
	exit 1
fi

need_cmd() {
	if ! command -v "$1" >/dev/null 2>&1; then
		echo "missing required command: $1" >&2
		exit 1
	fi
}

for cmd in Xephyr xdpyinfo xprop xdotool xterm xwininfo awk sed grep mktemp; do
	need_cmd "${cmd}"
done

xephyr_pid=""
wm_pid=""
last_spawn_pid=""
declare -a app_pids=()

cleanup() {
	for pid in "${app_pids[@]}"; do
		if [ -n "${pid}" ] && kill -0 "${pid}" 2>/dev/null; then
			kill "${pid}" 2>/dev/null || true
		fi
	done
	if [ -n "${wm_pid}" ] && kill -0 "${wm_pid}" 2>/dev/null; then
		kill "${wm_pid}" 2>/dev/null || true
	fi
	if [ -n "${xephyr_pid}" ] && kill -0 "${xephyr_pid}" 2>/dev/null; then
		kill "${xephyr_pid}" 2>/dev/null || true
	fi
}
trap cleanup EXIT INT TERM

fail() {
	printf 'smoke test failed: %s\n' "$*" >"${LOG_DIR}/failure.log" 2>/dev/null || true
	echo "smoke test failed: $*" >&2
	echo "xephyr log: ${LOG_DIR}/xephyr.log" >&2
	echo "wm log: ${LOG_DIR}/wm.log" >&2
	echo "logs dir: ${LOG_DIR}" >&2
	exit 1
}

wm_alive() {
	if ! kill -0 "${wm_pid}" 2>/dev/null; then
		fail "window manager exited unexpectedly"
	fi
}

wait_for_display() {
	for _ in $(seq 1 100); do
		if xdpyinfo >/dev/null 2>&1; then
			return 0
		fi
		if [ -n "${xephyr_pid}" ] && ! kill -0 "${xephyr_pid}" 2>/dev/null; then
			return 1
		fi
		sleep 0.1
	done
	return 1
}

choose_test_display() {
	local num="${DISPLAY_NUM}"
	for _ in $(seq 1 50); do
		if [ ! -e "/tmp/.X${num}-lock" ]; then
			if ! DISPLAY=":${num}" xdpyinfo >/dev/null 2>&1; then
				TEST_DISPLAY=":${num}"
				export DISPLAY="${TEST_DISPLAY}"
				return 0
			fi
		fi
		num=$((num + 1))
	done
	return 1
}

is_viewable_id() {
	local wid="$1"
	xwininfo -id "${wid}" 2>/dev/null | grep -q "Map State: IsViewable"
}

wait_visible_id() {
	local wid="$1"
	for _ in $(seq 1 100); do
		if is_viewable_id "${wid}"; then
			echo "${wid}"
			return 0
		fi
		sleep 0.1
	done
	return 1
}

wait_invisible_id() {
	local wid="$1"
	for _ in $(seq 1 100); do
		if ! is_viewable_id "${wid}"; then
			return 0
		fi
		sleep 0.1
	done
	return 1
}

wait_visible_window_by_name() {
	local pattern="$1"
	local wid=""
	for _ in $(seq 1 100); do
		wid="$(xdotool search --onlyvisible --name "${pattern}" 2>/dev/null | head -n1 || true)"
		if [ -n "${wid}" ]; then
			echo "${wid}"
			return 0
		fi
		sleep 0.1
	done
	return 1
}

wait_visible_window_by_pid() {
	local pid="$1"
	local wid=""
	for _ in $(seq 1 100); do
		if ! kill -0 "${pid}" 2>/dev/null; then
			return 1
		fi
		wid="$(xdotool search --onlyvisible --pid "${pid}" 2>/dev/null | head -n1 || true)"
		if [ -n "${wid}" ]; then
			echo "${wid}"
			return 0
		fi
		sleep 0.1
	done
	return 1
}

root_current_desktop() {
	xprop -root _NET_CURRENT_DESKTOP 2>/dev/null | awk -F' = ' '/_NET_CURRENT_DESKTOP/{gsub(/[[:space:]]/, "", $2); print $2; exit}'
}

root_active_window() {
	xprop -root _NET_ACTIVE_WINDOW 2>/dev/null | awk -F' = ' '/_NET_ACTIVE_WINDOW/ {
		gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2);
		if ($2 ~ /^0x[0-9a-fA-F]+$/) print tolower($2);
		exit
	}'
}

wait_active_window() {
	local want="$1"
	for _ in $(seq 1 100); do
		local got=""
		got="$(root_active_window || true)"
		if [ -n "${got}" ] && [ "${got}" = "${want}" ]; then
			echo "${got}"
			return 0
		fi
		sleep 0.05
	done
	return 1
}

focused_window_hex() {
	local got=""
	got="$(xdotool getwindowfocus 2>/dev/null || true)"
	if [ -n "${got}" ] && [ "${got}" -gt 0 ] 2>/dev/null; then
		printf '0x%x\n' "${got}" | tr '[:upper:]' '[:lower:]'
		return 0
	fi
	return 1
}

wait_focus_window() {
	local want="$1"
	for _ in $(seq 1 100); do
		local root_got=""
		local focus_got=""
		root_got="$(root_active_window || true)"
		focus_got="$(focused_window_hex || true)"
		if [ "${root_got}" = "${want}" ] || [ "${focus_got}" = "${want}" ]; then
			echo "${want}"
			return 0
		fi
		sleep 0.05
	done
	return 1
}

wait_not_focused_window() {
	local reject="$1"
	for _ in $(seq 1 120); do
		local root_got=""
		local focus_got=""
		root_got="$(root_active_window || true)"
		focus_got="$(focused_window_hex || true)"
		if [ "${root_got}" != "${reject}" ] && [ "${focus_got}" != "${reject}" ]; then
			return 0
		fi
		sleep 0.05
	done
	return 1
}

wait_current_desktop() {
	local want="$1"
	for _ in $(seq 1 100); do
		if [ "$(root_current_desktop)" = "${want}" ]; then
			return 0
		fi
		sleep 0.1
	done
	return 1
}

window_desktop() {
	local wid="$1"
	xprop -id "${wid}" _NET_WM_DESKTOP 2>/dev/null | awk -F' = ' '/_NET_WM_DESKTOP/{gsub(/[[:space:]]/, "", $2); print $2; exit}'
}

monitor_count() {
	local vals
	local fields
	vals="$(xprop -root _NET_WORKAREA 2>/dev/null | sed -n 's/.*= //p')"
	if [ -z "${vals}" ]; then
		echo 0
		return 0
	fi
	fields="$(echo "${vals}" | awk -F',' '{print NF}')"
	if [ -z "${fields}" ] || [ "${fields}" -lt 4 ]; then
		echo 0
		return 0
	fi
	echo $((fields / 4))
}

send_key() {
	local key="$1"
	xdotool key --clearmodifiers "${key}" >/dev/null 2>&1 || true
	sleep 0.25
}

focus_window() {
	local wid="$1"
	xdotool windowactivate "${wid}" >/dev/null 2>&1 || true
	sleep 0.25
}

spawn_xterm() {
	local tag="$1"
	shift
	xterm "$@" >"${LOG_DIR}/xterm-${tag}.log" 2>&1 &
	last_spawn_pid="$!"
	app_pids+=("${last_spawn_pid}")
}

launch_xephyr() {
	local dual="${1:-1}"
	if [ "${dual}" = "1" ]; then
		DISPLAY="${HOST_DISPLAY}" Xephyr "${TEST_DISPLAY}" -screen 960x720+0+0 -screen 960x720+960+0 -ac -nolisten tcp >"${LOG_DIR}/xephyr.log" 2>&1 &
	else
		DISPLAY="${HOST_DISPLAY}" Xephyr "${TEST_DISPLAY}" -screen 1920x720 -ac -nolisten tcp >"${LOG_DIR}/xephyr.log" 2>&1 &
	fi
	xephyr_pid=$!
}

choose_test_display || fail "could not find a free X display starting at :${DISPLAY_NUM}"
launch_xephyr 1
if ! wait_for_display; then
	if [ -n "${xephyr_pid}" ] && kill -0 "${xephyr_pid}" 2>/dev/null; then
		kill "${xephyr_pid}" 2>/dev/null || true
	fi
	launch_xephyr 0
fi
wait_for_display || fail "Xephyr did not become ready"

"${WM_BIN}" >"${LOG_DIR}/wm.log" 2>&1 &
wm_pid=$!
sleep 1
wm_alive

xprop -root _NET_SUPPORTING_WM_CHECK >"${LOG_DIR}/root-props.log" 2>&1 || true
grep -q "_NET_SUPPORTING_WM_CHECK" "${LOG_DIR}/root-props.log" || fail "EWMH root property not set"

xprop -root _NET_CLIENT_LIST >"${LOG_DIR}/client-list-init.log" 2>&1 || true
grep -q "_NET_CLIENT_LIST" "${LOG_DIR}/client-list-init.log" || fail "_NET_CLIENT_LIST missing"
xprop -root _NET_CLIENT_LIST_STACKING >"${LOG_DIR}/client-list-stacking-init.log" 2>&1 || true
grep -q "_NET_CLIENT_LIST_STACKING" "${LOG_DIR}/client-list-stacking-init.log" || fail "_NET_CLIENT_LIST_STACKING missing"

spawn_xterm "ws-a" -title "ws-a" -class st -geometry 80x24+50+50
pid_a="${last_spawn_pid}"
wid_a="$(wait_visible_window_by_pid "${pid_a}" || wait_visible_window_by_name '^ws-a$' || true)"
[ -n "${wid_a}" ] || fail "workspace test window did not appear"
wait_visible_id "${wid_a}" >/dev/null || fail "workspace test window is not viewable"

focus_window "${wid_a}"
sleep 0.1
wid_a_hex="0x$(printf '%x' "${wid_a}")"
wait_focus_window "${wid_a_hex}" >/dev/null || true

spawn_xterm "rule-substr-nofocus" -title "my-cupidwm-substr-nofocus-window" -class st -geometry 80x24+120+120
pid_rule="${last_spawn_pid}"
wid_rule="$(wait_visible_window_by_pid "${pid_rule}" || wait_visible_window_by_name '^my-cupidwm-substr-nofocus-window$' || true)"
[ -n "${wid_rule}" ] || fail "rule no-focus test window did not appear"
wait_visible_id "${wid_rule}" >/dev/null || fail "rule no-focus test window is not viewable"

sleep 0.2
wid_rule_hex="0x$(printf '%x' "${wid_rule}")"
wait_not_focused_window "${wid_rule_hex}" || fail "no-focus rule window became focused after map"

	focus_window "${wid_a}"
send_key "super+2"
[ "$(root_current_desktop)" = "1" ] || fail "workspace switch to 2 failed"
send_key "super+1"
[ "$(root_current_desktop)" = "0" ] || fail "workspace switch back to 1 failed"

focus_window "${wid_a}"
send_key "super+shift+3"
[ "$(window_desktop "${wid_a}")" = "2" ] || fail "move-to-workspace (3) failed"
send_key "super+3"
[ "$(root_current_desktop)" = "2" ] || fail "workspace switch to 3 failed"
wait_visible_id "${wid_a}" >/dev/null || fail "moved window not visible on workspace 3"
xdotool windowunmap "${wid_a}" >/dev/null 2>&1 || fail "failed to unmap visible window"
sleep 0.25
wait_invisible_id "${wid_a}" || fail "visible window did not unmap"
send_key "super+1"
send_key "super+3"
wait_current_desktop "2" || fail "workspace switch to 3 before remap check failed"
wait_invisible_id "${wid_a}" || fail "explicit unmap state was lost on workspace switch"
xdotool windowmap "${wid_a}" >/dev/null 2>&1 || fail "failed to remap explicitly unmapped window"
wait_visible_id "${wid_a}" >/dev/null || fail "remapped explicitly unmapped window did not become visible"
send_key "super+1"
wait_current_desktop "0" || fail "workspace switch to 1 before scratch tests failed"

spawn_xterm "scratch" -title "scratch-one" -class st -geometry 80x24+180+80
pid_sp="${last_spawn_pid}"
wid_sp="$(wait_visible_window_by_pid "${pid_sp}" || wait_visible_window_by_name '^scratch-one$' || true)"
[ -n "${wid_sp}" ] || fail "scratchpad test window did not appear"
focus_window "${wid_sp}"
send_key "super+alt+1"
wait_invisible_id "${wid_sp}" || fail "scratchpad create/hide failed"
send_key "super+ctrl+1"
wait_visible_id "${wid_sp}" >/dev/null || fail "scratchpad toggle restore failed"

spawn_xterm "scratch-replace" -title "scratch-two" -class st -geometry 80x24+220+120
pid_sp2="${last_spawn_pid}"
wid_sp2="$(wait_visible_window_by_pid "${pid_sp2}" || wait_visible_window_by_name '^scratch-two$' || true)"
[ -n "${wid_sp2}" ] || fail "scratchpad replacement window did not appear"
focus_window "${wid_sp2}"
send_key "super+alt+1"
wait_invisible_id "${wid_sp2}" || fail "replacement scratchpad client was not hidden"
wait_visible_id "${wid_sp}" >/dev/null || fail "previous scratchpad client did not remap after slot replacement"
focus_window "${wid_sp}"
send_key "super+alt+2"
wait_invisible_id "${wid_sp}" || fail "remapped scratchpad client was not interactable"
send_key "super+ctrl+2"
wait_visible_id "${wid_sp}" >/dev/null || fail "scratchpad interactability regression after slot replacement"

send_key "super+1"
spawn_xterm "swallower" -title "swallower" -class st -geometry 80x24+300+130
pid_sw="${last_spawn_pid}"
wid_sw="$(wait_visible_window_by_pid "${pid_sw}" || wait_visible_window_by_name '^swallower$' || true)"
[ -n "${wid_sw}" ] || fail "swallow swallower window did not appear"
focus_window "${wid_sw}"

spawn_xterm "swallow-target" -title "swallow-target" -class thunar -geometry 80x24+360+180
pid_tg="${last_spawn_pid}"
wid_tg="$(wait_visible_window_by_pid "${pid_tg}" || wait_visible_window_by_name '^swallow-target$' || true)"
[ -n "${wid_tg}" ] || fail "swallow target window did not appear"

swallowed_ok=0
for _ in $(seq 1 80); do
	sw_viewable=0
	tg_viewable=0
	if is_viewable_id "${wid_sw}"; then
		sw_viewable=1
	fi
	if is_viewable_id "${wid_tg}"; then
		tg_viewable=1
	fi
	if [ "${sw_viewable}" = "0" ] && [ "${tg_viewable}" = "1" ]; then
		swallowed_ok=1
		break
	fi
	sleep 0.1
done
[ "${swallowed_ok}" = "1" ] || fail "swallowing behavior did not trigger as expected"

mons="$(monitor_count)"
if [ "${mons}" -ge 2 ]; then
	send_key "super+1"
	spawn_xterm "mon-left" -title "mon-left" -class st -geometry 80x24+80+80
	pid_left="${last_spawn_pid}"
	wid_left="$(wait_visible_window_by_pid "${pid_left}" || wait_visible_window_by_name '^mon-left$' || true)"
	[ -n "${wid_left}" ] || fail "left-monitor workspace test window did not appear"
	wait_visible_id "${wid_left}" >/dev/null || fail "left-monitor workspace test window is not viewable"

	send_key "super+period"
	spawn_xterm "mon-right" -title "mon-right" -class st -geometry 80x24+1080+80
	pid_right="${last_spawn_pid}"
	wid_right="$(wait_visible_window_by_pid "${pid_right}" || wait_visible_window_by_name '^mon-right$' || true)"
	[ -n "${wid_right}" ] || fail "right-monitor workspace test window did not appear"
	wait_visible_id "${wid_right}" >/dev/null || fail "right-monitor workspace test window is not viewable"

	send_key "super+3"
	wait_invisible_id "${wid_right}" || fail "right-monitor workspace switch did not hide the right monitor window"
	wait_visible_id "${wid_left}" >/dev/null || fail "switching the right monitor workspace affected the left monitor window"

	send_key "super+period"
	wait_current_desktop "0" || fail "left monitor did not keep its own workspace after switching the right monitor"
	wait_visible_id "${wid_left}" >/dev/null || fail "left monitor window disappeared after cross-monitor workspace switch"

	focus_window "${wid_tg}"
	x_before="$(xwininfo -id "${wid_tg}" | awk '/Absolute upper-left X:/ {print $4; exit}')"
	send_key "super+period"
	wm_alive
	send_key "super+shift+period"
	wm_alive
	x_after="$(xwininfo -id "${wid_tg}" | awk '/Absolute upper-left X:/ {print $4; exit}')"
	if [ -n "${x_before}" ] && [ -n "${x_after}" ] && [ "${x_before}" = "${x_after}" ]; then
		fail "monitor move test did not change window geometry on multi-monitor setup"
	fi
else
	echo "monitor move/focus checks skipped (single monitor Xephyr setup)" >"${LOG_DIR}/monitor-skip.log"
	send_key "super+period"
	send_key "super+shift+period"
	wm_alive
fi

xprop -root _NET_CLIENT_LIST >"${LOG_DIR}/client-list-final.log" 2>&1 || true
grep -q "_NET_CLIENT_LIST" "${LOG_DIR}/client-list-final.log" || fail "_NET_CLIENT_LIST missing at end of smoke run"
xprop -root _NET_CLIENT_LIST_STACKING >"${LOG_DIR}/client-list-stacking-final.log" 2>&1 || true
grep -q "_NET_CLIENT_LIST_STACKING" "${LOG_DIR}/client-list-stacking-final.log" || fail "_NET_CLIENT_LIST_STACKING missing at end of smoke run"

wm_alive

echo "smoke test passed"

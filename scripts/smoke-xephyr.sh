#!/usr/bin/env bash
set -euo pipefail

WM_BIN="${1:-./cupidwm}"
DISPLAY_NUM="${DISPLAY_NUM:-99}"
export DISPLAY=":${DISPLAY_NUM}"

LOG_DIR="${TMPDIR:-/tmp}/cupidwm-smoke"
mkdir -p "${LOG_DIR}"

need_cmd() {
	if ! command -v "$1" >/dev/null 2>&1; then
		echo "missing required command: $1" >&2
		exit 1
	fi
}

for cmd in Xephyr xdpyinfo xprop xdotool xterm xwininfo awk sed grep; do
	need_cmd "${cmd}"
done

xephyr_pid=""
wm_pid=""
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

root_current_desktop() {
	xprop -root _NET_CURRENT_DESKTOP 2>/dev/null | awk -F' = ' '/_NET_CURRENT_DESKTOP/{gsub(/[[:space:]]/, "", $2); print $2; exit}'
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
	app_pids+=("$!")
}

launch_xephyr() {
	local dual="${1:-1}"
	if [ "${dual}" = "1" ]; then
		Xephyr "${DISPLAY}" -screen 960x720+0+0 -screen 960x720+960+0 -ac -nolisten tcp >"${LOG_DIR}/xephyr.log" 2>&1 &
	else
		Xephyr "${DISPLAY}" -screen 1920x720 -ac -nolisten tcp >"${LOG_DIR}/xephyr.log" 2>&1 &
	fi
	xephyr_pid=$!
}

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

spawn_xterm "ws-a" -title "ws-a" -class st -geometry 80x24+50+50
wid_a="$(wait_visible_window_by_name '^ws-a$' || true)"
[ -n "${wid_a}" ] || fail "workspace test window did not appear"
wait_visible_id "${wid_a}" >/dev/null || fail "workspace test window is not viewable"

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
send_key "super+1"

spawn_xterm "scratch" -title "scratch-one" -class st -geometry 80x24+180+80
wid_sp="$(wait_visible_window_by_name '^scratch-one$' || true)"
[ -n "${wid_sp}" ] || fail "scratchpad test window did not appear"
focus_window "${wid_sp}"
send_key "super+alt+1"
wait_invisible_id "${wid_sp}" || fail "scratchpad create/hide failed"
send_key "super+ctrl+1"
wait_visible_id "${wid_sp}" >/dev/null || fail "scratchpad toggle restore failed"

send_key "super+1"
spawn_xterm "swallower" -title "swallower" -class st -geometry 80x24+300+130
wid_sw="$(wait_visible_window_by_name '^swallower$' || true)"
[ -n "${wid_sw}" ] || fail "swallow swallower window did not appear"
focus_window "${wid_sw}"

spawn_xterm "swallow-target" -title "swallow-target" -class thunar -geometry 80x24+360+180
wid_tg="$(wait_visible_window_by_name '^swallow-target$' || true)"
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

wm_alive

echo "smoke test passed"
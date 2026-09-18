#!/usr/bin/env bash
set -euo pipefail

WM_BIN="${1:-./cupidwm}"
HOST_DISPLAY="${DISPLAY:-}"
TEST_DISPLAY=""
CC="${CC:-cc}"
if [ -n "${LOG_DIR:-}" ]; then
	mkdir -p "${LOG_DIR}"
else
	LOG_DIR="$(mktemp -d "${TMPDIR:-/tmp}/cupidwm-input.XXXXXX")"
fi
RUNTIME_DIR="${LOG_DIR}/runtime"
mkdir -p "${RUNTIME_DIR}"

xephyr_pid=""
wm_pid=""
sink_pid=""
failures=0

cleanup() {
	for pid in "${sink_pid}" "${wm_pid}" "${xephyr_pid}"; do
		if [ -n "${pid}" ] && kill -0 "${pid}" 2>/dev/null; then
			kill "${pid}" 2>/dev/null || true
		fi
	done
	for pid in "${sink_pid}" "${wm_pid}" "${xephyr_pid}"; do
		[ -z "${pid}" ] || wait "${pid}" 2>/dev/null || true
	done
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

need_cmd() {
	command -v "$1" >/dev/null 2>&1 || {
		echo "missing required command: $1" >&2
		exit 2
	}
}

record_failure() {
	printf 'FAIL: %s\n' "$*" | tee -a "${LOG_DIR}/failure.log" >&2
	failures=$((failures + 1))
}

wait_for_display() {
	for _ in $(seq 1 100); do
		if timeout 2s env DISPLAY="${TEST_DISPLAY}" xdpyinfo >/dev/null 2>&1; then
			return 0
		fi
		if [ -n "${xephyr_pid}" ] && ! kill -0 "${xephyr_pid}" 2>/dev/null; then
			return 1
		fi
		sleep 0.05
	done
	return 1
}

wait_for_wm() {
	for _ in $(seq 1 100); do
		if DISPLAY="${TEST_DISPLAY}" xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -Eq 'window id # 0x[0-9a-fA-F]+'; then
			return 0
		fi
		if ! kill -0 "${wm_pid}" 2>/dev/null; then
			return 1
		fi
		sleep 0.05
	done
	return 1
}

wait_visible() {
	local wid="$1"
	for _ in $(seq 1 100); do
		if DISPLAY="${TEST_DISPLAY}" xwininfo -id "${wid}" 2>/dev/null | grep -q 'Map State: IsViewable'; then
			return 0
		fi
		sleep 0.05
	done
	return 1
}

is_visible() {
	DISPLAY="${TEST_DISPLAY}" xwininfo -id "$1" 2>/dev/null | grep -q 'Map State: IsViewable'
}

window_value() {
	local wid="$1"
	local field="$2"
	DISPLAY="${TEST_DISPLAY}" xwininfo -id "${wid}" 2>/dev/null | awk -v field="${field}" '$0 ~ field {print $NF; exit}'
}

focus_window_id() {
	DISPLAY="${TEST_DISPLAY}" xdotool getwindowfocus 2>/dev/null || true
}

wait_managed_focus() {
	local wid="$1"
	local hex
	printf -v hex '0x%x' "${wid}"
	for _ in $(seq 1 100); do
		if DISPLAY="${TEST_DISPLAY}" xprop -root _NET_CLIENT_LIST | grep -qw "${hex}" &&
			[ "$(focus_window_id)" = "${wid}" ]; then
			# Let the map handler's final cursor warp reach the server as well.
			sleep 0.15
			return 0
		fi
		sleep 0.05
	done
	return 1
}

pointer_value() {
	local field="$1"
	DISPLAY="${TEST_DISPLAY}" xdotool getmouselocation --shell 2>/dev/null | awk -F= -v field="${field}" '$1 == field {print $2; exit}'
}

move_pointer_stable() {
	local want_x="$1"
	local want_y="$2"
	for _ in $(seq 1 5); do
		DISPLAY="${TEST_DISPLAY}" xdotool mousemove --sync "${want_x}" "${want_y}"
		sleep 0.15
		if [ "$(pointer_value X)" = "${want_x}" ] && [ "$(pointer_value Y)" = "${want_y}" ]; then
			return 0
		fi
	done
	return 1
}

root_window_id() {
	DISPLAY="${TEST_DISPLAY}" xwininfo -root -int 2>/dev/null | awk '/Window id:/ {print $4; exit}'
}

root_desktop() {
	DISPLAY="${TEST_DISPLAY}" xprop -root _NET_CURRENT_DESKTOP 2>/dev/null | awk -F' = ' '/_NET_CURRENT_DESKTOP/ {gsub(/[[:space:]]/, "", $2); print $2; exit}'
}

window_desktop() {
	DISPLAY="${TEST_DISPLAY}" xprop -id "$1" _NET_WM_DESKTOP 2>/dev/null | awk -F' = ' '/_NET_WM_DESKTOP/ {gsub(/[[:space:]]/, "", $2); print $2; exit}'
}

move_pointer() {
	DISPLAY="${TEST_DISPLAY}" xdotool mousemove --sync "$1" "$2"
	sleep 0.1
}

send_key() {
	DISPLAY="${TEST_DISPLAY}" xdotool key --clearmodifiers "$1"
	sleep 0.15
}

for cmd in Xephyr xdpyinfo xprop xdotool xwininfo awk grep mktemp timeout "${CC}"; do
	need_cmd "${cmd}"
done

if [ -z "${HOST_DISPLAY}" ]; then
	echo "host DISPLAY is not set; run this test under xvfb-run -a" >&2
	exit 2
fi

for n in $(seq 120 169); do
	if [ ! -e "/tmp/.X${n}-lock" ] && ! timeout 1s env DISPLAY=":${n}" xdpyinfo >/dev/null 2>&1; then
		TEST_DISPLAY=":${n}"
		break
	fi
done
[ -n "${TEST_DISPLAY}" ] || {
	echo "could not find a free nested X display" >&2
	exit 2
}

"${CC}" -Wall -Wextra -Werror "$(dirname "$0")/key-sink.c" -o "${LOG_DIR}/key-sink" -lX11

DISPLAY="${HOST_DISPLAY}" Xephyr "${TEST_DISPLAY}" \
	-screen 960x720+0+0 \
	-screen 960x720+960+0 \
	+xinerama -extension RANDR -ac -nolisten tcp \
	>"${LOG_DIR}/xephyr.log" 2>&1 &
xephyr_pid=$!

wait_for_display || {
	echo "Xephyr did not become ready; log: ${LOG_DIR}/xephyr.log" >&2
	exit 1
}

heads="$(DISPLAY="${TEST_DISPLAY}" xdpyinfo -ext XINERAMA 2>/dev/null | grep -c 'head #')"
if [ "${heads}" -ne 2 ]; then
	echo "expected two Xinerama heads, got ${heads}; log: ${LOG_DIR}/xephyr.log" >&2
	exit 1
fi

DISPLAY="${TEST_DISPLAY}" XDG_RUNTIME_DIR="${RUNTIME_DIR}" "${WM_BIN}" >"${LOG_DIR}/wm.log" 2>&1 &
wm_pid=$!
wait_for_wm || {
	echo "cupidwm did not become ready; log: ${LOG_DIR}/wm.log" >&2
	exit 1
}

marker="${LOG_DIR}/keys.txt"
title="cupidwm-input-probe"
: >"${marker}"
DISPLAY="${TEST_DISPLAY}" "${LOG_DIR}/key-sink" "${marker}" "${title}" >"${LOG_DIR}/key-sink.log" 2>&1 &
sink_pid=$!

wid=""
for _ in $(seq 1 100); do
	wid="$(DISPLAY="${TEST_DISPLAY}" xdotool search --onlyvisible --name "^${title}$" 2>/dev/null | awk 'NR == 1 {print; exit}' || true)"
	[ -n "${wid}" ] && break
	sleep 0.05
done
[ -n "${wid}" ] || {
	echo "input probe did not appear; log: ${LOG_DIR}/key-sink.log" >&2
	exit 1
}
wait_visible "${wid}" || {
	echo "input probe is not viewable" >&2
	exit 1
}

wait_managed_focus "${wid}" || {
	echo "input probe was not managed and focused" >&2
	exit 1
}

probe_x="$(window_value "${wid}" 'Absolute upper-left X:')"
if [ -z "${probe_x}" ] || [ "${probe_x}" -ge 960 ]; then
	echo "input probe did not start on the left monitor" >&2
	exit 1
fi

# Issue #11: a workspace click on the right bar must not change the left monitor.
move_pointer_stable 970 10 || {
	echo "could not hold pointer on the right workspace bar" >&2
	exit 1
}
DISPLAY="${TEST_DISPLAY}" xdotool click 1
sleep 0.25

if ! is_visible "${wid}"; then
	record_failure "right-monitor workspace click changed the left monitor view"
fi

focused_now="$(focus_window_id)"
root_id="$(root_window_id)"
if [ -z "${root_id}" ] || [ "${focused_now}" != "${root_id}" ]; then
	record_failure "right-monitor workspace click did not focus the empty right monitor root"
	{
		printf 'focused=%s root=%s\n' "${focused_now}" "${root_id}"
		printf 'pointer_x=%s pointer_y=%s\n' "$(pointer_value X)" "$(pointer_value Y)"
		DISPLAY="${TEST_DISPLAY}" xprop -root _NET_ACTIVE_WINDOW 2>&1 || true
		DISPLAY="${TEST_DISPLAY}" xdpyinfo -ext XINERAMA 2>&1 | grep 'head #' || true
		DISPLAY="${TEST_DISPLAY}" xwininfo -root -tree 2>&1 || true
	} >"${LOG_DIR}/bar-focus-debug.log"
fi

if [ "$(root_desktop)" != "0" ]; then
	record_failure "right-monitor workspace click did not select workspace 1"
fi

# Restart the WM with a fresh runtime directory so the drag checks do not inherit
# any monitor state that issue #11 may have corrupted.
kill "${sink_pid}" 2>/dev/null || true
wait "${sink_pid}" 2>/dev/null || true
sink_pid=""
kill "${wm_pid}" 2>/dev/null || true
wait "${wm_pid}" 2>/dev/null || true
wm_pid=""
DISPLAY="${TEST_DISPLAY}" xprop -root -remove _NET_SUPPORTING_WM_CHECK >/dev/null 2>&1 || true

RUNTIME_DIR="${LOG_DIR}/runtime-drag"
mkdir -p "${RUNTIME_DIR}"
DISPLAY="${TEST_DISPLAY}" XDG_RUNTIME_DIR="${RUNTIME_DIR}" "${WM_BIN}" >"${LOG_DIR}/wm-drag.log" 2>&1 &
wm_pid=$!
wait_for_wm || {
	echo "cupidwm did not become ready for drag checks; log: ${LOG_DIR}/wm-drag.log" >&2
	exit 1
}

move_pointer 100 700

marker="${LOG_DIR}/drag-keys.txt"
: >"${marker}"
DISPLAY="${TEST_DISPLAY}" "${LOG_DIR}/key-sink" "${marker}" "${title}" >"${LOG_DIR}/key-sink-drag.log" 2>&1 &
sink_pid=$!

wid=""
for _ in $(seq 1 100); do
	wid="$(DISPLAY="${TEST_DISPLAY}" xdotool search --onlyvisible --name "^${title}$" 2>/dev/null | awk 'NR == 1 {print; exit}' || true)"
	[ -n "${wid}" ] && break
	sleep 0.05
done
[ -n "${wid}" ] || {
	echo "drag input probe did not appear; log: ${LOG_DIR}/key-sink-drag.log" >&2
	exit 1
}
wait_visible "${wid}" || {
	echo "drag input probe is not viewable" >&2
	exit 1
}
wait_managed_focus "${wid}" || {
	echo "drag input probe was not managed and focused" >&2
	exit 1
}

probe_x="$(window_value "${wid}" 'Absolute upper-left X:')"
if [ -z "${probe_x}" ] || [ "${probe_x}" -ge 960 ]; then
	echo "drag input probe did not start on the left monitor" >&2
	exit 1
fi

# Empty-monitor focus is expected to land on the root window.
DISPLAY="${TEST_DISPLAY}" xdotool windowactivate "${wid}" >/dev/null 2>&1 || true
sleep 0.15
send_key 'super+period'
root_id="$(root_window_id)"
focused_now="$(focus_window_id)"
if [ -z "${root_id}" ] || [ "${focused_now}" != "${root_id}" ]; then
	record_failure "focusing an empty monitor did not move input focus to the root window"
fi
send_key 'super+comma'

# Keyboard monitor moves are a positive control for workspace and input routing.
DISPLAY="${TEST_DISPLAY}" xdotool windowactivate "${wid}" >/dev/null 2>&1 || true
sleep 0.15
send_key 'super+shift+period'
wait_visible "${wid}" || record_failure "keyboard monitor move hid the probe on the right monitor"
probe_x="$(window_value "${wid}" 'Absolute upper-left X:')"
if [ -z "${probe_x}" ] || [ "${probe_x}" -lt 960 ]; then
	record_failure "keyboard monitor move did not place the probe on the right monitor"
fi
send_key 'k'
if ! grep -q 'k' "${marker}"; then
	record_failure "keyboard input did not reach the probe after keyboard monitor move"
fi
send_key 'super+shift+comma'
wait_visible "${wid}" || record_failure "keyboard monitor move back hid the probe"
probe_x="$(window_value "${wid}" 'Absolute upper-left X:')"
if [ -z "${probe_x}" ] || [ "${probe_x}" -ge 960 ]; then
	record_failure "keyboard monitor move back did not return the probe to the left monitor"
fi

# Issue #10: modifier-dragging across displays must update monitor/workspace membership.
probe_x="$(window_value "${wid}" 'Absolute upper-left X:')"
probe_y="$(window_value "${wid}" 'Absolute upper-left Y:')"
probe_w="$(window_value "${wid}" 'Width:')"
probe_h="$(window_value "${wid}" 'Height:')"
if [ -z "${probe_x}" ] || [ -z "${probe_y}" ] || [ -z "${probe_w}" ] || [ -z "${probe_h}" ]; then
	echo "failed to read probe geometry before drag" >&2
	exit 1
fi

start_x=$((probe_x + probe_w / 2))
start_y=$((probe_y + probe_h / 2))
move_pointer "${start_x}" "${start_y}"
DISPLAY="${TEST_DISPLAY}" xdotool keydown Super_L
DISPLAY="${TEST_DISPLAY}" xdotool mousedown 1
sleep 0.1
DISPLAY="${TEST_DISPLAY}" xdotool mousemove --sync 1240 280
sleep 0.2
DISPLAY="${TEST_DISPLAY}" xdotool mouseup 1
DISPLAY="${TEST_DISPLAY}" xdotool keyup Super_L
sleep 0.2

probe_x="$(window_value "${wid}" 'Absolute upper-left X:')"
probe_y="$(window_value "${wid}" 'Absolute upper-left Y:')"
probe_w="$(window_value "${wid}" 'Width:')"
probe_h="$(window_value "${wid}" 'Height:')"
if [ -z "${probe_x}" ] || [ "${probe_x}" -lt 960 ]; then
	record_failure "modifier drag did not move the probe into right-monitor geometry"
fi
if [ "$(window_desktop "${wid}")" != "1" ]; then
	record_failure "modifier drag did not attach the probe to the right monitor workspace"
fi
if [ "$(root_desktop)" != "1" ]; then
	record_failure "modifier drag did not make the right monitor workspace current"
fi

# Focus changes suppress pointer-driven monitor activation briefly. Let that expire,
# then select the left monitor by moving over its root before changing its workspace.
sleep 1.3
move_pointer 100 700
send_key 'super+3'
sleep 0.2

target_x=$((probe_x + probe_w / 2))
target_y=$((probe_y + probe_h / 2))
move_pointer "${target_x}" "${target_y}"
DISPLAY="${TEST_DISPLAY}" xdotool click 1
sleep 0.1
send_key 'x'

if ! is_visible "${wid}"; then
	record_failure "dragged probe stayed attached to the left monitor/workspace and became hidden"
fi
if ! grep -q 'x' "${marker}"; then
	record_failure "keyboard input did not reach the probe after a cross-monitor modifier drag"
fi

# A normal keyboard monitor move should still work after the drag transition.
send_key 'super+shift+comma'
sleep 0.2
probe_x="$(window_value "${wid}" 'Absolute upper-left X:')"
if ! is_visible "${wid}" || [ -z "${probe_x}" ] || [ "${probe_x}" -ge 960 ]; then
	record_failure "keyboard monitor move did not return the dragged probe to the left monitor"
fi

# Fullscreen state and restore geometry must follow a keyboard monitor transfer.
DISPLAY="${TEST_DISPLAY}" xdotool windowactivate "${wid}" >/dev/null 2>&1 || true
sleep 0.15
send_key 'super+shift+f'
send_key 'super+shift+period'
sleep 0.2

probe_x="$(window_value "${wid}" 'Absolute upper-left X:')"
probe_y="$(window_value "${wid}" 'Absolute upper-left Y:')"
probe_w="$(window_value "${wid}" 'Width:')"
probe_h="$(window_value "${wid}" 'Height:')"
if [ "${probe_x}" != "960" ] || [ "${probe_y}" != "20" ] || [ "${probe_w}" != "960" ] || [ "${probe_h}" != "700" ]; then
	record_failure "fullscreen keyboard transfer did not fill the right monitor workarea"
fi

send_key 'z'
if ! grep -q 'z' "${marker}"; then
	record_failure "keyboard input did not reach the fullscreen probe after monitor transfer"
fi

send_key 'super+shift+f'
sleep 0.2
probe_x="$(window_value "${wid}" 'Absolute upper-left X:')"
if ! is_visible "${wid}" || [ -z "${probe_x}" ] || [ "${probe_x}" -lt 960 ]; then
	record_failure "leaving fullscreen restored the probe onto the wrong monitor"
fi

if [ "${failures}" -ne 0 ]; then
	echo "two-monitor regression failed with ${failures} problem(s); logs: ${LOG_DIR}" >&2
	exit 1
fi

echo "two-monitor regression passed"

#!/usr/bin/env bash
set -euo pipefail

WM_BIN="${1:-./cupidwm}"
HOST_DISPLAY="${DISPLAY:-}"
DISPLAY_NUM="${DISPLAY_NUM:-139}"
TEST_DISPLAY=""
CC_BIN="${CC:-cc}"

SCRIPT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
LOG_DIR="${LOG_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/cupidwm-protocols.XXXXXX")}"
mkdir -p "$LOG_DIR"
PROBE_BIN="${LOG_DIR}/protocols"
xephyr_pid=""
wm_pid=""

cleanup() {
	if [ -n "$wm_pid" ] && kill -0 "$wm_pid" 2>/dev/null; then
		kill "$wm_pid" 2>/dev/null || true
	fi
	if [ -n "$xephyr_pid" ] && kill -0 "$xephyr_pid" 2>/dev/null; then
		kill "$xephyr_pid" 2>/dev/null || true
	fi
}
trap cleanup EXIT INT TERM

fail() {
	printf 'protocol test failed: %s\n' "$*" >"${LOG_DIR}/failure.log" 2>/dev/null || true
	echo "protocol test failed: $*" >&2
	exit 1
}

for cmd in Xephyr xdpyinfo mktemp timeout; do
	command -v "$cmd" >/dev/null 2>&1 || fail "missing required command: $cmd"
done
command -v "$CC_BIN" >/dev/null 2>&1 || fail "missing compiler: $CC_BIN"
[ -n "$HOST_DISPLAY" ] || fail "host DISPLAY is not set"

num="$DISPLAY_NUM"
for _ in $(seq 1 40); do
	if [ ! -e "/tmp/.X${num}-lock" ] && ! DISPLAY=":${num}" timeout 1s xdpyinfo >/dev/null 2>&1; then
		TEST_DISPLAY=":${num}"
		break
	fi
	num=$((num + 1))
done
[ -n "$TEST_DISPLAY" ] || fail "could not find a free test display"

"$CC_BIN" -std=c99 -Wall -Wextra -Werror "${SCRIPT_DIR}/protocols.c" -o "$PROBE_BIN" -lXtst -lX11 \
	|| fail "failed to build protocol probe"

DISPLAY="$HOST_DISPLAY" Xephyr "$TEST_DISPLAY" -screen 1024x640 -ac -nolisten tcp >"${LOG_DIR}/xephyr.log" 2>&1 &
xephyr_pid=$!
export DISPLAY="$TEST_DISPLAY"

for _ in $(seq 1 80); do
	if timeout 2s xdpyinfo >/dev/null 2>&1; then
		break
	fi
	sleep 0.1
done
timeout 2s xdpyinfo >/dev/null 2>&1 || fail "Xephyr did not become ready"

"$WM_BIN" >"${LOG_DIR}/wm.log" 2>&1 &
wm_pid=$!
sleep 0.5
kill -0 "$wm_pid" 2>/dev/null || fail "window manager exited early"

if ! timeout 8s "$PROBE_BIN" >"${LOG_DIR}/probe.log" 2>&1; then
	cat "${LOG_DIR}/probe.log" >&2 || true
	fail "protocol probe failed"
fi
cat "${LOG_DIR}/probe.log"

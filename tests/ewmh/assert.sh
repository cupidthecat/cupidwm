#!/usr/bin/env bash
set -euo pipefail

fail() {
	if [ -n "${LOG_DIR:-}" ]; then
		printf 'ewmh test failed: %s\n' "$*" >"${LOG_DIR}/failure.log" 2>/dev/null || true
	fi
	echo "ewmh test failed: $*" >&2
	exit 1
}

need_cmd() {
	if ! command -v "$1" >/dev/null 2>&1; then
		fail "missing required command: $1"
	fi
}

assert_prop_contains() {
	local target="$1"
	local prop="$2"
	local expect="$3"
	local out=""
	if [ "$target" = "root" ]; then
		out="$(xprop -root "$prop" 2>/dev/null || true)"
	else
		out="$(xprop -id "$target" "$prop" 2>/dev/null || true)"
	fi
	if ! echo "$out" | grep -q "$expect"; then
		fail "$prop missing expected token '$expect' (got: $out)"
	fi
}

assert_prop_not_contains() {
	local target="$1"
	local prop="$2"
	local reject="$3"
	local out=""
	if [ "$target" = "root" ]; then
		out="$(xprop -root "$prop" 2>/dev/null || true)"
	else
		out="$(xprop -id "$target" "$prop" 2>/dev/null || true)"
	fi
	if echo "$out" | grep -q "$reject"; then
		fail "$prop unexpectedly contains '$reject' (got: $out)"
	fi
}

assert_int_eq() {
	local got="$1"
	local want="$2"
	local msg="$3"
	[ "$got" = "$want" ] || fail "$msg (got=$got want=$want)"
}

root_prop_int() {
	local prop="$1"
	xprop -root "$prop" 2>/dev/null | awk -F' = ' -v p="$prop" '$0 ~ p {gsub(/[[:space:]]/, "", $2); print $2; exit}'
}

prop_hex_list() {
	local target="$1"
	local prop="$2"
	local out=""
	if [ "$target" = "root" ]; then
		out="$(xprop -root "$prop" 2>/dev/null || true)"
	else
		out="$(xprop -id "$target" "$prop" 2>/dev/null || true)"
	fi
	echo "$out" | grep -Eo '0x[0-9a-fA-F]+' || true
}

prop_order_matches() {
	local target="$1"
	local prop="$2"
	local first="$3"
	local second="$4"
	local first_idx=""
	local second_idx=""

	first_idx="$(prop_hex_list "$target" "$prop" | awk -v want="$first" '$0 == want { print NR; exit }')"
	second_idx="$(prop_hex_list "$target" "$prop" | awk -v want="$second" '$0 == want { print NR; exit }')"

	[ -n "$first_idx" ] && [ -n "$second_idx" ] && [ "$first_idx" -lt "$second_idx" ]
}

assert_prop_order() {
	local target="$1"
	local prop="$2"
	local first="$3"
	local second="$4"
	if ! prop_order_matches "$target" "$prop" "$first" "$second"; then
		fail "$prop order mismatch: expected '$first' before '$second'"
	fi
}

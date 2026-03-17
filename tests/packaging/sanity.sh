#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)"

fail() {
	echo "packaging test failed: $*" >&2
	exit 1
}

require_file() {
	local path="$1"
	[ -s "$ROOT_DIR/$path" ] || fail "missing or empty file: $path"
}

require_file "examples/sxhkd/cupidwm.sxhkdrc"
require_file "examples/xinitrc"
require_file "examples/display-manager/README.md"
require_file "completions/cupidwmctl.bash"
require_file "packaging/arch/PKGBUILD"

bash -lc '
	set -euo pipefail
	source "'"$ROOT_DIR"'/completions/cupidwmctl.bash"

	COMP_WORDS=(cupidwmctl q)
	COMP_CWORD=1
	_cupidwmctl_complete
	printf "%s\n" "${COMPREPLY[@]}" | grep -Fx "query" >/dev/null

	COMP_WORDS=(cupidwmctl query c)
	COMP_CWORD=2
	_cupidwmctl_complete
	printf "%s\n" "${COMPREPLY[@]}" | grep -Fx "clients" >/dev/null

	COMP_WORDS=(cupidwmctl layout m)
	COMP_CWORD=2
	_cupidwmctl_complete
	printf "%s\n" "${COMPREPLY[@]}" | grep -Fx "monocle" >/dev/null

	COMP_WORDS=(cupidwmctl layout g)
	COMP_CWORD=2
	_cupidwmctl_complete
	printf "%s\n" "${COMPREPLY[@]}" | grep -Fx "grid" >/dev/null

	COMP_WORDS=(cupidwmctl workspace-name s)
	COMP_CWORD=2
	_cupidwmctl_complete
	printf "%s\n" "${COMPREPLY[@]}" | grep -Fx "set" >/dev/null
'

echo "packaging sanity passed"

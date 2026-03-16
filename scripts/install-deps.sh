#!/usr/bin/env bash
set -euo pipefail

usage() {
	cat <<'EOF'
Usage: ./scripts/install-deps.sh [options]

Install cupidwm dependencies on Arch, Debian/Ubuntu, or Fedora.

Options:
  --build-only   Install build/runtime dependencies only (skip smoke-test tools)
  --yes          Non-interactive install (auto-confirm package manager prompts)
  --dry-run      Print the install command without executing it
  -h, --help     Show this help text
EOF
}

with_smoke=1
assume_yes=0
dry_run=0

while [ "$#" -gt 0 ]; do
	case "$1" in
	--build-only)
		with_smoke=0
		;;
	--yes)
		assume_yes=1
		;;
	--dry-run)
		dry_run=1
		;;
	-h|--help)
		usage
		exit 0
		;;
	*)
		echo "Unknown option: $1" >&2
		usage >&2
		exit 1
		;;
	esac
	shift
done

if [ ! -r /etc/os-release ]; then
	echo "Cannot detect distribution: /etc/os-release is missing." >&2
	exit 1
fi

# shellcheck disable=SC1091
. /etc/os-release

id="${ID:-}"
id_like="${ID_LIKE:-}"

family=""
case "$id" in
arch|manjaro)
	family="arch"
	;;
debian|ubuntu|linuxmint|pop)
	family="debian"
	;;
fedora)
	family="fedora"
	;;
*)
	if [[ "$id_like" == *arch* ]]; then
		family="arch"
	elif [[ "$id_like" == *debian* ]] || [[ "$id_like" == *ubuntu* ]]; then
		family="debian"
	elif [[ "$id_like" == *fedora* ]] || [[ "$id_like" == *rhel* ]]; then
		family="fedora"
	fi
	;;
esac

if [ -z "$family" ]; then
	echo "Unsupported distribution: ID='${id:-unknown}' ID_LIKE='${id_like:-unknown}'." >&2
	exit 1
fi

sudo_cmd=()
if [ "${EUID:-$(id -u)}" -ne 0 ]; then
	if command -v sudo >/dev/null 2>&1; then
		sudo_cmd=(sudo)
	else
		echo "Need root privileges. Re-run as root or install sudo." >&2
		exit 1
	fi
fi

run_cmd() {
	if [ "$dry_run" -eq 1 ]; then
		printf '[dry-run] '
		printf '%q ' "$@"
		printf '\n'
	else
		"$@"
	fi
}

build_pkgs=()
smoke_pkgs=()

case "$family" in
arch)
	build_pkgs=(
		xorg-server xorg-xinit base-devel pkgconf
		libx11 libxinerama libxcursor libxft fontconfig freetype2
	)
	smoke_pkgs=(
		xorg-server-xephyr xorg-xdpyinfo xorg-xprop xorg-xwininfo
		xdotool xterm gawk sed grep
	)
	;;
debian)
	build_pkgs=(
		xorg xinit build-essential pkg-config
		libx11-dev libxinerama-dev libxcursor-dev
		libxft-dev libfontconfig-dev libfreetype6-dev
	)
	smoke_pkgs=(
		xserver-xephyr x11-utils xdotool xterm gawk sed grep
	)
	;;
fedora)
	build_pkgs=(
		xorg-x11-server-Xorg xorg-x11-xinit gcc make pkgconf-pkg-config
		libX11-devel libXinerama-devel libXcursor-devel
		libXft-devel fontconfig-devel freetype-devel
	)
	smoke_pkgs=(
		xorg-x11-server-Xephyr xorg-x11-utils xdpyinfo
		xdotool xterm gawk sed grep
	)
	;;
esac

pkgs=("${build_pkgs[@]}")
if [ "$with_smoke" -eq 1 ]; then
	pkgs+=("${smoke_pkgs[@]}")
fi

echo "Detected distro family: $family"
if [ "$with_smoke" -eq 1 ]; then
	echo "Installing build + smoke-test dependencies."
else
	echo "Installing build/runtime dependencies only."
fi

case "$family" in
arch)
	if ! command -v pacman >/dev/null 2>&1; then
		echo "pacman not found." >&2
		exit 1
	fi
	pacman_flags=(-S --needed)
	if [ "$assume_yes" -eq 1 ]; then
		pacman_flags+=(--noconfirm)
	fi
	run_cmd "${sudo_cmd[@]}" pacman "${pacman_flags[@]}" "${pkgs[@]}"
	;;
debian)
	if ! command -v apt-get >/dev/null 2>&1; then
		echo "apt-get not found." >&2
		exit 1
	fi
	apt_flags=()
	if [ "$assume_yes" -eq 1 ]; then
		apt_flags+=(-y)
	fi
	run_cmd "${sudo_cmd[@]}" apt-get update
	run_cmd "${sudo_cmd[@]}" apt-get install --no-install-recommends "${apt_flags[@]}" "${pkgs[@]}"
	;;
fedora)
	if ! command -v dnf >/dev/null 2>&1; then
		echo "dnf not found." >&2
		exit 1
	fi
	dnf_flags=()
	if [ "$assume_yes" -eq 1 ]; then
		dnf_flags+=(-y)
	fi
	run_cmd "${sudo_cmd[@]}" dnf install "${dnf_flags[@]}" "${pkgs[@]}"
	;;
esac

echo "Dependency installation complete."
echo "Security note: keep your distro's libX11 packages up to date."
echo "Known advisories reviewed for this project include CVE-2025-26597 and CVE-2020-14363."

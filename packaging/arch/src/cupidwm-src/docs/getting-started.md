# Getting Started

## Dependencies

Required runtime/build stack:

- Xorg server and `xinit`
- C compiler and `make`
- Development headers for `libX11`, `libXinerama`, `libXrandr`, `libXcursor`, `libXft`, `fontconfig`, `freetype`

Install with distro detection:

```sh
./scripts/install-deps.sh --yes
```

Useful flags:

- `--build-only`: skip smoke-test tools
- `--dry-run`: print commands without installing

## Build

```sh
make
```

Debug + sanitizers build:

```sh
make debug
```

## Run

Run directly:

```sh
./cupidwm
```

Run from `startx` by creating `~/.xinitrc`:

```sh
cp examples/xinitrc ~/.xinitrc
chmod +x ~/.xinitrc
```

Display manager install path (when using `make install`):

- `/usr/share/xsessions/cupidwm.desktop`

For display-manager/session and hotkey-manager recipes, see:

- `examples/display-manager/README.md`
- `examples/sxhkd/cupidwm.sxhkdrc`
- [docs/ecosystem.md](ecosystem.md)

## Test

```sh
make test-smoke
make test-ewmh
```

Or run full local checks:

```sh
make check
```

Headless CI-style run (if `xvfb-run` is installed):

```sh
xvfb-run -a make check
```

`make check` will automatically choose this mode when `DISPLAY` is unset and
`xvfb-run` is available.

If Xephyr/X11 test dependencies are not installed, `make check` will skip the
integration suites and still run build/static steps.

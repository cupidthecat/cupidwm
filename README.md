# cupidwm

`cupidwm` is a compact source-configured X11 window manager with workspaces,
multiple tiling layouts, scratchpads, swallowing, monitor-aware behavior, and
built-in status rendering.

![cupidwm screenshot](img/cupidwm.png)

## Highlights

- 9 workspaces with move-to-workspace support
- Layouts: tile, monocle, floating, fibonacci, dwindle
- Scratchpads, swallowing, and monitor focus/move actions
- RandR-driven monitor topology updates with fallback probing
- EWMH support for desktop state, active window, client list, stacking list, workarea, and expanded `_NET_WM_STATE` handling
- Built-in status modules (disk/CPU/RAM/battery/time)
- Optional external status provider command (disabled by default)
- Optional local IPC socket + `cupidwmctl` control tool (disabled by default)

## Quick Start

1. Install dependencies:
   - `./scripts/install-deps.sh --yes`
2. Build:
   - `make`
3. Run without install:
   - `./cupidwm`
4. Install system-wide (optional):
   - `sudo make install`

For a full setup flow, see [docs/getting-started.md](docs/getting-started.md).

## Configuration Model

`config.def.h` is the tracked default template.

- First build creates local `config.h` from `config.def.h`.
- Edit `config.h` for local keybindings/commands/layout tuning.
- `config.h` is treated as local generated config and is git-ignored.

Reference docs:

- [docs/configuration.md](docs/configuration.md)
- [docs/compatibility.md](docs/compatibility.md)
- [docs/ipc.md](docs/ipc.md)
- [docs/troubleshooting.md](docs/troubleshooting.md)
- [SECURITY.md](SECURITY.md)
- [THIRD_PARTY_NOTICE.md](THIRD_PARTY_NOTICE.md)
- [CHANGELOG.md](CHANGELOG.md)

## Testing

- Smoke test (Xephyr): `make test-smoke`
- EWMH invariants suite (Xephyr): `make test-ewmh`
- Full local gate: `make check`
  - Builds sanitizer debug binary
  - Runs `cppcheck` and `shellcheck` when available
  - Runs smoke + EWMH Xephyr suites when X11 test deps are present
  - Auto-uses `xvfb-run` when no `DISPLAY` is set (if available)

## Operations

- Show resolved IPC path from config/env: `cupidwm --print-ipc-socket`
- Show default client socket path: `cupidwmctl --print-socket`
- Inspect active IPC socket from a running session: `xprop -root _CUPIDWM_IPC_SOCKET`
- Override client socket explicitly: `CUPIDWM_IPC_SOCKET=/path/to.sock cupidwmctl status`
- For recovery/debug steps, see [docs/troubleshooting.md](docs/troubleshooting.md)

## Release

- Build source tarball: `make dist`
- Validate tarball contents: `make distcheck`

See [docs/releasing.md](docs/releasing.md) for release details.

## License

Project source: GPLv3 ([LICENSE](LICENSE)).
Bundled font assets: see [THIRD_PARTY_NOTICE.md](THIRD_PARTY_NOTICE.md).

# cupidwm

`cupidwm` is a compact source-configured X11 window manager with workspaces,
multiple tiling layouts, scratchpads, swallowing, monitor-aware behavior, and
built-in status rendering.

![cupidwm screenshot](img/cupidwm.png)

## Highlights

- 9 workspaces with per-monitor views and move-to-workspace support
- Layouts: tile, monocle, floating, fibonacci, dwindle, grid, columns
- Scratchpads, swallowing, and monitor focus/move actions
- Geometry-aware directional focus/swap/move actions for tiled workflows
- Expressive rules: monitor target, geometry/centering, sticky/skip defaults, scratchpad slot, and no-focus-on-map
- Per-workspace layout state, gaps, and master-width tuning
- Session state persistence for workspace/layout/focus/scratchpads across restart/reload
- RandR-driven monitor topology updates with fallback probing
- EWMH support for desktop state, active window, client list, stacking list, workarea, and expanded `_NET_WM_STATE` handling
- EWMH edge-case support for `_NET_MOVERESIZE_WINDOW`, `_NET_REQUEST_FRAME_EXTENTS`, `_NET_WM_ALLOWED_ACTIONS`, urgency sync, and explicit restack requests
- Built-in status modules (disk/CPU/RAM/battery/time)
- Optional external status provider command (disabled by default; runs in a helper that times out after ~200 ms, so prefer `CUPIDWM_BAR_FIFO` pushes when possible)
- Optional local IPC socket + `cupidwmctl` control tool (disabled by default)
- IPC supports query/control commands, `--json` output, and `subscribe` event streaming, with nonblocking accepted sockets staged in a pending-client queue and a short command deadline so slow writers cannot stall the WM loop
- IPC JSON output now escapes dynamic string fields (status text, labels, event details, and errors) for robust machine parsing
- Workspace names can be updated at runtime via IPC and are exported through `_NET_DESKTOP_NAMES`
- Bar tab clicks support focus/toggle workflows (left click focus, right click toggle)
- Supports slstatus, dmenu, other suckless software

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
- Each monitor keeps its own viewed workspace.
- Layout, gaps, and master-width state are tracked per workspace.

Reference docs:

- [docs/configuration.md](docs/configuration.md)
- [docs/compatibility.md](docs/compatibility.md)
- [docs/ecosystem.md](docs/ecosystem.md)
- [docs/ipc.md](docs/ipc.md)
- [docs/troubleshooting.md](docs/troubleshooting.md)
- [SECURITY.md](SECURITY.md)
- [THIRD_PARTY_NOTICE.md](THIRD_PARTY_NOTICE.md)
- [CHANGELOG.md](CHANGELOG.md)

## Testing

- Smoke test (Xephyr): `make test-smoke`
  - Includes a regression check for independent monitor workspace views
- EWMH invariants suite (Xephyr): `make test-ewmh`
- IPC roundtrip suite (Xephyr, requires an IPC-enabled build): `make test-ipc`
  - Includes persistence regression for workspace/layout recovery after `reload`
  - Includes JSON escaping regression coverage for status payloads with quotes/backslashes
  - Includes low-level socket regression checks for slow/incomplete and oversized commands
- Packaging/ecosystem sanity checks: `make test-packaging`
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
- Use machine-readable IPC responses: `cupidwmctl --json query clients`
- Stream IPC events: `cupidwmctl --json subscribe`
- Directional focus/swap/move via IPC: `cupidwmctl focus left`, `cupidwmctl swap right`, `cupidwmctl move down`
- Directional monitor control via IPC: `cupidwmctl focus monitor-left`, `cupidwmctl move-monitor right`
- Rename a workspace: `cupidwmctl workspace-name set 3 coding`
- Fallback socket base without `$XDG_RUNTIME_DIR`: `/tmp/cupidwm-<uid>/`
- Session state file path: `$XDG_RUNTIME_DIR/cupidwm-<display>.session` (or `/tmp/cupidwm-<uid>/...` fallback)
- Enable shell completion for `cupidwmctl`: `source completions/cupidwmctl.bash`
- For recovery/debug steps, see [docs/troubleshooting.md](docs/troubleshooting.md)
- Push status updates via `CUPIDWM_BAR_FIFO` when possible so the bar sees immediate, nonblocking data instead of waiting on `status_external_cmd`

## Ecosystem

- Sample sxhkd config: `examples/sxhkd/cupidwm.sxhkdrc`
- Sample `~/.xinitrc`: `examples/xinitrc`
- Display-manager session recipes: `examples/display-manager/README.md`
- Arch Linux package recipe: `packaging/arch/PKGBUILD`
- `cupidwmctl` bash completion: `completions/cupidwmctl.bash`

## Release

- Build source tarball: `make dist`
- Validate tarball contents: `make distcheck`

See [docs/releasing.md](docs/releasing.md) for release details.

## License

Project source: GPLv3 ([LICENSE](LICENSE)).
Bundled font assets: see [THIRD_PARTY_NOTICE.md](THIRD_PARTY_NOTICE.md).

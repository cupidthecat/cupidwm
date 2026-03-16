# Changelog

## 2026-03-17

- Reworked IPC command intake to stage accepted sockets in a pending nonblocking client queue integrated into the WM `select()` loop, so slow writers no longer risk delaying unrelated event-loop work.
- Added explicit low-level IPC command guardrails:
  - newline-terminated command deadline (~200 ms) with `error read timeout`
  - oversized command rejection with `error command too long`
- Hardened JSON IPC output by escaping dynamic string fields in JSON replies/events (`status`, labels, layout names, error text, and event details).
- Extended `tests/ipc/roundtrip.sh` with regressions for JSON escaping and low-level socket behavior (slow/incomplete command timeout and oversized command rejection).
- External status commands now run inside a helper process, only the first line is consumed, and the helper is killed after ~200 ms if no output arrives; prefer feeding updates via `CUPIDWM_BAR_FIFO` instead of relying on long-running status scripts.

## 2026-03-16

- Reworked workspace viewing to be monitor-local so each monitor now keeps its
  own active workspace instead of sharing one global view.
- Moved layout state, gaps, and master-width tuning to per-workspace state so
  switching workspaces preserves each workspace's layout configuration.
- Updated bar rendering, focus handling, and workspace switching paths to use
  monitor-local workspace views consistently.
- Extended the Xephyr smoke suite with a regression check that verifies
  switching one monitor's workspace does not disturb the other monitor's view.
- Fixed EWMH client list publication so `_NET_CLIENT_LIST` now follows initial
  mapping order and `_NET_CLIENT_LIST_STACKING` follows live stacking order.
- Extended the EWMH Xephyr suite to assert client-list ordering and stacking
  updates after a raise/focus change.
- Hardened IPC `/tmp` fallback paths to use a private per-user directory and to
  reject unexpected pre-existing files or ownership/permission mismatches.
- Expanded IPC into a broader control/query surface:
  - `query` commands for monitors/workspaces/layouts/focused-client/clients
  - control commands for gaps, bar visibility, scratchpads, fullscreen,
    floating, monitor movement, and directional focus
  - `--json` machine-readable output mode and `subscribe` event stream mode
  - event notifications for `focus`, `workspace`, `layout`, `client_add`,
    `client_remove`, and `monitor_change`
- Changed bar tab right-click behavior to toggle tab visibility (hide/show)
  instead of triggering close, aligning with expected dwm-style tab toggling.
- Added geometry-aware directional navigation semantics:
  - directional focus (`left|right|up|down`)
  - directional swap of tiled clients
  - directional move of tiled clients between tiled slots
  - directional monitor focus and directional monitor-send actions
- Exposed directional swap/move and directional monitor focus/send through IPC
  and documented the related keybindings/commands.
- Expanded rule expressiveness with new per-rule controls:
  - monitor targeting, initial geometry, centered-on-open placement
  - sticky/skip-taskbar/skip-pager defaults
  - scratchpad slot assignment and optional do-not-focus-on-map behavior
  - exact, substring, and regex class/instance/title matching modes
- Extended the smoke Xephyr suite with a regression check for
  substring-matched `no_focus_on_map` rule behavior.
- Added session persistence and restart recovery:
  - persist monitor/workspace view state, per-workspace layouts, focus hints,
    and scratchpad slot state to a runtime session file
  - restore persisted session metadata during startup/reload restart flow
  - save session state on reload/quit and state-mutating workspace/layout/
    scratchpad transitions
- Extended the IPC roundtrip Xephyr suite with a restart-recovery regression
  that verifies workspace/layout state survives `reload`.
- Finished additional EWMH edge-case handling:
  - `_NET_MOVERESIZE_WINDOW` client messages now apply geometry requests for managed windows
  - `_NET_REQUEST_FRAME_EXTENTS` requests now refresh `_NET_FRAME_EXTENTS`
  - `_NET_WM_ALLOWED_ACTIONS` + standard `_NET_WM_ACTION_*` capabilities are now published per client
  - `_NET_RESTACK_WINDOW` now applies explicit restack requests and refreshes stacking exports
  - urgency synchronization now keeps `WM_HINTS` urgency and `_NET_WM_STATE_DEMANDS_ATTENTION` in sync and clears demand on focus
- Extended the EWMH invariant suite with coverage for moveresize requests,
  frame-extents requests, allowed-actions publication, demands-attention
  clear-on-focus behavior, and explicit restack ordering.
- Added packaging and onboarding ecosystem resources:
  - sample `sxhkd` config and sample `~/.xinitrc` under `examples/`
  - display-manager session recipes under `examples/display-manager/`
  - bash completion for `cupidwmctl` under `completions/`
  - Arch Linux PKGBUILD build recipe under `packaging/arch/`
  - packaging sanity test target (`make test-packaging`) and tarball coverage for ecosystem assets
- Hardened `packaging/arch/PKGBUILD` to stage a clean source copy under
  `packaging/arch/src/` for `makepkg -si`, avoiding both prebuilt-tarball
  requirements and failures caused by root-owned local build artifacts.
- Added `cupidwmctl.1`, installed it via `make install`, and included it in
  release tarballs.
- Added a dedicated IPC roundtrip Xephyr test plus a CI step that builds with
  IPC enabled for that scenario.
- Added `.gitignore` and stopped tracking generated `config.h` so the repo
  matches the documented local-config workflow.

## 2026-03-16

- Restored CI with GitHub Actions (`.github/workflows/ci.yml`) including gcc/clang matrix,
  Xephyr tests under `xvfb-run`, and failure log artifacts.
- Optimized `UnmapNotify` handling by returning early for internally ignored unmap events.
- Added IPC socket path discoverability:
  - `cupidwm --print-ipc-socket`
  - `cupidwmctl --print-socket`
  - root window property `_CUPIDWM_IPC_SOCKET` while IPC is active.
- Added display-aware default IPC socket naming to reduce path collisions across sessions.
- Hardened IPC reads to handle partial input and reject oversized commands cleanly.
- Added IPC client read timeout handling to prevent stalled local connections from blocking the WM loop.
- Added IPC socket hint files under `$XDG_RUNTIME_DIR` and client-side auto-discovery (`cupidwmctl`).
- Hardened IPC server reply writes to handle short writes/retries.
- Expanded EWMH handling:
  - `_NET_CLOSE_WINDOW` client messages
  - `_NET_WM_STATE_MODAL` floating behavior sync
  - `_NET_WM_STATE_ABOVE` / `_NET_WM_STATE_BELOW` stack hints
- Expanded `_NET_WM_STATE` coverage and behavior:
  - maximize semantics for `_NET_WM_STATE_MAXIMIZED_HORZ` / `_NET_WM_STATE_MAXIMIZED_VERT`
  - minimize semantics for `_NET_WM_STATE_HIDDEN`
  - advisory/state tracking for `_NET_WM_STATE_STICKY`, `_NET_WM_STATE_SHADED`,
    `_NET_WM_STATE_SKIP_TASKBAR`, `_NET_WM_STATE_SKIP_PAGER`, `_NET_WM_STATE_DEMANDS_ATTENTION`
  - WM-managed `_NET_WM_STATE_FOCUSED` sync on focus changes
- Added `_NET_WM_DESKTOP` all-desktops (`0xFFFFFFFF`) handling to sync sticky state.
- Extended `make lint` to run `shellcheck` on `scripts/` and `tests/` when available.
- Expanded operator documentation and recovery guidance:
  - richer `cupidwm.1`
  - new `docs/troubleshooting.md`
  - updated IPC/config/security docs.
- Added non-Linux status fallback guards so CPU/RAM/battery probes degrade safely.
- Removed snprintf truncation warning in process FD inspection path handling.
- Added monitor topology probe in the event loop to react to hotplug/geometry changes.
- Expanded EWMH invariants coverage for `_NET_WM_STATE_MODAL` and `_NET_CLOSE_WINDOW`.
- Added `xvfb` install coverage in dependency helper scripts for CI-style local checks.
- Updated `make check` to auto-run under `xvfb-run` when `DISPLAY` is unset and tooling is available.
- Added RandR event integration for monitor hotplug updates, with fallback probing for non-RandR paths.
- Expanded monitor discovery to prefer RandR monitor objects, then RandR CRTCs, with geometry dedupe before Xinerama fallback.
- Fixed `make check` failure propagation so integration subtarget failures now stop the overall check target.
- Hardened smoke coverage for unmap persistence by validating explicit unmap on a visible window across workspace switches.
- Made default autostart cursor setup resilient when `xsetroot` is not installed.
- Added `xsetroot` packages to CI and dependency installer smoke-tool sets.
- Added explicit workspace-state waits in smoke tests before scratchpad assertions to reduce CI timing races.
- Made smoke xterm discovery PID-first (with title fallback) to avoid CI flakiness from rapid window-title changes.
- Added `failure.log` emission in smoke artifacts so CI failures include the exact assertion message.
- Made EWMH invariants xterm discovery PID-first (with title fallback) to reduce early window-detection flakes.
- Added `failure.log` emission for EWMH assertions to surface the exact failing check in CI artifacts.

## 2026-03-15

- Added repository hygiene files and local-config policy (`.gitignore`, docs updates).
- Added dedicated security and third-party licensing documentation.
- Added distribution/release documentation and expanded release tarball contents.
- Added `make release`, `make debug`, `make lint`, and improved `make check` behavior.
- Improved EWMH handling for `_NET_ACTIVE_WINDOW`, `_NET_WM_DESKTOP`, and dock strut updates.
- Exported `_NET_CLIENT_LIST_STACKING` alongside `_NET_CLIENT_LIST`.
- Improved workspace switching behavior by unmapping previous workspace windows consistently.
- Added a dedicated EWMH invariant suite at `tests/ewmh/invariants.sh`.
- Added optional local IPC server support and `cupidwmctl` utility (disabled by default).
- Added optional external status provider command support (disabled by default).
- Tightened bundled font provenance metadata (version/source/hash) in `THIRD_PARTY_NOTICE.md`.

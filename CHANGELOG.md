# Changelog

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

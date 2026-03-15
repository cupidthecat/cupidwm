# Changelog

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

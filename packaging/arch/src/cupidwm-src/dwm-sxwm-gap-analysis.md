# cupidwm vs dwm vs sxwm

## Scope

This document is based on the current `cupidwm` source tree, the GitHub repo state, and the upstream project material for `dwm` and `sxwm`.

The goal is not to list every possible feature. The goal is to identify the additions that would materially improve `cupidwm` and clarify where it currently sits between `dwm` and `sxwm`.

## Current Position

`cupidwm` is already ahead of stock `dwm` in several areas:

- broader EWMH/ICCCM support
- RandR-aware monitor updates
- built-in bar and fallback status modules
- scratchpads and swallowing in-tree
- optional IPC with a companion client
- Xephyr integration tests and release/test hygiene

At the same time, the project still behaves more like `dwm` than `sxwm` in one important respect: configuration is still compile-time first. `reload_config()` currently restarts the WM, and most behavior still flows from `config.h`.

Relative to `sxwm`, the biggest missing piece is not one more layout or one more keybinding. It is a stronger runtime control model.

## Comparison Summary

| Area | dwm | sxwm | cupidwm now | Main gap |
|---|---|---|---|---|
| Configuration | source-configured, patch-driven | runtime shell-command oriented | source-configured with restart-based reload | live configuration |
| Workspace model | strong tag workflow, multi-monitor views | scriptable/runtime-driven | single global workspace view across monitors | per-monitor workspace state |
| External control | minimal by default | core design point | small request/reply IPC | richer command/query/event API |
| Interop | minimal stock EWMH | practical and script-oriented | already fairly strong | finish edge-case EWMH actions |
| Bar/status model | simple root-name workflow | external tooling friendly | built-in bar plus fallback modules | event-driven status/panel hooks |
| Testing | lightweight | external/script-driven | strongest of the three here | keep extending regression coverage |

## What Should Be Added


### 9. Tighten packaging and user ecosystem support

The code is ahead of the onboarding story.

What to add:

- sample `sxhkd` config
- sample `xinitrc` and display-manager session recipes
- shell completion for `cupidwmctl`
- distro packaging metadata or build recipes
- a migration guide for `dwm` users and a scripting guide for `sxwm` users

Why it matters:

- this is the lowest-effort way to make the project feel more complete
- it reduces friction for the users most likely to try this WM

## Suggested Order

### P0

- per-monitor workspace state
- runtime configuration layer
- expanded IPC command/query/event surface

### P1

- directional focus and movement
- richer rules
- session persistence

### P2

- bar/status protocol improvements
- remaining EWMH edge cases
- packaging and migration material

## Recommendation

If `cupidwm` wants to stay closer to `dwm`, then the best next step is per-monitor workspace state and a few ergonomic additions.

If `cupidwm` wants to become meaningfully distinct, then it should move toward `sxwm` in exactly one area: runtime control. The highest-value path is:

1. make monitor/workspace state less global
2. add real runtime config
3. turn IPC into a proper automation interface

That combination would give `cupidwm` a clear identity: a small C tiling WM with `dwm`-style simplicity, but with `sxwm`-style control surfaces.

## Sources

- `cupidwm` local source tree and docs
- GitHub repo: <https://github.com/cupidthecat/cupidwm>
- `dwm`: <https://dwm.suckless.org/>
- `dwm` patches index: <https://dwm.suckless.org/patches/>
- `sxwm` README: <https://raw.githubusercontent.com/baskerville/sxwm/master/README.md>
- `sxwm` man page: <https://raw.githubusercontent.com/baskerville/sxwm/master/doc/sxwm.1.asciidoc>

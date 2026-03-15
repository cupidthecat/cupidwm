# Compatibility

This document describes the currently supported EWMH/ICCCM behaviors in cupidwm.

## EWMH Root Properties

cupidwm maintains:

- `_NET_SUPPORTED`
- `_NET_CURRENT_DESKTOP`
- `_NET_NUMBER_OF_DESKTOPS`
- `_NET_DESKTOP_NAMES`
- `_NET_ACTIVE_WINDOW`
- `_NET_CLIENT_LIST`
- `_NET_CLIENT_LIST_STACKING`
- `_NET_WORKAREA`
- `_NET_SUPPORTING_WM_CHECK`
- `_NET_WM_NAME`

## EWMH Client Behaviors

cupidwm handles:

- `_NET_WM_STATE` (fullscreen)
- `_NET_ACTIVE_WINDOW` client messages
- `_NET_WM_DESKTOP` client messages
- `_NET_CURRENT_DESKTOP` client messages
- `_NET_WM_STRUT` and `_NET_WM_STRUT_PARTIAL` for dock/workarea reservation

## Conformance Tests

Dedicated EWMH invariant tests live in `tests/ewmh/invariants.sh` and run via:

```sh
make test-ewmh
```

The suite validates root property publication, active-window tracking,
workspace desktop metadata, client list exports, and workarea/strut updates.

## Notes

- Not every EWMH state atom is implemented; unsupported states are ignored.
- cupidwm is X11-focused and tuned for practical compatibility with panels/pagers.
- Workarea and strut updates are recalculated on relevant dock property changes.

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

- `_NET_WM_STATE`:
  - behavioral: `fullscreen`, `modal`, `maximized_horz`, `maximized_vert`, `hidden`, `above`, `below`
  - state-tracked/advisory: `sticky`, `shaded`, `skip_taskbar`, `skip_pager`, `demands_attention`
  - WM-managed focus sync: `focused`
- `_NET_CLOSE_WINDOW` client messages
- `_NET_ACTIVE_WINDOW` client messages
- `_NET_WM_DESKTOP` client messages
- `_NET_CURRENT_DESKTOP` client messages
- `_NET_REQUEST_FRAME_EXTENTS` requests
- `_NET_MOVERESIZE_WINDOW` client messages
- `_NET_RESTACK_WINDOW` client messages
- `_NET_WM_ALLOWED_ACTIONS` publication with standard `_NET_WM_ACTION_*` capabilities
- `_NET_WM_STRUT` and `_NET_WM_STRUT_PARTIAL` for dock/workarea reservation
- urgency synchronization between `WM_HINTS` urgency and `_NET_WM_STATE_DEMANDS_ATTENTION`

## ICCCM Behaviors

cupidwm preserves each client's `WM_PROTOCOLS` property. When offering focus,
it sends `WM_TAKE_FOCUS` only to clients that advertise that protocol, with a
server timestamp in the message. It also respects the `WM_HINTS` input flag
when deciding whether to set focus directly.

Ordinary clicks are replayed to the application after focus is selected.
Window-manager move and resize gestures consume both the press and release,
so applications do not receive half of a mouse gesture. Temporary keyboard
grabs and override-redirect menus can keep their own focus.

These behaviors follow the focus and client-property conventions in the
[ICCCM](https://www.x.org/releases/current/doc/xorg-docs/icccm/icccm.html).

## Monitor Workspaces

Changing a workspace affects the selected monitor. Selecting a workspace that
another monitor already displays does not swap the two views. Focus restoration
uses windows on the selected monitor, including when that monitor is empty.

Moving a window to another monitor updates its monitor assignment, workspace,
stacking list, and desktop property together. Keyboard moves and completed
mouse drags use the same transfer path.

## Conformance Tests

Dedicated EWMH invariant tests live in `tests/ewmh/invariants.sh` and run via:

```sh
make test-ewmh
```

The suite validates root property publication, active-window tracking,
workspace desktop metadata, client list ordering/exports, and workarea/strut
updates. `make test-protocols` checks client protocols, focus timestamps, click
delivery, and idle property traffic with an Xlib client. `make test-input`
checks workspace selection and keyboard delivery across two monitors.

Both regression suites run through `make check` and CI. They test X11 behavior
directly; they do not launch packaged Electron applications.

## Notes

- cupidwm is X11-focused and tuned for practical compatibility with panels/pagers.
- Workarea and strut updates are recalculated on relevant dock property changes.
- RandR monitor change events trigger monitor/bar/layout refresh; fallback topology probing remains for environments without RandR events.

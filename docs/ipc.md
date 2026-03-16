# IPC

cupidwm includes an optional local IPC server and a companion client tool,
`cupidwmctl`.

## Enabling IPC

Set in `config.h`:

- `ipc_enable = 1`
- `ipc_socket_path = ""` (empty uses default path)

Default socket path resolution:

1. `$XDG_RUNTIME_DIR/cupidwm-<display>.sock` (or `cupidwm.sock` if display is unavailable)
2. `/tmp/cupidwm-<uid>/cupidwm-<display>.sock` (or `/tmp/cupidwm-<uid>/cupidwm.sock`)

Socket permissions are restricted to `0600`.
When falling back to `/tmp`, cupidwm creates and requires a private `0700`
per-user directory before binding the socket.

## Socket Discovery

You can inspect the resolved socket path without launching the WM:

```sh
cupidwm --print-ipc-socket
cupidwmctl --print-socket
```

When IPC is enabled, cupidwm also publishes the active path on the X11 root
window property `_CUPIDWM_IPC_SOCKET`:

```sh
xprop -root _CUPIDWM_IPC_SOCKET
```

`cupidwmctl` resolves socket paths in this order:

1. `-s <path>`
2. `CUPIDWM_IPC_SOCKET` environment variable
3. Runtime hint file in `$XDG_RUNTIME_DIR` (`cupidwm-<display>.sockpath` or `cupidwm.sockpath`)
4. Built-in default socket path rules

## Commands

Example usage:

```sh
cupidwmctl status
cupidwmctl focus next
cupidwmctl focus left
cupidwmctl focus monitor-left
cupidwmctl swap right
cupidwmctl move down
cupidwmctl workspace 3
cupidwmctl move-workspace 5
cupidwmctl layout monocle
cupidwmctl query monitors
cupidwmctl query clients
cupidwmctl gaps set 12
cupidwmctl bar toggle
cupidwmctl floating toggle
cupidwmctl fullscreen on
cupidwmctl move-monitor next
cupidwmctl move-monitor left
cupidwmctl scratchpad toggle 1
cupidwmctl --json status
cupidwmctl --json query workspaces
cupidwmctl subscribe
cupidwmctl reload
cupidwmctl quit
```

Specify a custom socket path:

```sh
cupidwmctl -s /path/to/cupidwm.sock status
```

## Command Surface

Core commands:

- `ping`
- `help`
- `status`
- `reload`
- `quit`

Query commands:

- `query monitors`
- `query workspaces`
- `query layouts`
- `query focused-client`
- `query clients`
- `query bar`

Control commands:

- `focus next|prev|left|right|up|down|monitor-next|monitor-prev|monitor-left|monitor-right|monitor-up|monitor-down`
- `swap left|right|up|down`
- `move left|right|up|down`
- `workspace N`
- `move-workspace N`
- `move-monitor next|prev|left|right|up|down|N`
- `layout tile|monocle|floating|fibonacci|dwindle`
- `gaps inc|dec|set N|get`
- `bar show|hide|toggle|get`
- `scratchpad toggle N`
- `scratchpad set N`
- `scratchpad remove N`
- `fullscreen toggle|on|off`
- `floating toggle|on|off|get`

## JSON Output

Pass `--json` to `cupidwmctl` to request machine-readable replies:

```sh
cupidwmctl --json status
cupidwmctl --json query clients
```

JSON replies escape dynamic string fields (status text, labels, layout names,
event details, and error messages), so payloads remain valid when values
contain quotes, backslashes, or control characters.

## Event Stream

Use subscribe mode to keep a socket open and receive line-delimited events:

```sh
cupidwmctl subscribe
cupidwmctl --json subscribe
```

Current event names include:

- `focus`
- `workspace`
- `layout`
- `client_add`
- `client_remove`
- `monitor_change`

## Restart Recovery

`reload` now performs restart recovery using a persisted session state file.
The restored metadata includes monitor/workspace view state, workspace layout,
focus hints, and scratchpad slot mappings.

Session state file path resolution:

1. `$XDG_RUNTIME_DIR/cupidwm-<display>.session` (or `cupidwm.session`)
2. `/tmp/cupidwm-<uid>/cupidwm-<display>.session` (or `cupidwm.session`)

## Bar Tab Behavior

For built-in bar tabs (outside IPC), default mouse behavior is:

- left click: focus clicked tab
- right click: toggle clicked tab visibility (hide/show)

## Notes

- IPC is disabled by default.
- Commands are intentionally small and local-only.
- Use this only on trusted local sessions.
- Prefer paths under `$XDG_RUNTIME_DIR` and avoid world-accessible directories.
- The `/tmp` fallback refuses directories or socket paths it does not own.
- Each accepted client socket is marked nonblocking and staged in a pending-client queue.
- A complete newline-terminated command must arrive within a short deadline (~200 ms),
  otherwise the server responds with `error read timeout` and closes that client.
- Oversized commands return `error command too long`.

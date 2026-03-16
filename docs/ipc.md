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
cupidwmctl workspace 3
cupidwmctl move-workspace 5
cupidwmctl layout monocle
cupidwmctl reload
cupidwmctl quit
```

Specify a custom socket path:

```sh
cupidwmctl -s /path/to/cupidwm.sock status
```

## Notes

- IPC is disabled by default.
- Commands are intentionally small and local-only.
- Use this only on trusted local sessions.
- Prefer paths under `$XDG_RUNTIME_DIR` and avoid world-accessible directories.
- The `/tmp` fallback refuses directories or socket paths it does not own.
- IPC read handling uses a short socket receive timeout to avoid event-loop stalls from hung clients.

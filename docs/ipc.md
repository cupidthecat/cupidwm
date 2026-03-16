# IPC

cupidwm includes an optional local IPC server and a companion client tool,
`cupidwmctl`.

## Enabling IPC

Set in `config.h`:

- `ipc_enable = 1`
- `ipc_socket_path = ""` (empty uses default path)

Default socket path resolution:

1. `$XDG_RUNTIME_DIR/cupidwm.sock`
2. `/tmp/cupidwm-<uid>.sock`

Socket permissions are restricted to `0600`.

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

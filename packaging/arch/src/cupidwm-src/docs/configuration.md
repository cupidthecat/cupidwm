# Configuration

## Files

- `config.def.h`: project default template
- `config.h`: local generated configuration (created on first build)

Reset local config to defaults:

```sh
cp config.def.h config.h
```

## Core Options

Appearance and layout options include:

- `borderpx`, `snap`, `showbar`, `topbar`, `barheight`, `fontname`
- `default_gaps`, `master_width_default`
- color constants (`col_*`)

Behavior options include:

- `new_window_focus`, `focus_follows_mouse`, `warp_cursor_on`
- `floating_on_top`, `new_window_master`
- movement/resize step sizes and throttle settings

## Commands

Update default launch commands in `config.h`:

- `termcmd`
- `dmenucmd`
- `browsercmd`

## Rules

`rules[]` in `config.h` supports expressive client defaults with class/instance/title matching.

Available fields include:

- target selection: `workspace`, `monitor`, `scratchpad`
- placement: `x`, `y`, `w`, `h`, `centered`
- state defaults: `is_floating`, `start_fullscreen`, `sticky`, `skip_taskbar`, `skip_pager`
- behavior defaults: `no_focus_on_map`, `can_swallow`, `can_be_swallowed`
- matching strategy: `match_mode`

Matching modes:

- `RuleMatchExact` (default)
- `RuleMatchSubstring` (case-insensitive substring)
- `RuleMatchRegex` (case-insensitive POSIX extended regex)

Notes:

- Use `-1` for `workspace`/`monitor`/`scratchpad` to disable targeting.
- Use `-1` for `x`/`y`/`w`/`h` to leave that geometry component unchanged.
- `no_focus_on_map` is useful for utility windows and script-launched helpers.

## Keybindings (Default)

- `MOD + Return`: launch terminal
- `MOD + p`: launcher
- `MOD + b`: browser
- `MOD + Shift + q`: close focused window
- `MOD + Shift + Escape`: quit
- `MOD + Shift + r`: restart
	- Restart restores persisted session state (workspace, layout, focus hints, scratchpads)
- `MOD + j/k`: focus next/prev
- `MOD + Alt + h/j/k/l`: focus left/down/up/right (geometry-aware)
- `MOD + Alt + Shift + h/j/k/l`: swap focused tiled client left/down/up/right
- `MOD + Alt + Ctrl + h/j/k/l`: move focused tiled client left/down/up/right between tiled slots
- `MOD + h/l`: shrink/grow master area
- `MOD + Ctrl + h/l`: shrink/grow stack client
- `MOD + Alt + Arrow`: focus monitor by direction
- `MOD + Alt + Shift + Arrow`: send focused client to monitor by direction
- `MOD + t/m/f/r/y`: tile/monocle/floating/fibonacci/dwindle
- `MOD + Space`: toggle floating (focused)
- `MOD + Shift + Space`: toggle global floating
- `MOD + 1..9`: view workspace
- `MOD + Shift + 1..9`: move focused window to workspace
- `MOD + Tab`: previous workspace
- `MOD + Alt + 1..4`: create scratchpad slot
- `MOD + Ctrl + 1..4`: toggle scratchpad slot
- `MOD + Alt + Shift + 1..4`: remove scratchpad slot

## Mouse (Default)

- Workspace tags in the bar:
	- Left click: view workspace
	- Shift + left click: move focused client to workspace
- Layout symbol in the bar:
	- Left click: cycle layout
- Window title tabs in the bar:
	- Left click: focus clicked tab
	- Right click: toggle clicked tab visibility (hide/show), matching dwm-style tab toggling behavior

## Status Modules

Built-in fallback sections are controlled via:

- `status_show_disk`, `status_show_cpu`, `status_show_ram`, `status_show_battery`, `status_show_time`
- `status_section_order` and `status_separator`
- `status_*_label` and formatting options

CPU/RAM/battery fallback probes are Linux-specific (`/proc`, `/sys`); on non-Linux
platforms those sections are omitted automatically.

Optional external provider (disabled by default):

- `status_allow_external_cmd`
- `status_external_cmd`

When enabled, the external command is executed and its first output line is used
as status text when root-name status is empty.
Treat this as shell command execution in your session context.

## IPC Options

Optional UNIX socket IPC (disabled by default):

- `ipc_enable`
- `ipc_socket_path`

Use `cupidwmctl` to send commands when IPC is enabled. See [docs/ipc.md](ipc.md).
Use `cupidwm --print-ipc-socket` to view the resolved socket path.

## Session Persistence

On restart/reload, cupidwm restores persisted session metadata:

- active monitor/workspace view
- per-workspace layout selection
- per-workspace focused client hints (when windows still exist)
- scratchpad slot assignment and enabled state

State file path resolution mirrors IPC runtime behavior:

1. `$XDG_RUNTIME_DIR/cupidwm-<display>.session` (or `cupidwm.session`)
2. `/tmp/cupidwm-<uid>/cupidwm-<display>.session` (or `cupidwm.session`)

# bash completion for cupidwmctl

_cupidwmctl_complete() {
	local cur prev
	COMPREPLY=()
	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"

	local top_commands="ping help status query focus swap move workspace move-workspace move-monitor layout gaps bar scratchpad fullscreen floating subscribe reload quit"
	local query_commands="monitors workspaces layouts focused-client clients bar"
	local focus_commands="next prev left right up down monitor-next monitor-prev monitor-left monitor-right monitor-up monitor-down"
	local dir_commands="left right up down"
	local move_monitor_commands="next prev left right up down"
	local layout_commands="tile monocle floating fibonacci dwindle"
	local gaps_commands="inc dec get set"
	local bar_commands="show hide toggle get status action click"
	local bar_status_commands="set clear"
	local bar_action_commands="set clear"
	local scratchpad_commands="toggle set remove"
	local fullscreen_commands="toggle on off"
	local floating_commands="toggle on off get"

	if [[ $COMP_CWORD -eq 1 ]]; then
		COMPREPLY=( $(compgen -W "$top_commands" -- "$cur") )
		return 0
	fi

	case "${COMP_WORDS[1]}" in
		query)
			COMPREPLY=( $(compgen -W "$query_commands" -- "$cur") )
			return 0
			;;
		focus)
			COMPREPLY=( $(compgen -W "$focus_commands" -- "$cur") )
			return 0
			;;
		swap|move)
			COMPREPLY=( $(compgen -W "$dir_commands" -- "$cur") )
			return 0
			;;
		move-monitor)
			COMPREPLY=( $(compgen -W "$move_monitor_commands" -- "$cur") )
			return 0
			;;
		layout)
			COMPREPLY=( $(compgen -W "$layout_commands" -- "$cur") )
			return 0
			;;
		gaps)
			COMPREPLY=( $(compgen -W "$gaps_commands" -- "$cur") )
			return 0
			;;
		bar)
			if [[ $COMP_CWORD -eq 2 ]]; then
				COMPREPLY=( $(compgen -W "$bar_commands" -- "$cur") )
				return 0
			fi
			if [[ ${COMP_WORDS[2]} == "status" && $COMP_CWORD -eq 3 ]]; then
				COMPREPLY=( $(compgen -W "$bar_status_commands" -- "$cur") )
				return 0
			fi
			if [[ ${COMP_WORDS[2]} == "action" && $COMP_CWORD -eq 3 ]]; then
				COMPREPLY=( $(compgen -W "$bar_action_commands" -- "$cur") )
				return 0
			fi
			;;
		scratchpad)
			COMPREPLY=( $(compgen -W "$scratchpad_commands" -- "$cur") )
			return 0
			;;
		fullscreen)
			COMPREPLY=( $(compgen -W "$fullscreen_commands" -- "$cur") )
			return 0
			;;
		floating)
			COMPREPLY=( $(compgen -W "$floating_commands" -- "$cur") )
			return 0
			;;
		workspace|move-workspace)
			COMPREPLY=( $(compgen -W "1 2 3 4 5 6 7 8 9" -- "$cur") )
			return 0
			;;
	esac

	if [[ "$prev" == "-s" ]]; then
		COMPREPLY=( $(compgen -f -- "$cur") )
		return 0
	fi
}

complete -F _cupidwmctl_complete cupidwmctl

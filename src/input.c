/* Extracted from cupidwm.c: input and X event handlers. */

void hdl_button(XEvent *xev)
{
	XButtonEvent *xbutton = &xev->xbutton;
	for (int m = 0; m < n_mons; m++) {
		if (xbutton->window != mons[m].barwin)
			continue;

		if (m != current_mon) {
			current_mon = m;
			sync_active_monitor_state();
		}

		int bar_ws = mons[m].view_ws;
		unsigned int click = ClkWinTitle;
		Arg arg;
		memset(&arg, 0, sizeof(arg));
		int x = 0;
		int tw = 0;
		int lw = 0;
		int title_x = 0;
		int title_w = 0;
		Client *title_target = NULL;
		tw = textw(stext) + 12;
		if (m != current_mon)
			tw = 0;

		lw = textw(layouts[workspace_layout_for(bar_ws)].symbol) + 14;

		for (int i = 0; i < NUM_WORKSPACES; i++) {
			int w = textw(tags[i]) + 14;
			if (xbutton->x >= x && xbutton->x < x + w) {
				click = ClkTagBar;
				arg.ui = (unsigned int)i;
				break;
			}
			x += w;
		}

		if (click != ClkTagBar) {
			if (xbutton->x < x + lw)
				click = ClkLtSymbol;
			else if (tw > 0 && xbutton->x > mons[m].w - tw)
				click = ClkStatusText;
		}

		if (click == ClkWinTitle && user_config.bar_show_tabs) {
			title_x = x + lw;
			title_w = MAX(0, mons[m].w - title_x - tw);
			title_target = bar_client_at(&mons[m], xbutton->x, title_x, title_w);
		}

		if (click == ClkStatusText && tw > 0) {
			int status_x = mons[m].w - tw + 6;
			int rel_x = xbutton->x - status_x;
			if (status_dispatch_click_at(rel_x, xbutton->button))
				return;
		}

		unsigned int state = clean_mask(xbutton->state);
		unsigned int keymods = state & (ShiftMask | LockMask | ControlMask | Mod1Mask |
		                               Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask);

		if (click == ClkWinTitle &&
		    title_target &&
		    user_config.bar_show_tabs &&
		    user_config.bar_click_focus_tabs &&
		    xbutton->button == Button1 &&
		    keymods == 0) {
			if (!title_target->mapped) {
				XMapWindow(dpy, title_target->win);
				title_target->mapped = True;
				if (!global_floating && !title_target->floating && !title_target->fullscreen)
					tile();
				set_input_focus(title_target, True, False);
				update_borders();
				drawbars();
			}
			else if (title_target == focused) {
				XUnmapWindow(dpy, title_target->win);
				title_target->mapped = False;

				Client *next_focus = first_visible_client(bar_ws, title_target->mon, title_target);
				if (next_focus)
					set_input_focus(next_focus, True, False);
				else
					set_input_focus(NULL, False, False);

				tile();
				update_borders();
				drawbars();
			}
			else {
				set_input_focus(title_target, True, False);
			}
			return;
		}

		if (click == ClkWinTitle &&
		    title_target &&
		    user_config.bar_show_tabs &&
		    user_config.bar_click_focus_tabs &&
		    xbutton->button == Button3 &&
		    keymods == 0) {
			if (title_target->mapped) {
				XUnmapWindow(dpy, title_target->win);
				title_target->mapped = False;

				Client *next_focus = first_visible_client(bar_ws, title_target->mon, title_target);
				if (next_focus)
					set_input_focus(next_focus, True, False);
				else
					set_input_focus(NULL, False, False);
			}
			else {
				XMapWindow(dpy, title_target->win);
				title_target->mapped = True;
				if (!global_floating && !title_target->floating && !title_target->fullscreen)
					tile();
				set_input_focus(title_target, True, False);
			}

			tile();
			update_borders();
			drawbars();
			return;
		}

		if (click == ClkTagBar && xbutton->button == Button1) {
			if (clean_mask(xbutton->state) & ShiftMask)
				movetows(&arg);
			else
				viewws(&arg);
			return;
		}

		for (size_t i = 0; i < LENGTH(buttons); i++) {
			if (click != buttons[i].click)
				continue;
			if (buttons[i].button != xbutton->button)
				continue;
			if (clean_mask((int)buttons[i].mask) != clean_mask(xbutton->state))
				continue;
			if (!buttons[i].func)
				continue;
			if (click == ClkWinTitle) {
				if (!title_target)
					continue;
				if (title_target != focused)
					set_input_focus(title_target, True, False);
			}
			if (click == ClkTagBar && buttons[i].arg.i == 0)
				buttons[i].func(&arg);
			else
				buttons[i].func(&buttons[i].arg);
		}
		return;
	}

	Window w = (xbutton->subwindow != None) ? xbutton->subwindow : xbutton->window;
	w = find_toplevel(w);

	Mask left_click = Button1;
	Mask right_click = Button3;

	XAllowEvents(dpy, ReplayPointer, xbutton->time);
	if (!w)
		return;

	Client *c = find_client(w);
	if (c && client_is_visible(c)) {

		Bool is_swap_mode =
			(xbutton->state & user_config.modkey) &&
			(xbutton->state & ShiftMask) &&
			xbutton->button == left_click && !c->floating;
		if (is_swap_mode) {
			drag_client = c;
			drag_start_x = xbutton->x_root;
			drag_start_y = xbutton->y_root;
			drag_orig_x = c->x;
			drag_orig_y = c->y;
			drag_orig_w = c->w;
			drag_orig_h = c->h;
			drag_mode = DRAG_SWAP;
			XGrabPointer(dpy, root, True, ButtonReleaseMask | PointerMotionMask,
				     GrabModeAsync, GrabModeAsync, None, cursor_move, CurrentTime);
			focused = c;
			set_input_focus(focused, False, False);
			XSetWindowBorder(dpy, c->win, user_config.border_swap_col);
			return;
		}

		Bool is_move_resize =
			(xbutton->state & user_config.modkey) &&
			(xbutton->button == left_click ||
			 xbutton->button == right_click) && !c->floating;
		if (is_move_resize) {
			focused = c;
			toggle_floating();
		}

		Bool is_single_click = 
			!(xbutton->state & user_config.modkey) &&
			xbutton->button == left_click;
		if (is_single_click) {
			focused = c;
			set_input_focus(focused, True, False);
			return;
		}

		if (!c->floating)
			return;

		if (c->fixed && xbutton->button == right_click)
			return;

		Cursor cursor = (xbutton->button == left_click) ? cursor_move : cursor_resize;
		XGrabPointer(dpy, root, True, ButtonReleaseMask | PointerMotionMask,
				     GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime);

		drag_client = c;
		drag_start_x = xbutton->x_root;
		drag_start_y = xbutton->y_root;
		drag_orig_x = c->x;
		drag_orig_y = c->y;
		drag_orig_w = c->w;
		drag_orig_h = c->h;
		drag_mode = (xbutton->button == left_click) ? DRAG_MOVE : DRAG_RESIZE;
		focused = c;

		set_input_focus(focused, True, False);
		return;
	}

	update_struts();
	tile();
	update_borders();
}

void hdl_button_release(XEvent *xev)
{
	(void)xev;

	if (drag_mode == DRAG_SWAP) {
		if (swap_target) {
			XSetWindowBorder(dpy, swap_target->win, (swap_target == focused ?
						     user_config.border_foc_col : user_config.border_ufoc_col));
			swap_clients(drag_client, swap_target);
		}
		tile();
		update_borders();
	}

	XUngrabPointer(dpy, CurrentTime);

	drag_mode = DRAG_NONE;
	drag_client = NULL;
	swap_target = NULL;
}

static Bool resolve_state_action(Bool current, long action, Bool *want_out)
{
	if (!want_out)
		return False;

	switch (action) {
	case 0:
		*want_out = False;
		return True;
	case 1:
		*want_out = True;
		return True;
	case 2:
		*want_out = !current;
		return True;
	default:
		return False;
	}
}

static Client *next_focus_candidate(Client *skip)
{
	return first_visible_client(current_ws, current_mon, skip);
}

static void sync_urgency_from_wm_hints(Client *c)
{
	if (!c)
		return;

	XWMHints *hints = XGetWMHints(dpy, c->win);
	if (!hints)
		return;

	Bool urgent = (hints->flags & XUrgencyHint) ? True : False;
	if (c == focused && urgent) {
		hints->flags &= ~XUrgencyHint;
		XSetWMHints(dpy, c->win, hints);
		urgent = False;
	}
	window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_DEMANDS_ATTENTION], urgent);
	XFree(hints);
}

static void set_client_urgency_hint(Client *c, Bool urgent)
{
	if (!c)
		return;

	XWMHints *hints = XGetWMHints(dpy, c->win);
	if (!hints) {
		hints = XAllocWMHints();
		if (!hints)
			return;
		hints->flags = 0;
	}

	if (urgent)
		hints->flags |= XUrgencyHint;
	else
		hints->flags &= ~XUrgencyHint;

	XSetWMHints(dpy, c->win, hints);
	XFree(hints);
}

static void apply_hidden_state(Client *c, Bool want_hidden, Bool *needs_layout, Bool *needs_borders, Bool *needs_refocus)
{
	if (!c || want_hidden == c->hidden)
		return;

	c->hidden = want_hidden;
	window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_HIDDEN], want_hidden);

	if (want_hidden) {
		if (!c->mapped)
			return;

		c->ignore_unmap_events++;
		XUnmapWindow(dpy, c->win);
		c->mapped = False;

			if (client_should_be_visible(c)) {
				if (needs_layout)
					*needs_layout = True;
				if (needs_borders)
				*needs_borders = True;
			if (needs_refocus && focused == c)
				*needs_refocus = True;
		}
		return;
	}

	c->mapped = True;
	if (!client_should_be_visible(c))
		return;

	XMapWindow(dpy, c->win);
	if (needs_borders)
		*needs_borders = True;
	if (needs_layout && !c->floating && !c->fullscreen)
		*needs_layout = True;
}

static void apply_maximized_state(Client *c, Bool want_horz, Bool want_vert, Bool *needs_layout, Bool *needs_borders)
{
	if (!c)
		return;

	Bool changed = (want_horz != c->maximized_horz || want_vert != c->maximized_vert);
	if (!changed)
		return;

	c->maximized_horz = want_horz;
	c->maximized_vert = want_vert;
	window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_MAXIMIZED_HORZ], want_horz);
	window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_MAXIMIZED_VERT], want_vert);

	if (c->fullscreen)
		return;

	if (want_horz || want_vert) {
		if (!c->max_restore_valid) {
			XWindowAttributes wa;
			if (XGetWindowAttributes(dpy, c->win, &wa)) {
				c->max_restore_x = wa.x;
				c->max_restore_y = wa.y;
				c->max_restore_w = wa.width;
				c->max_restore_h = wa.height;
			}
			else {
				c->max_restore_x = c->x;
				c->max_restore_y = c->y;
				c->max_restore_w = c->w;
				c->max_restore_h = c->h;
			}
			c->max_restore_floating = c->floating;
			c->max_restore_valid = True;
		}

		if (!c->floating) {
			c->floating = True;
			c->max_forced_floating = True;
				if (needs_layout && client_should_be_visible(c))
					*needs_layout = True;
		}

		int mon = CLAMP(c->mon, 0, n_mons - 1);
		int wx, wy, ww, wh;
		monitor_workarea(mon, &wx, &wy, &ww, &wh);

		int nx = want_horz ? wx : c->x;
		int ny = want_vert ? wy : c->y;
		int nw = want_horz ? MAX(1, ww) : c->w;
		int nh = want_vert ? MAX(1, wh) : c->h;
		XMoveResizeWindow(dpy, c->win, nx, ny, (unsigned int)nw, (unsigned int)nh);
		c->x = nx;
		c->y = ny;
		c->w = nw;
		c->h = nh;
			if (needs_borders && client_should_be_visible(c))
				*needs_borders = True;
		return;
	}

	if (c->max_restore_valid) {
		XMoveResizeWindow(
			dpy,
			c->win,
			c->max_restore_x,
			c->max_restore_y,
			(unsigned int)MAX(1, c->max_restore_w),
			(unsigned int)MAX(1, c->max_restore_h)
		);
		c->x = c->max_restore_x;
		c->y = c->max_restore_y;
		c->w = MAX(1, c->max_restore_w);
		c->h = MAX(1, c->max_restore_h);
	}

	if (c->max_forced_floating && !global_floating && !c->modal_forced_floating) {
		c->floating = c->max_restore_floating;
		if (!c->floating)
			c->mon = get_monitor_for(c);
		if (needs_layout && client_should_be_visible(c))
			*needs_layout = True;
	}

	c->max_forced_floating = False;
	c->max_restore_valid = False;
	if (needs_borders && client_should_be_visible(c))
		*needs_borders = True;
}

void hdl_client_msg(XEvent *xev)
{
	XClientMessageEvent *client_msg_ev = &xev->xclient;

	if (client_msg_ev->message_type == atoms[ATOM_NET_REQUEST_FRAME_EXTENTS]) {
		set_frame_extents(client_msg_ev->window);
		return;
	}

	if (client_msg_ev->message_type == atoms[ATOM_NET_MOVERESIZE_WINDOW]) {
		Client *c = find_client(find_toplevel(client_msg_ev->window));
		if (!c || c->fullscreen)
			return;

		unsigned long packed = (unsigned long)client_msg_ev->data.l[0];
		unsigned long flags = (packed >> 8) & 0x0f;
		int nx = c->x;
		int ny = c->y;
		int nw = c->w;
		int nh = c->h;

		if (flags & (1UL << 0))
			nx = (int)client_msg_ev->data.l[1];
		if (flags & (1UL << 1))
			ny = (int)client_msg_ev->data.l[2];
		if (flags & (1UL << 2))
			nw = MAX(1, (int)client_msg_ev->data.l[3]);
		if (flags & (1UL << 3))
			nh = MAX(1, (int)client_msg_ev->data.l[4]);

		if (!c->floating) {
			c->floating = True;
			c->modal_forced_floating = False;
		}

		XMoveResizeWindow(dpy, c->win, nx, ny, (unsigned int)nw, (unsigned int)nh);
		c->x = nx;
		c->y = ny;
		c->w = nw;
		c->h = nh;
		update_borders();
		return;
	}

	if (client_msg_ev->message_type == atoms[ATOM_NET_RESTACK_WINDOW]) {
		Client *c = find_client(find_toplevel(client_msg_ev->window));
		if (!c)
			return;

		XWindowChanges wc = {0};
		unsigned int mask = CWStackMode;
		int detail = (int)client_msg_ev->data.l[2];
		if (detail < Above || detail > Opposite)
			detail = Above;
		wc.stack_mode = detail;

		Window sibling = (Window)client_msg_ev->data.l[1];
		if (sibling != None) {
			wc.sibling = sibling;
			mask |= CWSibling;
		}

		XConfigureWindow(dpy, c->win, mask, &wc);
		update_net_client_list();
		return;
	}

	if (client_msg_ev->message_type == atoms[ATOM_NET_CURRENT_DESKTOP]) {
		int ws = (int)client_msg_ev->data.l[0];
		change_workspace(ws);
		return;
	}

	if (client_msg_ev->message_type == atoms[ATOM_NET_ACTIVE_WINDOW]) {
		Window w = find_toplevel(client_msg_ev->window);
		Client *c = find_client(w);
		if (!c)
			return;

		if (c->mon != current_mon) {
			current_mon = CLAMP(c->mon, 0, n_mons - 1);
			sync_active_monitor_state();
		}
		if (c->ws != current_ws)
			change_workspace(c->ws);
		if (c->hidden) {
			Bool relayout = False;
			Bool borders = False;
			Bool refocus = False;
			apply_hidden_state(c, False, &relayout, &borders, &refocus);
		}
		if (!c->mapped) {
			XMapWindow(dpy, c->win);
			c->mapped = True;
			tile();
		}

		set_input_focus(c, True, True);
		return;
	}

	if (client_msg_ev->message_type == atoms[ATOM_NET_CLOSE_WINDOW]) {
		Window w = find_toplevel(client_msg_ev->window);
		Client *c = find_client(w);
		if (!c)
			return;

		Atom *protocols = NULL;
		int n_protocols = 0;
		if (XGetWMProtocols(dpy, c->win, &protocols, &n_protocols) && protocols) {
			for (int i = 0; i < n_protocols; i++) {
				if (protocols[i] != atoms[ATOM_WM_DELETE_WINDOW])
					continue;

				XEvent ev = {.xclient = {
					.type = ClientMessage,
					.window = c->win,
					.message_type = atoms[ATOM_WM_PROTOCOLS],
					.format = 32
				}};
				ev.xclient.data.l[0] = atoms[ATOM_WM_DELETE_WINDOW];
				ev.xclient.data.l[1] = CurrentTime;
				XSendEvent(dpy, c->win, False, NoEventMask, &ev);
				XFree(protocols);
				return;
			}
			XFree(protocols);
		}

		XKillClient(dpy, c->win);
		return;
	}

	if (client_msg_ev->message_type == atoms[ATOM_NET_WM_DESKTOP]) {
		Window w = find_toplevel(client_msg_ev->window);
		Client *c = find_client(w);
		if (!c)
			return;

		unsigned long desk = (unsigned long)client_msg_ev->data.l[0];
		if ((desk & 0xFFFFFFFFUL) == 0xFFFFFFFFUL) {
			c->sticky = True;
			window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_STICKY], True);
			update_client_desktop_properties();
			return;
		}

		int ws = (int)desk;
		if (ws < 0 || ws >= NUM_WORKSPACES)
			return;

		if (c->sticky) {
			c->sticky = False;
			window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_STICKY], False);
		}
		move_client_to_workspace(c, ws, False);
		return;
	}

	if (client_msg_ev->message_type == atoms[ATOM_NET_WM_STATE]) {
		Window w = client_msg_ev->window;
		Client *c = find_client(find_toplevel(w));
		if (!c)
			return;

		/* 0=remove, 1=add, 2=toggle */
		long action = client_msg_ev->data.l[0];
		Atom a1 = (Atom)client_msg_ev->data.l[1];
		Atom a2 = (Atom)client_msg_ev->data.l[2];
		Bool needs_layout = False;
		Bool needs_borders = False;
		Bool needs_refocus = False;
		Bool have_max_h = False;
		Bool have_max_v = False;
		Bool have_hidden = False;
		Bool have_sticky = False;
		Bool want_max_h = c->maximized_horz;
		Bool want_max_v = c->maximized_vert;
		Bool want_hidden = c->hidden;
		Bool want_sticky = c->sticky;

		Atom state_atoms[2] = { a1, a2 };
		for (int i = 0; i < 2; i++) {
			if (state_atoms[i] == None)
				continue;

			if (state_atoms[i] == atoms[ATOM_NET_WM_STATE_FULLSCREEN]) {
				Bool was_fullscreen = c->fullscreen;
				Bool want = was_fullscreen;
				if (!resolve_state_action(was_fullscreen, action, &want))
					continue;

				if (want != was_fullscreen)
					apply_fullscreen(c, want);
				if (want)
					XRaiseWindow(dpy, c->win);
				if (want != was_fullscreen)
					needs_borders = True;
				continue;
			}

			if (state_atoms[i] == atoms[ATOM_NET_WM_STATE_MODAL]) {
					Bool want_modal = window_has_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_MODAL]);
					if (!resolve_state_action(want_modal, action, &want_modal))
						continue;

					window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_MODAL], want_modal);

				if (want_modal) {
					if (!c->floating && !c->fullscreen) {
						c->floating = True;
						c->modal_forced_floating = True;
						needs_layout = True;
					}
					XRaiseWindow(dpy, c->win);
				}
				else {
					if (c->modal_forced_floating && !c->fullscreen && !global_floating) {
						c->floating = False;
						c->mon = get_monitor_for(c);
						needs_layout = True;
					}
					c->modal_forced_floating = False;
					}

					needs_borders = True;
					continue;
				}

			if (state_atoms[i] == atoms[ATOM_NET_WM_STATE_MAXIMIZED_HORZ]) {
				if (!resolve_state_action(want_max_h, action, &want_max_h))
					continue;
				have_max_h = True;
				continue;
			}

			if (state_atoms[i] == atoms[ATOM_NET_WM_STATE_MAXIMIZED_VERT]) {
				if (!resolve_state_action(want_max_v, action, &want_max_v))
					continue;
				have_max_v = True;
				continue;
			}

			if (state_atoms[i] == atoms[ATOM_NET_WM_STATE_HIDDEN]) {
				if (!resolve_state_action(want_hidden, action, &want_hidden))
					continue;
				have_hidden = True;
				continue;
			}

			if (state_atoms[i] == atoms[ATOM_NET_WM_STATE_STICKY]) {
				if (!resolve_state_action(want_sticky, action, &want_sticky))
					continue;
				have_sticky = True;
				continue;
			}

			if (state_atoms[i] == atoms[ATOM_NET_WM_STATE_ABOVE]) {
				Bool want_above = window_has_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_ABOVE]);
				if (!resolve_state_action(want_above, action, &want_above))
					continue;
				window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_ABOVE], want_above);
				if (want_above)
					XRaiseWindow(dpy, c->win);
				needs_borders = True;
				continue;
			}

			if (state_atoms[i] == atoms[ATOM_NET_WM_STATE_BELOW]) {
				Bool want_below = window_has_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_BELOW]);
				if (!resolve_state_action(want_below, action, &want_below))
					continue;
				window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_BELOW], want_below);
				if (want_below)
					XLowerWindow(dpy, c->win);
				needs_borders = True;
				continue;
			}

			if (state_atoms[i] == atoms[ATOM_NET_WM_STATE_SHADED] ||
			    state_atoms[i] == atoms[ATOM_NET_WM_STATE_SKIP_TASKBAR] ||
			    state_atoms[i] == atoms[ATOM_NET_WM_STATE_SKIP_PAGER] ||
			    state_atoms[i] == atoms[ATOM_NET_WM_STATE_DEMANDS_ATTENTION]) {
				Bool current = window_has_ewmh_state(c->win, state_atoms[i]);
				Bool want = current;
				if (!resolve_state_action(current, action, &want))
					continue;
				window_set_ewmh_state(c->win, state_atoms[i], want);
				if (state_atoms[i] == atoms[ATOM_NET_WM_STATE_DEMANDS_ATTENTION])
					set_client_urgency_hint(c, want);
			}
		}

		if (have_max_h || have_max_v)
			apply_maximized_state(c, want_max_h, want_max_v, &needs_layout, &needs_borders);
		if (have_hidden)
			apply_hidden_state(c, want_hidden, &needs_layout, &needs_borders, &needs_refocus);
		if (have_sticky) {
			c->sticky = want_sticky;
			window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_STICKY], want_sticky);
			update_client_desktop_properties();
		}

		if (needs_layout && client_should_be_visible(c))
			tile();
		if (needs_refocus && client_should_be_visible(c)) {
			Client *next = next_focus_candidate(c);
			set_input_focus(next, True, False);
		}
		else if (needs_borders && client_should_be_visible(c))
			update_borders();
		return;
	}
}

void hdl_config_ntf(XEvent *xev)
{
	if (xev->xconfigure.window == root) {
		update_mons();
		setup_bars();
		tile();
		update_borders();
	}
}

void hdl_config_req(XEvent *xev)
{
	XConfigureRequestEvent *config_ev = &xev->xconfigurerequest;
	Client *c = NULL;

	for (int i = 0; i < NUM_WORKSPACES && !c; i++)
		for (c = workspaces[i]; c; c = c->next)
			if (c->win == config_ev->window)
				break;

	if (!c || c->floating || c->fullscreen) {
		/* allow client to configure itself */
		XWindowChanges wc = {
			.x = config_ev->x,
			.y = config_ev->y,
			.width = config_ev->width,
			.height = config_ev->height,
			.border_width = config_ev->border_width,
			.sibling = config_ev->above,
			.stack_mode = config_ev->detail
		};
		XConfigureWindow(dpy, config_ev->window, config_ev->value_mask, &wc);
		return;
	}
}

void hdl_dummy(XEvent *xev)
{
	(void)xev;
}

void hdl_enter_ntf(XEvent *xev)
{
	if (!user_config.focus_follows_mouse || drag_mode != DRAG_NONE || in_ws_switch)
		return;

	XCrossingEvent *enter_ev = &xev->xcrossing;
	if ((enter_ev->mode != NotifyNormal && enter_ev->mode != NotifyUngrab) ||
		enter_ev->detail == NotifyInferior)
		return;

	Window w = find_toplevel(enter_ev->window);
	if (!w || w == root)
		return;

	Client *c = find_client(w);
	if (!client_is_visible(c) || c == focused)
		return;
	if (c->suppress_enter_focus_once) {
		c->suppress_enter_focus_once = False;
		return;
	}

	set_input_focus(c, True, False);
}

void hdl_destroy_ntf(XEvent *xev)
{
	Window w = xev->xdestroywindow.window;

	for (int i = 0; i < NUM_WORKSPACES; i++) {
		Client *prev = NULL;
		Client *c = workspaces[i];

		while (c && c->win != w) {
			prev = c;
			c = c->next;
		}

		if (!c)
			continue;

		/* if client is swallowed, restore swallower */
		if (c->swallower)
			unswallow_window(c);

		/* if this client had swallowed another, restore that child */
		if (c->swallowed) {
			Client *swallowed = c->swallowed;
			c->swallowed = NULL;
			swallowed->swallower = NULL;

			swallowed->mapped = True;

				if (monitor_views_workspace(swallowed->mon, i)) {
					XMapWindow(dpy, swallowed->win);
					set_input_focus(swallowed, False, True);
				}
			else {
				ws_focused[i] = swallowed;
			}
		}

			for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
				if (ws_focused[ws] == c)
					ws_focused[ws] = NULL;
			}

			if (focused == c)
				focused = NULL;

			char ipc_details[160];
			snprintf(ipc_details, sizeof(ipc_details), "win=0x%lx ws=%d mon=%d", (unsigned long)c->win, i + 1, c->mon);

			/* unlink from workspace list */
			if (!prev)
				workspaces[i] = c->next;
			else
				prev->next = c->next;

			ipc_notify_event("client_remove", ipc_details);
			free(c);
			update_net_client_list();
			if (open_windows > 0)
				open_windows--;

			if (workspace_is_visible(i)) {
				tile();
				update_borders();

				/* prefer previous window else next */
				Client *foc_new = NULL;
				if (prev && client_is_visible_on_monitor(prev, current_mon))
					foc_new = prev;
				else {
					for (Client *p = workspaces[i]; p; p = p->next) {
						if (!client_is_visible_on_monitor(p, current_mon))
							continue;
						foc_new = p;
						break;
				}
			}

				if (foc_new)
					set_input_focus(foc_new, True, True);
				else
					set_input_focus(NULL, False, False);
			}

		return;
	}
}

void hdl_keypress(XEvent *xev)
{
	unsigned int mods = (unsigned int)clean_mask(xev->xkey.state);
	KeySym keysym = XkbKeycodeToKeysym(dpy, (KeyCode)xev->xkey.keycode, 0, 0);
	if (keysym == NoSymbol)
		keysym = XLookupKeysym(&xev->xkey, 0);
	for (size_t i = 0; i < LENGTH(keys); i++) {
		if (keys[i].keysym != keysym)
			continue;
		if (clean_mask(keys[i].mod) != (int)mods)
			continue;
		if (keys[i].func)
			keys[i].func(&keys[i].arg);
		return;
	}
}

void hdl_mapping_ntf(XEvent *xev)
{
	XRefreshKeyboardMapping(&xev->xmapping);
	update_modifier_masks();
	grab_keys();
}

void hdl_map_req(XEvent *xev)
{
	Window w = xev->xmaprequest.window;
	XWindowAttributes win_attr;

	if (!XGetWindowAttributes(dpy, w, &win_attr))
		return;

	/* skips invisible windows */
	if (win_attr.override_redirect || win_attr.width <= 0 || win_attr.height <= 0) {
		XMapWindow(dpy, w);
		return;
	}

	/* check if this window is already managed on any workspace */
	Client *c = find_client(w);
	if (c) {
		if (c->hidden) {
			c->hidden = False;
			c->mapped = True;
			window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_HIDDEN], False);
		}
			if (client_should_be_visible(c)) {
				if (!c->mapped)
					c->mapped = True;
				XMapWindow(dpy, w);
				if (user_config.new_win_focus && !c->no_focus_on_map) {
					focused = c;
					set_input_focus(c, True, True);
				return; /* set_input_focus already calls update_borders */
			}
				if (c->no_focus_on_map)
					c->suppress_enter_focus_once = True;
			update_borders();
		}
		return;
	}

	if (window_is_dock(w)) {
		select_input(w, PropertyChangeMask | StructureNotifyMask);
		XMapWindow(dpy, w);
		update_struts();
		tile();
		update_borders();
		return;
	}


	Atom type;
	int format;
	unsigned long n_items, after;
	Atom *types = NULL;
	Bool should_float = False;

	if (XGetWindowProperty(dpy, w, atoms[ATOM_NET_WM_WINDOW_TYPE], 0, 8, False, XA_ATOM, &type, &format,
	    &n_items, &after, (unsigned char **)&types) == Success && types) {
		if (type == XA_ATOM && format == 32) {
			for (unsigned long i = 0; i < n_items; i++) {
				if (types[i] == atoms[ATOM_NET_WM_WINDOW_TYPE_UTILITY] ||
					types[i] == atoms[ATOM_NET_WM_WINDOW_TYPE_DIALOG]  ||
					types[i] == atoms[ATOM_NET_WM_WINDOW_TYPE_TOOLBAR] ||
					types[i] == atoms[ATOM_NET_WM_WINDOW_TYPE_SPLASH]  ||
					types[i] == atoms[ATOM_NET_WM_WINDOW_TYPE_POPUP_MENU] ||
					types[i] == atoms[ATOM_NET_WM_WINDOW_TYPE_DROPDOWN_MENU] ||
					types[i] == atoms[ATOM_NET_WM_WINDOW_TYPE_MENU] ||
					types[i] == atoms[ATOM_NET_WM_WINDOW_TYPE_TOOLTIP] ||
					types[i] == atoms[ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION]) {
					should_float = True;
					break;
				}
			}
		}
		XFree(types);
	}

	if (!should_float)
		should_float = window_should_float(w);

	Bool state_modal = False;
	Bool state_sticky = False;
	Bool state_hidden = False;
	Bool state_max_horz = False;
	Bool state_max_vert = False;

	Atom state_type = None;
	Atom *state_atoms = NULL;
	int state_format = 0;
	unsigned long bytes_after = 0;
	n_items = 0;
	if (XGetWindowProperty(dpy, w, atoms[ATOM_NET_WM_STATE], 0, 32, False, XA_ATOM, &state_type, &state_format,
			       &n_items, &bytes_after, (unsigned char**)&state_atoms) == Success && state_atoms) {
		if (state_type == XA_ATOM && state_format == 32) {
			for (unsigned long i = 0; i < n_items; i++) {
				if (state_atoms[i] == atoms[ATOM_NET_WM_STATE_MODAL])
					state_modal = True;
				else if (state_atoms[i] == atoms[ATOM_NET_WM_STATE_STICKY])
					state_sticky = True;
				else if (state_atoms[i] == atoms[ATOM_NET_WM_STATE_HIDDEN])
					state_hidden = True;
				else if (state_atoms[i] == atoms[ATOM_NET_WM_STATE_MAXIMIZED_HORZ])
					state_max_horz = True;
				else if (state_atoms[i] == atoms[ATOM_NET_WM_STATE_MAXIMIZED_VERT])
					state_max_vert = True;
			}
		}
		XFree(state_atoms);
	}

	if (!should_float && state_modal)
		should_float = True;

	if (open_windows >= MAX_CLIENTS) {
		fprintf(stderr, "cupidwm: max clients reached, ignoring map request\n");
		return;
	}

	int target_ws = get_workspace_for_window(w);
	c = add_client(w, target_ws);
	if (!c)
		return;
	set_wm_state(w, NormalState);
	c->sticky = (c->sticky || state_sticky);
	if (c->sticky)
		window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_STICKY], True);

	Window transient;
	if (!should_float && XGetTransientForHint(dpy, w, &transient))
		should_float = True;

	XSizeHints size_hints;
	long supplied_ret;

	if (!should_float &&
		XGetWMNormalHints(dpy, w, &size_hints, &supplied_ret) &&
		(size_hints.flags & PMinSize) && (size_hints.flags & PMaxSize) &&
		size_hints.min_width  == size_hints.max_width &&
		size_hints.min_height == size_hints.max_height) {

		should_float = True;
		c->fixed = True;
	}

	if (should_float || global_floating)
		c->floating = True;
	if (state_modal)
		c->modal_forced_floating = True;

	if (window_should_start_fullscreen(w)) {
		c->fullscreen = True;
		c->floating = False;
	}

	/* center floating windows & set border */
	if (c->floating && !c->fullscreen) {
		int w_ = MAX(c->w, 64), h_ = MAX(c->h, 64);
		update_struts();
		int mx, my, mw, mh;
		monitor_workarea(c->mon, &mx, &my, &mw, &mh);
		int x = mx + (mw - w_) / 2, y = my + (mh - h_) / 2;
		c->x = x;
		c->y = y;
		c->w = w_;
		c->h = h_;
		XMoveResizeWindow(dpy, w, x, y, w_, h_);
		XSetWindowBorderWidth(dpy, w, user_config.border_width);
	}

	if (state_max_horz || state_max_vert)
		apply_maximized_state(c, state_max_horz, state_max_vert, NULL, NULL);

	if (state_hidden) {
		c->hidden = True;
		c->mapped = False;
		window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_HIDDEN], True);
	}

	update_net_client_list();
		if (!monitor_views_workspace(c->mon, target_ws))
			return;

	/* map & borders */
	if (!global_floating && !c->floating)
		tile();
	else if (c->floating)
		XRaiseWindow(dpy, w);

	/* check for swallowing opportunities */
	{
		if (window_can_be_swallowed(w)) {
			Bool swallowed_now = False;
				for (Client *p = workspaces[current_ws]; p; p = p->next) {
					if (p == c || p->swallowed || !client_is_visible_on_monitor(p, current_mon))
						continue;

				if (window_can_swallow(p->win) && check_parent(p->pid, c->pid)) {
					swallow_window(p, c);
					swallowed_now = True;
					break;
				}
			}

			if (!swallowed_now) {
				Client *candidates[2] = {
					focused,
					(current_ws >= 0 && current_ws < NUM_WORKSPACES) ? ws_focused[current_ws] : NULL,
				};

				for (size_t i = 0; i < LENGTH(candidates); i++) {
					Client *cand = candidates[i];
						if (!cand || cand == c || !client_is_visible(cand) || cand->swallowed || cand->ws != current_ws)
							continue;
					if (!window_can_swallow(cand->win))
						continue;

					swallow_window(cand, c);
					swallowed_now = True;
					break;
				}
			}
		}
	}

	if (window_has_ewmh_state(w, atoms[ATOM_NET_WM_STATE_FULLSCREEN])) {
		c->fullscreen = True;
		c->floating = False;
	}

	if (!c->hidden) {
		XMapWindow(dpy, w);
		c->mapped = True;
		if (c->fullscreen)
			apply_fullscreen(c, True);
	}
	set_frame_extents(w);
	set_allowed_actions(w);

	if (user_config.new_win_focus && !c->hidden && !c->no_focus_on_map) {
		focused = c;
		set_input_focus(focused, True, True);
		return;
	}
	if (c->no_focus_on_map)
		c->suppress_enter_focus_once = True;
	update_borders();
}

void hdl_motion(XEvent *xev)
{
	XMotionEvent *motion_ev = &xev->xmotion;

	if (drag_mode == DRAG_NONE || !drag_client) {
		if (!user_config.focus_follows_mouse || in_ws_switch)
			return;

		Window w = find_toplevel(motion_ev->window);
		if (!w || w == root) {
			Window root_ret, child_ret;
			int root_x, root_y, win_x, win_y;
			unsigned int mask;
			if (XQueryPointer(dpy, root, &root_ret, &child_ret, &root_x, &root_y, &win_x, &win_y, &mask) &&
			    child_ret != None) {
				w = find_toplevel(child_ret);
			}
		}
		if (!w || w == root)
			return;

		Client *c = find_client(w);
			if (!client_is_visible(c) || c == focused)
				return;

		set_input_focus(c, True, False);
		return;
	}

	if (user_config.motion_throttle > 0) {
		Time min_interval = (Time)(1000 / (unsigned int)user_config.motion_throttle);
		if (motion_ev->time - last_motion_time <= min_interval)
			return;
	}
	last_motion_time = motion_ev->time;

	/* figure out which monitor the pointer is in right now */
	int mon = 0;
	for (int i = 0; i < n_mons; i++) {
		Bool is_current_mon =
			motion_ev->x_root >= mons[i].x &&
			motion_ev->x_root < mons[i].x + mons[i].w &&
			motion_ev->y_root >= mons[i].y &&
			motion_ev->y_root < mons[i].y + mons[i].h;

		if (is_current_mon) {
			mon = i;
			break;
		}
	}
	int work_x, work_y, work_w, work_h;
	monitor_workarea(mon, &work_x, &work_y, &work_w, &work_h);

	if (drag_mode == DRAG_SWAP) {
		Window root_ret, child;
		int rx, ry, wx, wy;
		unsigned int mask;
		XQueryPointer(dpy, root, &root_ret, &child, &rx, &ry, &wx, &wy, &mask);

		Client *new_target = NULL;

			for (Client *c = workspaces[current_ws]; c; c = c->next) {
				if (c == drag_client || c->floating)
					continue;
			if (c->win == child) {
				new_target = c;
				break;
			}
			Window root_ret2, parent;
			Window *children;
			unsigned int n_children;
			if (XQueryTree(dpy, child, &root_ret2, &parent, &children, &n_children)) {
				if (children)
					XFree(children);
				if (parent == c->win) {
					new_target = c;
					break;
				}
			}
		}

		if (new_target != swap_target) {
			if (swap_target) {
				XSetWindowBorder(
						dpy, swap_target->win, (swap_target == focused ?
						user_config.border_foc_col : user_config.border_ufoc_col)
				);
			}
			if (new_target)
				XSetWindowBorder(dpy, new_target->win, user_config.border_swap_col);
		}

		swap_target = new_target;
		return;
	}
	else if (drag_mode == DRAG_MOVE) {
		int dx = motion_ev->x_root - drag_start_x;
		int dy = motion_ev->y_root - drag_start_y;
		int nx = drag_orig_x + dx;
		int ny = drag_orig_y + dy;

		int outer_w = drag_client->w + 2 * user_config.border_width;
		int outer_h = drag_client->h + 2 * user_config.border_width;

		/* snap relative to this mons bounds: */
		int rel_x = nx - work_x;
		int rel_y = ny - work_y;

		rel_x = snap_coordinate(rel_x, outer_w, work_w, user_config.snap_distance);
		rel_y = snap_coordinate(rel_y, outer_h, work_h, user_config.snap_distance);

		nx = work_x + rel_x;
		ny = work_y + rel_y;

		if (!drag_client->floating && (UDIST(nx, drag_client->x) > user_config.snap_distance ||
			UDIST(ny, drag_client->y) > user_config.snap_distance)) {
			toggle_floating();
		}

		XMoveWindow(dpy, drag_client->win, nx, ny);
		drag_client->x = nx;
		drag_client->y = ny;
	}
	else if (drag_mode == DRAG_RESIZE) {
		int dx = motion_ev->x_root - drag_start_x;
		int dy = motion_ev->y_root - drag_start_y;
		int nw = drag_orig_w + dx;
		int nh = drag_orig_h + dy;

		/* clamp relative to this mon */
		int max_w = (work_w - (drag_client->x - work_x));
		int max_h = (work_h - (drag_client->y - work_y));

		drag_client->w = CLAMP(nw, MIN_WINDOW_SIZE, max_w);
		drag_client->h = CLAMP(nh, MIN_WINDOW_SIZE, max_h);

		XResizeWindow(dpy, drag_client->win, drag_client->w, drag_client->h);
	}
}

void hdl_property_ntf(XEvent *xev)
{
	XPropertyEvent *property_ev = &xev->xproperty;

	if (property_ev->window == root) {
		if (property_ev->atom == atoms[ATOM_NET_CURRENT_DESKTOP]) {
			long *val = NULL;
			Atom actual;
			int fmt;
			unsigned long n;
			unsigned long after;
			if (XGetWindowProperty(dpy, root, atoms[ATOM_NET_CURRENT_DESKTOP], 0, 1, False, XA_CARDINAL, &actual,
					       &fmt, &n, &after, (unsigned char **)&val) == Success && val) {
				if (actual == XA_CARDINAL && fmt == 32 && n >= 1)
					change_workspace((int)val[0]);
				XFree(val);
			}
		}
		else if (property_ev->atom == XA_WM_NAME || property_ev->atom == atoms[ATOM_NET_WM_NAME]) {
			updatestatus();
			drawbars();
		}
	}

	if (property_ev->atom == XA_WM_HINTS) {
		Client *c = find_client(find_toplevel(property_ev->window));
		if (c)
			sync_urgency_from_wm_hints(c);
	}

	if ((property_ev->atom == atoms[ATOM_NET_WM_STRUT] ||
	     property_ev->atom == atoms[ATOM_NET_WM_STRUT_PARTIAL]) &&
	    property_ev->window != root &&
	    window_is_dock(property_ev->window)) {
		update_struts();
		tile();
		update_borders();
		return;
	}

	/* client window properties */
	if (property_ev->atom == atoms[ATOM_NET_WM_STATE]) {
		Client *c = find_client(find_toplevel(property_ev->window));
		if (!c)
			return;

		Bool want = window_has_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_FULLSCREEN]);
		if (want != c->fullscreen)
			apply_fullscreen(c, want);

		Bool want_modal = window_has_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_MODAL]);
		Bool relayout = False;
		Bool needs_borders = False;
		Bool needs_refocus = False;
		if (want_modal) {
			if (!c->floating && !c->fullscreen) {
				c->floating = True;
				c->modal_forced_floating = True;
				relayout = True;
			}
		}
		else {
			if (c->modal_forced_floating && !c->fullscreen && !global_floating) {
				c->floating = False;
				c->mon = get_monitor_for(c);
				relayout = True;
			}
			c->modal_forced_floating = False;
		}

		Bool want_max_h = window_has_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_MAXIMIZED_HORZ]);
		Bool want_max_v = window_has_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_MAXIMIZED_VERT]);
		apply_maximized_state(c, want_max_h, want_max_v, &relayout, &needs_borders);

		Bool want_hidden = window_has_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_HIDDEN]);
		apply_hidden_state(c, want_hidden, &relayout, &needs_borders, &needs_refocus);

		c->sticky = window_has_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_STICKY]);
		update_client_desktop_properties();

		Bool want_above = window_has_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_ABOVE]);
		Bool want_below = window_has_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_BELOW]);
			if (want_above)
				XRaiseWindow(dpy, c->win);
			else if (want_below)
				XLowerWindow(dpy, c->win);

			if (relayout && client_should_be_visible(c))
				tile();
			if (needs_refocus && client_should_be_visible(c)) {
				Client *next = next_focus_candidate(c);
				set_input_focus(next, True, False);
			}
			else if ((needs_borders || relayout) && client_should_be_visible(c))
				update_borders();
		}
	}

void hdl_unmap_ntf(XEvent *xev)
{
	Window w = xev->xunmap.window;
	Client *unmapped = NULL;
	int unmapped_ws = -1;
	Bool found = False;
	for (int ws = 0; ws < NUM_WORKSPACES && !found; ws++) {
		for (Client *c = workspaces[ws]; c; c = c->next) {
			if (c->win != w)
				continue;

			if (c->ignore_unmap_events > 0) {
				c->ignore_unmap_events--;
				return;
			}

			c->mapped = False;
			unmapped = c;
			unmapped_ws = ws;
			found = True;
			break;
		}
	}

	if (unmapped) {
		Client *replacement = first_visible_client(unmapped_ws, unmapped->mon, unmapped);

		if (ws_focused[unmapped_ws] == unmapped)
			ws_focused[unmapped_ws] = replacement;
		if (workspace_states[unmapped_ws].focused == unmapped)
			workspace_states[unmapped_ws].focused = replacement;

		if (focused == unmapped)
			set_input_focus(replacement, True, False);
	}

	update_net_client_list();
	tile();
	update_borders();
}

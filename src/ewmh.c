/* Extracted from cupidwm.c: EWMH atoms and properties. */

void setup_atoms(void)
{
	for (int i = 0; i < ATOM_COUNT; i++)
		atoms[i] = XInternAtom(dpy, atom_names[i], False);

	/* checking window */
	wm_check_win = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	/* root property -> child window */
	XChangeProperty(dpy, root, atoms[ATOM_NET_SUPPORTING_WM_CHECK], XA_WINDOW, 32,
			        PropModeReplace, (unsigned char *)&wm_check_win, 1);
	/* child window -> child window */
	XChangeProperty(dpy, wm_check_win, atoms[ATOM_NET_SUPPORTING_WM_CHECK], XA_WINDOW, 32,
			        PropModeReplace, (unsigned char *)&wm_check_win, 1);
	/* name the wm */
	const char *wmname = "cupidwm";
	XChangeProperty(dpy, wm_check_win, atoms[ATOM_NET_WM_NAME], atoms[ATOM_UTF8_STRING], 8,
			        PropModeReplace, (const unsigned char *)wmname, strlen(wmname));
	XStoreName(dpy, wm_check_win, wmname);
	{
		XClassHint ch = {0};
		ch.res_name = (char *)wmname;
		ch.res_class = (char *)wmname;
		XSetClassHint(dpy, wm_check_win, &ch);
	}
	XChangeProperty(dpy, root, atoms[ATOM_NET_WM_NAME], atoms[ATOM_UTF8_STRING], 8,
		        PropModeReplace, (const unsigned char *)wmname, strlen(wmname));

	/* workspace setup */
	long num_workspaces = NUM_WORKSPACES;
	XChangeProperty(dpy, root, atoms[ATOM_NET_NUMBER_OF_DESKTOPS], XA_CARDINAL, 32,
			        PropModeReplace, (const unsigned char *)&num_workspaces, 1);

	const char workspace_names[] = WORKSPACE_NAMES;
	int names_len = sizeof(workspace_names);
	XChangeProperty(dpy, root, atoms[ATOM_NET_DESKTOP_NAMES], atoms[ATOM_UTF8_STRING], 8,
			        PropModeReplace, (const unsigned char *)workspace_names, names_len);

	XChangeProperty(dpy, root, atoms[ATOM_NET_CURRENT_DESKTOP], XA_CARDINAL, 32,
			        PropModeReplace, (const unsigned char *)&current_ws, 1);

	/* load supported list */
	XChangeProperty(dpy, root, atoms[ATOM_NET_SUPPORTED], XA_ATOM, 32,
			        PropModeReplace, (const unsigned char *)atoms, ATOM_COUNT);

	update_workarea();
}

void set_frame_extents(Window w)
{
	long extents[4] = {
		user_config.border_width,
		user_config.border_width,
		user_config.border_width,
		user_config.border_width
	};
	XChangeProperty(dpy, w, atoms[ATOM_NET_FRAME_EXTENTS], XA_CARDINAL, 32,
			        PropModeReplace, (unsigned char *)extents, 4);
}

void update_client_desktop_properties(void)
{
	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		for (Client *c = workspaces[ws]; c; c = c->next) {
			long desktop = ws;
			XChangeProperty(dpy, c->win, atoms[ATOM_NET_WM_DESKTOP], XA_CARDINAL, 32,
					        PropModeReplace, (unsigned char *)&desktop, 1);
		}
	}
}

void update_net_client_list(void)
{
	Window wins[MAX_CLIENTS];
	int n = 0;
	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		for (Client *c = workspaces[ws]; c; c = c->next) {
			if (n >= MAX_CLIENTS) {
				fprintf(stderr, "cupidwm: max clients reached while exporting _NET_CLIENT_LIST\n");
				goto publish_client_list;
			}
			wins[n++] = c->win;
		}
	}

publish_client_list:
	XChangeProperty(dpy, root, atoms[ATOM_NET_CLIENT_LIST], XA_WINDOW, 32, PropModeReplace, (unsigned char *)wins, n);
}

void update_struts(void)
{
	/* reset all reserves */
	for (int i = 0; i < n_mons; i++) {
		mons[i].reserve_left   = 0;
		mons[i].reserve_right  = 0;
		mons[i].reserve_top    = 0;
		mons[i].reserve_bottom = 0;
	}

	Window root_ret;
	Window parent_ret;
	Window *children = NULL;
	unsigned int n_children = 0;

	if (!XQueryTree(dpy, root, &root_ret, &parent_ret, &children, &n_children))
		return;

	int screen_w = scr_width;
	int screen_h = scr_height;

	for (unsigned int i = 0; i < n_children; i++) {
		Window w = children[i];

		XWindowAttributes wa;
		if (!XGetWindowAttributes(dpy, w, &wa) || wa.map_state != IsViewable)
			continue;

		Atom actual_type;
		int actual_format;
		unsigned long n_items, bytes_after;
		Atom *types = NULL;

		if (XGetWindowProperty(dpy, w, atoms[ATOM_NET_WM_WINDOW_TYPE], 0, 4, False, XA_ATOM,
			&actual_type, &actual_format, &n_items, &bytes_after,
			(unsigned char **)&types) != Success || !types)
			continue;

		if (actual_type != XA_ATOM || actual_format != 32) {
			XFree(types);
			continue;
		}

		Bool is_dock = False;
		for (unsigned long j = 0; j < n_items; j++) {
			if (types[j] == atoms[ATOM_NET_WM_WINDOW_TYPE_DOCK]) {
				is_dock = True;
				break;
			}
		}
		XFree(types);
		if (!is_dock)
			continue;

		long *str = NULL;
		Atom actual;
		int sfmt;
		unsigned long len;
		unsigned long rem;

		if (XGetWindowProperty(dpy, w, atoms[ATOM_NET_WM_STRUT_PARTIAL], 0, 12, False, XA_CARDINAL,
					&actual, &sfmt, &len, &rem,
					(unsigned char **)&str) == Success && str) {
			if (actual != XA_CARDINAL || sfmt != 32 || len < 12) {
				XFree(str);
				continue;
			}

			/*
			 ewmh:
			 [0] left, [1] right, [2] top, [3] bottom
			 
			 [4] left_start_y,   [5] left_end_y
			 [6] right_start_y,  [7] right_end_y
			 [8] top_start_x,    [9] top_end_x
			 [10] bottom_start_x,[11] bottom_end_x
			 
			 all coords are in root space.
			 */
			long left = str[0];
			long right = str[1];
			long top = str[2];
			long bottom = str[3];
			long left_start_y = str[4];
			long left_end_y = str[5];
			long right_start_y = str[6];
			long right_end_y = str[7];
			long top_start_x = str[8];
			long top_end_x = str[9];
			long bot_start_x = str[10];
			long bot_end_x = str[11];

			XFree(str);

			/* skip empty struts */
			if (!left && !right && !top && !bottom)
				continue;

			for (int m = 0; m < n_mons; m++) {
				int mx = mons[m].x;
				int my = mons[m].y;
				int mw = mons[m].w;
				int mh = mons[m].h;

				/* strip monitors whose vertical span dostn intersect */
				if (left > 0) {
					long span_start = left_start_y;
					long span_end   = left_end_y;
					if (span_end >= my && span_start <= my + mh - 1) {
						/*
						 left is distance from root left edge to reserved area
						 to map to mon, the portion is:
						     reserve_left = MAX(0, left - mx)
						 */
						int reserve = (int)MAX(0, left - mx);
						if (reserve > 0)
							mons[m].reserve_left = MAX(mons[m].reserve_left, reserve);
					}
				}

				if (right > 0) {
					long span_start = right_start_y;
					long span_end   = right_end_y;
					if (span_end >= my && span_start <= my + mh - 1) {
						/*
						 right is distance from root right edge to reserved area:
						     right edge = screen_w
							 mons right edge = mx + mw
							 amount that cuts into monitor = MAX(0, (screen_w - right) - mx)
						 */
						int global_reserved_left = screen_w - (int)right;
						int overlap = (mx + mw) - global_reserved_left;
						int reserve = MAX(0, overlap);
						if (reserve > 0)
							mons[m].reserve_right = MAX(mons[m].reserve_right, reserve);
					}
				}

				if (top > 0) {
					long span_start = top_start_x;
					long span_end   = top_end_x;
					if (span_end >= mx && span_start <= mx + mw - 1) {
						/*
						 top is distance from root top to reserved area
							 mons top is at my, amount eaten:
							 reserve_top = MAX(0, top - my)
						 */
						int reserve = (int)MAX(0, top - my);
						if (reserve > 0)
							mons[m].reserve_top = MAX(mons[m].reserve_top, reserve);
					}
				}

				if (bottom > 0) {
					long span_start = bot_start_x;
					long span_end   = bot_end_x;
					if (span_end >= mx && span_start <= mx + mw - 1) {
						/*
						 bottom is distance from root bottom to reserved area
						 global_reserved_top = screen_h - bottom;
						 overlap to mon:
						   overlap = (my + mh) - global_reserved_top;
						   reserve_bottom = MAX(0, overlap)
						 */
						int global_reserved_top = screen_h - (int)bottom;
						int overlap = (my + mh) - global_reserved_top;
						int reserve = MAX(0, overlap);
						if (reserve > 0)
							mons[m].reserve_bottom = MAX(mons[m].reserve_bottom, reserve);
					}
				}
			}
		}
	}

	if (children)
		XFree(children);

	if (user_config.showbar) {
		for (int i = 0; i < n_mons; i++) {
			if (user_config.topbar)
				mons[i].reserve_top += user_config.bar_height;
			else
				mons[i].reserve_bottom += user_config.bar_height;
		}
	}

	update_workarea();
}

void update_workarea(void)
{
	long workarea[4 * MAX_MONITORS];
	int workarea_mons = MIN(n_mons, MAX_MONITORS);

	for (int i = 0; i < workarea_mons; i++) {
		workarea[i * 4 + 0] = mons[i].x + mons[i].reserve_left;
		workarea[i * 4 + 1] = mons[i].y + mons[i].reserve_top;
		workarea[i * 4 + 2] = mons[i].w - mons[i].reserve_left - mons[i].reserve_right;
		workarea[i * 4 + 3] = mons[i].h - mons[i].reserve_top - mons[i].reserve_bottom;
	}

	XChangeProperty(dpy, root, atoms[ATOM_NET_WORKAREA], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)workarea, workarea_mons * 4);
}

Bool window_has_ewmh_state(Window w, Atom state)
{
	Atom type;
	int format;
	unsigned long n_atoms = 0;
	unsigned long unread = 0;
	Atom *found_atoms = NULL;

	if (XGetWindowProperty(dpy, w, atoms[ATOM_NET_WM_STATE], 0, 1024, False, XA_ATOM, &type,
		&format, &n_atoms, &unread, (unsigned char**)&found_atoms) == Success && found_atoms) {
		if (type == XA_ATOM && format == 32) {
			for (unsigned long i = 0; i < n_atoms; i++) {
				if (found_atoms[i] == state) {
					XFree(found_atoms);
					return True;
				}
			}
		}
		XFree(found_atoms);
	}
	return False;
}

void window_set_ewmh_state(Window w, Atom state, Bool add)
{
	Atom type;
	int format;
	unsigned long n_atoms = 0;
	unsigned long unread = 0;
	Atom *found_atoms = NULL;

	if (XGetWindowProperty(dpy, w, atoms[ATOM_NET_WM_STATE], 0, 1024, False, XA_ATOM, &type,
		&format, &n_atoms, &unread, (unsigned char**)&found_atoms) != Success) {
		found_atoms = NULL;
		n_atoms = 0;
	}
	else if (found_atoms && (type != XA_ATOM || format != 32)) {
		XFree(found_atoms);
		found_atoms = NULL;
		n_atoms = 0;
	}

	unsigned long capacity = n_atoms + (add ? 1UL : 0UL);
	Atom *list = NULL;
	unsigned long list_len = 0;
	if (capacity > 0) {
		list = calloc(capacity, sizeof(Atom));
		if (!list) {
			if (found_atoms)
				XFree(found_atoms);
			return;
		}
	}

	if (found_atoms) {
		for (unsigned long i = 0; i < n_atoms; i++) {
			if (found_atoms[i] != state)
				list[list_len++] = found_atoms[i];
		}
	}
	if (add)
		list[list_len++] = state;

	if (list_len == 0)
		XDeleteProperty(dpy, w, atoms[ATOM_NET_WM_STATE]);
	else
		XChangeProperty(dpy, w, atoms[ATOM_NET_WM_STATE], XA_ATOM, 32, PropModeReplace, (unsigned char*)list, list_len);

	free(list);
	if (found_atoms)
		XFree(found_atoms);
}

/* Extracted from cupidwm.c: EWMH atoms and properties. */

static int client_map_seq_cmp(const void *lhs, const void *rhs)
{
	const Client *const *a = lhs;
	const Client *const *b = rhs;

	if ((*a)->map_seq < (*b)->map_seq)
		return -1;
	if ((*a)->map_seq > (*b)->map_seq)
		return 1;
	return 0;
}

static int collect_managed_clients(Client **clients, int max_clients)
{
	int n = 0;

	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		for (Client *c = workspaces[ws]; c; c = c->next) {
			if (n >= max_clients) {
				fprintf(stderr, "cupidwm: max clients reached while exporting _NET_CLIENT_LIST\n");
				return n;
			}
			clients[n++] = c;
		}
	}

	return n;
}

static Bool client_list_contains(Client *const *clients, int n, Client *target)
{
	for (int i = 0; i < n; i++) {
		if (clients[i] == target)
			return True;
	}

	return False;
}

Bool window_is_dock(Window w)
{
	Atom actual_type = None;
	int actual_format = 0;
	unsigned long n_items = 0;
	unsigned long bytes_after = 0;
	Atom *types = NULL;
	Bool is_dock = False;

	if (XGetWindowProperty(dpy, w, atoms[ATOM_NET_WM_WINDOW_TYPE], 0, 8, False, XA_ATOM,
			       &actual_type, &actual_format, &n_items, &bytes_after,
			       (unsigned char **)&types) != Success || !types)
		return False;

	if (actual_type == XA_ATOM && actual_format == 32) {
		for (unsigned long i = 0; i < n_items; i++) {
			if (types[i] == atoms[ATOM_NET_WM_WINDOW_TYPE_DOCK]) {
				is_dock = True;
				break;
			}
		}
	}

	XFree(types);
	return is_dock;
}

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

void update_focused_ewmh_state(Client *active)
{
	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		for (Client *c = workspaces[ws]; c; c = c->next)
			window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_FOCUSED], c == active);
	}
}

void update_client_desktop_properties(void)
{
	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		for (Client *c = workspaces[ws]; c; c = c->next) {
			unsigned long desktop = c->sticky ? 0xFFFFFFFFUL : (unsigned long)ws;
			XChangeProperty(dpy, c->win, atoms[ATOM_NET_WM_DESKTOP], XA_CARDINAL, 32,
					        PropModeReplace, (unsigned char *)&desktop, 1);
		}
	}
}

void update_net_client_list(void)
{
	Client *clients[MAX_CLIENTS];
	Client *stacking_clients[MAX_CLIENTS];
	Window map_list[MAX_CLIENTS];
	Window stacking_list[MAX_CLIENTS];
	int n_clients = collect_managed_clients(clients, MAX_CLIENTS);
	int n_stacking = 0;

	if (n_clients > 1)
		qsort(clients, (size_t)n_clients, sizeof(clients[0]), client_map_seq_cmp);

	for (int i = 0; i < n_clients; i++)
		map_list[i] = clients[i]->win;

	Window root_ret = None;
	Window parent_ret = None;
	Window *children = NULL;
	unsigned int n_children = 0;
	if (XQueryTree(dpy, root, &root_ret, &parent_ret, &children, &n_children)) {
		for (unsigned int i = 0; i < n_children && n_stacking < n_clients; i++) {
			Client *c = find_client(children[i]);
			if (!c || client_list_contains(stacking_clients, n_stacking, c))
				continue;
			stacking_clients[n_stacking] = c;
			stacking_list[n_stacking++] = c->win;
		}
	}
	if (children)
		XFree(children);

	for (int i = 0; i < n_clients && n_stacking < n_clients; i++) {
		if (client_list_contains(stacking_clients, n_stacking, clients[i]))
			continue;
		stacking_clients[n_stacking] = clients[i];
		stacking_list[n_stacking++] = clients[i]->win;
	}

	XChangeProperty(dpy, root, atoms[ATOM_NET_CLIENT_LIST], XA_WINDOW, 32, PropModeReplace,
	                (unsigned char *)map_list, n_clients);
	XChangeProperty(dpy, root, atoms[ATOM_NET_CLIENT_LIST_STACKING], XA_WINDOW, 32, PropModeReplace,
	                (unsigned char *)stacking_list, n_stacking);
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

		if (!window_is_dock(w))
			continue;

		long left = 0;
		long right = 0;
		long top = 0;
		long bottom = 0;
		long left_start_y = 0;
		long left_end_y = screen_h - 1;
		long right_start_y = 0;
		long right_end_y = screen_h - 1;
		long top_start_x = 0;
		long top_end_x = screen_w - 1;
		long bot_start_x = 0;
		long bot_end_x = screen_w - 1;
		Bool have_strut = False;

		long *str = NULL;
		Atom actual = None;
		int sfmt = 0;
		unsigned long len = 0;
		unsigned long rem = 0;

		if (XGetWindowProperty(dpy, w, atoms[ATOM_NET_WM_STRUT_PARTIAL], 0, 12, False, XA_CARDINAL,
				       &actual, &sfmt, &len, &rem,
				       (unsigned char **)&str) == Success && str) {
			if (actual == XA_CARDINAL && sfmt == 32 && len >= 12) {
				left = str[0];
				right = str[1];
				top = str[2];
				bottom = str[3];
				left_start_y = str[4];
				left_end_y = str[5];
				right_start_y = str[6];
				right_end_y = str[7];
				top_start_x = str[8];
				top_end_x = str[9];
				bot_start_x = str[10];
				bot_end_x = str[11];
				have_strut = True;
			}
			XFree(str);
		}

		if (!have_strut) {
			str = NULL;
			if (XGetWindowProperty(dpy, w, atoms[ATOM_NET_WM_STRUT], 0, 4, False, XA_CARDINAL,
					       &actual, &sfmt, &len, &rem,
					       (unsigned char **)&str) == Success && str) {
				if (actual == XA_CARDINAL && sfmt == 32 && len >= 4) {
					left = str[0];
					right = str[1];
					top = str[2];
					bottom = str[3];
					have_strut = True;
				}
				XFree(str);
			}
		}

		/* skip empty struts */
		if (!have_strut || (!left && !right && !top && !bottom))
			continue;

		for (int m = 0; m < n_mons; m++) {
			int mx = mons[m].x;
			int my = mons[m].y;
			int mw = mons[m].w;
			int mh = mons[m].h;

			/* strip monitors whose vertical span does not intersect */
			if (left > 0) {
				long span_start = left_start_y;
				long span_end = left_end_y;
				if (span_end >= my && span_start <= my + mh - 1) {
					int reserve = (int)MAX(0, left - mx);
					if (reserve > 0)
						mons[m].reserve_left = MAX(mons[m].reserve_left, reserve);
				}
			}

			if (right > 0) {
				long span_start = right_start_y;
				long span_end = right_end_y;
				if (span_end >= my && span_start <= my + mh - 1) {
					int global_reserved_left = screen_w - (int)right;
					int overlap = (mx + mw) - global_reserved_left;
					int reserve = MAX(0, overlap);
					if (reserve > 0)
						mons[m].reserve_right = MAX(mons[m].reserve_right, reserve);
				}
			}

			if (top > 0) {
				long span_start = top_start_x;
				long span_end = top_end_x;
				if (span_end >= mx && span_start <= mx + mw - 1) {
					int reserve = (int)MAX(0, top - my);
					if (reserve > 0)
						mons[m].reserve_top = MAX(mons[m].reserve_top, reserve);
				}
			}

			if (bottom > 0) {
				long span_start = bot_start_x;
				long span_end = bot_end_x;
				if (span_end >= mx && span_start <= mx + mw - 1) {
					int global_reserved_top = screen_h - (int)bottom;
					int overlap = (my + mh) - global_reserved_top;
					int reserve = MAX(0, overlap);
					if (reserve > 0)
						mons[m].reserve_bottom = MAX(mons[m].reserve_bottom, reserve);
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

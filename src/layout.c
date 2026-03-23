/* Layout engines and geometry application for tiled/floating arrangements. */

static void apply_client_geom(Client *c, int x, int y, int w, int h)
{
	int nx = x;
	int ny = y;
	int iw = MAX(1, w - (2 * user_config.border_width));
	int ih = MAX(1, h - (2 * user_config.border_width));

	apply_size_hints(c, &nx, &ny, &iw, &ih, False);

	XWindowChanges wc = {
		.x = nx,
		.y = ny,
		.width = iw,
		.height = ih,
		.border_width = user_config.border_width
	};

	Bool geom_differ =
		c->x != wc.x || c->y != wc.y ||
		c->w != wc.width || c->h != wc.height;
	if (geom_differ)
		XConfigureWindow(dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);

	c->x = wc.x;
	c->y = wc.y;
	c->w = wc.width;
	c->h = wc.height;
}

static void arrange_fibonacci(Client *tileable[], int n_tileable, int x, int y, int w, int h,
			      Bool dwindle, int gaps)
{
	int rx = x;
	int ry = y;
	int rw = w;
	int rh = h;
	int dir = 0;
	const float phi = 0.618f;

	for (int i = 0; i < n_tileable; i++) {
		Client *c = tileable[i];
		int cx = rx;
		int cy = ry;
		int cw = rw;
		int ch = rh;

		if (i < n_tileable - 1) {
			if (dir % 2 == 0) {
				int split = MAX(1, (int)(rw * phi));
				if (split >= rw)
					split = MAX(1, rw / 2);

				if (dir == 0) {
					cw = split;
					rx += split + gaps;
					rw = MAX(1, rw - split - gaps);
				}
				else {
					cx = rx + rw - split;
					cw = split;
					rw = MAX(1, rw - split - gaps);
				}
			}
			else {
				int split = MAX(1, (int)(rh * phi));
				if (split >= rh)
					split = MAX(1, rh / 2);

				if (dir == 1) {
					ch = split;
					ry += split + gaps;
					rh = MAX(1, rh - split - gaps);
				}
				else {
					cy = ry + rh - split;
					ch = split;
					rh = MAX(1, rh - split - gaps);
				}
			}
			dir = (dir + (dwindle ? 3 : 1)) % 4;
		}

		apply_client_geom(c, cx, cy, cw, ch);
	}
}

static void arrange_grid(Client *tileable[], int n_tileable, int x, int y, int w, int h, int gaps)
{
	if (!tileable || n_tileable <= 0)
		return;

	int cols = 1;
	while (cols * cols < n_tileable)
		cols++;
	int rows = (n_tileable + cols - 1) / cols;

	int row_h = rows > 0 ? MAX(1, (h - (rows - 1) * gaps) / rows) : h;
	int idx = 0;
	int cy = y;

	for (int row = 0; row < rows; row++) {
		int rem = n_tileable - idx;
		if (rem <= 0)
			break;
		int row_cols = MIN(cols, rem);
		int col_w = row_cols > 0 ? MAX(1, (w - (row_cols - 1) * gaps) / row_cols) : w;
		int cx = x;
		int ch = (row == rows - 1) ? MAX(1, y + h - cy) : row_h;

		for (int col = 0; col < row_cols; col++) {
			int cw = (col == row_cols - 1) ? MAX(1, x + w - cx) : col_w;
			apply_client_geom(tileable[idx++], cx, cy, cw, ch);
			cx += cw + gaps;
		}

		cy += ch + gaps;
	}
}

static void arrange_columns(Client *tileable[], int n_tileable, int x, int y, int w, int h, int gaps)
{
	if (!tileable || n_tileable <= 0)
		return;

	int col_w = n_tileable > 0 ? MAX(1, (w - (n_tileable - 1) * gaps) / n_tileable) : w;
	int cx = x;

	for (int i = 0; i < n_tileable; i++) {
		int cw = (i == n_tileable - 1) ? MAX(1, x + w - cx) : col_w;
		apply_client_geom(tileable[i], cx, y, cw, h);
		cx += cw + gaps;
	}
}

void tile(void)
{
	update_struts();

	for (int m = 0; m < n_mons; m++) {
		int ws = mons[m].view_ws;
		Client *head = workspaces[ws];
		int mode = layouts[workspace_layout_for(ws)].mode;
		int gaps = workspace_gaps_for(ws);
		int mon_x = mons[m].x + mons[m].reserve_left;
		int mon_y = mons[m].y + mons[m].reserve_top;
		int mon_width = MAX(1, mons[m].w - mons[m].reserve_left - mons[m].reserve_right);
		int mon_height = MAX(1, mons[m].h - mons[m].reserve_top  - mons[m].reserve_bottom);

		Client *tileable[MAX_CLIENTS] = {0};
		int n_tileable = 0;
		for (Client *c = head; c && n_tileable < MAX_CLIENTS; c = c->next) {
			if (client_is_visible(c) && c->mon == m && !c->floating && !c->fullscreen)
				tileable[n_tileable++] = c;
		}

		if (n_tileable == 0)
			continue;

		int tile_x = mon_x + gaps;
		int tile_y = mon_y + gaps;
		int tile_width = MAX(1, mon_width - 2 * gaps);
		int tile_height = MAX(1, mon_height - 2 * gaps);

		if (mode == LayoutMonocle) {
			for (int i = 0; i < n_tileable; i++) {
				Client *c = tileable[i];
				apply_client_geom(c, tile_x, tile_y, tile_width, tile_height);
			}

			if (focused && client_is_visible(focused) && focused->mon == m &&
			    !focused->floating && !focused->fullscreen)
				XRaiseWindow(dpy, focused->win);
			continue;
		}

		if (mode == LayoutFibonacci || mode == LayoutDwindle) {
			arrange_fibonacci(tileable, n_tileable, tile_x, tile_y, tile_width, tile_height,
			                  mode == LayoutDwindle, gaps);
			continue;
		}
		if (mode == LayoutGrid) {
			arrange_grid(tileable, n_tileable, tile_x, tile_y, tile_width, tile_height, gaps);
			continue;
		}
		if (mode == LayoutColumns) {
			arrange_columns(tileable, n_tileable, tile_x, tile_y, tile_width, tile_height, gaps);
			continue;
		}

		float *master_width_slot = workspace_master_width_for(ws, m);
		float master_frac = CLAMP(master_width_slot ? *master_width_slot : master_width_default, MF_MIN, MF_MAX);
		int nmaster = MIN(workspace_nmaster_for(ws), n_tileable);
		Bool has_stack = n_tileable > nmaster;
		int master_width = has_stack ? (int)(tile_width * master_frac) : tile_width;
		int stack_width = has_stack ? (tile_width - master_width - gaps) : 0;

		int border_width = 2 * user_config.border_width;
		int master_y = tile_y;
		for (int i = 0; i < nmaster; i++) {
			int masters_left = nmaster - i;
			int remaining_master_h = tile_y + tile_height - master_y;
			int mh = MAX(1, (remaining_master_h - (masters_left - 1) * gaps) / masters_left);
			Client *c = tileable[i];
			apply_client_geom(c, tile_x, master_y, master_width, mh);

			master_y += mh + gaps;
		}

		if (!has_stack)
			continue;

		int stack_start = nmaster;
		int n_stack = n_tileable - stack_start;
		int min_stack_height = border_width + 1;
		int total_fixed_heights = 0;
		int n_auto = 0; /* automatically take up leftover space */
		int heights_final[MAX_CLIENTS] = {0};

		for (int i = stack_start; i < n_tileable; i++) {
			if (tileable[i]->custom_stack_height > 0)
				total_fixed_heights += tileable[i]->custom_stack_height;
			else
				n_auto++;
		}

		int total_vgaps = (n_stack - 1) * gaps;
		int remaining = tile_height - total_fixed_heights - total_vgaps;

		if (n_auto > 0 && remaining >= n_auto * min_stack_height) {
			int used = 0;
			int count = 0;
			int auto_height = remaining / n_auto;

			for (int i = stack_start; i < n_tileable; i++) {
				if (tileable[i]->custom_stack_height > 0) {
					heights_final[i] = tileable[i]->custom_stack_height;
				}
				else {
					count++;
					heights_final[i] = (count < n_auto) ? auto_height : remaining - used;
					used += auto_height;
				}
			}
		}
		else {
			for (int i = stack_start; i < n_tileable; i++) {
				if (tileable[i]->custom_stack_height > 0)
					heights_final[i] = tileable[i]->custom_stack_height;
				else
					heights_final[i] = min_stack_height;
			}
		}

		int total_height = total_vgaps;
		for (int i = stack_start; i < n_tileable; i++)
			total_height += heights_final[i];

		int overfill = total_height - tile_height;
		if (overfill > 0) {
			/* shrink from top down, excluding bottom */
			for (int i = stack_start; i < n_tileable - 1 && overfill > 0; i++) {
				int shrink = MIN(overfill, heights_final[i] - min_stack_height);
				heights_final[i] -= shrink;
				overfill -= shrink;
			}
		}

		/* if its not perfectly filled stretch bottom to absorb remainder */
		int actual_height = total_vgaps;
		for (int i = stack_start; i < n_tileable; i++)
			actual_height += heights_final[i];

		int shortfall = tile_height - actual_height;
		if (shortfall > 0)
			heights_final[n_tileable - 1] += shortfall;

		int stack_y = tile_y;
		for (int i = stack_start; i < n_tileable; i++) {
			Client *c = tileable[i];
			apply_client_geom(c, tile_x + master_width + gaps, stack_y,
			                  stack_width, heights_final[i]);

			stack_y += heights_final[i] + gaps;
		}
	}

	for (int m = 0; m < n_mons; m++)
		restack_monitor(m);

	update_borders();
}

void toggle_floating(void)
{
	if (!focused)
		return;

	if (focused->fullscreen) {
		focused->fullscreen = False;
		tile();
		XSetWindowBorderWidth(dpy, focused->win, user_config.border_width);
	}

	focused->floating = !focused->floating;

	if (focused->floating) {
		XWindowAttributes wa;
		if (XGetWindowAttributes(dpy, focused->win, &wa)) {
			int nx = wa.x;
			int ny = wa.y;
			int nw = wa.width;
			int nh = wa.height;
			apply_size_hints(focused, &nx, &ny, &nw, &nh, True);
			focused->x = nx;
			focused->y = ny;
			focused->w = nw;
			focused->h = nh;

			XWindowChanges wc = {
				.x = focused->x,
				.y = focused->y,
				.width = focused->w,
				.height = focused->h
			};
			XConfigureWindow(dpy, focused->win, CWX | CWY | CWWidth | CWHeight, &wc);
		}
	}
	else {
		focused->mon = get_monitor_for(focused);
	}

	if (!focused->floating)
		focused->mon = get_monitor_for(focused);

	tile();
	update_borders();

	/* raise and refocus floating window */
	if (focused->floating)
		set_input_focus(focused, True, False);
}

void toggle_floating_global(void)
{
	global_floating = !global_floating;

	for (Client *c = workspaces[current_ws]; c; c = c->next) {
		if (global_floating) {
			if (!c->floating_saved) {
				c->prev_floating = c->floating;
				c->floating_saved = True;
			}

			c->floating = True;
			if (!c->fullscreen) {
				XWindowAttributes wa;
				if (XGetWindowAttributes(dpy, c->win, &wa)) {
					int nx = wa.x;
					int ny = wa.y;
					int nw = wa.width;
					int nh = wa.height;
					apply_size_hints(c, &nx, &ny, &nw, &nh, True);
					c->x = nx;
					c->y = ny;
					c->w = nw;
					c->h = nh;

					XWindowChanges wc = {
						.x = c->x,
						.y = c->y,
						.width = c->w,
						.height = c->h
					};
					XConfigureWindow(dpy, c->win, CWX | CWY | CWWidth | CWHeight, &wc);
				}
				XRaiseWindow(dpy, c->win);
			}
		}
		else if (c->floating_saved) {
			c->floating = c->prev_floating;
			c->floating_saved = False;
			if (!c->floating && !c->fullscreen)
				c->mon = get_monitor_for(c);
		}
	}

	tile();
	update_borders();
}

void toggle_fullscreen(void)
{
	if (!focused)
		return;

	apply_fullscreen(focused, !focused->fullscreen);
}

void toggle_monocle(void)
{
	int layout = workspace_layout_for(current_ws);
	if (layouts[layout].mode == LayoutMonocle)
		workspace_states[current_ws].layout = layout_index_from_mode(LayoutTile);
	else
		workspace_states[current_ws].layout = layout_index_from_mode(LayoutMonocle);
	if (workspace_states[current_ws].layout < 0)
		workspace_states[current_ws].layout = 0;
	sync_active_monitor_state();
	tile();
	update_borders();
	session_state_save();
	if (focused)
		set_input_focus(focused, True, True);
}

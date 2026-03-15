/* Extracted from cupidwm.c: layout and tiling transitions. */

static void apply_client_geom(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc = {
		.x = x,
		.y = y,
		.width = MAX(1, w - (2 * user_config.border_width)),
		.height = MAX(1, h - (2 * user_config.border_width)),
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

static void arrange_fibonacci(Client *tileable[], int n_tileable, int x, int y, int w, int h, Bool dwindle)
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
					rx += split + user_config.gaps;
					rw = MAX(1, rw - split - user_config.gaps);
				}
				else {
					cx = rx + rw - split;
					cw = split;
					rw = MAX(1, rw - split - user_config.gaps);
				}
			}
			else {
				int split = MAX(1, (int)(rh * phi));
				if (split >= rh)
					split = MAX(1, rh / 2);

				if (dir == 1) {
					ch = split;
					ry += split + user_config.gaps;
					rh = MAX(1, rh - split - user_config.gaps);
				}
				else {
					cy = ry + rh - split;
					ch = split;
					rh = MAX(1, rh - split - user_config.gaps);
				}
			}
			dir = (dir + (dwindle ? 3 : 1)) % 4;
		}

		apply_client_geom(c, cx, cy, cw, ch);
	}
}

void tile(void)
{
	update_struts();
	Client *head = workspaces[current_ws];
	int mode = layouts[current_layout].mode;
	if (monocle)
		mode = LayoutMonocle;
	int total = 0;

	for (Client *c = head; c; c = c->next) {
		if (c->mapped && !c->floating && !c->fullscreen)
			total++;
	}

	if (total == 0)
		return;

	if (mode == LayoutMonocle) {
		for (Client *c = head; c; c = c->next) {
			if (!c->mapped || c->floating || c->fullscreen)
				continue;

			int border_width = user_config.border_width;
			int gaps = user_config.gaps;

			int mon = CLAMP(c->mon, 0, n_mons - 1);
			int x = mons[mon].x + mons[mon].reserve_left + gaps;
			int y = mons[mon].y + mons[mon].reserve_top + gaps;
			int w = mons[mon].w - mons[mon].reserve_left - mons[mon].reserve_right - 2 * gaps;
			int h = mons[mon].h - mons[mon].reserve_top - mons[mon].reserve_bottom - 2 * gaps;

			XWindowChanges wc = {
				.x = x,
				.y = y,
				.width = MAX(1, w - 2 * border_width),
				.height = MAX(1, h - 2 * border_width),
				.border_width = border_width
			};
			XConfigureWindow(dpy, c->win,
					CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);

			c->x = wc.x;
			c->y = wc.y;
			c->w = wc.width;
			c->h = wc.height;
		}

		if (focused && focused->mapped && !focused->floating && !focused->fullscreen)
			XRaiseWindow(dpy, focused->win);

		update_borders();
		return;
	}

	for (int m = 0; m < n_mons; m++) {
		int mon_x = mons[m].x + mons[m].reserve_left;
		int mon_y = mons[m].y + mons[m].reserve_top;
		int mon_width = MAX(1, mons[m].w - mons[m].reserve_left - mons[m].reserve_right);
		int mon_height = MAX(1, mons[m].h - mons[m].reserve_top  - mons[m].reserve_bottom);

		Client *tileable[MAX_CLIENTS] = {0};
		int n_tileable = 0;
		for (Client *c = head; c && n_tileable < MAX_CLIENTS; c = c->next) {
			if (c->mapped && !c->floating && !c->fullscreen && c->mon == m)
				tileable[n_tileable++] = c;
		}

		if (n_tileable == 0)
			continue;

		int gaps = user_config.gaps;
		int tile_x = mon_x + gaps;
		int tile_y = mon_y + gaps;
		int tile_width = MAX(1, mon_width - 2 * gaps);
		int tile_height = MAX(1, mon_height - 2 * gaps);

		if (mode == LayoutFibonacci || mode == LayoutDwindle) {
			arrange_fibonacci(tileable, n_tileable, tile_x, tile_y, tile_width, tile_height, mode == LayoutDwindle);
			update_borders();
			continue;
		}

		float master_frac = CLAMP(user_config.master_width[m], MF_MIN, MF_MAX);
		int master_width = (n_tileable > 1) ? (int)(tile_width * master_frac) : tile_width;
		int stack_width = (n_tileable > 1) ? (tile_width - master_width - gaps) : 0;

		{
			Client *c = tileable[0];
			int border_width = 2 * user_config.border_width;
			XWindowChanges wc = {
				.x = tile_x,
				.y = tile_y,
				.width = MAX(1, master_width - border_width),
				.height = MAX(1, tile_height - border_width),
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

		if (n_tileable == 1) {
			update_borders();
			continue;
		}

		int border_width = 2 * user_config.border_width;
		int n_stack = n_tileable - 1;
		int min_stack_height = border_width + 1;
		int total_fixed_heights = 0;
		int n_auto = 0; /* automatically take up leftover space */
		int heights_final[MAX_CLIENTS] = {0};

		for (int i = 1 ; i < n_tileable; i++) { /* i=1 - we are excluding master */
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

			for (int i = 1; i < n_tileable; i++) {
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
			for (int i = 1; i < n_tileable; i++) {
				if (tileable[i]->custom_stack_height > 0)
					heights_final[i] = tileable[i]->custom_stack_height;
				else
					heights_final[i] = min_stack_height;
			}
		}

		int total_height = total_vgaps;
		for (int i = 1; i < n_tileable; i++)
			total_height += heights_final[i];

		int overfill = total_height - tile_height;
		if (overfill > 0) {
			/* shrink from top down, excluding bottom */
			for (int i = 1; i < n_tileable - 1 && overfill > 0; i++) {
				int shrink = MIN(overfill, heights_final[i] - min_stack_height);
				heights_final[i] -= shrink;
				overfill -= shrink;
			}
		}

		/* if its not perfectly filled stretch bottom to absorb remainder */
		int actual_height = total_vgaps;
		for (int i = 1; i < n_tileable; i++)
			actual_height += heights_final[i];

		int shortfall = tile_height - actual_height;
		if (shortfall > 0)
			heights_final[n_tileable - 1] += shortfall;

		int stack_y = tile_y;
		for (int i = 1; i < n_tileable; i++) {
			Client *c = tileable[i];
			XWindowChanges wc = {
				.x = tile_x + master_width + gaps,
				.y = stack_y,
				.width = MAX(1, stack_width - (2 * user_config.border_width)),
				.height = MAX(1, heights_final[i] - (2 * user_config.border_width)),
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

			stack_y += heights_final[i] + gaps;
		}
		update_borders();
	}
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
			focused->x = wa.x;
			focused->y = wa.y;
			focused->w = wa.width;
			focused->h = wa.height;

			XConfigureWindow(dpy, focused->win, CWX | CWY | CWWidth | CWHeight, &(XWindowChanges){
					.x = focused->x,
					.y = focused->y,
					.width = focused->w,
					.height = focused->h
			});
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
					c->x = wa.x;
					c->y = wa.y;
					c->w = wa.width;
					c->h = wa.height;

					XConfigureWindow(dpy, c->win, CWX | CWY | CWWidth | CWHeight,
					             &(XWindowChanges){.x = c->x, .y = c->y, .width = c->w, .height = c->h});
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
	if (layouts[current_layout].mode == LayoutMonocle || monocle) {
		monocle = False;
		current_layout = LayoutTile;
	}
	else {
		monocle = True;
		current_layout = LayoutMonocle;
	}
	tile();
	update_borders();
	if (focused)
		set_input_focus(focused, True, True);
}

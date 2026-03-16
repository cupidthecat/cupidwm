/* Extracted from cupidwm.c: bar drawing and interactions. */

int collect_bar_clients(int mon, Client **list, int max_clients)
{
	if (!list || max_clients <= 0)
		return 0;

	int n = 0;
	for (Client *c = workspaces[current_ws]; c && n < max_clients; c = c->next) {
		if (c->mon != mon || c->swallowed)
			continue;
		list[n++] = c;
	}

	return n;
}

Client *bar_client_at(Monitor *m, int click_x, int title_x, int title_w)
{
	if (!m || title_w <= 0)
		return NULL;

	int mon = (int)(m - mons);
	if (mon < 0 || mon >= n_mons)
		return NULL;

	if (click_x < title_x || click_x >= title_x + title_w)
		return NULL;

	Client *clients[MAX_CLIENTS];
	int n = collect_bar_clients(mon, clients, MAX_CLIENTS);
	if (n <= 0)
		return NULL;

	int each = title_w / n;
	int rem = title_w % n;
	int x = title_x;

	for (int i = 0; i < n; i++) {
		int w = each + (i == n - 1 ? rem : 0);
		if (click_x >= x && click_x < x + w)
			return clients[i];
		x += w;
	}

	return NULL;
}

void setup_bars(void)
{
	update_struts();

	if (!bar_font) {
		const char *font_files[] = {
			"undefined-medium.ttf",
			"undefinemedium.ttf",
			NULL
		};

		for (int i = 0; font_files[i] != NULL; i++) {
			if (access(font_files[i], R_OK) == 0)
				FcConfigAppFontAddFile(NULL, (const FcChar8 *)font_files[i]);
		}

		char exepath[PATH_MAX];
		ssize_t exelen = readlink("/proc/self/exe", exepath, sizeof(exepath) - 1);
		if (exelen > 0) {
			exepath[exelen] = '\0';
			char *slash = strrchr(exepath, '/');
			if (slash) {
				*slash = '\0';
				for (int i = 0; font_files[i] != NULL; i++) {
					char pathbuf[PATH_MAX];
					size_t base_len = strlen(exepath);
					size_t file_len = strlen(font_files[i]);
					if (base_len + 1 + file_len + 1 > sizeof(pathbuf))
						continue;
					memcpy(pathbuf, exepath, base_len);
					pathbuf[base_len] = '/';
					memcpy(pathbuf + base_len + 1, font_files[i], file_len);
					pathbuf[base_len + 1 + file_len] = '\0';
					if (access(pathbuf, R_OK) == 0)
						FcConfigAppFontAddFile(NULL, (const FcChar8 *)pathbuf);
				}
			}
		}

		const char *xft_candidates[] = {
			fontname,
			"undefined-medium",
			"undefinemedium",
			"sans-10",
			NULL
		};

		for (int i = 0; xft_candidates[i] != NULL && !bar_xft_font; i++) {
			if (!xft_candidates[i][0])
				continue;
			bar_xft_font = XftFontOpenName(dpy, DefaultScreen(dpy), xft_candidates[i]);
		}

		const char *font_candidates[] = {
			fontname,
			"undefined-medium",
			"undefinemedium",
			"fixed",
			NULL
		};

		for (int i = 0; font_candidates[i] != NULL && !bar_font; i++) {
			if (!font_candidates[i][0])
				continue;
			bar_font = XLoadQueryFont(dpy, font_candidates[i]);
		}
	}

	if (!bar_gc) {
		XGCValues gcv = {0};
		bar_gc = XCreateGC(dpy, root, 0, &gcv);
	}

	if (bar_xft_font) {
		if (user_config.bar_height <= 0)
			user_config.bar_height = bar_xft_font->ascent + bar_xft_font->descent + 6;
	}
	else if (bar_font) {
		XSetFont(dpy, bar_gc, bar_font->fid);
		if (user_config.bar_height <= 0)
			user_config.bar_height = bar_font->ascent + bar_font->descent + 6;
	}
	if (user_config.bar_height <= 0)
		user_config.bar_height = 20;

	for (int i = 0; i < n_mons; i++) {
		if (mons[i].barwin) {
			XDestroyWindow(dpy, mons[i].barwin);
			mons[i].barwin = 0;
		}

		if (!user_config.showbar)
			continue;

		if (user_config.topbar)
			mons[i].bar_y = mons[i].y + mons[i].reserve_top - user_config.bar_height;
		else
			mons[i].bar_y = mons[i].y + mons[i].h - mons[i].reserve_bottom;
		XSetWindowAttributes wa = {
			.override_redirect = True,
			.background_pixel = pixel(user_config.bar_bg_col),
			.border_pixel = pixel(user_config.bar_bg_col),
			.event_mask = ExposureMask | ButtonPressMask
		};
		mons[i].barwin = XCreateWindow(
			dpy, root, mons[i].x, mons[i].bar_y, (unsigned int)mons[i].w,
			(unsigned int)user_config.bar_height, 0,
			DefaultDepth(dpy, DefaultScreen(dpy)), CopyFromParent,
			DefaultVisual(dpy, DefaultScreen(dpy)),
			CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &wa
		);
		{
			Atom dock = atoms[ATOM_NET_WM_WINDOW_TYPE_DOCK];
			XChangeProperty(dpy, mons[i].barwin, atoms[ATOM_NET_WM_WINDOW_TYPE], XA_ATOM, 32,
						PropModeReplace, (unsigned char *)&dock, 1);
		}
		XMapRaised(dpy, mons[i].barwin);
	}

	drawbars();
}

void drawbar(Monitor *m)
{
	if (!user_config.showbar || !m || !m->barwin)
		return;

	int bh = user_config.bar_height;
	int x = 0;
	int font_h = bar_xft_font ? (bar_xft_font->ascent + bar_xft_font->descent)
				     : (bar_font ? (bar_font->ascent + bar_font->descent) : 8);
	int y = (bh - font_h) / 2 + (bar_xft_font ? bar_xft_font->ascent : (bar_font ? bar_font->ascent : bh - 6));
	int is_curr_mon = (m == &mons[current_mon]);
	Drawable d = m->barwin;
	Pixmap barbuf = XCreatePixmap(dpy, m->barwin, (unsigned int)m->w, (unsigned int)bh,
				      (unsigned int)DefaultDepth(dpy, DefaultScreen(dpy)));
	XftDraw *xftdraw = NULL;
	if (barbuf != None)
		d = barbuf;
	if (bar_xft_font)
		xftdraw = XftDrawCreate(dpy, d, DefaultVisual(dpy, DefaultScreen(dpy)), DefaultColormap(dpy, DefaultScreen(dpy)));

	XSetForeground(dpy, bar_gc, pixel(user_config.bar_bg_col));
	XFillRectangle(dpy, d, bar_gc, 0, 0, (unsigned int)m->w, (unsigned int)bh);

	for (int i = 0; i < NUM_WORKSPACES; i++) {
		int tw = textw(tags[i]) + 14;
		Bool selected = (i == current_ws);

		XSetForeground(dpy, bar_gc, selected ? pixel(user_config.bar_sel_bg_col) : pixel(user_config.bar_bg_col));
		XFillRectangle(dpy, d, bar_gc, x, 0, (unsigned int)tw, (unsigned int)bh);
		XSetForeground(dpy, bar_gc, selected ? pixel(user_config.bar_sel_fg_col) : pixel(user_config.bar_fg_col));
		if (bar_xft_font)
			xft_draw_text(xftdraw, pixel(selected ? user_config.bar_sel_fg_col : user_config.bar_fg_col), x + 7, y,
				      tags[i], (int)strlen(tags[i]));
		else
			XDrawString(dpy, d, bar_gc, x + 7, y, tags[i], (int)strlen(tags[i]));
		x += tw;
	}

	{
		const char *lsym = layouts[current_layout].symbol;
		int tw = textw(lsym) + 14;
		XSetForeground(dpy, bar_gc, pixel(user_config.bar_fg_col));
		if (bar_xft_font)
			xft_draw_text(xftdraw, pixel(user_config.bar_fg_col), x + 7, y, lsym, (int)strlen(lsym));
		else
			XDrawString(dpy, d, bar_gc, x + 7, y, lsym, (int)strlen(lsym));
		x += tw;
	}

	int status_w = is_curr_mon ? (textw(stext) + 12) : 0;
	int title_x = x;
	int title_w = MAX(0, m->w - title_x - status_w);
	if (title_w > 8) {
		int mon = (int)(m - mons);
		Client *clients[MAX_CLIENTS];
		int n = user_config.bar_show_tabs ? collect_bar_clients(mon, clients, MAX_CLIENTS) : 0;

		if (n > 0) {
			int each = title_w / n;
			int rem = title_w % n;
			int tx = title_x;

			for (int i = 0; i < n; i++) {
				Client *c = clients[i];
				int tab_w = each + (i == n - 1 ? rem : 0);
				Bool selected = (c == focused);
				char title[256];
				get_window_title(c->win, title, sizeof(title));

				XSetForeground(dpy, bar_gc, selected ? pixel(user_config.bar_sel_bg_col) : pixel(user_config.bar_bg_col));
				XFillRectangle(dpy, d, bar_gc, tx, 0, (unsigned int)tab_w, (unsigned int)bh);

				XSetForeground(dpy, bar_gc, selected ? pixel(user_config.bar_sel_fg_col) : pixel(user_config.bar_fg_col));
				if (tab_w > 10 && title[0]) {
					char tbuf[256];
					size_t tlen = strlen(title);
					if (tlen >= sizeof(tbuf))
						tlen = sizeof(tbuf) - 1;
					memcpy(tbuf, title, tlen);
					tbuf[tlen] = '\0';

					while (tlen > 0 && textw(tbuf) > tab_w - 10) {
						tlen--;
						tbuf[tlen] = '\0';
					}

					if (tlen > 0) {
						if (bar_xft_font)
							xft_draw_text(xftdraw, pixel(selected ? user_config.bar_sel_fg_col : user_config.bar_fg_col),
								      tx + 5, y, tbuf, (int)tlen);
						else
							XDrawString(dpy, d, bar_gc, tx + 5, y, tbuf, (int)tlen);
					}
				}

				tx += tab_w;
			}
		}
		else if (user_config.bar_show_title_fallback) {
			Client *tc = NULL;
			if (focused && focused->mapped && focused->mon == mon && focused->ws == current_ws && !focused->swallower)
				tc = focused;
			if (!tc) {
				for (Client *c = workspaces[current_ws]; c; c = c->next) {
					if (!c->mapped || c->mon != mon || c->swallower)
						continue;
					tc = c;
					break;
				}
			}

			XSetForeground(dpy, bar_gc, pixel(user_config.bar_bg_col));
			XFillRectangle(dpy, d, bar_gc, title_x, 0, (unsigned int)title_w, (unsigned int)bh);

			if (tc) {
				char title[256];
				get_window_title(tc->win, title, sizeof(title));

				if (title[0]) {
					char tbuf[256];
					size_t tlen = strlen(title);
					if (tlen >= sizeof(tbuf))
						tlen = sizeof(tbuf) - 1;
					memcpy(tbuf, title, tlen);
					tbuf[tlen] = '\0';

					while (tlen > 0 && textw(tbuf) > title_w - 10) {
						tlen--;
						tbuf[tlen] = '\0';
					}

					if (tlen > 0) {
						XSetForeground(dpy, bar_gc, pixel(user_config.bar_fg_col));
						if (bar_xft_font)
							xft_draw_text(xftdraw, pixel(user_config.bar_fg_col), title_x + 5, y, tbuf, (int)tlen);
						else
							XDrawString(dpy, d, bar_gc, title_x + 5, y, tbuf, (int)tlen);
					}
				}

			}
		}
		else {
			XSetForeground(dpy, bar_gc, pixel(user_config.bar_bg_col));
			XFillRectangle(dpy, d, bar_gc, title_x, 0, (unsigned int)title_w, (unsigned int)bh);
		}
	}

	if (is_curr_mon && status_w > 0) {
		XSetForeground(dpy, bar_gc, pixel(user_config.bar_fg_col));
		if (bar_xft_font)
			xft_draw_text(xftdraw, pixel(user_config.bar_fg_col), m->w - status_w + 6, y, stext, (int)strlen(stext));
		else
			XDrawString(dpy, d, bar_gc, m->w - status_w + 6, y, stext, (int)strlen(stext));
	}

	if (xftdraw)
		XftDrawDestroy(xftdraw);

	if (barbuf != None) {
		XCopyArea(dpy, barbuf, m->barwin, bar_gc, 0, 0, (unsigned int)m->w, (unsigned int)bh, 0, 0);
		XFreePixmap(dpy, barbuf);
	}
}

void drawbars(void)
{
	if (!user_config.showbar)
		return;
	for (int i = 0; i < n_mons; i++)
		drawbar(&mons[i]);
}

void hdl_expose(XEvent *xev)
{
	XExposeEvent *ee = &xev->xexpose;
	if (ee->count != 0)
		return;
	for (int i = 0; i < n_mons; i++) {
		if (ee->window == mons[i].barwin) {
			drawbar(&mons[i]);
			return;
		}
	}
}

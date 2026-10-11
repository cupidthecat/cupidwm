/* Rotate master clients without changing other workspace clients. */

void move_master_next(void)
{
	if (current_ws < 0 || current_ws >= NUM_WORKSPACES || !workspaces[current_ws])
		return;

	Client *ordered[MAX_CLIENTS] = {0};
	int n_clients = 0;
	Client *remainder = workspaces[current_ws];
	while (remainder && n_clients < MAX_CLIENTS) {
		ordered[n_clients++] = remainder;
		remainder = remainder->next;
	}
	if (n_clients < 2)
		return;

	int tiled_idx[MAX_CLIENTS] = {0};
	int n_tiled = 0;
	for (int i = 0; i < n_clients; i++) {
		Client *c = ordered[i];
		if (!client_is_visible_on_monitor(c, current_mon) || !c->mapped || c->floating || c->fullscreen)
			continue;
		tiled_idx[n_tiled++] = i;
	}

	int nmaster = MIN(workspace_nmaster_for(current_ws), n_tiled);
	if (nmaster < 2)
		return;

	Client *old_focused = focused;
	Client *first_master = ordered[tiled_idx[0]];

	for (int i = 0; i < nmaster - 1; i++)
		ordered[tiled_idx[i]] = ordered[tiled_idx[i + 1]];
	ordered[tiled_idx[nmaster - 1]] = first_master;

	for (int i = 0; i < n_clients - 1; i++)
		ordered[i]->next = ordered[i + 1];
	ordered[n_clients - 1]->next = remainder;
	workspaces[current_ws] = ordered[0];

	tile();

	if (user_config.warp_cursor && old_focused)
		warp_cursor(old_focused);

	if (old_focused)
		send_wm_take_focus(old_focused->win);

	update_borders();
}

void move_master_prev(void)
{
	if (current_ws < 0 || current_ws >= NUM_WORKSPACES || !workspaces[current_ws])
		return;

	Client *ordered[MAX_CLIENTS] = {0};
	int n_clients = 0;
	Client *remainder = workspaces[current_ws];
	while (remainder && n_clients < MAX_CLIENTS) {
		ordered[n_clients++] = remainder;
		remainder = remainder->next;
	}
	if (n_clients < 2)
		return;

	int tiled_idx[MAX_CLIENTS] = {0};
	int n_tiled = 0;
	for (int i = 0; i < n_clients; i++) {
		Client *c = ordered[i];
		if (!client_is_visible_on_monitor(c, current_mon) || !c->mapped || c->floating || c->fullscreen)
			continue;
		tiled_idx[n_tiled++] = i;
	}

	int nmaster = MIN(workspace_nmaster_for(current_ws), n_tiled);
	if (nmaster < 2)
		return;

	Client *old_focused = focused;
	Client *last_master = ordered[tiled_idx[nmaster - 1]];

	for (int i = nmaster - 1; i > 0; i--)
		ordered[tiled_idx[i]] = ordered[tiled_idx[i - 1]];
	ordered[tiled_idx[0]] = last_master;

	for (int i = 0; i < n_clients - 1; i++)
		ordered[i]->next = ordered[i + 1];
	ordered[n_clients - 1]->next = remainder;
	workspaces[current_ws] = ordered[0];

	tile();
	if (user_config.warp_cursor && old_focused)
		warp_cursor(old_focused);
	if (old_focused)
		send_wm_take_focus(old_focused->win);

	update_borders();
}


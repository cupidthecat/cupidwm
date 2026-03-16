/* Optional local UNIX socket IPC server for cupidwmctl. */

static int ipc_server_fd = -1;
static char ipc_socket_path_buf[PATH_MAX] = {0};

static void ipc_write_reply(int fd, const char *msg)
{
	if (fd < 0 || !msg)
		return;

	ssize_t wrote = write(fd, msg, strlen(msg));
	(void)wrote;
}

static void ipc_trim(char *s)
{
	if (!s)
		return;

	size_t len = strlen(s);
	while (len > 0 &&
	       (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ' || s[len - 1] == '\t')) {
		s[len - 1] = '\0';
		len--;
	}

	char *p = s;
	while (*p == ' ' || *p == '\t')
		p++;
	if (p != s)
		memmove(s, p, strlen(p) + 1);
}

static int ipc_set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL);
	if (flags < 0)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int ipc_set_cloexec(int fd)
{
	int flags = fcntl(fd, F_GETFD);
	if (flags < 0)
		return -1;
	return fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static void ipc_reply_status(int client_fd)
{
	Window fw = focused ? focused->win : None;
	char out[256];
	snprintf(out, sizeof(out),
	         "ok workspace=%d monitor=%d focused=0x%lx layout=%s\n",
	         current_ws + 1, current_mon, (unsigned long)fw,
	         layouts[current_layout].symbol ? layouts[current_layout].symbol : "?");
	ipc_write_reply(client_fd, out);
}

static Bool ipc_parse_layout(const char *name, int *layout_out)
{
	if (!name || !layout_out)
		return False;

	if (strcasecmp(name, "tile") == 0 || strcmp(name, "[]=") == 0) {
		*layout_out = LayoutTile;
		return True;
	}
	if (strcasecmp(name, "monocle") == 0 || strcmp(name, "[M]") == 0) {
		*layout_out = LayoutMonocle;
		return True;
	}
	if (strcasecmp(name, "floating") == 0 || strcmp(name, "><>") == 0) {
		*layout_out = LayoutFloating;
		return True;
	}
	if (strcasecmp(name, "fibonacci") == 0 || strcmp(name, "[@]") == 0) {
		*layout_out = LayoutFibonacci;
		return True;
	}
	if (strcasecmp(name, "dwindle") == 0 || strcmp(name, "[\\]") == 0) {
		*layout_out = LayoutDwindle;
		return True;
	}

	return False;
}

static Bool ipc_parse_workspace(const char *cmd, const char *prefix, int *ws_out)
{
	int ws = -1;
	int matched = sscanf(cmd, prefix, &ws);
	if (matched != 1)
		return False;

	if (ws >= 1 && ws <= NUM_WORKSPACES)
		ws -= 1;

	if (ws < 0 || ws >= NUM_WORKSPACES)
		return False;

	*ws_out = ws;
	return True;
}

static void ipc_dispatch(int client_fd, char *cmd)
{
	if (!cmd || !cmd[0]) {
		ipc_write_reply(client_fd, "error empty command\n");
		return;
	}

	if (strcmp(cmd, "ping") == 0) {
		ipc_write_reply(client_fd, "ok pong\n");
		return;
	}

	if (strcmp(cmd, "help") == 0) {
		ipc_write_reply(client_fd,
		                "ok commands: ping, help, status, focus next, focus prev, "
		                "workspace N, move-workspace N, "
		                "layout <tile|monocle|floating|fibonacci|dwindle>, reload, quit\n");
		return;
	}

	if (strcmp(cmd, "status") == 0) {
		ipc_reply_status(client_fd);
		return;
	}

	if (strcmp(cmd, "focus next") == 0) {
		focus_next();
		ipc_write_reply(client_fd, "ok\n");
		return;
	}

	if (strcmp(cmd, "focus prev") == 0) {
		focus_prev();
		ipc_write_reply(client_fd, "ok\n");
		return;
	}

	if (strcmp(cmd, "reload") == 0) {
		reload_config();
		ipc_write_reply(client_fd, "ok\n");
		return;
	}

	if (strcmp(cmd, "quit") == 0) {
		ipc_write_reply(client_fd, "ok\n");
		quit();
		return;
	}

	int ws = -1;
	if (ipc_parse_workspace(cmd, "workspace %d", &ws)) {
		change_workspace(ws);
		ipc_write_reply(client_fd, "ok\n");
		return;
	}
	if (strncmp(cmd, "workspace ", 10) == 0) {
		ipc_write_reply(client_fd, "error workspace out of range\n");
		return;
	}

	if (ipc_parse_workspace(cmd, "move-workspace %d", &ws)) {
		move_to_workspace(ws);
		ipc_write_reply(client_fd, "ok\n");
		return;
	}
	if (strncmp(cmd, "move-workspace ", 15) == 0) {
		ipc_write_reply(client_fd, "error workspace out of range\n");
		return;
	}

	if (strncmp(cmd, "layout ", 7) == 0) {
		int target = -1;
		char *layout_name = cmd + 7;
		ipc_trim(layout_name);
		if (!ipc_parse_layout(layout_name, &target)) {
			ipc_write_reply(client_fd, "error unknown layout\n");
			return;
		}

		Arg arg = {.i = target};
		setlayoutcmd(&arg);
		ipc_write_reply(client_fd, "ok\n");
		return;
	}

	ipc_write_reply(client_fd, "error unknown command\n");
}

void ipc_handle_connection(void)
{
	if (ipc_server_fd < 0)
		return;

	for (;;) {
		int client_fd = accept(ipc_server_fd, NULL, NULL);
		if (client_fd < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			return;
		}

		ipc_set_cloexec(client_fd);
		ipc_set_nonblock(client_fd);

		char buf[512];
		ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
		if (n <= 0) {
			ipc_write_reply(client_fd, "error failed to read command\n");
			close(client_fd);
			continue;
		}

		buf[n] = '\0';
		ipc_trim(buf);
		ipc_dispatch(client_fd, buf);
		close(client_fd);
	}
}

int ipc_get_server_fd(void)
{
	return ipc_server_fd;
}

void ipc_cleanup(void)
{
	if (ipc_server_fd >= 0) {
		close(ipc_server_fd);
		ipc_server_fd = -1;
	}

	if (ipc_socket_path_buf[0]) {
		unlink(ipc_socket_path_buf);
		ipc_socket_path_buf[0] = '\0';
	}
}

int ipc_setup(void)
{
	if (!user_config.ipc_enable)
		return -1;
	if (ipc_server_fd >= 0)
		return ipc_server_fd;

	char socket_path[PATH_MAX] = {0};
	if (user_config.ipc_socket_path && user_config.ipc_socket_path[0]) {
		snprintf(socket_path, sizeof(socket_path), "%s", user_config.ipc_socket_path);
	}
	else {
		const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
		if (runtime_dir && runtime_dir[0])
			snprintf(socket_path, sizeof(socket_path), "%s/cupidwm.sock", runtime_dir);
		else
			snprintf(socket_path, sizeof(socket_path), "/tmp/cupidwm-%u.sock", (unsigned)getuid());
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;

	size_t path_len = strlen(socket_path);
	if (path_len == 0 || path_len >= sizeof(addr.sun_path)) {
		fprintf(stderr, "cupidwm: ipc socket path invalid: %s\n", socket_path);
		return -1;
	}
	memcpy(addr.sun_path, socket_path, path_len + 1);

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("cupidwm: ipc socket");
		return -1;
	}

	ipc_set_cloexec(fd);
	ipc_set_nonblock(fd);

	unlink(socket_path);

	mode_t oldmask = umask(0077);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		umask(oldmask);
		perror("cupidwm: ipc bind");
		close(fd);
		return -1;
	}
	umask(oldmask);

	if (chmod(socket_path, 0600) < 0) {
		perror("cupidwm: ipc chmod");
		close(fd);
		unlink(socket_path);
		return -1;
	}

	if (listen(fd, 8) < 0) {
		perror("cupidwm: ipc listen");
		close(fd);
		unlink(socket_path);
		return -1;
	}

	ipc_server_fd = fd;
	snprintf(ipc_socket_path_buf, sizeof(ipc_socket_path_buf), "%s", socket_path);
	fprintf(stderr, "cupidwm: ipc enabled on %s\n", ipc_socket_path_buf);
	return ipc_server_fd;
}

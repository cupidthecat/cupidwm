/* Optional local UNIX socket IPC server for cupidwmctl. */

#define IPC_SOCKET_ROOT_PROPERTY "_CUPIDWM_IPC_SOCKET"

static int ipc_server_fd = -1;
static char ipc_socket_path_buf[PATH_MAX] = {0};
static char ipc_socket_hint_path_buf[PATH_MAX] = {0};

static void ipc_display_token(char *out, size_t outsz)
{
	if (!out || outsz == 0)
		return;

	out[0] = '\0';
	const char *display = getenv("DISPLAY");
	if (!display || !display[0])
		return;

	size_t j = 0;
	for (size_t i = 0; display[i] && j < outsz - 1; i++) {
		unsigned char ch = (unsigned char)display[i];
		if ((ch >= 'a' && ch <= 'z') ||
		    (ch >= 'A' && ch <= 'Z') ||
		    (ch >= '0' && ch <= '9') ||
		    ch == '-' || ch == '_')
			out[j++] = (char)ch;
		else
			out[j++] = '_';
	}
	out[j] = '\0';
}

static void ipc_hint_path(char *out, size_t outsz)
{
	if (!out || outsz == 0)
		return;

	out[0] = '\0';
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir || !runtime_dir[0])
		return;

	size_t n = strlen(runtime_dir);
	while (n > 1 && runtime_dir[n - 1] == '/')
		n--;

	char display_tok[64] = {0};
	ipc_display_token(display_tok, sizeof(display_tok));
	if (display_tok[0])
		snprintf(out, outsz, "%.*s/cupidwm-%s.sockpath", (int)n, runtime_dir, display_tok);
	else
		snprintf(out, outsz, "%.*s/cupidwm.sockpath", (int)n, runtime_dir);
}

static void ipc_tmp_dir(char *out, size_t outsz)
{
	if (!out || outsz == 0)
		return;

	snprintf(out, outsz, "/tmp/cupidwm-%u", (unsigned)getuid());
}

static void ipc_default_socket_path(char *out, size_t outsz)
{
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	char display_tok[64] = {0};

	if (!out || outsz == 0)
		return;

	ipc_display_token(display_tok, sizeof(display_tok));
	if (runtime_dir && runtime_dir[0]) {
		size_t n = strlen(runtime_dir);
		while (n > 1 && runtime_dir[n - 1] == '/')
			n--;
		if (display_tok[0])
			snprintf(out, outsz, "%.*s/cupidwm-%s.sock", (int)n, runtime_dir, display_tok);
		else
			snprintf(out, outsz, "%.*s/cupidwm.sock", (int)n, runtime_dir);
		return;
	}

	char tmp_dir[PATH_MAX] = {0};
	ipc_tmp_dir(tmp_dir, sizeof(tmp_dir));
	if (display_tok[0])
		snprintf(out, outsz, "%s/cupidwm-%s.sock", tmp_dir, display_tok);
	else
		snprintf(out, outsz, "%s/cupidwm.sock", tmp_dir);
}

static void ipc_write_hint_file(const char *socket_path)
{
	if (!socket_path || !socket_path[0])
		return;

	char hint_path[PATH_MAX] = {0};
	ipc_hint_path(hint_path, sizeof(hint_path));
	if (!hint_path[0])
		return;

	int fd = open(hint_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	if (fd < 0)
		return;

	size_t len = strlen(socket_path);
	size_t sent = 0;
	while (sent < len) {
		ssize_t w = write(fd, socket_path + sent, len - sent);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (w == 0)
			break;
		sent += (size_t)w;
	}

	char nl = '\n';
	for (;;) {
		ssize_t w = write(fd, &nl, 1);
		if (w < 0) {
			if (errno == EINTR)
				continue;
		}
		break;
	}
	close(fd);

	snprintf(ipc_socket_hint_path_buf, sizeof(ipc_socket_hint_path_buf), "%s", hint_path);
}

const char *ipc_resolve_socket_path(const char *configured_path, char *out, size_t outsz)
{
	if (!out || outsz == 0)
		return NULL;

	out[0] = '\0';
	if (configured_path && configured_path[0]) {
		snprintf(out, outsz, "%s", configured_path);
		return out;
	}

	ipc_default_socket_path(out, outsz);
	return out;
}

static int ipc_ensure_private_tmp_dir(void)
{
	char dir[PATH_MAX] = {0};
	struct stat st;

	ipc_tmp_dir(dir, sizeof(dir));
	if (mkdir(dir, 0700) < 0 && errno != EEXIST) {
		fprintf(stderr, "cupidwm: ipc mkdir %s failed: %s\n", dir, strerror(errno));
		return -1;
	}

	if (lstat(dir, &st) < 0) {
		fprintf(stderr, "cupidwm: ipc stat %s failed: %s\n", dir, strerror(errno));
		return -1;
	}
	if (!S_ISDIR(st.st_mode)) {
		fprintf(stderr, "cupidwm: ipc fallback path is not a directory: %s\n", dir);
		return -1;
	}
	if (st.st_uid != getuid()) {
		fprintf(stderr, "cupidwm: ipc fallback directory is not owned by uid %u: %s\n",
		        (unsigned)getuid(), dir);
		return -1;
	}
	if ((st.st_mode & 077) != 0) {
		fprintf(stderr, "cupidwm: ipc fallback directory must be private (0700): %s\n", dir);
		return -1;
	}

	return 0;
}

static int ipc_prepare_socket_path(const char *socket_path)
{
	struct stat st;

	if (!socket_path || !socket_path[0])
		return -1;

	if (lstat(socket_path, &st) < 0) {
		if (errno == ENOENT)
			return 0;
		fprintf(stderr, "cupidwm: ipc stat %s failed: %s\n", socket_path, strerror(errno));
		return -1;
	}

	if (!S_ISSOCK(st.st_mode)) {
		fprintf(stderr, "cupidwm: ipc path exists and is not a socket: %s\n", socket_path);
		return -1;
	}
	if (st.st_uid != getuid()) {
		fprintf(stderr, "cupidwm: ipc socket is not owned by uid %u: %s\n",
		        (unsigned)getuid(), socket_path);
		return -1;
	}
	if (unlink(socket_path) < 0) {
		fprintf(stderr, "cupidwm: ipc unlink %s failed: %s\n", socket_path, strerror(errno));
		return -1;
	}

	return 0;
}

static void ipc_publish_socket_path(const char *path)
{
	if (!dpy || root == None)
		return;

	Atom prop = XInternAtom(dpy, IPC_SOCKET_ROOT_PROPERTY, False);
	if (prop == None)
		return;

	if (path && path[0]) {
		XChangeProperty(dpy, root, prop, XA_STRING, 8, PropModeReplace,
		                (unsigned char *)path, (int)strlen(path));
	}
	else {
		XDeleteProperty(dpy, root, prop);
	}
}

static void ipc_write_reply(int fd, const char *msg)
{
	if (fd < 0 || !msg)
		return;

	size_t len = strlen(msg);
	size_t sent = 0;
	while (sent < len) {
		ssize_t n = write(fd, msg + sent, len - sent);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return;
		}
		if (n == 0)
			return;
		sent += (size_t)n;
	}
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

static int ipc_set_recv_timeout(int fd, int timeout_ms)
{
	if (fd < 0 || timeout_ms < 0)
		return -1;

	struct timeval tv;
	tv.tv_sec = timeout_ms / 1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;
	return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
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
		ipc_set_recv_timeout(client_fd, 200);

		char buf[512];
		size_t used = 0;
		Bool truncated = False;
		for (;;) {
			if (used >= sizeof(buf) - 1) {
				truncated = True;
				break;
			}

			ssize_t n = read(client_fd, buf + used, sizeof(buf) - 1 - used);
			if (n > 0) {
				used += (size_t)n;
				if (memchr(buf, '\n', used) || memchr(buf, '\r', used))
					break;
				continue;
			}

			if (n == 0)
				break;

			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (used > 0)
					break;
				ipc_write_reply(client_fd, "error read timeout\n");
				close(client_fd);
				goto next_client;
			}

			ipc_write_reply(client_fd, "error failed to read command\n");
			close(client_fd);
			goto next_client;
		}

		if (truncated) {
			ipc_write_reply(client_fd, "error command too long\n");
			close(client_fd);
			goto next_client;
		}

		if (used == 0) {
			ipc_write_reply(client_fd, "error empty command\n");
			close(client_fd);
			goto next_client;
		}

		buf[used] = '\0';
		ipc_trim(buf);
		ipc_dispatch(client_fd, buf);
		close(client_fd);
next_client:
		;
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
		ipc_publish_socket_path(NULL);
		unlink(ipc_socket_path_buf);
		ipc_socket_path_buf[0] = '\0';
	}

	if (ipc_socket_hint_path_buf[0]) {
		unlink(ipc_socket_hint_path_buf);
		ipc_socket_hint_path_buf[0] = '\0';
	}
}

int ipc_setup(void)
{
	if (!user_config.ipc_enable)
		return -1;
	if (ipc_server_fd >= 0)
		return ipc_server_fd;

	char socket_path[PATH_MAX] = {0};
	if (!ipc_resolve_socket_path(user_config.ipc_socket_path, socket_path, sizeof(socket_path)))
		return -1;

	if ((!user_config.ipc_socket_path || !user_config.ipc_socket_path[0]) &&
	    (!getenv("XDG_RUNTIME_DIR") || !getenv("XDG_RUNTIME_DIR")[0]) &&
	    ipc_ensure_private_tmp_dir() < 0)
		return -1;

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

	if (ipc_prepare_socket_path(socket_path) < 0) {
		close(fd);
		return -1;
	}

	mode_t oldmask = umask(0077);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		umask(oldmask);
		fprintf(stderr, "cupidwm: ipc bind %s failed: %s\n", socket_path, strerror(errno));
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
	ipc_write_hint_file(ipc_socket_path_buf);
	ipc_publish_socket_path(ipc_socket_path_buf);
	fprintf(stderr, "cupidwm: ipc enabled on %s\n", ipc_socket_path_buf);
	return ipc_server_fd;
}

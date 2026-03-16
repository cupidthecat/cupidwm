/* Optional local UNIX socket IPC server for cupidwmctl. */

#define IPC_SOCKET_ROOT_PROPERTY "_CUPIDWM_IPC_SOCKET"

static int ipc_server_fd = -1;
static char ipc_socket_path_buf[PATH_MAX] = {0};
static char ipc_socket_hint_path_buf[PATH_MAX] = {0};

#define IPC_MAX_SUBSCRIBERS 16

typedef struct {
	int fd;
	Bool json;
} IpcSubscriber;

static IpcSubscriber ipc_subscribers[IPC_MAX_SUBSCRIBERS];

static void ipc_trim(char *s);

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
		unsigned char propbuf[PATH_MAX];
		size_t len = strlen(path);
		if (len >= sizeof(propbuf))
			len = sizeof(propbuf) - 1;
		memcpy(propbuf, path, len);
		XChangeProperty(dpy, root, prop, XA_STRING, 8, PropModeReplace,
		                propbuf, (int)len);
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

static void ipc_write_replyf(int fd, const char *fmt, ...)
{
	if (fd < 0 || !fmt)
		return;

	char out[2048];
	va_list ap;
	va_start(ap, fmt);
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
	vsnprintf(out, sizeof(out), fmt, ap);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
	va_end(ap);
	ipc_write_reply(fd, out);
}

static int ipc_count_clients(void)
{
	int n = 0;
	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		for (Client *c = workspaces[ws]; c; c = c->next)
			n++;
	}
	return n;
}

static const char *ipc_layout_name(int idx)
{
	if (idx < 0 || idx >= (int)LENGTH(layouts))
		return "unknown";

	if (layouts[idx].mode == LayoutTile)
		return "tile";
	if (layouts[idx].mode == LayoutMonocle)
		return "monocle";
	if (layouts[idx].mode == LayoutFloating)
		return "floating";
	if (layouts[idx].mode == LayoutFibonacci)
		return "fibonacci";
	if (layouts[idx].mode == LayoutDwindle)
		return "dwindle";
	return layouts[idx].symbol ? layouts[idx].symbol : "unknown";
}

static Bool ipc_extract_json_mode(char *cmd)
{
	if (!cmd || !cmd[0])
		return False;

	ipc_trim(cmd);
	Bool json = False;

	if (strncmp(cmd, "json ", 5) == 0) {
		memmove(cmd, cmd + 5, strlen(cmd + 5) + 1);
		json = True;
		ipc_trim(cmd);
	}

	if (strcmp(cmd, "--json") == 0) {
		cmd[0] = '\0';
		return True;
	}

	size_t len = strlen(cmd);
	const char *suffix = " --json";
	size_t slen = strlen(suffix);
	if (len >= slen && strcmp(cmd + len - slen, suffix) == 0) {
		cmd[len - slen] = '\0';
		json = True;
		ipc_trim(cmd);
	}

	return json;
}

static int ipc_find_subscriber_slot(int fd)
{
	for (int i = 0; i < IPC_MAX_SUBSCRIBERS; i++) {
		if (ipc_subscribers[i].fd == fd)
			return i;
	}
	return -1;
}

static int ipc_add_subscriber(int fd, Bool json)
{
	if (fd < 0)
		return -1;

	if (ipc_find_subscriber_slot(fd) >= 0)
		return 0;

	for (int i = 0; i < IPC_MAX_SUBSCRIBERS; i++) {
		if (ipc_subscribers[i].fd >= 0)
			continue;
		ipc_subscribers[i].fd = fd;
		ipc_subscribers[i].json = json;
		return 0;
	}

	return -1;
}

static void ipc_remove_subscriber_fd(int fd)
{
	int slot = ipc_find_subscriber_slot(fd);
	if (slot < 0)
		return;

	close(ipc_subscribers[slot].fd);
	ipc_subscribers[slot].fd = -1;
	ipc_subscribers[slot].json = False;
}

void ipc_notify_event(const char *event, const char *details)
{
	if (!event || !event[0])
		return;

	for (int i = 0; i < IPC_MAX_SUBSCRIBERS; i++) {
		int fd = ipc_subscribers[i].fd;
		if (fd < 0)
			continue;

		char line[2048];
		if (ipc_subscribers[i].json)
			snprintf(line, sizeof(line),
			         "{\"event\":\"%s\",\"details\":\"%s\"}\n",
			         event,
			         details ? details : "");
		else if (details && details[0])
			snprintf(line, sizeof(line), "event %s %s\n", event, details);
		else
			snprintf(line, sizeof(line), "event %s\n", event);

		size_t len = strlen(line);
		size_t sent = 0;
		Bool failed = False;
		while (sent < len) {
			ssize_t n = write(fd, line + sent, len - sent);
			if (n < 0) {
				if (errno == EINTR)
					continue;
				failed = True;
				break;
			}
			if (n == 0) {
				failed = True;
				break;
			}
			sent += (size_t)n;
		}

		if (failed)
			ipc_remove_subscriber_fd(fd);
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

static void ipc_reply_status_json(int client_fd)
{
	Window fw = focused ? focused->win : None;
	ipc_write_replyf(client_fd,
	                "{\"ok\":true,\"workspace\":%d,\"monitor\":%d,\"focused\":\"0x%lx\",\"layout\":\"%s\"}\n",
	                current_ws + 1,
	                current_mon,
	                (unsigned long)fw,
	                ipc_layout_name(current_layout));
}

static void ipc_reply_ok(int client_fd, Bool json)
{
	if (json)
		ipc_write_reply(client_fd, "{\"ok\":true}\n");
	else
		ipc_write_reply(client_fd, "ok\n");
}

static void ipc_reply_error(int client_fd, Bool json, const char *msg)
{
	if (json)
		ipc_write_replyf(client_fd, "{\"ok\":false,\"error\":\"%s\"}\n", msg ? msg : "error");
	else
		ipc_write_replyf(client_fd, "error %s\n", msg ? msg : "error");
}

static void ipc_reply_query_monitors(int client_fd, Bool json)
{
	if (!json) {
		ipc_write_replyf(client_fd, "ok monitors=%d\n", n_mons);
		for (int i = 0; i < n_mons; i++) {
			ipc_write_replyf(client_fd,
			                "monitor id=%d x=%d y=%d w=%d h=%d workspace=%d current=%d\n",
			                i,
			                mons[i].x,
			                mons[i].y,
			                mons[i].w,
			                mons[i].h,
			                mons[i].view_ws + 1,
			                i == current_mon ? 1 : 0);
		}
		return;
	}

	ipc_write_replyf(client_fd, "{\"ok\":true,\"monitors\":[");
	for (int i = 0; i < n_mons; i++) {
		ipc_write_replyf(client_fd,
		                "%s{\"id\":%d,\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"workspace\":%d,\"current\":%s}",
		                i == 0 ? "" : ",",
		                i,
		                mons[i].x,
		                mons[i].y,
		                mons[i].w,
		                mons[i].h,
		                mons[i].view_ws + 1,
		                i == current_mon ? "true" : "false");
	}
	ipc_write_reply(client_fd, "]}\n");
}

static void ipc_reply_query_workspaces(int client_fd, Bool json)
{
	if (!json)
		ipc_write_reply(client_fd, "ok\n");

	if (json)
		ipc_write_reply(client_fd, "{\"ok\":true,\"workspaces\":[");

	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		int count = 0;
		for (Client *c = workspaces[ws]; c; c = c->next)
			count++;

		Bool visible = workspace_is_visible(ws);
		if (!json) {
			ipc_write_replyf(client_fd,
			                "workspace id=%d visible=%d clients=%d layout=%s focused=0x%lx\n",
			                ws + 1,
			                visible ? 1 : 0,
			                count,
			                ipc_layout_name(workspace_layout_for(ws)),
			                (unsigned long)(workspace_states[ws].focused ? workspace_states[ws].focused->win : None));
		}
		else {
			ipc_write_replyf(client_fd,
			                "%s{\"id\":%d,\"visible\":%s,\"clients\":%d,\"layout\":\"%s\",\"focused\":\"0x%lx\"}",
			                ws == 0 ? "" : ",",
			                ws + 1,
			                visible ? "true" : "false",
			                count,
			                ipc_layout_name(workspace_layout_for(ws)),
			                (unsigned long)(workspace_states[ws].focused ? workspace_states[ws].focused->win : None));
		}
	}

	if (json)
		ipc_write_reply(client_fd, "]}\n");
}

static void ipc_reply_query_layouts(int client_fd, Bool json)
{
	if (!json)
		ipc_write_reply(client_fd, "ok\n");
	else
		ipc_write_reply(client_fd, "{\"ok\":true,\"layouts\":[");

	for (int i = 0; i < (int)LENGTH(layouts); i++) {
		const char *name = ipc_layout_name(i);
		const char *symbol = layouts[i].symbol ? layouts[i].symbol : "?";
		if (!json) {
			ipc_write_replyf(client_fd,
			                "layout id=%d name=%s symbol=%s active=%d\n",
			                i,
			                name,
			                symbol,
			                i == workspace_layout_for(current_ws) ? 1 : 0);
		}
		else {
			ipc_write_replyf(client_fd,
			                "%s{\"id\":%d,\"name\":\"%s\",\"symbol\":\"%s\",\"active\":%s}",
			                i == 0 ? "" : ",",
			                i,
			                name,
			                symbol,
			                i == workspace_layout_for(current_ws) ? "true" : "false");
		}
	}

	if (json)
		ipc_write_reply(client_fd, "]}\n");
}

static void ipc_reply_query_focused_client(int client_fd, Bool json)
{
	if (!focused) {
		if (json)
			ipc_write_reply(client_fd, "{\"ok\":true,\"focused\":null}\n");
		else
			ipc_write_reply(client_fd, "ok focused=none\n");
		return;
	}

	if (!json) {
		ipc_write_replyf(client_fd,
		                "ok focused=0x%lx ws=%d mon=%d mapped=%d floating=%d fullscreen=%d sticky=%d\n",
		                (unsigned long)focused->win,
		                focused->ws + 1,
		                focused->mon,
		                focused->mapped ? 1 : 0,
		                focused->floating ? 1 : 0,
		                focused->fullscreen ? 1 : 0,
		                focused->sticky ? 1 : 0);
		return;
	}

	ipc_write_replyf(client_fd,
	                "{\"ok\":true,\"focused\":{\"win\":\"0x%lx\",\"ws\":%d,\"mon\":%d,\"mapped\":%s,\"floating\":%s,\"fullscreen\":%s,\"sticky\":%s}}\n",
	                (unsigned long)focused->win,
	                focused->ws + 1,
	                focused->mon,
	                focused->mapped ? "true" : "false",
	                focused->floating ? "true" : "false",
	                focused->fullscreen ? "true" : "false",
	                focused->sticky ? "true" : "false");
}

static void ipc_reply_query_clients(int client_fd, Bool json)
{
	if (!json)
		ipc_write_replyf(client_fd, "ok clients=%d\n", ipc_count_clients());
	else
		ipc_write_reply(client_fd, "{\"ok\":true,\"clients\":[");

	int emitted = 0;
	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		for (Client *c = workspaces[ws]; c; c = c->next) {
			if (!json) {
				ipc_write_replyf(client_fd,
				                "client win=0x%lx ws=%d mon=%d mapped=%d floating=%d fullscreen=%d sticky=%d focused=%d\n",
				                (unsigned long)c->win,
				                c->ws + 1,
				                c->mon,
				                c->mapped ? 1 : 0,
				                c->floating ? 1 : 0,
				                c->fullscreen ? 1 : 0,
				                c->sticky ? 1 : 0,
				                c == focused ? 1 : 0);
			}
			else {
				ipc_write_replyf(client_fd,
				                "%s{\"win\":\"0x%lx\",\"ws\":%d,\"mon\":%d,\"mapped\":%s,\"floating\":%s,\"fullscreen\":%s,\"sticky\":%s,\"focused\":%s}",
				                emitted == 0 ? "" : ",",
				                (unsigned long)c->win,
				                c->ws + 1,
				                c->mon,
				                c->mapped ? "true" : "false",
				                c->floating ? "true" : "false",
				                c->fullscreen ? "true" : "false",
				                c->sticky ? "true" : "false",
				                c == focused ? "true" : "false");
			}
			emitted++;
		}
	}

	if (json)
		ipc_write_reply(client_fd, "]}\n");
}

static Bool ipc_parse_button_name(const char *name, unsigned int *button_out)
{
	if (!name || !button_out)
		return False;

	if (strcasecmp(name, "left") == 0 || strcmp(name, "1") == 0) {
		*button_out = Button1;
		return True;
	}
	if (strcasecmp(name, "middle") == 0 || strcmp(name, "2") == 0) {
		*button_out = Button2;
		return True;
	}
	if (strcasecmp(name, "right") == 0 || strcmp(name, "3") == 0) {
		*button_out = Button3;
		return True;
	}

	return False;
}

static void ipc_reply_query_bar(int client_fd, Bool json)
{
	int segments = status_segment_count();

	if (!json) {
		ipc_write_replyf(client_fd, "ok show=%d override=%d segments=%d\n",
		                user_config.showbar ? 1 : 0,
		                status_text_override_active() ? 1 : 0,
		                segments);
		ipc_write_replyf(client_fd, "status text=%s\n", stext);

		for (int i = 0; i < segments; i++) {
			char label[STATUS_MAX_LABEL] = "";
			status_segment_label_at(i, label, sizeof(label));
			ipc_write_replyf(client_fd,
			                "segment id=%d label=%s left=%d middle=%d right=%d\n",
			                i + 1,
			                label,
			                status_action_command_get(i, Button1) ? 1 : 0,
			                status_action_command_get(i, Button2) ? 1 : 0,
			                status_action_command_get(i, Button3) ? 1 : 0);
		}

		for (int i = 0; i < n_mons; i++) {
			int ws = mons[i].view_ws;
			Client *wf = (ws >= 0 && ws < NUM_WORKSPACES) ? workspace_states[ws].focused : NULL;
			ipc_write_replyf(client_fd,
			                "monitor id=%d workspace=%d layout=%s focused=0x%lx current=%d\n",
			                i,
			                ws + 1,
			                ipc_layout_name(workspace_layout_for(ws)),
			                (unsigned long)(wf ? wf->win : None),
			                i == current_mon ? 1 : 0);
		}
		return;
	}

	ipc_write_replyf(client_fd,
	                "{\"ok\":true,\"bar\":{\"show\":%s,\"override\":%s,\"status\":\"%s\",\"segments\":[",
	                user_config.showbar ? "true" : "false",
	                status_text_override_active() ? "true" : "false",
	                stext);

	for (int i = 0; i < segments; i++) {
		char label[STATUS_MAX_LABEL] = "";
		status_segment_label_at(i, label, sizeof(label));
		ipc_write_replyf(client_fd,
		                "%s{\"id\":%d,\"label\":\"%s\",\"left\":%s,\"middle\":%s,\"right\":%s}",
		                i == 0 ? "" : ",",
		                i + 1,
		                label,
		                status_action_command_get(i, Button1) ? "true" : "false",
		                status_action_command_get(i, Button2) ? "true" : "false",
		                status_action_command_get(i, Button3) ? "true" : "false");
	}

	ipc_write_reply(client_fd, "],\"monitors\":[");
	for (int i = 0; i < n_mons; i++) {
		int ws = mons[i].view_ws;
		Client *wf = (ws >= 0 && ws < NUM_WORKSPACES) ? workspace_states[ws].focused : NULL;
		ipc_write_replyf(client_fd,
		                "%s{\"id\":%d,\"workspace\":%d,\"layout\":\"%s\",\"focused\":\"0x%lx\",\"current\":%s}",
		                i == 0 ? "" : ",",
		                i,
		                ws + 1,
		                ipc_layout_name(workspace_layout_for(ws)),
		                (unsigned long)(wf ? wf->win : None),
		                i == current_mon ? "true" : "false");
	}
	ipc_write_reply(client_fd, "]}}\n");
}

static Bool ipc_parse_int(const char *s, int *out)
{
	if (!s || !s[0] || !out)
		return False;

	char *end = NULL;
	long v = strtol(s, &end, 10);
	if (end == s || *end != '\0')
		return False;
	if (v < INT_MIN || v > INT_MAX)
		return False;
	*out = (int)v;
	return True;
}

static Bool ipc_parse_index_1based_or_0based(const char *s, int max_count, int *out)
{
	int v = 0;
	if (!ipc_parse_int(s, &v))
		return False;

	if (v >= 1 && v <= max_count) {
		*out = v - 1;
		return True;
	}
	if (v >= 0 && v < max_count) {
		*out = v;
		return True;
	}
	return False;
}

static void ipc_bar_set_visible(Bool on)
{
	if (user_config.showbar == on)
		return;
	user_config.showbar = on;
	setup_bars();
	tile();
	update_borders();
}

static void ipc_move_focused_to_monitor_index(int target_mon)
{
	if (!focused || n_mons <= 1)
		return;

	int src = CLAMP(focused->mon, 0, n_mons - 1);
	target_mon = CLAMP(target_mon, 0, n_mons - 1);
	if (target_mon == src)
		return;

	int next_steps = (target_mon - src + n_mons) % n_mons;
	int prev_steps = (src - target_mon + n_mons) % n_mons;
	if (next_steps <= prev_steps) {
		for (int i = 0; i < next_steps; i++)
			move_next_mon();
	}
	else {
		for (int i = 0; i < prev_steps; i++)
			move_prev_mon();
	}
}

static Bool ipc_parse_layout(const char *name, int *layout_out)
{
	if (!name || !layout_out)
		return False;
	for (int i = 0; i < (int)LENGTH(layouts); i++) {
		if (layouts[i].symbol && strcmp(name, layouts[i].symbol) == 0) {
			*layout_out = i;
			return True;
		}
	}

	int want_mode = -1;
	if (strcasecmp(name, "tile") == 0)
		want_mode = LayoutTile;
	else if (strcasecmp(name, "monocle") == 0)
		want_mode = LayoutMonocle;
	else if (strcasecmp(name, "floating") == 0)
		want_mode = LayoutFloating;
	else if (strcasecmp(name, "fibonacci") == 0)
		want_mode = LayoutFibonacci;
	else if (strcasecmp(name, "dwindle") == 0)
		want_mode = LayoutDwindle;

	if (want_mode >= 0) {
		for (int i = 0; i < (int)LENGTH(layouts); i++) {
			if (layouts[i].mode == want_mode) {
				*layout_out = i;
				return True;
			}
		}
	}

	return False;
}

static Bool ipc_parse_workspace(const char *cmd, const char *prefix, int *ws_out)
{
	if (!cmd || !prefix || !ws_out)
		return False;

	size_t prefix_len = strlen(prefix);
	if (prefix_len < 2 || strcmp(prefix + prefix_len - 2, "%d") != 0)
		return False;

	prefix_len -= 2;
	if (strncmp(cmd, prefix, prefix_len) != 0)
		return False;

	char *end = NULL;
	long ws = strtol(cmd + prefix_len, &end, 10);
	if (end == cmd + prefix_len || *end != '\0')
		return False;

	if (ws >= 1 && ws <= NUM_WORKSPACES)
		ws -= 1;

	if (ws < 0 || ws >= NUM_WORKSPACES)
		return False;

	*ws_out = (int)ws;
	return True;
}

static Bool ipc_dispatch(int client_fd, char *cmd)
{
	if (!cmd || !cmd[0]) {
		ipc_write_reply(client_fd, "error empty command\n");
		return False;
	}

	Bool json = ipc_extract_json_mode(cmd);
	if (!cmd[0]) {
		ipc_reply_error(client_fd, json, "empty command");
		return False;
	}

	if (strcmp(cmd, "ping") == 0) {
		if (json)
			ipc_write_reply(client_fd, "{\"ok\":true,\"pong\":true}\n");
		else
			ipc_write_reply(client_fd, "ok pong\n");
		return False;
	}

	if (strcmp(cmd, "help") == 0) {
		if (!json) {
			ipc_write_reply(client_fd,
			                "ok commands: ping, help, status, query <monitors|workspaces|layouts|focused-client|clients|bar>, "
			                "focus <next|prev|left|right|up|down|monitor-next|monitor-prev|monitor-left|monitor-right|monitor-up|monitor-down>, "
			                "swap <left|right|up|down>, move <left|right|up|down>, "
			                "workspace N, move-workspace N, move-monitor <next|prev|N>, "
			                "layout <tile|monocle|floating|fibonacci|dwindle>, "
			                "gaps <inc|dec|set N|get>, bar <show|hide|toggle|get|status set|status clear|action set|action clear|click>, "
			                "scratchpad <toggle|set|remove> N, fullscreen <toggle|on|off>, "
			                "floating <toggle|on|off|get>, subscribe, reload, quit\n");
		}
		else {
			ipc_write_reply(client_fd,
			                "{\"ok\":true,\"commands\":[\"ping\",\"help\",\"status\",\"query\",\"focus\",\"swap\",\"move\",\"workspace\",\"move-workspace\",\"move-monitor\",\"layout\",\"gaps\",\"bar\",\"scratchpad\",\"fullscreen\",\"floating\",\"subscribe\",\"reload\",\"quit\"]}\n");
		}
		return False;
	}

	if (strcmp(cmd, "status") == 0) {
		if (json)
			ipc_reply_status_json(client_fd);
		else
			ipc_reply_status(client_fd);
		return False;
	}

	if (strcmp(cmd, "subscribe") == 0) {
		if (ipc_add_subscriber(client_fd, json) < 0) {
			ipc_reply_error(client_fd, json, "subscriber limit reached");
			return False;
		}
		if (json)
			ipc_write_reply(client_fd, "{\"ok\":true,\"subscribed\":true}\n");
		else
			ipc_write_reply(client_fd, "ok subscribed\n");
		return True;
	}

	if (strncmp(cmd, "query ", 6) == 0) {
		char *what = cmd + 6;
		ipc_trim(what);
		if (strcmp(what, "monitors") == 0) {
			ipc_reply_query_monitors(client_fd, json);
			return False;
		}
		if (strcmp(what, "workspaces") == 0) {
			ipc_reply_query_workspaces(client_fd, json);
			return False;
		}
		if (strcmp(what, "layouts") == 0) {
			ipc_reply_query_layouts(client_fd, json);
			return False;
		}
		if (strcmp(what, "focused-client") == 0) {
			ipc_reply_query_focused_client(client_fd, json);
			return False;
		}
		if (strcmp(what, "clients") == 0) {
			ipc_reply_query_clients(client_fd, json);
			return False;
		}
		if (strcmp(what, "bar") == 0) {
			ipc_reply_query_bar(client_fd, json);
			return False;
		}
		ipc_reply_error(client_fd, json, "unknown query");
		return False;
	}

	if (strcmp(cmd, "focus next") == 0) {
		focus_next();
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "focus prev") == 0) {
		focus_prev();
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "focus monitor-next") == 0) {
		focus_next_mon();
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "focus monitor-prev") == 0) {
		focus_prev_mon();
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "focus monitor-left") == 0) {
		focus_mon_dir(DIR_LEFT);
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "focus monitor-right") == 0) {
		focus_mon_dir(DIR_RIGHT);
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "focus monitor-up") == 0) {
		focus_mon_dir(DIR_UP);
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "focus monitor-down") == 0) {
		focus_mon_dir(DIR_DOWN);
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strncmp(cmd, "focus ", 6) == 0) {
		char *dir = cmd + 6;
		ipc_trim(dir);
		if (strcmp(dir, "left") == 0 || strcmp(dir, "right") == 0 ||
		    strcmp(dir, "up") == 0 || strcmp(dir, "down") == 0) {
			int direction = DIR_LEFT;
			if (strcmp(dir, "right") == 0)
				direction = DIR_RIGHT;
			else if (strcmp(dir, "up") == 0)
				direction = DIR_UP;
			else if (strcmp(dir, "down") == 0)
				direction = DIR_DOWN;

			Client *before = focused;
			focus_dir(direction);
			if (focused == before) {
				ipc_reply_error(client_fd, json, "no directional target");
				return False;
			}
			ipc_reply_ok(client_fd, json);
			return False;
		}
	}

	if (strncmp(cmd, "swap ", 5) == 0) {
		char *dir = cmd + 5;
		ipc_trim(dir);
		if (strcmp(dir, "left") == 0 || strcmp(dir, "right") == 0 ||
		    strcmp(dir, "up") == 0 || strcmp(dir, "down") == 0) {
			int direction = DIR_LEFT;
			if (strcmp(dir, "right") == 0)
				direction = DIR_RIGHT;
			else if (strcmp(dir, "up") == 0)
				direction = DIR_UP;
			else if (strcmp(dir, "down") == 0)
				direction = DIR_DOWN;

			swap_dir(direction);
			ipc_reply_ok(client_fd, json);
			return False;
		}
		ipc_reply_error(client_fd, json, "unknown swap direction");
		return False;
	}

	if (strncmp(cmd, "move ", 5) == 0) {
		char *dir = cmd + 5;
		ipc_trim(dir);
		if (strcmp(dir, "left") == 0 || strcmp(dir, "right") == 0 ||
		    strcmp(dir, "up") == 0 || strcmp(dir, "down") == 0) {
			int direction = DIR_LEFT;
			if (strcmp(dir, "right") == 0)
				direction = DIR_RIGHT;
			else if (strcmp(dir, "up") == 0)
				direction = DIR_UP;
			else if (strcmp(dir, "down") == 0)
				direction = DIR_DOWN;

			move_dir(direction);
			ipc_reply_ok(client_fd, json);
			return False;
		}
		ipc_reply_error(client_fd, json, "unknown move direction");
		return False;
	}

	if (strcmp(cmd, "reload") == 0) {
		ipc_reply_ok(client_fd, json);
		reload_config();
		return False;
	}

	if (strcmp(cmd, "quit") == 0) {
		ipc_reply_ok(client_fd, json);
		quit();
		return False;
	}

	int ws = -1;
	if (ipc_parse_workspace(cmd, "workspace %d", &ws)) {
		change_workspace(ws);
		ipc_reply_ok(client_fd, json);
		return False;
	}
	if (strncmp(cmd, "workspace ", 10) == 0) {
		ipc_reply_error(client_fd, json, "workspace out of range");
		return False;
	}

	if (ipc_parse_workspace(cmd, "move-workspace %d", &ws)) {
		move_to_workspace(ws);
		ipc_reply_ok(client_fd, json);
		return False;
	}
	if (strncmp(cmd, "move-workspace ", 15) == 0) {
		ipc_reply_error(client_fd, json, "workspace out of range");
		return False;
	}

	if (strncmp(cmd, "move-monitor ", 13) == 0) {
		char *target = cmd + 13;
		ipc_trim(target);
		if (strcmp(target, "left") == 0) {
			send_to_mon_dir(DIR_LEFT);
			ipc_reply_ok(client_fd, json);
			return False;
		}
		if (strcmp(target, "right") == 0) {
			send_to_mon_dir(DIR_RIGHT);
			ipc_reply_ok(client_fd, json);
			return False;
		}
		if (strcmp(target, "up") == 0) {
			send_to_mon_dir(DIR_UP);
			ipc_reply_ok(client_fd, json);
			return False;
		}
		if (strcmp(target, "down") == 0) {
			send_to_mon_dir(DIR_DOWN);
			ipc_reply_ok(client_fd, json);
			return False;
		}
		if (strcmp(target, "next") == 0) {
			move_next_mon();
			ipc_reply_ok(client_fd, json);
			return False;
		}
		if (strcmp(target, "prev") == 0) {
			move_prev_mon();
			ipc_reply_ok(client_fd, json);
			return False;
		}

		int mon = -1;
		if (!ipc_parse_index_1based_or_0based(target, n_mons, &mon)) {
			ipc_reply_error(client_fd, json, "monitor out of range");
			return False;
		}
		ipc_move_focused_to_monitor_index(mon);
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strncmp(cmd, "layout ", 7) == 0) {
		int target = -1;
		char *layout_name = cmd + 7;
		ipc_trim(layout_name);
		if (!ipc_parse_layout(layout_name, &target)) {
			ipc_reply_error(client_fd, json, "unknown layout");
			return False;
		}

		Arg arg = {.i = target};
		setlayoutcmd(&arg);
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "gaps inc") == 0) {
		inc_gaps();
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "gaps dec") == 0) {
		dec_gaps();
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "gaps get") == 0 || strcmp(cmd, "gaps") == 0) {
		int gaps = workspace_gaps_for(current_ws);
		if (json)
			ipc_write_replyf(client_fd, "{\"ok\":true,\"gaps\":%d}\n", gaps);
		else
			ipc_write_replyf(client_fd, "ok gaps=%d\n", gaps);
		return False;
	}

	if (strncmp(cmd, "gaps set ", 9) == 0) {
		int v = 0;
		if (!ipc_parse_int(cmd + 9, &v) || v < 0) {
			ipc_reply_error(client_fd, json, "invalid gaps value");
			return False;
		}
		workspace_states[current_ws].gaps = v;
		tile();
		update_borders();
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strncmp(cmd, "bar status set ", 15) == 0) {
		status_set_text_override(cmd + 15);
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "bar status clear") == 0) {
		status_clear_text_override();
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strncmp(cmd, "bar action set ", 15) == 0) {
		char *rest = cmd + 15;
		ipc_trim(rest);
		char *sp1 = strchr(rest, ' ');
		if (!sp1) {
			ipc_reply_error(client_fd, json, "bar action requires segment and button");
			return False;
		}
		*sp1 = '\0';
		char *button_s = sp1 + 1;
		ipc_trim(button_s);
		char *sp2 = strchr(button_s, ' ');
		if (!sp2) {
			ipc_reply_error(client_fd, json, "bar action set requires command");
			return False;
		}
		*sp2 = '\0';
		char *cmd_s = sp2 + 1;
		ipc_trim(cmd_s);

		int seg = -1;
		unsigned int button = 0;
		if (!ipc_parse_index_1based_or_0based(rest, STATUS_MAX_SEGMENTS, &seg)) {
			ipc_reply_error(client_fd, json, "segment out of range");
			return False;
		}
		if (!ipc_parse_button_name(button_s, &button)) {
			ipc_reply_error(client_fd, json, "unknown button");
			return False;
		}
		if (!status_action_command_set(seg, button, cmd_s)) {
			ipc_reply_error(client_fd, json, "invalid action command");
			return False;
		}

		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strncmp(cmd, "bar action clear ", 17) == 0) {
		char *rest = cmd + 17;
		ipc_trim(rest);
		char *sp1 = strchr(rest, ' ');
		char *button_s = NULL;
		if (sp1) {
			*sp1 = '\0';
			button_s = sp1 + 1;
			ipc_trim(button_s);
		}

		int seg = -1;
		if (!ipc_parse_index_1based_or_0based(rest, STATUS_MAX_SEGMENTS, &seg)) {
			ipc_reply_error(client_fd, json, "segment out of range");
			return False;
		}

		if (!button_s || !button_s[0] || strcasecmp(button_s, "all") == 0) {
			status_action_command_clear(seg, 0, True);
			ipc_reply_ok(client_fd, json);
			return False;
		}

		unsigned int button = 0;
		if (!ipc_parse_button_name(button_s, &button)) {
			ipc_reply_error(client_fd, json, "unknown button");
			return False;
		}

		status_action_command_clear(seg, button, False);
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strncmp(cmd, "bar click ", 10) == 0) {
		char *rest = cmd + 10;
		ipc_trim(rest);
		char *sp1 = strchr(rest, ' ');
		if (!sp1) {
			ipc_reply_error(client_fd, json, "bar click requires segment and button");
			return False;
		}
		*sp1 = '\0';
		char *button_s = sp1 + 1;
		ipc_trim(button_s);

		int seg = -1;
		unsigned int button = 0;
		if (!ipc_parse_index_1based_or_0based(rest, STATUS_MAX_SEGMENTS, &seg)) {
			ipc_reply_error(client_fd, json, "segment out of range");
			return False;
		}
		if (!ipc_parse_button_name(button_s, &button)) {
			ipc_reply_error(client_fd, json, "unknown button");
			return False;
		}

		const char *action = status_action_command_get(seg, button);
		if (!action || !action[0]) {
			ipc_reply_error(client_fd, json, "no action for segment/button");
			return False;
		}

		const char *argv[] = {"/bin/sh", "-c", action, NULL};
		spawn(argv);
		char details[128];
		snprintf(details, sizeof(details), "segment=%d button=%u", seg + 1, button);
		ipc_notify_event("status_click", details);

		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "bar show") == 0) {
		ipc_bar_set_visible(True);
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "bar hide") == 0) {
		ipc_bar_set_visible(False);
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "bar toggle") == 0) {
		ipc_bar_set_visible(!user_config.showbar);
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "bar get") == 0 || strcmp(cmd, "bar") == 0) {
		if (json)
			ipc_write_replyf(client_fd, "{\"ok\":true,\"show\":%s,\"override\":%s}\n",
			                user_config.showbar ? "true" : "false",
			                status_text_override_active() ? "true" : "false");
		else
			ipc_write_replyf(client_fd, "ok showbar=%d override=%d\n",
			                user_config.showbar ? 1 : 0,
			                status_text_override_active() ? 1 : 0);
		return False;
	}

	if (strncmp(cmd, "scratchpad ", 11) == 0) {
		char *rest = cmd + 11;
		ipc_trim(rest);
		char *sep = strchr(rest, ' ');
		if (!sep) {
			ipc_reply_error(client_fd, json, "scratchpad index required");
			return False;
		}
		*sep = '\0';
		char *index_s = sep + 1;
		ipc_trim(index_s);
		int idx = -1;
		if (!ipc_parse_index_1based_or_0based(index_s, MAX_SCRATCHPADS, &idx)) {
			ipc_reply_error(client_fd, json, "scratchpad index out of range");
			return False;
		}

		if (strcmp(rest, "toggle") == 0)
			toggle_scratchpad(idx);
		else if (strcmp(rest, "set") == 0)
			set_win_scratchpad(idx);
		else if (strcmp(rest, "remove") == 0)
			remove_scratchpad(idx);
		else {
			ipc_reply_error(client_fd, json, "unknown scratchpad action");
			return False;
		}

		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "fullscreen toggle") == 0) {
		toggle_fullscreen();
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "fullscreen on") == 0 || strcmp(cmd, "fullscreen off") == 0) {
		Bool on = strcmp(cmd, "fullscreen on") == 0;
		if (!focused) {
			ipc_reply_error(client_fd, json, "no focused client");
			return False;
		}
		apply_fullscreen(focused, on);
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "floating toggle") == 0) {
		toggle_floating();
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "floating on") == 0 || strcmp(cmd, "floating off") == 0) {
		Bool on = strcmp(cmd, "floating on") == 0;
		if (!focused) {
			ipc_reply_error(client_fd, json, "no focused client");
			return False;
		}
		if (focused->floating != on)
			toggle_floating();
		ipc_reply_ok(client_fd, json);
		return False;
	}

	if (strcmp(cmd, "floating get") == 0 || strcmp(cmd, "floating") == 0) {
		if (json)
			ipc_write_replyf(client_fd, "{\"ok\":true,\"floating\":%s}\n",
			                focused && focused->floating ? "true" : "false");
		else
			ipc_write_replyf(client_fd, "ok floating=%d\n", focused && focused->floating ? 1 : 0);
		return False;
	}

	ipc_reply_error(client_fd, json, "unknown command");
	return False;
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
		if (ipc_set_nonblock(client_fd) < 0) {
			close(client_fd);
			continue;
		}
		ipc_set_recv_timeout(client_fd, 200);

		char buf[512];
		size_t used = 0;
		Bool truncated = False;
		Bool have_line = False;
		for (;;) {
			if (used >= sizeof(buf) - 1) {
				truncated = True;
				break;
			}

			ssize_t n = read(client_fd, buf + used, sizeof(buf) - 1 - used);
			if (n > 0) {
				used += (size_t)n;
				if (memchr(buf, '\n', used) || memchr(buf, '\r', used)) {
					have_line = True;
					break;
				}
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

		if (!have_line) {
			ipc_write_reply(client_fd, "error incomplete command\n");
			close(client_fd);
			goto next_client;
		}

		buf[used] = '\0';
		ipc_trim(buf);
		Bool keep_open = ipc_dispatch(client_fd, buf);
		if (!keep_open)
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

	for (int i = 0; i < IPC_MAX_SUBSCRIBERS; i++) {
		if (ipc_subscribers[i].fd >= 0) {
			close(ipc_subscribers[i].fd);
			ipc_subscribers[i].fd = -1;
			ipc_subscribers[i].json = False;
		}
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

	for (int i = 0; i < IPC_MAX_SUBSCRIBERS; i++) {
		ipc_subscribers[i].fd = -1;
		ipc_subscribers[i].json = False;
	}

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

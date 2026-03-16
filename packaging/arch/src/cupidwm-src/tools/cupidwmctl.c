#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define PATH_MAX 4096

static void usage(const char *argv0)
{
	fprintf(stderr, "usage: %s [options] <command...>\n", argv0);
	fprintf(stderr, "  -s <path>               Use explicit IPC socket path\n");
	fprintf(stderr, "  --json                  Request JSON-formatted response\n");
	fprintf(stderr, "  --print-socket          Print resolved IPC socket path and exit\n");
	fprintf(stderr, "  -h, --help              Show this help text\n");
}

static void display_token(char *out, size_t outsz)
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

static void default_socket_path(char *out, size_t outsz)
{
	char display_tok[64] = {0};
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	display_token(display_tok, sizeof(display_tok));
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
	snprintf(tmp_dir, sizeof(tmp_dir), "/tmp/cupidwm-%u", (unsigned)getuid());
	if (display_tok[0])
		snprintf(out, outsz, "%s/cupidwm-%s.sock", tmp_dir, display_tok);
	else
		snprintf(out, outsz, "%s/cupidwm.sock", tmp_dir);
}

static void hint_path(char *out, size_t outsz, const char *display_tok)
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

	if (display_tok && display_tok[0])
		snprintf(out, outsz, "%.*s/cupidwm-%s.sockpath", (int)n, runtime_dir, display_tok);
	else
		snprintf(out, outsz, "%.*s/cupidwm.sockpath", (int)n, runtime_dir);
}

static int read_socket_hint_file(const char *path, char *out, size_t outsz)
{
	if (!path || !path[0] || !out || outsz == 0)
		return 0;

	FILE *f = fopen(path, "r");
	if (!f)
		return 0;
	if (!fgets(out, (int)outsz, f)) {
		fclose(f);
		out[0] = '\0';
		return 0;
	}
	fclose(f);

	size_t n = strlen(out);
	while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ' || out[n - 1] == '\t')) {
		out[n - 1] = '\0';
		n--;
	}
	while (*out == ' ' || *out == '\t')
		memmove(out, out + 1, strlen(out));
	return out[0] != '\0';
}

static int read_socket_hint(char *out, size_t outsz)
{
	if (!out || outsz == 0)
		return 0;

	char display_tok[64] = {0};
	char path[PATH_MAX] = {0};
	display_token(display_tok, sizeof(display_tok));

	if (display_tok[0]) {
		hint_path(path, sizeof(path), display_tok);
		if (read_socket_hint_file(path, out, outsz))
			return 1;
	}

	hint_path(path, sizeof(path), "");
	if (read_socket_hint_file(path, out, outsz))
		return 1;

	return 0;
}

static void resolve_socket_path(char *out, size_t outsz)
{
	if (!out || outsz == 0)
		return;

	const char *env_path = getenv("CUPIDWM_IPC_SOCKET");
	if (env_path && env_path[0]) {
		snprintf(out, outsz, "%s", env_path);
		return;
	}

	if (read_socket_hint(out, outsz))
		return;

	default_socket_path(out, outsz);
}

static int connect_socket(const char *path)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("cupidwmctl: socket");
		return -1;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;

	if (strlen(path) >= sizeof(addr.sun_path)) {
		fprintf(stderr, "cupidwmctl: socket path too long: %s\n", path);
		close(fd);
		return -1;
	}
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "cupidwmctl: connect %s failed: %s\n", path, strerror(errno));
		close(fd);
		return -1;
	}

	return fd;
}

static int set_recv_timeout(int fd, int timeout_ms)
{
	struct timeval tv;

	if (fd < 0 || timeout_ms < 0)
		return -1;

	tv.tv_sec = timeout_ms / 1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;
	return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

int main(int argc, char **argv)
{
	const char *socket_path = NULL;
	int want_json = 0;
	int stream_mode = 0;
	int cmd_start = 1;

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
			return 0;
		}

		if (strcmp(argv[i], "--print-socket") == 0 || strcmp(argv[i], "--print-default-socket") == 0) {
			char default_sock[PATH_MAX];
			resolve_socket_path(default_sock, sizeof(default_sock));
			puts(default_sock);
			return 0;
		}

		if (strcmp(argv[i], "--json") == 0) {
			want_json = 1;
			cmd_start = i + 1;
			continue;
		}

		if (strcmp(argv[i], "-s") == 0) {
			if (i + 1 >= argc) {
				usage(argv[0]);
				return 1;
			}
			socket_path = argv[i + 1];
			cmd_start = i + 2;
			i++;
			continue;
		}

		cmd_start = i;
		break;
	}

	if (cmd_start >= argc) {
		if (socket_path) {
			usage(argv[0]);
			return 1;
		}

		fprintf(stderr, "cupidwmctl: missing command\n");
		return 1;
	}

	char default_sock[PATH_MAX];
	if (!socket_path) {
		resolve_socket_path(default_sock, sizeof(default_sock));
		socket_path = default_sock;
	}

	char cmd[512] = {0};
	for (int i = cmd_start; i < argc; i++) {
		size_t used = strlen(cmd);
		if (used >= sizeof(cmd) - 1)
			break;

		if (i > cmd_start) {
			size_t rem = sizeof(cmd) - used - 1;
			if (rem == 0)
				break;
			strncat(cmd, " ", rem);
		}

		used = strlen(cmd);
		if (used >= sizeof(cmd) - 1)
			break;
		size_t rem = sizeof(cmd) - used - 1;
		strncat(cmd, argv[i], rem);
	}

	if (cmd[0] == '\0') {
		fprintf(stderr, "cupidwmctl: empty command\n");
		return 1;
	}

	if (strncmp(cmd, "subscribe", 9) == 0 &&
	    (cmd[9] == '\0' || cmd[9] == ' ' || cmd[9] == '\t'))
		stream_mode = 1;

	int fd = connect_socket(socket_path);
	if (fd < 0)
		return 1;
	if (!stream_mode)
		set_recv_timeout(fd, 1000);

	char payload[640];
	if (want_json)
		snprintf(payload, sizeof(payload), "json %s\n", cmd);
	else
		snprintf(payload, sizeof(payload), "%s\n", cmd);
	size_t payload_len = strlen(payload);
	size_t sent = 0;
	while (sent < payload_len) {
		ssize_t w = write(fd, payload + sent, payload_len - sent);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			perror("cupidwmctl: write");
			close(fd);
			return 1;
		}
		if (w == 0)
			break;
		sent += (size_t)w;
	}

	char buf[32768];
	size_t used = 0;
	for (;;) {
		if (used >= sizeof(buf) - 1)
			break;

		ssize_t n = read(fd, buf + used, sizeof(buf) - 1 - used);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				fprintf(stderr, "cupidwmctl: timed out waiting for reply from %s\n", socket_path);
				close(fd);
				return 1;
			}
			perror("cupidwmctl: read");
			close(fd);
			return 1;
		}
		if (n == 0)
			break;
		used += (size_t)n;
	}
	buf[used] = '\0';

	if (used > 0)
		fputs(buf, stdout);

	close(fd);

	if (stream_mode)
		return 0;
	if (want_json) {
		if (strstr(buf, "\"ok\":true"))
			return 0;
	}
	if (strncmp(buf, "ok", 2) == 0)
		return 0;
	if (used > 0)
		return 2;
	return 1;
}

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define PATH_MAX 4096

static void usage(const char *argv0)
{
	fprintf(stderr, "usage: %s [-s /path/to/cupidwm.sock] <command...>\n", argv0);
}

static void default_socket_path(char *out, size_t outsz)
{
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (runtime_dir && runtime_dir[0]) {
		snprintf(out, outsz, "%s/cupidwm.sock", runtime_dir);
		return;
	}

	snprintf(out, outsz, "/tmp/cupidwm-%u.sock", (unsigned)getuid());
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

int main(int argc, char **argv)
{
	const char *socket_path = NULL;
	int cmd_start = 1;

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "-s") == 0) {
		if (argc < 4) {
			usage(argv[0]);
			return 1;
		}
		socket_path = argv[2];
		cmd_start = 3;
	}

	if (cmd_start >= argc) {
		fprintf(stderr, "cupidwmctl: missing command\n");
		return 1;
	}

	char default_sock[PATH_MAX];
	if (!socket_path) {
		default_socket_path(default_sock, sizeof(default_sock));
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

	int fd = connect_socket(socket_path);
	if (fd < 0)
		return 1;

	char payload[560];
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

	char buf[2048];
	size_t used = 0;
	for (;;) {
		if (used >= sizeof(buf) - 1)
			break;

		ssize_t n = read(fd, buf + used, sizeof(buf) - 1 - used);
		if (n < 0) {
			if (errno == EINTR)
				continue;
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

	if (strncmp(buf, "ok", 2) == 0)
		return 0;
	if (used > 0)
		return 2;
	return 1;
}

/* Exercise the real CLI with an in-memory transport. No X server is needed. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int connection_count;
static int read_count;
static char request[1024];
static size_t request_len;

static int fake_socket(int domain, int type, int protocol)
{
	(void)domain; (void)type; (void)protocol;
	connection_count++;
	return 10;
}
static int fake_connect(int fd, const struct sockaddr *address, socklen_t size)
{
	(void)fd; (void)address; (void)size;
	return 0;
}
static int fake_setsockopt(int fd, int level, int option, const void *value, socklen_t size)
{
	(void)fd; (void)level; (void)option; (void)value; (void)size;
	return 0;
}
static ssize_t fake_write(int fd, const void *buffer, size_t size)
{
	(void)fd;
	assert(request_len + size < sizeof(request));
	memcpy(request + request_len, buffer, size);
	request_len += size;
	request[request_len] = '\0';
	return (ssize_t)size;
}
static ssize_t fake_read(int fd, void *buffer, size_t size)
{
	(void)fd;
	if (read_count++)
		return 0;
	const char *reply = strncmp(request, "json ", 5) == 0 ? "{\"ok\":true}\n" : "ok\n";
	size_t length = strlen(reply);
	assert(size >= length);
	memcpy(buffer, reply, length);
	return (ssize_t)length;
}
static int fake_close(int fd) { (void)fd; return 0; }

#define socket fake_socket
#define connect fake_connect
#define setsockopt fake_setsockopt
#define write fake_write
#define read fake_read
#define close fake_close
#define main control_main
int control_main(int argc, char **argv);
#include "../../tools/cupidwmctl.c"
#undef main

static void run_case(size_t length, int json, int valid)
{
	char command[700];
	assert(length < sizeof(command));
	memset(command, 'x', length);
	command[length] = '\0';
	char *args[] = {(char[]){"cupidwmctl"}, (char[]){"-s"}, (char[]){"/unused"}, (char[]){"--json"}, command, NULL};
	connection_count = read_count = 0;
	request_len = 0;
	if (!json)
		args[3] = command;
	int result = control_main(json ? 5 : 4, args);
	if (valid) {
		assert(result == 0);
		assert(connection_count == 1);
		size_t prefix = json ? 5 : 0;
		assert(request_len == length + prefix + 1);
		if (json)
			assert(strncmp(request, "json ", 5) == 0);
		assert(memcmp(request + prefix, command, length) == 0);
		assert(request[request_len - 1] == '\n');
	} else {
		assert(result != 0);
		assert(connection_count == 0);
		assert(request_len == 0);
	}
}

int main(void)
{
	run_case(505, 1, 1);
	run_case(506, 1, 0);
	run_case(600, 1, 0);
	run_case(510, 0, 1);
	run_case(511, 0, 0);
	run_case(600, 0, 0);
	char *bad[] = {(char[]){"cupidwmctl"}, (char[]){"-s"}, (char[]){"/unused"},
	               (char[]){"workspace-name"}, (char[]){"set"}, (char[]){"3"}, (char[]){"work\nquit"}, NULL};
	connection_count = 0;
	assert(control_main(7, bad) != 0);
	assert(connection_count == 0);
	bad[6] = (char[]){"work\rquit"};
	assert(control_main(7, bad) != 0);
	assert(connection_count == 0);
	/* Include spaces between arguments when checking the wire limit. */
	char tail[506];
	memset(tail, 'x', sizeof(tail) - 1);
	tail[sizeof(tail) - 1] = '\0';
	char *split[] = {(char[]){"cupidwmctl"}, (char[]){"-s"}, (char[]){"/unused"},
	                 (char[]){"ping"}, tail, NULL};
	connection_count = read_count = 0;
	request_len = 0;
	assert(control_main(5, split) == 0);
	assert(request_len == 511);
	assert(strncmp(request, "ping ", 5) == 0);
	split[3] = (char[]){"pings"};
	connection_count = 0;
	request_len = 0;
	assert(control_main(5, split) != 0);
	assert(connection_count == 0 && request_len == 0);
	puts("control input tests passed");
	return 0;
}

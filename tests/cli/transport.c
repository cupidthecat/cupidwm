/* Feed the real CLI through stdin without requiring an X server or IPC socket. */
#include <sys/socket.h>
#include <unistd.h>

static int test_socket(int domain, int type, int protocol)
{
	(void)domain;
	(void)type;
	(void)protocol;
	return dup(STDIN_FILENO);
}

static int test_connect(int fd, const struct sockaddr *addr, socklen_t len)
{
	(void)fd;
	(void)addr;
	(void)len;
	return 0;
}

static int test_setsockopt(int fd, int level, int name,
                          const void *value, socklen_t len)
{
	(void)fd;
	(void)level;
	(void)name;
	(void)value;
	(void)len;
	return 0;
}

static ssize_t test_write(int fd, const void *buf, size_t len)
{
	(void)fd;
	(void)buf;
	return (ssize_t)len;
}

#define socket test_socket
#define connect test_connect
#define setsockopt test_setsockopt
#define write test_write
#include "../../tools/cupidwmctl.c"

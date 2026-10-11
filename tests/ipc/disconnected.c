#define main cupidwm_program_main
int cupidwm_program_main(int ac, char **av);
#include "../../src/cupidwm.c"
#undef main
#include <assert.h>

int main(void)
{
	/* Some test runners inherit SIG_IGN. Exercise the normal default instead. */
	signal(SIGPIPE, SIG_DFL);
	ipc_init_state_once();
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(fd >= 0);
	/* An unconnected stream deterministically produces the same EPIPE error. */
	ipc_write_reply(fd, "ok pong\n");
	assert(errno == EPIPE || errno == ENOTCONN);
	ipc_write_replyf(fd, "ok count=%d\n", 1);
	assert(ipc_add_subscriber(fd, False) >= 0);
	ipc_notify_event("workspace", "workspace=2");
	assert(ipc_find_subscriber_slot(fd) < 0);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(fd >= 0);
	assert(ipc_add_subscriber(fd, True) >= 0);
	ipc_notify_event("workspace", "workspace=3");
	assert(ipc_find_subscriber_slot(fd) < 0);
	puts("disconnected IPC peer tests passed");
	return 0;
}

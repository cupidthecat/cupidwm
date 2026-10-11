#define main cupidwm_program_main
int cupidwm_program_main(int ac, char **av);
#include "../../src/cupidwm.c"
#undef main
#include <assert.h>

int main(void)
{
	ipc_init_state_once();
	assert(ipc_pending_add_client(-1) == -1);
	assert(ipc_pending_add_client(FD_SETSIZE) == -1);
	assert(ipc_pending_add_client(INT_MAX) == -1);

	fd_set fds;
	FD_ZERO(&fds);
	int maxfd = -1;
	ipc_server_fd = FD_SETSIZE;
	ipc_pending_clients[0].fd = INT_MAX;
	ipc_fdset_prepare(&fds, &maxfd);
	assert(maxfd == -1);
	assert(!ipc_fdset_has_ready(&fds));

	ipc_server_fd = FD_SETSIZE - 1;
	ipc_pending_clients[0].fd = 0;
	ipc_fdset_prepare(&fds, &maxfd);
	assert(maxfd == FD_SETSIZE - 1);
	assert(FD_ISSET(FD_SETSIZE - 1, &fds));
	assert(FD_ISSET(0, &fds));
	assert(ipc_fdset_has_ready(&fds));
	ipc_server_fd = -1;
	assert(ipc_fdset_has_ready(&fds));
	ipc_pending_clients[0].fd = -1;
	assert(!ipc_fdset_has_ready(&fds));
	puts("descriptor bounds tests passed");
	return 0;
}

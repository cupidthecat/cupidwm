#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static int pipes_created, pipe_attempts, closed, fork_count;
static int fail_pipe, fail_fork;
static void *dpy;
#define ConnectionNumber(display) ((void)(display), 9)
static void *test_malloc(size_t size) { return size ? malloc(size) : NULL; }
static int test_pipe(int fds[2])
{
	if (++pipe_attempts == fail_pipe) {
		errno = EMFILE;
		return -1;
	}
	fds[0] = 10 + pipes_created * 2;
	fds[1] = fds[0] + 1;
	pipes_created++;
	return 0;
}
static int test_close(int fd)
{
	assert(fd >= 10 && fd < 10 + 2 * pipes_created);
	unsigned int bit = 1U << (fd - 10);
	assert(((unsigned int)closed & bit) == 0);
	closed |= (int)bit;
	return 0;
}
static pid_t test_fork(void)
{
	if (++fork_count == fail_fork) {
		errno = EAGAIN;
		return -1;
	}
	return 123; /* Exercise parent cleanup without launching a process. */
}
#define malloc test_malloc
#define pipe test_pipe
#define close test_close
#define fork test_fork
void spawn(const char *const *argv);
#include "../../src/spawn.c"

static void reset(void)
{
	pipes_created = pipe_attempts = closed = fork_count = 0;
	fail_pipe = fail_fork = 0;
}

int main(void)
{
	const char *pipeline[] = {"one", "|", "two", "|", "three", NULL};
	reset();
	fail_pipe = 2;
	spawn(pipeline);
	assert(pipes_created == 1 && fork_count == 0);
	assert(closed == 3);
	reset();
	spawn(pipeline);
	assert(pipes_created == 2 && fork_count == 3 && closed == 15);
	reset();
	fail_fork = 2;
	spawn(pipeline);
	assert(pipes_created == 2 && fork_count == 2 && closed == 15);
	reset();
	const char *single[] = {"one", NULL};
	spawn(single);
	assert(pipe_attempts == 0 && fork_count == 1 && closed == 0);
	puts("spawn cleanup tests passed");
	return 0;
}

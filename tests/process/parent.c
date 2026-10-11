#include <assert.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif
#include "../../src/proc.c"

int main(void)
{
	assert(proc_parent_from_stat("123 (plain) S 42 0") == 42);
	assert(proc_parent_from_stat("123 (has spaces) S 42 0") == 42);
	assert(proc_parent_from_stat("123 (name) with) brackets) S 42 0") == 42);
	assert(proc_parent_from_stat("123 (has\nnewline) S 42 0") == 42);
	assert(proc_parent_from_stat("123 (plain) S 0 0") == 0);
	assert(proc_parent_from_stat("") == 0);
	assert(proc_parent_from_stat("123 (unfinished") == 0);
	assert(proc_parent_from_stat("123 (done)") == 0);
	assert(proc_parent_from_stat("123 (done) ") == 0);
	assert(proc_parent_from_stat("123 (done) S") == 0);
	assert(proc_parent_from_stat("123 (done) S ") == 0);
	assert(proc_parent_from_stat("123 (done) S -1 0") == 0);
	assert(proc_parent_from_stat("123 (done) S 42oops 0") == 0);
	assert(proc_parent_from_stat("123 (done) S 999999999999999999999999 0") == 0);
	assert(get_parent_process(0) == 0);
	assert(get_parent_process(-1) == 0);
#if defined(__linux__)
	char original[16];
	assert(prctl(PR_GET_NAME, original, 0, 0, 0) == 0);
	const char *names[] = { "plain", "has spaces", "name) tricky", "has\nnewline" };
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		assert(prctl(PR_SET_NAME, names[i], 0, 0, 0) == 0);
		assert(get_parent_process(getpid()) == getppid());
	}
	assert(prctl(PR_SET_NAME, original, 0, 0, 0) == 0);
#endif
	puts("process parent tests passed");
	return 0;
}

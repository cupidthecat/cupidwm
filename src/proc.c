/* Linux process ancestry. The parenthesized comm field may contain whitespace. */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

pid_t get_parent_process(pid_t pid);

static pid_t proc_parent_from_stat(const char *record)
{
	const char *end_name = strrchr(record, ')');
	if (!end_name || end_name[1] != ' ' || end_name[2] == '\0' ||
	    end_name[3] != ' ')
		return 0;

	const char *start = end_name + 4;
	char *end = NULL;
	errno = 0;
	long parent = strtol(start, &end, 10);
	if (errno || end == start || parent < 0 || parent > INT_MAX ||
	    (*end != ' ' && *end != '\n' && *end != '\0'))
		return 0;
	return (pid_t)parent;
}

pid_t get_parent_process(pid_t pid)
{
	if (pid <= 0)
		return 0;

	char path[64];
	snprintf(path, sizeof(path), "/proc/%ld/stat", (long)pid);
	FILE *f = fopen(path, "r");
	if (!f)
		return 0;

	/* Read the record, including newlines embedded in the process name. */
	char record[4096];
	size_t used = fread(record, 1, sizeof(record) - 1, f);
	int invalid = ferror(f) || used == sizeof(record) - 1;
	fclose(f);
	if (invalid)
		return 0;
	record[used] = '\0';
	return proc_parent_from_stat(record);
}

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

static int status_fifo_reader_fd = 10;
static char status_fifo_buf[1024];
static size_t status_fifo_buf_used;
static const char *input;
static size_t input_length, input_offset, chunk_size;
static int updates, clears;
static char last_text[1024];

static ssize_t test_read(int fd, void *dest, size_t capacity)
{
	assert(fd == status_fifo_reader_fd);
	if (input_offset == input_length) {
		errno = EAGAIN;
		return -1;
	}
	size_t n = input_length - input_offset;
	if (n > capacity) n = capacity;
	if (n > chunk_size) n = chunk_size;
	memcpy(dest, input + input_offset, n);
	input_offset += n;
	return (ssize_t)n;
}
static void status_trim_inline(char *line)
{
	size_t n = strlen(line);
	while (n && (line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t'))
		line[--n] = '\0';
	while (*line == ' ' || *line == '\t') memmove(line, line + 1, strlen(line));
}
static void status_set_text_override(const char *line)
{
	assert(strlen(line) < sizeof(last_text));
	strcpy(last_text, line);
	updates++;
}
static void status_clear_text_override(void) { clears++; }
#define read test_read
void status_fifo_handle_readable(void);
#include "../../src/status-input.c"

static void feed(const char *data, size_t length, size_t chunk)
{
	input = data;
	input_length = length;
	input_offset = 0;
	chunk_size = chunk;
	status_fifo_handle_readable();
	assert(input_offset == input_length);
}

int main(void)
{
	char stream[1400];
	memset(stream, 'x', 1100);
	memcpy(stream + 1100, "\nfresh\n", 7);
	feed(stream, 1107, 256);
	assert(updates == 1 && clears == 0);
	assert(strcmp(last_text, "fresh") == 0);
	/* Exact capacity, fragmented reads, and multiple updates in one read. */
	updates = clears = 0;
	memset(stream, 'a', 1023);
	stream[1023] = '\n';
	feed(stream, 1024, 1);
	assert(updates == 1 && strlen(last_text) == 1023);
	feed("one\ntwo\nclear\nreset\n", 20, 256);
	assert(updates == 3 && clears == 2);
	assert(strcmp(last_text, "two") == 0);
	/* A partial update must survive EAGAIN between reads. */
	feed("par", 3, 256);
	assert(updates == 3);
	feed("tial\r\n", 6, 2);
	assert(updates == 4 && strcmp(last_text, "partial") == 0);
	/* Overflow cannot turn a trailing fragment into a clear command. */
	memset(stream, 'z', 1024);
	feed(stream, 1024, 256);
	feed("clear\nvalid\n", 12, 1);
	assert(clears == 2 && updates == 5 && strcmp(last_text, "valid") == 0);
	const char binary[] = "bad\0clear\nnext\n";
	feed(binary, sizeof(binary) - 1, 2);
	assert(clears == 2 && updates == 6 && strcmp(last_text, "next") == 0);
	puts("status FIFO framing tests passed");
	return 0;
}

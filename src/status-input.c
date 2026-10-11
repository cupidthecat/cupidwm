/* Read newline-delimited status updates from the bar FIFO. */

static int status_fifo_discard_line;

void status_fifo_handle_readable(void)
{
	if (status_fifo_reader_fd < 0)
		return;

	char chunk[256];
	for (;;) {
		ssize_t n = read(status_fifo_reader_fd, chunk, sizeof(chunk));
		if (n < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (n == 0)
			break;

		for (ssize_t i = 0; i < n; i++) {
			char ch = chunk[i];
			if (ch == '\n') {
				if (!status_fifo_discard_line) {
					status_fifo_buf[status_fifo_buf_used] = '\0';
					status_trim_inline(status_fifo_buf);
					if (strcmp(status_fifo_buf, "clear") == 0 || strcmp(status_fifo_buf, "reset") == 0)
						status_clear_text_override();
					else if (status_fifo_buf[0])
						status_set_text_override(status_fifo_buf);
				}
				status_fifo_buf_used = 0;
				status_fifo_discard_line = 0;
			} else if (!status_fifo_discard_line) {
				if (ch == '\0' || status_fifo_buf_used == sizeof(status_fifo_buf) - 1) {
					/* Discard the entire invalid line, including any later fragments. */
					status_fifo_buf_used = 0;
					status_fifo_discard_line = 1;
				} else {
					status_fifo_buf[status_fifo_buf_used++] = ch;
				}
			}
		}
	}
}

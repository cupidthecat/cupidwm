/* Extracted from cupidwm.c: status-related rendering and metrics. */

static unsigned long pixel(long col)
{
	return ((unsigned long)col) & 0x00ffffffUL;
}

static int textw(const char *text)
{
	if (!text)
		return 0;
	if (bar_xft_font) {
		XGlyphInfo ext = {0};
		XftTextExtentsUtf8(dpy, bar_xft_font, (const FcChar8 *)text, (int)strlen(text), &ext);
		return (int)ext.xOff;
	}
	if (!bar_font)
		return (int)strlen(text) * 8;
	return XTextWidth(bar_font, text, (int)strlen(text));
}

static Bool xft_draw_text(XftDraw *draw, unsigned long color, int x, int y, const char *text, int len)
{
	if (!draw || !bar_xft_font || !text || len <= 0)
		return False;

	XRenderColor xr;
	xr.red = (unsigned short)(((color >> 16) & 0xff) * 257);
	xr.green = (unsigned short)(((color >> 8) & 0xff) * 257);
	xr.blue = (unsigned short)((color & 0xff) * 257);
	xr.alpha = 0xffff;

	XftColor xc;
	if (!XftColorAllocValue(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
				 DefaultColormap(dpy, DefaultScreen(dpy)), &xr, &xc))
		return False;

	XftDrawStringUtf8(draw, &xc, bar_xft_font, x, y, (const FcChar8 *)text, len);
	XftColorFree(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
		     DefaultColormap(dpy, DefaultScreen(dpy)), &xc);
	return True;
}

static void status_append_segment(char *dest, size_t destsz, const char *sep, const char *segment)
{
	if (!dest || destsz == 0 || !segment || !segment[0])
		return;

	size_t used = strlen(dest);
	if (used >= destsz - 1)
		return;

	if (used > 0 && sep && sep[0]) {
		size_t rem = destsz - used - 1;
		size_t seplen = strlen(sep);
		if (seplen > rem)
			seplen = rem;
		memcpy(dest + used, sep, seplen);
		used += seplen;
		dest[used] = '\0';
	}

	if (used < destsz - 1) {
		size_t rem = destsz - used - 1;
		size_t seglen = strlen(segment);
		if (seglen > rem)
			seglen = rem;
		memcpy(dest + used, segment, seglen);
		used += seglen;
		dest[used] = '\0';
	}
}

static size_t status_format_time(char *buf, size_t bufsz, const char *fmt, const struct tm *tm_now)
{
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
	size_t written = strftime(buf, bufsz, fmt, tm_now);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
	return written;
}

static Bool read_cpu_percent(double *usage_out)
{
#if !defined(__linux__)
	(void)usage_out;
	return False;
#else
	static unsigned long long prev_total = 0;
	static unsigned long long prev_idle = 0;

	if (!usage_out)
		return False;

	FILE *f = fopen("/proc/stat", "r");
	if (!f)
		return False;

	char line[512];
	if (!fgets(line, sizeof(line), f)) {
		fclose(f);
		return False;
	}
	fclose(f);

	unsigned long long user = 0, nice = 0, system = 0, idle = 0;
	unsigned long long iowait = 0, irq = 0, softirq = 0, steal = 0;
	int n = sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
		       &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
	if (n < 4)
		return False;

	unsigned long long idle_all = idle + iowait;
	unsigned long long non_idle = user + nice + system + irq + softirq + steal;
	unsigned long long total = idle_all + non_idle;

	if (prev_total == 0 || total < prev_total || idle_all < prev_idle) {
		prev_total = total;
		prev_idle = idle_all;
		return False;
	}

	unsigned long long dtotal = total - prev_total;
	unsigned long long didle = idle_all - prev_idle;

	prev_total = total;
	prev_idle = idle_all;

	if (dtotal == 0)
		return False;

	*usage_out = 100.0 * (double)(dtotal - didle) / (double)dtotal;
	if (*usage_out < 0.0)
		*usage_out = 0.0;
	if (*usage_out > 100.0)
		*usage_out = 100.0;
	return True;
#endif
}

static Bool read_ram_info(unsigned long long *used_mb, unsigned long long *total_mb, double *used_percent)
{
#if !defined(__linux__)
	(void)used_mb;
	(void)total_mb;
	(void)used_percent;
	return False;
#else
	if (!used_mb || !total_mb || !used_percent)
		return False;

	FILE *f = fopen("/proc/meminfo", "r");
	if (!f)
		return False;

	unsigned long long mem_total_kb = 0;
	unsigned long long mem_avail_kb = 0;
	char key[64];
	unsigned long long val = 0;
	char unit[32];

	while (fscanf(f, "%63[^:]: %llu %31s\n", key, &val, unit) == 3) {
		if (strcmp(key, "MemTotal") == 0)
			mem_total_kb = val;
		else if (strcmp(key, "MemAvailable") == 0)
			mem_avail_kb = val;

		if (mem_total_kb > 0 && mem_avail_kb > 0)
			break;
	}
	fclose(f);

	if (mem_total_kb == 0 || mem_avail_kb > mem_total_kb)
		return False;

	unsigned long long used_kb = mem_total_kb - mem_avail_kb;
	*used_mb = used_kb / 1024ULL;
	*total_mb = mem_total_kb / 1024ULL;
	*used_percent = (double)used_kb * 100.0 / (double)mem_total_kb;
	if (*used_percent < 0.0)
		*used_percent = 0.0;
	if (*used_percent > 100.0)
		*used_percent = 100.0;
	return True;
#endif
}

static Bool read_battery_info(const char *base_path, int *capacity_out, char *status_out, size_t status_out_len)
{
#if !defined(__linux__)
	(void)base_path;
	(void)capacity_out;
	(void)status_out;
	(void)status_out_len;
	return False;
#else
	if (!base_path || !base_path[0] || !capacity_out)
		return False;

	char path[PATH_MAX];
	FILE *f;
	int cap = -1;

	snprintf(path, sizeof(path), "%s/capacity", base_path);
	f = fopen(path, "r");
	if (!f)
		return False;
	if (fscanf(f, "%d", &cap) != 1) {
		fclose(f);
		return False;
	}
	fclose(f);

	if (cap < 0)
		return False;
	if (cap > 100)
		cap = 100;

	*capacity_out = cap;

	if (status_out && status_out_len > 0) {
		status_out[0] = '\0';
		snprintf(path, sizeof(path), "%s/status", base_path);
		f = fopen(path, "r");
		if (f) {
			if (fgets(status_out, (int)status_out_len, f)) {
				size_t n = strlen(status_out);
				while (n > 0 && (status_out[n - 1] == '\n' || status_out[n - 1] == '\r')) {
					status_out[n - 1] = '\0';
					n--;
				}
			}
			fclose(f);
		}
	}

	return True;
#endif
}

static Bool find_battery_path(char *out_path, size_t out_path_len)
{
#if !defined(__linux__)
	(void)out_path;
	(void)out_path_len;
	return False;
#else
	if (!out_path || out_path_len == 0)
		return False;

	out_path[0] = '\0';

	DIR *dir = opendir("/sys/class/power_supply");
	if (!dir)
		return False;

	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.')
			continue;

		char type_path[PATH_MAX];
		snprintf(type_path, sizeof(type_path), "/sys/class/power_supply/%s/type", ent->d_name);

		FILE *f = fopen(type_path, "r");
		if (!f)
			continue;

		char type[64] = "";
		Bool is_battery = False;
		if (fgets(type, sizeof(type), f)) {
			size_t n = strlen(type);
			while (n > 0 && (type[n - 1] == '\n' || type[n - 1] == '\r')) {
				type[n - 1] = '\0';
				n--;
			}
			is_battery = (strcasecmp(type, "Battery") == 0);
		}
		fclose(f);

		if (!is_battery)
			continue;

		snprintf(out_path, out_path_len, "/sys/class/power_supply/%s", ent->d_name);
		closedir(dir);
		return True;
	}

	closedir(dir);
	return False;
#endif
}

static Bool status_append_named_segment(char *dest, size_t destsz, const char *sep,
					       const char *name,
					       const char *diskbuf,
					       const char *cpubuf,
					       const char *rambuf,
					       const char *batbuf,
					       const char *timebuf)
{
	if (!name || !name[0])
		return False;

	if (strcasecmp(name, "disk") == 0)
		status_append_segment(dest, destsz, sep, diskbuf);
	else if (strcasecmp(name, "cpu") == 0)
		status_append_segment(dest, destsz, sep, cpubuf);
	else if (strcasecmp(name, "ram") == 0 || strcasecmp(name, "memory") == 0 || strcasecmp(name, "mem") == 0)
		status_append_segment(dest, destsz, sep, rambuf);
	else if (strcasecmp(name, "battery") == 0 || strcasecmp(name, "bat") == 0)
		status_append_segment(dest, destsz, sep, batbuf);
	else if (strcasecmp(name, "time") == 0 || strcasecmp(name, "date") == 0 || strcasecmp(name, "clock") == 0)
		status_append_segment(dest, destsz, sep, timebuf);
	else
		return False;

	return True;
}

enum { STATUS_EXTERNAL_CMD_TIMEOUT_MS = 200 };

static long status_elapsed_ms(const struct timespec *start)
{
	if (!start)
		return -1;

	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
		return -1;

	long secdiff = now.tv_sec - start->tv_sec;
	long nsecdiff = now.tv_nsec - start->tv_nsec;
	if (nsecdiff < 0) {
		secdiff -= 1;
		nsecdiff += 1000000000L;
	}
	return secdiff * 1000L + nsecdiff / 1000000L;
}

static Bool run_external_status_command(char *dest, size_t destsz)
{
	if (!dest || destsz == 0 || !user_config.status_external_cmd || !user_config.status_external_cmd[0])
		return False;

	int pipefds[2];
	if (pipe(pipefds) < 0)
		return False;

	pid_t pid = fork();
	if (pid < 0) {
		close(pipefds[0]);
		close(pipefds[1]);
		return False;
	}

	if (pid == 0) {
		close(pipefds[0]);
		if (dup2(pipefds[1], STDOUT_FILENO) < 0)
			_exit(127);
		if (dup2(pipefds[1], STDERR_FILENO) < 0)
			_exit(127);
		close(pipefds[1]);
		execl("/bin/sh", "sh", "-c", user_config.status_external_cmd, (char *)NULL);
		_exit(127);
	}

	close(pipefds[1]);

	int flags = fcntl(pipefds[0], F_GETFL);
	if (flags >= 0)
		fcntl(pipefds[0], F_SETFL, flags | O_NONBLOCK);

	dest[0] = '\0';
	struct timespec start = {0};
	if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
		start.tv_sec = start.tv_nsec = 0;

	size_t used = 0;
	Bool have_line = False;
	Bool timed_out = False;

	while (used < destsz - 1 && !have_line && !timed_out) {
		long elapsed = status_elapsed_ms(&start);
		if (elapsed < 0) {
			break;
		}

		long remaining = STATUS_EXTERNAL_CMD_TIMEOUT_MS - elapsed;
		if (remaining <= 0) {
			timed_out = True;
			break;
		}

		struct timeval timeout = {
			.tv_sec = remaining / 1000,
			.tv_usec = (remaining % 1000) * 1000
		};

		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(pipefds[0], &rfds);

		int sel = select(pipefds[0] + 1, &rfds, NULL, NULL, &timeout);
		if (sel < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (sel == 0) {
			timed_out = True;
			break;
		}

		if (!FD_ISSET(pipefds[0], &rfds))
			continue;

		ssize_t n = read(pipefds[0], dest + used, destsz - 1 - used);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			break;
		}
		if (n == 0) {
			have_line = used > 0;
			break;
		}

		size_t prev = used;
		used += (size_t)n;
		if (used > destsz - 1)
			used = destsz - 1;

		for (size_t i = prev; i < used; i++) {
			if (dest[i] == '\n' || dest[i] == '\r') {
				used = i;
				have_line = True;
				break;
			}
		}
	}

	close(pipefds[0]);

	int status = 0;
	pid_t waited = waitpid(pid, &status, WNOHANG);
	if (waited == 0) {
		kill(pid, SIGKILL);
		while (waitpid(pid, &status, 0) < 0) {
			if (errno != EINTR)
				break;
		}
	}
	else if (waited == -1 && errno != ECHILD) {
		kill(pid, SIGKILL);
		while (waitpid(pid, &status, 0) < 0) {
			if (errno != EINTR)
				break;
		}
	}

	if (timed_out) {
		dest[0] = '\0';
		return False;
	}

	dest[used] = '\0';
	while (used > 0 && (dest[used - 1] == '\n' || dest[used - 1] == '\r')) {
		dest[used - 1] = '\0';
		used--;
	}

	return used > 0;
}

typedef struct {
	int start_px;
	int end_px;
	char label[STATUS_MAX_LABEL];
} StatusSegment;

static int status_fifo_reader_fd = -1;
static int status_fifo_writer_fd = -1;
static char status_fifo_buf[1024] = {0};
static size_t status_fifo_buf_used = 0;

static int status_button_slot(unsigned int button)
{
	if (button == Button1)
		return 0;
	if (button == Button2)
		return 1;
	if (button == Button3)
		return 2;
	return -1;
}

static const char *status_separator_value(void)
{
	const char *sep = user_config.status_separator;
	if (!sep || !sep[0])
		return " ";
	return sep;
}

static void status_trim_inline(char *s)
{
	if (!s)
		return;

	size_t len = strlen(s);
	while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ' || s[len - 1] == '\t')) {
		s[len - 1] = '\0';
		len--;
	}

	char *p = s;
	while (*p == ' ' || *p == '\t')
		p++;
	if (p != s)
		memmove(s, p, strlen(p) + 1);
}

static int status_collect_segments(StatusSegment *segs, int max_segments)
{
	if (!segs || max_segments <= 0 || !stext[0])
		return 0;

	const char *sep = status_separator_value();
	size_t seplen = strlen(sep);
	int sep_w = textw(sep);
	const char *cursor = stext;
	int count = 0;
	int x = 0;

	while (*cursor && count < max_segments) {
		const char *next = (seplen > 0) ? strstr(cursor, sep) : NULL;
		size_t label_len = next ? (size_t)(next - cursor) : strlen(cursor);

		if (label_len >= sizeof(segs[count].label))
			label_len = sizeof(segs[count].label) - 1;
		memcpy(segs[count].label, cursor, label_len);
		segs[count].label[label_len] = '\0';

		int label_w = textw(segs[count].label);
		segs[count].start_px = x;
		segs[count].end_px = x + MAX(0, label_w) - 1;
		x += MAX(0, label_w);
		count++;

		if (!next)
			break;
		x += MAX(0, sep_w);
		cursor = next + seplen;
	}

	return count;
}

Bool status_text_override_active(void)
{
	return status_override_active;
}

void status_set_text_override(const char *text)
{
	const char *src = text ? text : "";
	strncpy(stext, src, sizeof(stext) - 1);
	stext[sizeof(stext) - 1] = '\0';
	status_override_active = True;
	drawbars();
	ipc_notify_event("status", "source=override");
}

void status_clear_text_override(void)
{
	status_override_active = False;
	updatestatus();
	drawbars();
	ipc_notify_event("status", "source=internal");
}

int status_segment_count(void)
{
	StatusSegment segs[STATUS_MAX_SEGMENTS];
	return status_collect_segments(segs, (int)LENGTH(segs));
}

Bool status_segment_label_at(int index, char *out, size_t outsz)
{
	if (!out || outsz == 0 || index < 0)
		return False;

	out[0] = '\0';
	StatusSegment segs[STATUS_MAX_SEGMENTS];
	int n = status_collect_segments(segs, (int)LENGTH(segs));
	if (index >= n)
		return False;

	strncpy(out, segs[index].label, outsz - 1);
	out[outsz - 1] = '\0';
	return True;
}

const char *status_action_command_get(int index, unsigned int button)
{
	int slot = status_button_slot(button);
	if (slot < 0 || index < 0 || index >= STATUS_MAX_SEGMENTS)
		return NULL;
	if (!status_action_cmd[index][slot][0])
		return NULL;
	return status_action_cmd[index][slot];
}

Bool status_action_command_set(int index, unsigned int button, const char *cmd)
{
	int slot = status_button_slot(button);
	if (slot < 0 || index < 0 || index >= STATUS_MAX_SEGMENTS || !cmd)
		return False;

	char cleaned[STATUS_MAX_ACTION];
	snprintf(cleaned, sizeof(cleaned), "%s", cmd);
	status_trim_inline(cleaned);
	if (!cleaned[0])
		return False;

	snprintf(status_action_cmd[index][slot], STATUS_MAX_ACTION, "%s", cleaned);
	return True;
}

Bool status_action_command_clear(int index, unsigned int button, Bool all_buttons)
{
	if (index < 0 || index >= STATUS_MAX_SEGMENTS)
		return False;

	if (all_buttons) {
		for (int i = 0; i < 3; i++)
			status_action_cmd[index][i][0] = '\0';
		return True;
	}

	int slot = status_button_slot(button);
	if (slot < 0)
		return False;
	status_action_cmd[index][slot][0] = '\0';
	return True;
}

Bool status_dispatch_click_at(int rel_x, unsigned int button)
{
	if (rel_x < 0)
		return False;

	int slot = status_button_slot(button);
	if (slot < 0)
		return False;

	StatusSegment segs[STATUS_MAX_SEGMENTS];
	int n = status_collect_segments(segs, (int)LENGTH(segs));
	if (n <= 0)
		return False;

	int hit = -1;
	for (int i = 0; i < n; i++) {
		if (rel_x >= segs[i].start_px && rel_x <= segs[i].end_px) {
			hit = i;
			break;
		}
	}
	if (hit < 0 || hit >= STATUS_MAX_SEGMENTS)
		return False;

	const char *cmd = status_action_cmd[hit][slot];
	if (!cmd || !cmd[0])
		return False;

	const char *argv[] = {"/bin/sh", "-c", cmd, NULL};
	spawn(argv);

	char details[256];
	snprintf(details, sizeof(details), "segment=%d button=%u label=%s", hit + 1, button, segs[hit].label);
	ipc_notify_event("status_click", details);
	return True;
}

void status_fifo_setup(void)
{
	const char *fifo_path = getenv("CUPIDWM_BAR_FIFO");
	if (!fifo_path || !fifo_path[0])
		return;

	struct stat st;
	if (lstat(fifo_path, &st) < 0) {
		if (errno != ENOENT)
			return;
		if (mkfifo(fifo_path, 0600) < 0)
			return;
	}
	else if (!S_ISFIFO(st.st_mode)) {
		return;
	}

	if (status_fifo_reader_fd >= 0)
		close(status_fifo_reader_fd);
	if (status_fifo_writer_fd >= 0)
		close(status_fifo_writer_fd);

	status_fifo_reader_fd = open(fifo_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (status_fifo_reader_fd < 0)
		return;

	status_fifo_writer_fd = open(fifo_path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
}

int status_fifo_fd(void)
{
	return status_fifo_reader_fd;
}

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
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			break;
		}
		if (n == 0)
			break;

		size_t copy_n = (size_t)n;
		if (copy_n > sizeof(status_fifo_buf) - 1 - status_fifo_buf_used)
			copy_n = sizeof(status_fifo_buf) - 1 - status_fifo_buf_used;
		if (copy_n == 0) {
			status_fifo_buf_used = 0;
			continue;
		}

		memcpy(status_fifo_buf + status_fifo_buf_used, chunk, copy_n);
		status_fifo_buf_used += copy_n;
		status_fifo_buf[status_fifo_buf_used] = '\0';

		char *line_start = status_fifo_buf;
		char *nl = NULL;
		while ((nl = strchr(line_start, '\n')) != NULL) {
			*nl = '\0';
			char line[sizeof(status_fifo_buf)];
			strncpy(line, line_start, sizeof(line) - 1);
			line[sizeof(line) - 1] = '\0';
			status_trim_inline(line);

			if (line[0]) {
				if (strcmp(line, "clear") == 0 || strcmp(line, "reset") == 0)
					status_clear_text_override();
				else
					status_set_text_override(line);
			}

			line_start = nl + 1;
		}

		size_t rem = strlen(line_start);
		if (line_start != status_fifo_buf)
			memmove(status_fifo_buf, line_start, rem + 1);
		status_fifo_buf_used = rem;
	}
}

void updatestatus(void)
{
	if (status_override_active)
		return;

	char *name = NULL;
	stext[0] = '\0';

	if (user_config.status_use_root_name && XFetchName(dpy, root, &name) && name) {
		strncpy(stext, name, sizeof(stext) - 1);
		stext[sizeof(stext) - 1] = '\0';
		XFree(name);
	}

	if (stext[0] == '\0' && user_config.status_allow_external_cmd)
		run_external_status_command(stext, sizeof(stext));

	if (stext[0] == '\0' && user_config.status_enable_fallback) {
		struct statvfs vfs;
		unsigned long long avail_gb = 0;
		unsigned long long total_gb = 0;
		char diskbuf[96] = "";
		char cpubuf[64] = "";
		char rambuf[64] = "";
		char batbuf[96] = "";
		char timebuf[96] = "";
		const char *sep = user_config.status_separator ? user_config.status_separator : " ";

		const char *mount = user_config.status_disk_path ? user_config.status_disk_path : "/";
		if (user_config.status_show_disk && statvfs(mount, &vfs) == 0) {
			unsigned long long avail = (unsigned long long)vfs.f_bavail * (unsigned long long)vfs.f_frsize;
			unsigned long long total = (unsigned long long)vfs.f_blocks * (unsigned long long)vfs.f_frsize;
			avail_gb = avail / (1024ULL * 1024ULL * 1024ULL);
			total_gb = total / (1024ULL * 1024ULL * 1024ULL);
			const char *disk_label = user_config.status_disk_label ? user_config.status_disk_label : "";
			const char *disk_name = disk_label[0] ? disk_label : mount;

			if (user_config.status_show_disk_total && total_gb > 0)
				snprintf(diskbuf, sizeof(diskbuf), "%s %llu/%lluG", disk_name, avail_gb, total_gb);
			else
				snprintf(diskbuf, sizeof(diskbuf), "%s %lluG", disk_name, avail_gb);
		}

		if (user_config.status_show_cpu) {
			double cpu_percent = 0.0;
			if (read_cpu_percent(&cpu_percent)) {
				const char *cpu_label = user_config.status_cpu_label ? user_config.status_cpu_label : "CPU";
				snprintf(cpubuf, sizeof(cpubuf), "%s %.0f%%", cpu_label, cpu_percent);
			}
		}

		if (user_config.status_show_ram) {
			unsigned long long used_mb = 0;
			unsigned long long total_mb = 0;
			double used_percent = 0.0;
			if (read_ram_info(&used_mb, &total_mb, &used_percent)) {
				const char *ram_label = user_config.status_ram_label ? user_config.status_ram_label : "RAM";
				if (user_config.status_ram_show_percent || total_mb == 0)
					snprintf(rambuf, sizeof(rambuf), "%s %.0f%%", ram_label, used_percent);
				else
					snprintf(rambuf, sizeof(rambuf), "%s %.1f/%.1fG", ram_label,
					         (double)used_mb / 1024.0, (double)total_mb / 1024.0);
			}
		}

		if (user_config.status_show_battery) {
			int capacity = 0;
			char battery_state[32] = "";
			Bool have_battery = False;
			const char *battery_path = user_config.status_battery_path;
			if (battery_path && battery_path[0])
				have_battery = read_battery_info(battery_path, &capacity, battery_state, sizeof(battery_state));

			if (!have_battery) {
				static Bool autodetect_attempted = False;
				static char autodetected_battery_path[PATH_MAX] = "";
				if (!autodetect_attempted) {
					autodetect_attempted = True;
					find_battery_path(autodetected_battery_path, sizeof(autodetected_battery_path));
				}
				if (autodetected_battery_path[0])
					have_battery = read_battery_info(autodetected_battery_path, &capacity, battery_state, sizeof(battery_state));
			}

			if (have_battery) {
				const char *bat_label = user_config.status_battery_label ? user_config.status_battery_label : "BAT";
				if (user_config.status_battery_show_state && battery_state[0])
					snprintf(batbuf, sizeof(batbuf), "%s %d%% %s", bat_label, capacity, battery_state);
				else
					snprintf(batbuf, sizeof(batbuf), "%s %d%%", bat_label, capacity);
			}
		}

		time_t now = time(NULL);
		struct tm tm_now;
		if (user_config.status_show_time &&
		    localtime_r(&now, &tm_now) &&
		    user_config.status_time_format &&
		    user_config.status_time_format[0]) {
			char tval[64] = "";
			status_format_time(tval, sizeof(tval), user_config.status_time_format, &tm_now);
			if (tval[0]) {
				const char *time_label = user_config.status_time_label ? user_config.status_time_label : "";
				if (time_label[0])
					snprintf(timebuf, sizeof(timebuf), "%s %s", time_label, tval);
				else
					strncpy(timebuf, tval, sizeof(timebuf) - 1);
				timebuf[sizeof(timebuf) - 1] = '\0';
			}
		}

		const char *order = user_config.status_section_order;
		if (order && order[0]) {
			char token[32];
			size_t tlen = 0;

			for (const char *p = order;; p++) {
				char ch = *p;
				Bool end = (ch == '\0');
				Bool sepch = (ch == ',' || ch == ';' || ch == '|' || ch == '/' || ch == ':' ||
				              ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');

				if (!end && !sepch) {
					if (tlen < sizeof(token) - 1)
						token[tlen++] = ch;
				}

				if (end || sepch) {
					if (tlen > 0) {
						token[tlen] = '\0';
						status_append_named_segment(stext, sizeof(stext), sep, token,
						                           diskbuf, cpubuf, rambuf, batbuf, timebuf);
						tlen = 0;
					}
					if (end)
						break;
				}
			}
		}

		if (stext[0] == '\0') {
			status_append_segment(stext, sizeof(stext), sep, diskbuf);
			status_append_segment(stext, sizeof(stext), sep, cpubuf);
			status_append_segment(stext, sizeof(stext), sep, rambuf);
			status_append_segment(stext, sizeof(stext), sep, batbuf);
			status_append_segment(stext, sizeof(stext), sep, timebuf);
		}
	}
}

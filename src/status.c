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

static Bool read_cpu_percent(double *usage_out)
{
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
}

static Bool read_ram_info(unsigned long long *used_mb, unsigned long long *total_mb, double *used_percent)
{
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
}

static Bool read_battery_info(const char *base_path, int *capacity_out, char *status_out, size_t status_out_len)
{
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
}

static Bool find_battery_path(char *out_path, size_t out_path_len)
{
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

void updatestatus(void)
{
	char *name = NULL;
	stext[0] = '\0';

	if (user_config.status_use_root_name && XFetchName(dpy, root, &name) && name) {
		strncpy(stext, name, sizeof(stext) - 1);
		stext[sizeof(stext) - 1] = '\0';
		XFree(name);
	}

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
			strftime(tval, sizeof(tval), user_config.status_time_format, &tm_now);
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

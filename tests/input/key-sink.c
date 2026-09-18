#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int append_key(const char *path, const char *text, int len)
{
	FILE *f = fopen(path, "a");
	if (!f)
		return -1;

	if (len > 0 && fwrite(text, 1, (size_t)len, f) != (size_t)len) {
		fclose(f);
		return -1;
	}

	if (fclose(f) != 0)
		return -1;
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s MARKER TITLE\n", argv[0]);
		return 2;
	}

	Display *dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "key-sink: failed to open DISPLAY\n");
		return 1;
	}

	int screen = DefaultScreen(dpy);
	Window root = RootWindow(dpy, screen);
	Window win = XCreateSimpleWindow(
		dpy,
		root,
		120,
		120,
		320,
		180,
		1,
		BlackPixel(dpy, screen),
		WhitePixel(dpy, screen)
	);

	XStoreName(dpy, win, argv[2]);
	XClassHint class_hint = {
		.res_name = "pcmanfm",
		.res_class = "pcmanfm",
	};
	XSetClassHint(dpy, win, &class_hint);

	XSizeHints size_hints;
	memset(&size_hints, 0, sizeof(size_hints));
	size_hints.flags = USPosition | USSize;
	size_hints.x = 120;
	size_hints.y = 120;
	size_hints.width = 320;
	size_hints.height = 180;
	XSetWMNormalHints(dpy, win, &size_hints);

	XSelectInput(dpy, win, KeyPressMask | StructureNotifyMask);
	XMapWindow(dpy, win);
	XFlush(dpy);

	printf("%lu\n", (unsigned long)win);
	fflush(stdout);

	for (;;) {
		XEvent ev;
		XNextEvent(dpy, &ev);
		if (ev.type == DestroyNotify)
			break;
		if (ev.type != KeyPress)
			continue;

		char buf[64];
		KeySym sym = NoSymbol;
		int len = XLookupString(&ev.xkey, buf, (int)sizeof(buf), &sym, NULL);
		if (len > 0 && append_key(argv[1], buf, len) != 0) {
			fprintf(stderr, "key-sink: failed to append %s: %s\n", argv[1], strerror(errno));
			break;
		}
	}

	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);
	return 0;
}

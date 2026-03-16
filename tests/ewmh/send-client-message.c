#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

static long parse_long(const char *s, int *ok)
{
	char *end = NULL;
	long v = strtol(s, &end, 0);
	if (!s || !s[0] || !end || *end != '\0') {
		*ok = 0;
		return 0;
	}
	*ok = 1;
	return v;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: %s <window-id> <message-atom> [data0..data4]\n", argv[0]);
		return 1;
	}

	int ok = 0;
	unsigned long win_id = strtoul(argv[1], NULL, 0);
	if (win_id == 0) {
		fprintf(stderr, "invalid window id: %s\n", argv[1]);
		return 1;
	}

	Display *dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "failed to open X display\n");
		return 1;
	}

	Atom message_type = XInternAtom(dpy, argv[2], False);
	if (message_type == None) {
		fprintf(stderr, "failed to intern atom: %s\n", argv[2]);
		XCloseDisplay(dpy);
		return 1;
	}

	XEvent ev;
	memset(&ev, 0, sizeof(ev));
	ev.xclient.type = ClientMessage;
	ev.xclient.window = (Window)win_id;
	ev.xclient.message_type = message_type;
	ev.xclient.format = 32;

	for (int i = 0; i < 5; i++)
		ev.xclient.data.l[i] = 0;

	for (int i = 3; i < argc && i < 8; i++) {
		long v = parse_long(argv[i], &ok);
		if (!ok) {
			Atom maybe_atom = XInternAtom(dpy, argv[i], False);
			v = (long)maybe_atom;
		}
		ev.xclient.data.l[i - 3] = v;
	}

	Window root = DefaultRootWindow(dpy);
	long mask = SubstructureRedirectMask | SubstructureNotifyMask;
	Status st = XSendEvent(dpy, root, False, mask, &ev);
	XSync(dpy, False);
	XCloseDisplay(dpy);

	if (!st) {
		fprintf(stderr, "XSendEvent failed\n");
		return 1;
	}

	return 0;
}

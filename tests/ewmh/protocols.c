#define _POSIX_C_SOURCE 200809L

#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

static long long monotonic_ms(void)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return 0;
	return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static int next_event(Display *dpy, XEvent *ev, int timeout_ms)
{
	if (XPending(dpy) > 0) {
		XNextEvent(dpy, ev);
		return 1;
	}

	struct pollfd pfd = {
		.fd = ConnectionNumber(dpy),
		.events = POLLIN,
	};
	int ready = poll(&pfd, 1, timeout_ms);
	if (ready <= 0 || !(pfd.revents & POLLIN))
		return 0;

	if (XPending(dpy) == 0)
		return 0;
	XNextEvent(dpy, ev);
	return 1;
}

static void drain_events(Display *dpy, int duration_ms)
{
	long long deadline = monotonic_ms() + duration_ms;
	XEvent ev;
	while (monotonic_ms() < deadline) {
		int remaining = (int)(deadline - monotonic_ms());
		if (remaining < 0)
			remaining = 0;
		if (!next_event(dpy, &ev, remaining))
			break;
	}
}

static int wait_viewable(Display *dpy, Window win, int timeout_ms)
{
	long long deadline = monotonic_ms() + timeout_ms;
	while (monotonic_ms() < deadline) {
		XWindowAttributes wa;
		if (XGetWindowAttributes(dpy, win, &wa) && wa.map_state == IsViewable)
			return 1;

		XEvent ev;
		(void)next_event(dpy, &ev, 20);
	}
	return 0;
}

static int protocol_present(Display *dpy, Window win, Atom protocol)
{
	Atom *protocols = NULL;
	int count = 0;
	int found = 0;

	if (!XGetWMProtocols(dpy, win, &protocols, &count))
		return 0;
	for (int i = 0; i < count; i++) {
		if (protocols[i] == protocol) {
			found = 1;
			break;
		}
	}
	if (protocols)
		XFree(protocols);
	return found;
}

static int send_active_window(Display *dpy, Window root, Window win, Atom net_active_window)
{
	XEvent ev;
	memset(&ev, 0, sizeof(ev));
	ev.xclient.type = ClientMessage;
	ev.xclient.window = win;
	ev.xclient.message_type = net_active_window;
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = 1;
	ev.xclient.data.l[1] = CurrentTime;

	Status sent = XSendEvent(dpy, root, False,
				SubstructureRedirectMask | SubstructureNotifyMask, &ev);
	XFlush(dpy);
	return sent != 0;
}

static int wait_take_focus(Display *dpy, Window win, Atom wm_protocols,
			   Atom wm_take_focus, int timeout_ms, Time *timestamp_out)
{
	long long deadline = monotonic_ms() + timeout_ms;
	while (monotonic_ms() < deadline) {
		XEvent ev;
		int remaining = (int)(deadline - monotonic_ms());
		if (!next_event(dpy, &ev, remaining))
			break;
		if (ev.type == ClientMessage &&
		    ev.xclient.window == win &&
		    ev.xclient.message_type == wm_protocols &&
		    ev.xclient.format == 32 &&
		    (Atom)ev.xclient.data.l[0] == wm_take_focus) {
			if (timestamp_out)
				*timestamp_out = (Time)ev.xclient.data.l[1];
			return 1;
		}
	}
	return 0;
}

static int wait_key_press(Display *dpy, Window win, KeyCode keycode, int timeout_ms)
{
	long long deadline = monotonic_ms() + timeout_ms;
	while (monotonic_ms() < deadline) {
		XEvent ev;
		int remaining = (int)(deadline - monotonic_ms());
		if (!next_event(dpy, &ev, remaining))
			break;
		if (ev.type == KeyPress && ev.xkey.window == win && ev.xkey.keycode == keycode)
			return 1;
	}
	return 0;
}

static int count_idle_client_list_events(Display *dpy, Window root, Atom client_list,
					 Atom stacking, int duration_ms)
{
	long long deadline = monotonic_ms() + duration_ms;
	int count = 0;
	while (monotonic_ms() < deadline) {
		XEvent ev;
		int remaining = (int)(deadline - monotonic_ms());
		if (!next_event(dpy, &ev, remaining))
			break;
		if (ev.type == PropertyNotify && ev.xproperty.window == root &&
		    (ev.xproperty.atom == client_list || ev.xproperty.atom == stacking))
			count++;
	}
	return count;
}

static int count_property_events(Display *dpy, Window win, Atom property, int duration_ms)
{
	long long deadline = monotonic_ms() + duration_ms;
	int count = 0;
	while (monotonic_ms() < deadline) {
		XEvent ev;
		int remaining = (int)(deadline - monotonic_ms());
		if (!next_event(dpy, &ev, remaining))
			break;
		if (ev.type == PropertyNotify && ev.xproperty.window == win &&
		    ev.xproperty.atom == property)
			count++;
	}
	return count;
}

static void window_center(Display *dpy, Window root, Window win, int *x, int *y)
{
	XWindowAttributes wa;
	Window child = None;
	int root_x = 0;
	int root_y = 0;

	if (!XGetWindowAttributes(dpy, win, &wa) ||
	    !XTranslateCoordinates(dpy, win, root, wa.width / 2, wa.height / 2,
				   &root_x, &root_y, &child)) {
		*x = 100;
		*y = 100;
		return;
	}
	*x = root_x;
	*y = root_y;
}

static void collect_button_events(Display *dpy, Window win, int duration_ms,
				  int *presses, int *releases)
{
	long long deadline = monotonic_ms() + duration_ms;
	*presses = 0;
	*releases = 0;

	while (monotonic_ms() < deadline) {
		XEvent ev;
		int remaining = (int)(deadline - monotonic_ms());
		if (!next_event(dpy, &ev, remaining))
			break;
		if (ev.xany.window != win)
			continue;
		if (ev.type == ButtonPress && ev.xbutton.button == Button1)
			(*presses)++;
		else if (ev.type == ButtonRelease && ev.xbutton.button == Button1)
			(*releases)++;
	}
}

static int fake_click(Display *dpy, int x, int y, KeyCode modifier,
		      int *presses, int *releases, Window win)
{
	if (!XTestFakeMotionEvent(dpy, DefaultScreen(dpy), x, y, CurrentTime))
		return 0;
	XSync(dpy, False);
	drain_events(dpy, 80);

	if (modifier != 0 && !XTestFakeKeyEvent(dpy, modifier, True, CurrentTime))
		return 0;
	if (!XTestFakeButtonEvent(dpy, Button1, True, CurrentTime))
		return 0;
	if (!XTestFakeButtonEvent(dpy, Button1, False, CurrentTime))
		return 0;
	if (modifier != 0 && !XTestFakeKeyEvent(dpy, modifier, False, CurrentTime))
		return 0;
	XSync(dpy, False);

	collect_button_events(dpy, win, 300, presses, releases);
	return 1;
}

int main(void)
{
	Display *dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "protocol probe: failed to open display\n");
		return 2;
	}

	int failures = 0;
	Window root = DefaultRootWindow(dpy);
	Atom wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
	Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	Atom wm_take_focus = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	Atom net_wm_ping = XInternAtom(dpy, "_NET_WM_PING", False);
	Atom net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	Atom net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
	Atom net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	Atom net_client_list_stacking = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);

	XSelectInput(dpy, root, PropertyChangeMask);
	Window win = XCreateSimpleWindow(dpy, root, 80, 80, 420, 240, 0, 0, 0x202020);
	XStoreName(dpy, win, "cupidwm-protocol-probe");
	XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask | FocusChangeMask |
				 KeyPressMask | ButtonPressMask | ButtonReleaseMask);

	Atom protocols[] = { wm_delete, wm_take_focus, net_wm_ping };
	XSetWMProtocols(dpy, win, protocols, 3);
	XWMHints hints;
	memset(&hints, 0, sizeof(hints));
	hints.flags = InputHint;
	hints.input = False;
	XSetWMHints(dpy, win, &hints);

	XMapWindow(dpy, win);
	XFlush(dpy);
	if (!wait_viewable(dpy, win, 2000)) {
		fprintf(stderr, "protocol probe: window was not mapped by the WM\n");
		XDestroyWindow(dpy, win);
		XCloseDisplay(dpy);
		return 2;
	}

	if (!protocol_present(dpy, win, wm_delete) ||
	    !protocol_present(dpy, win, wm_take_focus) ||
	    !protocol_present(dpy, win, net_wm_ping)) {
		fprintf(stderr, "protocol probe: WM_PROTOCOLS changed while the client was managed\n");
		failures++;
	}

	Window input_focus = None;
	int revert_to = RevertToNone;
	XGetInputFocus(dpy, &input_focus, &revert_to);
	if (input_focus == win) {
		fprintf(stderr, "protocol probe: InputHint=False client was focused before WM_TAKE_FOCUS\n");
		failures++;
	}

	drain_events(dpy, 100);
	Time take_focus_time = CurrentTime;
	if (!send_active_window(dpy, root, win, net_active_window) ||
	    !wait_take_focus(dpy, win, wm_protocols, wm_take_focus, 1000, &take_focus_time)) {
		fprintf(stderr, "protocol probe: WM_TAKE_FOCUS was not delivered on activation\n");
		failures++;
	}
	else if (take_focus_time == CurrentTime) {
		fprintf(stderr, "protocol probe: WM_TAKE_FOCUS used CurrentTime instead of an event timestamp\n");
		failures++;
	}
	else {
		XSetInputFocus(dpy, win, RevertToParent, take_focus_time);
		XSync(dpy, False);
		XGetInputFocus(dpy, &input_focus, &revert_to);
		if (input_focus != win) {
			fprintf(stderr, "protocol probe: client could not take focus with WM_TAKE_FOCUS timestamp\n");
			failures++;
		}
		else {
			KeyCode a_key = XKeysymToKeycode(dpy, XK_a);
			drain_events(dpy, 40);
			if (a_key == 0 ||
			    !XTestFakeKeyEvent(dpy, a_key, True, CurrentTime) ||
			    !XTestFakeKeyEvent(dpy, a_key, False, CurrentTime)) {
				fprintf(stderr, "protocol probe: could not synthesize focused key input\n");
				failures++;
			}
			else {
				XSync(dpy, False);
				if (!wait_key_press(dpy, win, a_key, 500)) {
					fprintf(stderr, "protocol probe: focused client did not receive key input\n");
					failures++;
				}
			}
		}
	}

	drain_events(dpy, 80);
	if (!send_active_window(dpy, root, win, net_active_window)) {
		fprintf(stderr, "protocol probe: failed to repeat active-window request\n");
		failures++;
	}
	else {
		int state_events = count_property_events(dpy, win, net_wm_state, 200);
		if (state_events != 0) {
			fprintf(stderr, "protocol probe: unchanged _NET_WM_STATE was republished %d time(s)\n",
				state_events);
			failures++;
		}
	}

	int center_x = 0;
	int center_y = 0;
	window_center(dpy, root, win, &center_x, &center_y);
	int presses = 0;
	int releases = 0;
	if (!fake_click(dpy, center_x, center_y, 0, &presses, &releases, win)) {
		fprintf(stderr, "protocol probe: XTEST plain click failed\n");
		failures++;
	}
	else if (presses < 1 || releases < 1 || presses != releases) {
		fprintf(stderr, "protocol probe: plain click was not replayed as a balanced press/release (press=%d release=%d)\n",
			presses, releases);
		failures++;
	}

	KeyCode super = XKeysymToKeycode(dpy, XK_Super_L);
	if (super == 0) {
		fprintf(stderr, "protocol probe: Super_L has no keycode\n");
		failures++;
	}
	else if (!fake_click(dpy, center_x, center_y, super, &presses, &releases, win)) {
		fprintf(stderr, "protocol probe: XTEST modifier click failed\n");
		failures++;
	}
	else if (presses != 0 || releases != 0) {
		fprintf(stderr, "protocol probe: WM modifier click leaked to the client (press=%d release=%d)\n",
			presses, releases);
		failures++;
	}

	/* Clear finite map/focus traffic before measuring truly idle publication. */
	drain_events(dpy, 120);
	int list_events = count_idle_client_list_events(dpy, root, net_client_list,
						       net_client_list_stacking, 300);
	if (list_events != 0) {
		fprintf(stderr, "protocol probe: idle WM republished client-list properties %d time(s)\n",
			list_events);
		failures++;
	}

	XDestroyWindow(dpy, win);
	XSync(dpy, False);
	XCloseDisplay(dpy);

	if (failures != 0)
		return 1;
	puts("protocol probe passed");
	return 0;
}

/* Exercise client destruction and deferred WM_DELETE_WINDOW handling. */
#define main cupidwm_program_main
int cupidwm_program_main(int ac, char **av);
#include "../../src/cupidwm.c"
#undef main
#include <assert.h>

int main(void)
{
	dpy = XOpenDisplay(NULL);
	assert(dpy);
	root = DefaultRootWindow(dpy);
	setup_atoms();
	ipc_init_state_once();

	Client *c = calloc(1, sizeof(*c));
	assert(c);
	c->win = XCreateSimpleWindow(dpy, root, 0, 0, 100, 100, 0, 0, 0);
	workspaces[0] = c;
	open_windows = 1;
	scratchpads[0].client = c;
	scratchpads[0].enabled = True;
	scratchpads[1].client = c;
	scratchpads[1].enabled = False;

	/* A client can defer or refuse a close request, e.g. to save a document. */
	Atom delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, c->win, &delete_window, 1);
	focused = c;
	close_focused();
	assert(scratchpads[0].client == c && scratchpads[0].enabled);
	assert(scratchpads[1].client == c && !scratchpads[1].enabled);

	Window win = c->win;
	XDestroyWindow(dpy, win);
	XEvent ev = {0};
	ev.xdestroywindow.window = win;
	hdl_destroy_ntf(&ev);
	assert(scratchpads[0].client == NULL && !scratchpads[0].enabled);
	assert(scratchpads[1].client == NULL && !scratchpads[1].enabled);
	assert(focused == NULL && workspaces[0] == NULL && open_windows == 0);

	/* These used to dereference the freed client after an application closed. */
	session_state_save();
	remove_scratchpad(0);
	toggle_scratchpad(1);
	hdl_destroy_ntf(&ev);
	XCloseDisplay(dpy);
	puts("scratchpad lifetime tests passed");
	return 0;
}

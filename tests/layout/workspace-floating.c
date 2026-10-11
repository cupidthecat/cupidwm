/* Run against an isolated X server; no user applications are launched. */
#define main cupidwm_program_main
int cupidwm_program_main(int ac, char **av);
#include "../../src/cupidwm.c"
#undef main
#include <assert.h>

static Client *new_client(int ws)
{
	Window w = XCreateSimpleWindow(dpy, root, 10, 10, 100, 100, 0, 0, 0);
	XMapWindow(dpy, w);
	XSync(dpy, False);
	Client *c = add_client(w, ws);
	assert(c);
	return c;
}

int main(void)
{
	dpy = XOpenDisplay(NULL);
	assert(dpy);
	root = DefaultRootWindow(dpy);
	setup_atoms();
	init_defaults();
	user_config.showbar = False;
	user_config.warp_cursor = False;
	user_config.focus_follows_mouse = False;
	scr_width = DisplayWidth(dpy, DefaultScreen(dpy));
	scr_height = DisplayHeight(dpy, DefaultScreen(dpy));
	Monitor monitor = {.w = scr_width, .h = scr_height};
	mons = &monitor;
	n_mons = 1;
	sync_active_monitor_state();
	ipc_init_state_once();

	Client *first = new_client(0);
	setlayoutcmd(&(Arg){.i = LayoutFloating});
	assert(first->floating);
	change_workspace(1);
	Client *second = new_client(1);
	/* Workspace 2 still has a tiled layout and must not inherit floating. */
	assert(!second->floating);
	assert(first->floating);
	change_workspace(0);
	Client *third = new_client(0);
	assert(third->floating);
	setlayoutcmd(&(Arg){.i = LayoutTile});
	assert(!first->floating && !third->floating);
	assert(!second->floating);
	XCloseDisplay(dpy);
	puts("workspace floating tests passed");
	return 0;
}

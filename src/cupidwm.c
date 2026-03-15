/* See LICENSE for more information.
 *
 * cupidwm is a compact X11 window manager with source-based configuration.
 * It provides workspace-driven tiling, floating, monocle, scratchpads,
 * swallowing and custom layouts.
 */

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <errno.h>
#include <dirent.h>

#include <X11/keysym.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

#include <fontconfig/fontconfig.h>
#include <X11/Xft/Xft.h>

#include <X11/extensions/Xinerama.h>
#include <X11/Xcursor/Xcursor.h>

#include "defs.h"

Client *add_client(Window w, int ws);
void apply_fullscreen(Client *c, Bool on);
/* void centre_window(void); */
void change_workspace(int ws);
int check_parent(pid_t p, pid_t c);
int clean_mask(int mask);
/* void close_focused(void); */
/* void dec_gaps(void); */
Client *find_client(Window w);
Window find_toplevel(Window w);
/* void focus_next(void); */
/* void focus_prev(void); */
/* void focus_next_mon(void); */
/* void focus_prev_mon(void); */
int get_monitor_for(Client *c);
pid_t get_parent_process(pid_t c);
pid_t get_pid(Window w);
int get_workspace_for_window(Window w);
void grab_button(Mask button, Mask mod, Window w, Bool owner_events, Mask masks);
void grab_keys(void);
void hdl_button(XEvent *xev);
void hdl_button_release(XEvent *xev);
void hdl_client_msg(XEvent *xev);
void hdl_config_ntf(XEvent *xev);
void hdl_config_req(XEvent *xev);
void hdl_dummy(XEvent *xev);
void hdl_destroy_ntf(XEvent *xev);
void hdl_enter_ntf(XEvent *xev);
void hdl_keypress(XEvent *xev);
void hdl_mapping_ntf(XEvent *xev);
void hdl_map_req(XEvent *xev);
void hdl_motion(XEvent *xev);
void hdl_property_ntf(XEvent *xev);
void hdl_unmap_ntf(XEvent *xev);
/* void inc_gaps(void); */
void init_defaults(void);
Bool is_child_proc(pid_t pid1, pid_t pid2);
void move_master_next(void);
void move_master_prev(void);
void move_next_mon(void);
void move_prev_mon(void);
void move_to_workspace(int ws);
void move_win_down(void);
void move_win_left(void);
void move_win_right(void);
void move_win_up(void);
void other_wm(void);
int other_wm_err(Display *d, XErrorEvent *ee);
long parse_col(const char *hex);
void quit(void);
void reload_config(void);
void remove_scratchpad(int n);
void resize_master_add(void);
void resize_master_sub(void);
void resize_stack_add(void);
void resize_stack_sub(void);
void resize_win_down(void);
void resize_win_left(void);
void resize_win_right(void);
void resize_win_up(void);
void run(void);
void reset_opacity(Window w);
void scan_existing_windows(void);
void select_input(Window w, Mask masks);
void send_wm_take_focus(Window w);
void setup(void);
void setup_atoms(void);
void set_frame_extents(Window w);
void set_input_focus(Client *c, Bool raise_win, Bool warp);
void set_opacity(Window w, double opacity);
void set_win_scratchpad(int n);
void set_wm_state(Window w, long state);
int snap_coordinate(int pos, int size, int screen_size, int snap_dist);
void spawn(const char * const *argv);
void startup_exec(void);
void swallow_window(Client *swallower, Client *swallowed);
void swap_clients(Client *a, Client *b);
void switch_previous_workspace(void);
void tile(void);
void toggle_floating(void);
void toggle_floating_global(void);
void toggle_fullscreen(void);
void toggle_monocle(void);
void toggle_scratchpad(int n);
void unswallow_window(Client *c);
void update_borders(void);
void update_client_desktop_properties(void);
void update_modifier_masks(void);
void update_mons(void);
void update_net_client_list(void);
void update_struts(void);
void update_workarea(void);
void warp_cursor(Client *c);
Bool window_has_ewmh_state(Window w, Atom state);
void window_set_ewmh_state(Window w, Atom state, Bool add);
Bool window_should_float(Window w);
Bool window_should_start_fullscreen(Window w);
int xerr(Display *d, XErrorEvent *ee);
void xev_case(XEvent *xev);

/* config action wrappers */
void centrewindowcmd(const Arg *arg);
void closewindowcmd(const Arg *arg);
void decreasegapscmd(const Arg *arg);
void focusnextcmd(const Arg *arg);
void focusnextmoncmd(const Arg *arg);
void focusprevcmd(const Arg *arg);
void focusprevmoncmd(const Arg *arg);
void increasegapscmd(const Arg *arg);
void killclientcmd(const Arg *arg);
void masternextcmd(const Arg *arg);
void masterprevcmd(const Arg *arg);
void masterincreasecmd(const Arg *arg);
void masterdecreasecmd(const Arg *arg);
void movenextmoncmd(const Arg *arg);
void moveprevmoncmd(const Arg *arg);
void movetows(const Arg *arg);
void movewindowncmd(const Arg *arg);
void movewinleftcmd(const Arg *arg);
void movewinrightcmd(const Arg *arg);
void movewinupcmd(const Arg *arg);
void movemousecmd(const Arg *arg);
void prevworkspacecmd(const Arg *arg);
void quitcmd(const Arg *arg);
void removescratchpadcmd(const Arg *arg);
void resizewindowncmd(const Arg *arg);
void resizewinleftcmd(const Arg *arg);
void resizewinrightcmd(const Arg *arg);
void resizewinupcmd(const Arg *arg);
void resizemousecmd(const Arg *arg);
void restartwm(const Arg *arg);
void setlayoutcmd(const Arg *arg);
void spawncmd(const Arg *arg);
void stackincreasecmd(const Arg *arg);
void stackdecreasecmd(const Arg *arg);
void swapmousecmd(const Arg *arg);
void togglefloatingcmd(const Arg *arg);
void togglefloatingglobalcmd(const Arg *arg);
void togglefullscreencmd(const Arg *arg);
void togglescratchpad(const Arg *arg);
void viewws(const Arg *arg);
void createscratchpadcmd(const Arg *arg);

/* bar */
void drawbar(Monitor *m);
void drawbars(void);
void hdl_expose(XEvent *xev);
void setup_bars(void);
static int textw(const char *text);
void updatestatus(void);
Client *bar_client_at(Monitor *m, int click_x, int title_x, int title_w);
int collect_bar_clients(int mon, Client **list, int max_clients);
void monitor_workarea(int mon, int *x, int *y, int *w, int *h);
Bool get_window_title(Window w, char *buf, size_t buflen);

#include "config.h"

static Atom atoms[ATOM_COUNT];
static const char *atom_names[ATOM_COUNT] = {
	[ATOM_NET_ACTIVE_WINDOW]             = "_NET_ACTIVE_WINDOW",
	[ATOM_NET_CURRENT_DESKTOP]           = "_NET_CURRENT_DESKTOP",
	[ATOM_NET_SUPPORTED]                 = "_NET_SUPPORTED",
	[ATOM_NET_WM_STATE]                  = "_NET_WM_STATE",
	[ATOM_NET_WM_STATE_FULLSCREEN]       = "_NET_WM_STATE_FULLSCREEN",
	[ATOM_WM_STATE]                      = "WM_STATE",
	[ATOM_NET_WM_WINDOW_TYPE]            = "_NET_WM_WINDOW_TYPE",
	[ATOM_NET_WORKAREA]                  = "_NET_WORKAREA",
	[ATOM_WM_DELETE_WINDOW]              = "WM_DELETE_WINDOW",
	[ATOM_NET_WM_STRUT]                  = "_NET_WM_STRUT",
	[ATOM_NET_WM_STRUT_PARTIAL]          = "_NET_WM_STRUT_PARTIAL",
	[ATOM_NET_SUPPORTING_WM_CHECK]       = "_NET_SUPPORTING_WM_CHECK",
	[ATOM_NET_WM_NAME]                   = "_NET_WM_NAME",
	[ATOM_UTF8_STRING]                   = "UTF8_STRING",
	[ATOM_NET_WM_DESKTOP]                = "_NET_WM_DESKTOP",
	[ATOM_NET_CLIENT_LIST]               = "_NET_CLIENT_LIST",
	[ATOM_NET_FRAME_EXTENTS]             = "_NET_FRAME_EXTENTS",
	[ATOM_NET_NUMBER_OF_DESKTOPS]        = "_NET_NUMBER_OF_DESKTOPS",
	[ATOM_NET_DESKTOP_NAMES]             = "_NET_DESKTOP_NAMES",
	[ATOM_NET_WM_PID]                    = "_NET_WM_PID",
	[ATOM_NET_WM_WINDOW_TYPE_DOCK]       = "_NET_WM_WINDOW_TYPE_DOCK",
	[ATOM_NET_WM_WINDOW_TYPE_UTILITY]    = "_NET_WM_WINDOW_TYPE_UTILITY",
	[ATOM_NET_WM_WINDOW_TYPE_DIALOG]     = "_NET_WM_WINDOW_TYPE_DIALOG",
	[ATOM_NET_WM_WINDOW_TYPE_TOOLBAR]    = "_NET_WM_WINDOW_TYPE_TOOLBAR",
	[ATOM_NET_WM_WINDOW_TYPE_SPLASH]     = "_NET_WM_WINDOW_TYPE_SPLASH",
	[ATOM_NET_WM_WINDOW_TYPE_POPUP_MENU] = "_NET_WM_WINDOW_TYPE_POPUP_MENU",
	[ATOM_NET_WM_WINDOW_TYPE_MENU]       = "_NET_WM_WINDOW_TYPE_MENU",
	[ATOM_NET_WM_WINDOW_TYPE_DROPDOWN_MENU] = "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
	[ATOM_NET_WM_WINDOW_TYPE_TOOLTIP]    = "_NET_WM_WINDOW_TYPE_TOOLTIP",
	[ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION] = "_NET_WM_WINDOW_TYPE_NOTIFICATION",
	[ATOM_NET_WM_STATE_MODAL]            = "_NET_WM_STATE_MODAL",
	[ATOM_WM_PROTOCOLS]                  = "WM_PROTOCOLS",
};

Cursor cursor_normal;
Cursor cursor_move;
Cursor cursor_resize;

Client *workspaces[NUM_WORKSPACES] = {NULL};
Config user_config;
DragMode drag_mode = DRAG_NONE;
Client *drag_client = NULL;
Client *swap_target = NULL;
Client *focused = NULL;
Client *ws_focused[NUM_WORKSPACES] = {NULL};
EventHandler evtable[LASTEvent];
Display *dpy;
Window root;
Window wm_check_win;
Monitor *mons = NULL;
Scratchpad scratchpads[MAX_SCRATCHPADS];
int scratchpad_count = 0;
int current_scratchpad = 0;
int n_mons = 0;
int previous_workspace = 0;
int current_ws = 0;
int current_mon = 0;
long last_motion_time = 0;
Bool global_floating = False;
Bool in_ws_switch = False;
Bool running = False;
Bool monocle = False;

Mask numlock_mask = 0;
Mask mode_switch_mask = 0;

int scr_width;
int scr_height;
int open_windows = 0;
int drag_start_x, drag_start_y;
int drag_orig_x, drag_orig_y, drag_orig_w, drag_orig_h;

int reserve_left = 0;
int reserve_right = 0;
int reserve_top = 0;
int reserve_bottom = 0;
int current_layout = 0;
XFontStruct *bar_font = NULL;
XftFont *bar_xft_font = NULL;
GC bar_gc;
char stext[256] = "";
int saved_argc = 0;
char **saved_argv = NULL;

void monitor_workarea(int mon, int *x, int *y, int *w, int *h)
{
	if (!mons || n_mons <= 0) {
		if (x)
			*x = 0;
		if (y)
			*y = 0;
		if (w)
			*w = MAX(1, scr_width);
		if (h)
			*h = MAX(1, scr_height);
		return;
	}

	int idx = CLAMP(mon, 0, n_mons - 1);
	int wx = mons[idx].x + mons[idx].reserve_left;
	int wy = mons[idx].y + mons[idx].reserve_top;
	int ww = mons[idx].w - mons[idx].reserve_left - mons[idx].reserve_right;
	int wh = mons[idx].h - mons[idx].reserve_top - mons[idx].reserve_bottom;

	if (x)
		*x = wx;
	if (y)
		*y = wy;
	if (w)
		*w = MAX(1, ww);
	if (h)
		*h = MAX(1, wh);
}

Bool get_window_title(Window w, char *buf, size_t buflen)
{
	if (!buf || buflen == 0)
		return False;

	buf[0] = '\0';

	XTextProperty prop = {0};
	if (XGetTextProperty(dpy, w, &prop, atoms[ATOM_NET_WM_NAME]) && prop.value && prop.nitems > 0) {
		size_t n = MIN((size_t)prop.nitems, buflen - 1);
		memcpy(buf, prop.value, n);
		buf[n] = '\0';
		XFree(prop.value);
		if (buf[0] != '\0')
			return True;
	}
	else if (prop.value) {
		XFree(prop.value);
	}

	char *name = NULL;
	if (XFetchName(dpy, w, &name) && name) {
		strncpy(buf, name, buflen - 1);
		buf[buflen - 1] = '\0';
		XFree(name);
		if (buf[0] != '\0')
			return True;
	}

	XClassHint ch = {0};
	if (XGetClassHint(dpy, w, &ch)) {
		if (ch.res_class && ch.res_class[0]) {
			strncpy(buf, ch.res_class, buflen - 1);
			buf[buflen - 1] = '\0';
		}
		if (ch.res_class)
			XFree(ch.res_class);
		if (ch.res_name)
			XFree(ch.res_name);
	}

	return buf[0] != '\0';
}

Client *add_client(Window w, int ws)
{
	Client *c = malloc(sizeof(Client));
	if (!c) {
		fprintf(stderr, "cupidwm: could not alloc memory for client\n");
		return NULL;
	}

	c->win = w;
	c->next = NULL;
	c->ws = ws;
	c->pid = get_pid(w);
	c->swallowed = NULL;
	c->swallower = NULL;

	if (!workspaces[ws]) {
		workspaces[ws] = c;
	}
	else {
		if (user_config.new_win_master) {
			c->next = workspaces[ws];
			workspaces[ws] = c;
		}
		else {
			Client *tail = workspaces[ws];
			while (tail->next)
				tail = tail->next;
			tail->next = c;
		}
	}
	open_windows++;

	/* subscribing to certain events */
	Mask window_masks = EnterWindowMask | LeaveWindowMask | FocusChangeMask | PropertyChangeMask |
		                StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
	select_input(w, window_masks);
	grab_button(Button1, None, w, False, ButtonPressMask);
	grab_button(Button1, user_config.modkey, w, False, ButtonPressMask);
	grab_button(Button1, user_config.modkey | ShiftMask, w, False, ButtonPressMask);
	grab_button(Button3, user_config.modkey, w, False, ButtonPressMask);

	/* allow for more graceful exitting */
	Atom protos[] = {atoms[ATOM_WM_DELETE_WINDOW]};
	XSetWMProtocols(dpy, w, protos, 1);

	XWindowAttributes wa;
	XGetWindowAttributes(dpy, w, &wa);
	c->x = wa.x;
	c->y = wa.y;
	c->w = wa.width;
	c->h = wa.height;

	/* set monitor based on cursor location */
	Window root_ret, child_ret;
	int root_x, root_y,
		win_x, win_y;
	unsigned int masks;
	int cursor_mon = 0;

	if (XQueryPointer(dpy, root, &root_ret, &child_ret, &root_x, &root_y, &win_x, &win_y, &masks)) {
		for (int i = 0; i < n_mons; i++) {
			Bool in_mon = root_x >= mons[i].x &&
				          root_x < mons[i].x + mons[i].w &&
				          root_y >= mons[i].y &&
			              root_y < mons[i].y + mons[i].h;
			if (in_mon) {
				cursor_mon = i;
				break;
			}
		}
	}

	/* set client defaults */
	c->mon = cursor_mon;
	c->fixed = False;
	c->floating = False;
	c->prev_floating = False;
	c->floating_saved = False;
	c->fullscreen = False;
	c->mapped = True;
	c->ignore_unmap_events = 0;
	c->custom_stack_height = 0;

	if (global_floating)
		c->floating = True;

	/* remember first created client per workspace as a fallback */
	if (!ws_focused[ws])
		ws_focused[ws] = c;

	if (ws == current_ws && !focused) {
		focused = c;
		current_mon = c->mon;
	}

	/* associate client with workspace n */
	long desktop = ws;
	XChangeProperty(dpy, w, atoms[ATOM_NET_WM_DESKTOP], XA_CARDINAL, 32,
			        PropModeReplace, (unsigned char *)&desktop, 1);
	XRaiseWindow(dpy, w);
	return c;
}

void apply_fullscreen(Client *c, Bool on)
{
	if (!c || !c->mapped || c->fullscreen == on)
		return;


	if (on) {
		XWindowAttributes win_attr;
		if (!XGetWindowAttributes(dpy, c->win, &win_attr))
			return;

		c->orig_x = win_attr.x;
		c->orig_y = win_attr.y;
		c->orig_w = win_attr.width;
		c->orig_h = win_attr.height;

		c->fullscreen = True;

		update_struts();

		int mon = CLAMP(c->mon, 0, n_mons - 1);
		int wx, wy, ww, wh;
		monitor_workarea(mon, &wx, &wy, &ww, &wh);

		/* make window fill monitor work area */
		XSetWindowBorderWidth(dpy, c->win, 0);
		XMoveResizeWindow(dpy, c->win, wx, wy, (unsigned int)ww, (unsigned int)wh);

		c->x = wx;
		c->y = wy;
		c->w = ww;
		c->h = wh;

		XRaiseWindow(dpy, c->win);
		window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_FULLSCREEN], True);
	}
	else {
		c->fullscreen = False;

		/* restore win attributes */
		XMoveResizeWindow(dpy, c->win, c->orig_x, c->orig_y, c->orig_w, c->orig_h);
		XSetWindowBorderWidth(dpy, c->win, user_config.border_width);
		window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_FULLSCREEN], False);

		c->x = c->orig_x;
		c->y = c->orig_y;
		c->w = c->orig_w;
		c->h = c->orig_h;

		if (!c->floating)
			c->mon = get_monitor_for(c);
		tile();
		update_borders();
	}
}

void centre_window(void)
{
	if (!focused || !focused->mapped || !focused->floating)
		return;

	focused->mon = CLAMP(get_monitor_for(focused), 0, n_mons - 1);
	int x = mons[focused->mon].x + (mons[focused->mon].w - focused->w) / 2;
	int y = mons[focused->mon].y + (mons[focused->mon].h - focused->h) / 2;
	x -= user_config.border_width;
	y -= user_config.border_width;

	focused->x = x;
	focused->y = y;
	XMoveWindow(dpy, focused->win, x, y);
}

static Bool is_scratchpad_client(const Client *c)
{
	if (!c)
		return False;

	for (int i = 0; i < MAX_SCRATCHPADS; i++) {
		if (scratchpads[i].client == c)
			return True;
	}

	return False;
}

void change_workspace(int ws)
{
	if (ws < 0 || ws >= NUM_WORKSPACES || ws == current_ws)
		return;

	/* remember last focus for workspace we are leaving */
	ws_focused[current_ws] = focused;

	in_ws_switch = True;
	XGrabServer(dpy); /* freeze rendering for tearless switching */

	/* scratchpads stay visible */
	Bool visible_scratchpads[MAX_SCRATCHPADS] = {False};
	for (int i = 0; i < MAX_SCRATCHPADS; i++) {
		if (scratchpads[i].client && scratchpads[i].enabled) {
			visible_scratchpads[i] = True;
			scratchpads[i].client->ignore_unmap_events += 2;
			XUnmapWindow(dpy, scratchpads[i].client->win);
			scratchpads[i].client->mapped = False;
		}
	}

	previous_workspace = current_ws;
	current_ws = ws;
	for (Client *c = workspaces[current_ws]; c; c = c->next) {
		if (c->mapped) {
			if (!is_scratchpad_client(c))
				XMapWindow(dpy, c->win);
		}
	}

	/* move visible scratchpads to new workspace and map them */
	for (int i = 0; i < MAX_SCRATCHPADS; i++) {
		if (visible_scratchpads[i] && scratchpads[i].client) {
			Client *c = scratchpads[i].client;

			/* remove from old workspace */
			Client **pp = &workspaces[c->ws];
			while (*pp && *pp != c)
				pp = &(*pp)->next;

			if (*pp)
				*pp = c->next;

			/* add to new workspace */
			c->next = workspaces[current_ws];
			workspaces[current_ws] = c;
			c->ws = current_ws;

			XMapWindow(dpy, c->win);
			c->mapped = True;
			XRaiseWindow(dpy, c->win);

			/* Update desktop property */
			long desktop = current_ws;
			XChangeProperty(dpy, c->win, atoms[ATOM_NET_WM_DESKTOP], XA_CARDINAL, 32,
					        PropModeReplace, (unsigned char *)&desktop, 1);
		}
	}

	tile();

	/* restore last focused client for this workspace */
	focused = ws_focused[current_ws];

	if (focused) {
		Client *found = NULL;
		for (Client *c = workspaces[current_ws]; c; c = c->next) {
			if (c == focused && c->mapped) {
				found = c;
				break;
			}
		}
		if (!found)
			focused = NULL;
	}

	/* fallback: choose a mapped client on current_ws, preferring current_mon */
	if (!focused && workspaces[current_ws]) {
		for (Client *c = workspaces[current_ws]; c; c = c->next) {
			if (!c->mapped)
				continue;
			if (c->mon == current_mon) {
				focused = c;
				break;
			}
			if (!focused)
				focused = c;
		}
		if (focused)
			current_mon = CLAMP(focused->mon, 0, n_mons - 1);
	}

	/* try focus focus scratchpad if no other window available */
	for (int i = 0; i < MAX_SCRATCHPADS; i++) {
		if (!focused && visible_scratchpads[i] && scratchpads[i].client) {
			focused = scratchpads[i].client;
			break;
		}
	}

	set_input_focus(focused, False, True);

	long current_desktop = current_ws;
	XChangeProperty(dpy, root, atoms[ATOM_NET_CURRENT_DESKTOP], XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&current_desktop, 1);
	update_client_desktop_properties();

	XUngrabServer(dpy);
	XSync(dpy, False);
	in_ws_switch = False;
}

int check_parent(pid_t p, pid_t c)
{
	while (p != c && c != 0) /* walk proc tree until parent found */
		c = get_parent_process(c);

	return (int)c;
}

int clean_mask(int mask)
{
	return mask & ~(LockMask | numlock_mask | mode_switch_mask);
}

void close_focused(void)
{
	if (!focused)
		return;

	for (int i = 0; i < MAX_SCRATCHPADS; i++) {
		if (scratchpads[i].client == focused) {
			scratchpads[i].client = NULL;
			scratchpads[i].enabled = False;
			break;
		}
	}

	Atom *protocols;
	int n_protocols;
	/* get number of protocols a window possesses and check if any == WM_DELETE_WINDOW (supports it) */
	if (XGetWMProtocols(dpy, focused->win, &protocols, &n_protocols) && protocols) {
		for (int i = 0; i < n_protocols; i++) {
			if (protocols[i] == atoms[ATOM_WM_DELETE_WINDOW]) {
				XEvent ev = {.xclient = {
					.type = ClientMessage,
					.window = focused->win,
					.message_type = atoms[ATOM_WM_PROTOCOLS],
					.format = 32}};

				ev.xclient.data.l[0] = atoms[ATOM_WM_DELETE_WINDOW];
				ev.xclient.data.l[1] = CurrentTime;
				XSendEvent(dpy, focused->win, False, NoEventMask, &ev);
				XFree(protocols);
				return;
			}
		}
		XUnmapWindow(dpy, focused->win);
		XFree(protocols);
	}
	XUnmapWindow(dpy, focused->win);
	XKillClient(dpy, focused->win);
}

void dec_gaps(void)
{
	if (user_config.gaps > 0) {
		user_config.gaps--;
		tile();
		update_borders();
	}
}

Client *find_client(Window w)
{
	for (int ws = 0; ws < NUM_WORKSPACES; ws++)
		for (Client *c = workspaces[ws]; c; c = c->next)
			if (c->win == w)
				return c;

	return NULL;
}

Window find_toplevel(Window w)
{
	if (!w || w == None)
		return root;

	Window root_win = None;
	Window parent;
	Window *kids;
	unsigned n_kids;

	while (True) {
		if (w == root_win)
			break;
		if (XQueryTree(dpy, w, &root_win, &parent, &kids, &n_kids) == 0)
			break;
		if (kids)
			XFree(kids);
		if (parent == root_win || parent == None)
			break;
		w = parent;
	}
	return w;
}

void focus_next(void)
{
	if (!workspaces[current_ws])
		return;

	Client *start = focused ? focused : workspaces[current_ws];
	Client *c = start;

	/* loop until we find a mapped client or return to start */
	do
		c = c->next ? c->next : workspaces[current_ws];
	while (( !c->mapped || c->mon != current_mon ) && c != start);

	/* if we return to start: */
	if (!c->mapped || c->mon != current_mon)
		return;

	focused = c;
	current_mon = c->mon;
	set_input_focus(focused, True, True);
}

void focus_prev(void)
{
	if (!workspaces[current_ws])
		return;

	Client *start = focused ? focused : workspaces[current_ws];
	Client *c = start;

	/* loop until we find a mapped client or return to starting point */
	do {
		Client *p = workspaces[current_ws];
		Client *prev = NULL;
		while (p && p != c) {
			prev = p;
			p = p->next;
		}

		if (prev) {
			c = prev;
		}
		else {
			/* wrap to tail */
			p = workspaces[current_ws];
			while (p->next)
				p = p->next;
			c = p;
		}
	} while (( !c->mapped || c->mon != current_mon ) && c != start);

	/* this stops invisible windows being detected or focused */
	if (!c->mapped || c->mon != current_mon)
		return;

	focused = c;
	current_mon = c->mon;
	set_input_focus(focused, True, True);
}

void focus_next_mon(void)
{
	if (n_mons <= 1)
		return;

	int target_mon = (current_mon + 1) % n_mons;
	/* find the first window on the target monitor in current workspace */
	Client *target_client = NULL;
	for (Client *c = workspaces[current_ws]; c; c = c->next) {
		if (c->mon == target_mon && c->mapped) {
			target_client = c;
			break;
		}
	}

	if (target_client) {
		/* focus the window on target monitor */
		focused = target_client;
		current_mon = target_mon;
		set_input_focus(focused, True, True);
	}
	else {
		/* no windows on target monitor, just move cursor to center and update current_mon */
		current_mon = target_mon;
		int center_x = mons[target_mon].x + mons[target_mon].w / 2;
		int center_y = mons[target_mon].y + mons[target_mon].h / 2;
		XWarpPointer(dpy, None, root, 0, 0, 0, 0, center_x, center_y);
		XSync(dpy, False);
	}
}

void focus_prev_mon(void)
{
	if (n_mons <= 1)
		return; /* only one monitor, nothing to switch to */

	int target_mon = (current_mon - 1 + n_mons) % n_mons;
	/* find the first window on the target monitor in current workspace */
	Client *target_client = NULL;
	for (Client *c = workspaces[current_ws]; c; c = c->next) {
		if (c->mon == target_mon && c->mapped) {
			target_client = c;
			break;
		}
	}

	if (target_client) {
		/* focus the window on target monitor */
		focused = target_client;
		current_mon = target_mon;
		set_input_focus(focused, True, True);
	}
	else {
		current_mon = target_mon;
		int center_x = mons[target_mon].x + mons[target_mon].w / 2;
		int center_y = mons[target_mon].y + mons[target_mon].h / 2;
		XWarpPointer(dpy, None, root, 0, 0, 0, 0, center_x, center_y);
		XSync(dpy, False);
	}
}

int get_monitor_for(Client *c)
{
	if (!mons || n_mons <= 0 || !c)
		return 0;

	int cx = c->x + c->w / 2;
	int cy = c->y + c->h / 2;
	for (int i = 0; i < n_mons; i++) {
		Bool in_mon_bounds =
			cx >= mons[i].x &&
			cx < mons[i].x + mons[i].w &&
			cy >= (int)mons[i].y &&
			cy < mons[i].y + mons[i].h;

		if (in_mon_bounds)
			return i;
	}
	return 0;
}

pid_t get_parent_process(pid_t c)
{
	pid_t v = -1;
	FILE *f;
	char buf[256];

	snprintf(buf, sizeof(buf), "/proc/%u/stat", (unsigned)c);
	if (!(f = fopen(buf, "r")))
		return 0;

	int no_error = fscanf(f, "%*u %*s %*c %d", &v);
	(void)no_error;
	fclose(f);
	return (pid_t)v;
}

pid_t get_pid(Window w)
{
	pid_t pid = 0;
	Atom actual_type;
	int actual_format;
	unsigned long n_items, bytes_after;
	unsigned char *prop = NULL;

	if (XGetWindowProperty(dpy, w, atoms[ATOM_NET_WM_PID], 0, 1, False, XA_CARDINAL, &actual_type,
				           &actual_format, &n_items, &bytes_after, &prop) == Success && prop) {
		if (actual_type == XA_CARDINAL && actual_format == 32 && n_items == 1) {
			unsigned long raw_pid = *(unsigned long *)prop;
			pid = (pid_t)raw_pid;
		}
		XFree(prop);
	}
	return pid;
}

static Bool rule_matches_window(const Rule *r, const XClassHint *ch, const char *title)
{
	Bool cls_match = True;
	Bool inst_match = True;
	Bool title_match = True;

	if (r->class)
		cls_match = (ch->res_class && strcasecmp(ch->res_class, r->class) == 0);
	if (r->instance)
		inst_match = (ch->res_name && strcasecmp(ch->res_name, r->instance) == 0);
	if (r->title)
		title_match = (title && strcasecmp(title, r->title) == 0);

	return cls_match && inst_match && title_match;
}

static Bool str_contains_ci(const char *haystack, const char *needle)
{
	if (!haystack || !needle || !needle[0])
		return False;

	size_t nlen = strlen(needle);
	for (const char *p = haystack; *p; p++) {
		if (strncasecmp(p, needle, nlen) == 0)
			return True;
	}

	return False;
}

static Bool class_hint_looks_terminal(const char *s)
{
	if (!s || !s[0])
		return False;

	return str_contains_ci(s, "terminal") ||
	       str_contains_ci(s, "term") ||
	       str_contains_ci(s, "tty") ||
	       str_contains_ci(s, "console") ||
	       str_contains_ci(s, "vt");
}

static Bool fd_path_is_pty(const char *link)
{
	if (!link || !link[0])
		return False;

	return strncmp(link, "/dev/pts/", 9) == 0 ||
	       strncmp(link, "/dev/tty", 8) == 0 ||
	       strcmp(link, "/dev/ptmx") == 0;
}

static Bool process_has_pty_fd(pid_t pid)
{
	if (pid <= 0)
		return False;

	char path[PATH_MAX] = {0};
	snprintf(path, sizeof(path), "/proc/%d/fd", pid);

	DIR *dir = opendir(path);
	if (!dir)
		return False;

	Bool has_pty = False;
	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.')
			continue;

		char fdpath[PATH_MAX] = {0};
		char link[PATH_MAX] = {0};
		snprintf(fdpath, sizeof(fdpath), "%s/%s", path, ent->d_name);
		ssize_t n = readlink(fdpath, link, sizeof(link) - 1);
		if (n <= 0)
			continue;

		link[n] = '\0';
		if (fd_path_is_pty(link)) {
			has_pty = True;
			break;
		}
	}

	closedir(dir);
	return has_pty;
}

static Bool process_tree_has_pty(pid_t root_pid)
{
	if (root_pid <= 0)
		return False;

	DIR *proc = opendir("/proc");
	if (!proc)
		return False;

	Bool has_pty = False;
	struct dirent *ent;
	while ((ent = readdir(proc)) != NULL) {
		if (ent->d_name[0] < '0' || ent->d_name[0] > '9')
			continue;

		char *end = NULL;
		long v = strtol(ent->d_name, &end, 10);
		if (!end || *end != '\0')
			continue;
		if (v <= 0)
			continue;

		pid_t pid = (pid_t)v;
		if (pid == root_pid)
			continue;
		if (!is_child_proc(root_pid, pid))
			continue;
		if (!process_has_pty_fd(pid))
			continue;

		has_pty = True;
		break;
	}

	closedir(proc);
	return has_pty;
}

static Bool pid_is_terminal_process(pid_t pid)
{
	if (pid <= 0)
		return False;

	if (process_has_pty_fd(pid) || process_tree_has_pty(pid))
		return True;

	char path[PATH_MAX] = {0};
	char comm[256] = {0};

	snprintf(path, sizeof(path), "/proc/%d/comm", pid);
	FILE *f = fopen(path, "r");
	if (f) {
		if (fgets(comm, sizeof(comm), f)) {
			size_t n = strlen(comm);
			if (n > 0 && comm[n - 1] == '\n')
				comm[n - 1] = '\0';
		}
		fclose(f);
	}

	if (class_hint_looks_terminal(comm))
		return True;

	return False;
}

static const Rule *rule_for_window(Window w)
{
	XClassHint ch = {0};
	char *title = NULL;
	const Rule *ret = NULL;

	if (!XGetClassHint(dpy, w, &ch))
		return NULL;

	XFetchName(dpy, w, &title);

	for (size_t i = 0; i < LENGTH(rules); i++) {
		if (rule_matches_window(&rules[i], &ch, title)) {
			ret = &rules[i];
			break;
		}
	}

	if (title)
		XFree(title);
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	return ret;
}

static Bool window_is_terminal(Window w)
{
	XClassHint ch = {0};
	Bool ret = False;

	if (!XGetClassHint(dpy, w, &ch))
		return False;

	if (class_hint_looks_terminal(ch.res_class) || class_hint_looks_terminal(ch.res_name))
		ret = True;

	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);

	if (!ret)
		ret = pid_is_terminal_process(get_pid(w));

	return ret;
}

static Bool window_can_swallow(Window w)
{
	const Rule *r = rule_for_window(w);
	if (r)
		return r->can_swallow;
	return window_is_terminal(w);
}

static Bool window_can_be_swallowed(Window w)
{
	const Rule *r = rule_for_window(w);
	return r && r->can_be_swallowed;
}

int get_workspace_for_window(Window w)
{
	const Rule *r = rule_for_window(w);
	if (r && r->workspace >= 0 && r->workspace < NUM_WORKSPACES)
		return r->workspace;
	return current_ws;
}

void grab_button(Mask button, Mask mod, Window w, Bool owner_events, Mask masks)
{
	if (w == root) /* grabbing for wm */
		XGrabButton(dpy, button, mod, w, owner_events, masks, GrabModeAsync, GrabModeAsync, None, None);
	else /* grabbing for windows */
		XGrabButton(dpy, button, mod, w, owner_events, masks, GrabModeSync, GrabModeAsync, None, None);
}

void grab_keys(void)
{
	Mask guards[] = {
		0, LockMask, numlock_mask, LockMask | numlock_mask, mode_switch_mask,
		LockMask | mode_switch_mask, numlock_mask | mode_switch_mask,
		LockMask | numlock_mask | mode_switch_mask
	};
	XUngrabKey(dpy, AnyKey, AnyModifier, root);

	for (size_t i = 0; i < LENGTH(keys); i++) {
		KeyCode keycode = XKeysymToKeycode(dpy, keys[i].keysym);
		if (!keycode)
			continue;

		for (size_t guard = 0; guard < sizeof(guards) / sizeof(guards[0]); guard++) {
			XGrabKey(
				dpy, keycode, keys[i].mod | guards[guard], root, True,
				GrabModeAsync, GrabModeAsync
			);
		}
	}
}

#include "status.c"
#include "bar.c"

#include "input.c"

void inc_gaps(void)
{
	user_config.gaps++;
	tile();
	update_borders();
}

void init_defaults(void)
{
	user_config.modkey = MODKEY;
	user_config.gaps = default_gaps;
	user_config.border_width = borderpx;
	user_config.border_foc_col = parse_col(col_border_focused);
	user_config.border_ufoc_col = parse_col(col_border_unfocused);
	user_config.border_swap_col = parse_col(col_border_swap);
	user_config.bar_bg_col = parse_col(col_bar_bg);
	user_config.bar_fg_col = parse_col(col_bar_fg);
	user_config.bar_sel_bg_col = parse_col(col_bar_sel_bg);
	user_config.bar_sel_fg_col = parse_col(col_bar_sel_fg);
	user_config.move_window_amt = move_window_amount;
	user_config.resize_window_amt = resize_window_amount;
	user_config.showbar = showbar ? True : False;
	user_config.topbar = topbar ? True : False;
	user_config.bar_height = barheight;
	user_config.bar_show_tabs = bar_show_tabs ? True : False;
	user_config.bar_click_focus_tabs = bar_click_focus_tabs ? True : False;
	user_config.bar_show_title_fallback = bar_show_title_fallback ? True : False;
	user_config.status_interval_sec = status_interval_sec;
	user_config.status_use_root_name = status_use_root_name ? True : False;
	user_config.status_enable_fallback = status_enable_fallback ? True : False;
	user_config.status_show_disk = status_show_disk ? True : False;
	user_config.status_show_disk_total = status_show_disk_total ? True : False;
	user_config.status_show_cpu = status_show_cpu ? True : False;
	user_config.status_show_ram = status_show_ram ? True : False;
	user_config.status_show_battery = status_show_battery ? True : False;
	user_config.status_show_time = status_show_time ? True : False;
	user_config.status_ram_show_percent = status_ram_show_percent ? True : False;
	user_config.status_battery_show_state = status_battery_show_state ? True : False;
	user_config.status_disk_path = status_disk_path;
	user_config.status_battery_path = status_battery_path;
	user_config.status_disk_label = status_disk_label;
	user_config.status_cpu_label = status_cpu_label;
	user_config.status_ram_label = status_ram_label;
	user_config.status_battery_label = status_battery_label;
	user_config.status_time_label = status_time_label;
	user_config.status_section_order = status_section_order;
	user_config.status_time_format = status_time_format;
	user_config.status_separator = status_separator;

	for (int i = 0; i < MAX_MONITORS; i++)
		user_config.master_width[i] = master_width_default;

	for (int i = 0; i < MAX_ITEMS; i++) {
		user_config.can_be_swallowed[i] = NULL;
		user_config.can_swallow[i] = NULL;
		user_config.open_in_workspace[i] = NULL;
		user_config.start_fullscreen[i] = NULL;
		user_config.should_float[i] = NULL;
		user_config.to_run[i] = NULL;
		user_config.binds[i].keysym = 0;
		user_config.binds[i].mods = 0;
		user_config.binds[i].keycode = 0;
		user_config.binds[i].action.fn = NULL;
		user_config.binds[i].type = -1;
	}

	user_config.motion_throttle = motion_throttle;
	user_config.resize_master_amt = resize_master_amount;
	user_config.resize_stack_amt = resize_stack_amount;
	user_config.snap_distance = snap;
	user_config.n_binds = 0;
	user_config.new_win_focus = new_window_focus ? True : False;
	user_config.focus_follows_mouse = focus_follows_mouse ? True : False;
	user_config.warp_cursor = warp_cursor_on ? True : False;
	user_config.new_win_master = new_window_master ? True : False;
	user_config.floating_on_top = floating_on_top ? True : False;
}

Bool is_child_proc(pid_t parent_pid, pid_t child_pid)
{
	if (parent_pid <= 0 || child_pid <= 0)
		return False;

	char path[PATH_MAX];
	FILE *f;
	pid_t current_pid = child_pid;
	int max_iterations = 20;

	while (current_pid > 1 && max_iterations-- > 0) {
		snprintf(path, sizeof(path), "/proc/%d/stat", current_pid);
		f = fopen(path, "r");
		if (!f) {
			fprintf(stderr, "cupidwm: could not open %s\n", path);
			return False;
		}

		int ppid = 0;
		if (fscanf(f, "%*d %*s %*c %d", &ppid) != 1) {
			fprintf(stderr, "cupidwm: failed to read ppid from %s\n", path);
			fclose(f);
			return False;
		}
		fclose(f);

		if (ppid == parent_pid)
			return True;

		if (ppid <= 1) {
			/* Reached init or kernel */
			fprintf(stderr, "cupidwm: reached init/kernel, no relationship found\n");
			break;
		}
		current_pid = ppid;
	}
	return False;
}

void move_master_next(void)
{
	if (!workspaces[current_ws] || !workspaces[current_ws]->next)
		return;

	Client *first = workspaces[current_ws];
	Client *old_focused = focused;

	workspaces[current_ws] = first->next;
	first->next = NULL;

	Client *tail = workspaces[current_ws];
	while (tail->next)
		tail = tail->next;
	
	tail->next = first;

	tile();

	if (user_config.warp_cursor && old_focused)
		warp_cursor(old_focused);

	if (old_focused)
		send_wm_take_focus(old_focused->win);

	update_borders();
}

void move_master_prev(void)
{
	if (!workspaces[current_ws] || !workspaces[current_ws]->next)
		return;

	Client *prev = NULL;
	Client *cur = workspaces[current_ws];
	Client *old_focused = focused;

	while (cur->next) {
		prev = cur;
		cur = cur->next;
	}

	if (prev)
		prev->next = NULL;

	cur->next = workspaces[current_ws];
	workspaces[current_ws] = cur;

	tile();
	if (user_config.warp_cursor && old_focused)
		warp_cursor(old_focused);
	if (old_focused)
		send_wm_take_focus(old_focused->win);

	update_borders();
}

void move_next_mon(void)
{
	if (!focused || n_mons <= 1)
		return; /* no focused window or only one monitor */

	int target_mon = (focused->mon + 1) % n_mons;

	/* update window's monitor assignment */
	focused->mon = target_mon;
	current_mon = target_mon;

	/* if window is floating, center it on the target monitor */
	if (focused->floating) {
		int mx = mons[target_mon].x, my = mons[target_mon].y;
		int mw = mons[target_mon].w, mh = mons[target_mon].h;
		int x = mx + (mw - focused->w) / 2;
		int y = my + (mh - focused->h) / 2;

		/* ensure window stays within monitor bounds */
		if (x < mx)
			x = mx;
		if (y < my)
			y = my;
		if (x + focused->w > mx + mw)
			x = mx + mw - focused->w;
		if (y + focused->h > my + mh)
			y = my + mh - focused->h;

		focused->x = x;
		focused->y = y;
		XMoveWindow(dpy, focused->win, x, y);
	}

	/* retile to update layouts on both monitors */
	tile();

	/* follow the window with cursor if enabled */
	if (user_config.warp_cursor)
		warp_cursor(focused);

	update_borders();
}

void move_prev_mon(void)
{
	if (!focused || n_mons <= 1)
		return; /* no focused window or only one monitor */

	int target_mon = (focused->mon - 1 + n_mons) % n_mons;

	/* update window's monitor assignment */
	focused->mon = target_mon;
	current_mon = target_mon;

	/* if window is floating, center it on the target monitor */
	if (focused->floating) {
		int mx = mons[target_mon].x, my = mons[target_mon].y;
		int mw = mons[target_mon].w, mh = mons[target_mon].h;
		int x = mx + (mw - focused->w) / 2;
		int y = my + (mh - focused->h) / 2;

		/* ensure window stays within monitor bounds */
		if (x < mx)
			x = mx;
		if (y < my)
			y = my;
		if (x + focused->w > mx + mw)
			x = mx + mw - focused->w;
		if (y + focused->h > my + mh)
			y = my + mh - focused->h;

		focused->x = x;
		focused->y = y;
		XMoveWindow(dpy, focused->win, x, y);
	}

	/* retile to update layouts on both monitors */
	tile();

	/* follow the window with cursor if enabled */
	if (user_config.warp_cursor)
		warp_cursor(focused);

	update_borders();
}

void move_to_workspace(int ws)
{
	if (ws < 0 || ws >= NUM_WORKSPACES || ws == current_ws)
		return;

	Client *moved = focused;
	if (!moved || moved->ws != current_ws || !moved->mapped) {
		Client *candidate = ws_focused[current_ws];
		if (candidate && candidate->ws == current_ws && candidate->mapped)
			moved = candidate;
		else
			moved = NULL;

		if (!moved) {
			for (Client *c = workspaces[current_ws]; c; c = c->next) {
				if (!c->mapped)
					continue;
				if (c->mon == current_mon) {
					moved = c;
					break;
				}
				if (!moved)
					moved = c;
			}
		}
	}

	if (!moved)
		return;

	int from_ws = current_ws;

	moved->ignore_unmap_events += 2;
	XUnmapWindow(dpy, moved->win);
	moved->mapped = True;

	/* remove from current list */
	Client **pp = &workspaces[from_ws];
	while (*pp && *pp != moved)
		pp = &(*pp)->next;

	if (*pp)
		*pp = moved->next;

	/* push to target list */
	moved->next = workspaces[ws];
	workspaces[ws] = moved;
	moved->ws = ws;
	long desktop = ws;
	XChangeProperty(dpy, moved->win, atoms[ATOM_NET_WM_DESKTOP], XA_CARDINAL, 32,
		        PropModeReplace, (unsigned char *)&desktop, 1);

	/* remember it as last-focused for the target workspace */
	ws_focused[ws] = moved;

	/* retile current workspace and pick a new focus there */
	tile();
	focused = workspaces[from_ws];
	if (focused)
		set_input_focus(focused, False, False);
	else
		set_input_focus(NULL, False, False);
}

void move_win_down(void)
{
	if (!focused || !focused->floating)
		return;

	focused->y += user_config.move_window_amt;
	XMoveWindow(dpy, focused->win, focused->x, focused->y);
}

void move_win_left(void)
{
	if (!focused || !focused->floating)
		return;

	focused->x -= user_config.move_window_amt;
	XMoveWindow(dpy, focused->win, focused->x, focused->y);
}

void move_win_right(void)
{
	if (!focused || !focused->floating)
		return;
	focused->x += user_config.move_window_amt;
	XMoveWindow(dpy, focused->win, focused->x, focused->y);
}

void move_win_up(void)
{
	if (!focused || !focused->floating)
		return;

	focused->y -= user_config.move_window_amt;
	XMoveWindow(dpy, focused->win, focused->x, focused->y);
}

void other_wm(void)
{
	XSetErrorHandler(other_wm_err);
	XChangeWindowAttributes(dpy, root, CWEventMask, &(XSetWindowAttributes){.event_mask = SubstructureRedirectMask});
	XSync(dpy, False);
	XSetErrorHandler(xerr);
	XChangeWindowAttributes(dpy, root, CWEventMask, &(XSetWindowAttributes){.event_mask = 0});
	XSync(dpy, False);
}

int other_wm_err(Display *d, XErrorEvent *ee)
{
	fprintf(stderr, "can't start because another window manager is already running");
	exit(EXIT_FAILURE);
	return 0;
	(void)d;
	(void)ee;
}

long parse_col(const char *hex)
{
	XColor col;
	Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));

	if (!XParseColor(dpy, cmap, hex, &col)) {
		fprintf(stderr, "cupidwm: cannot parse color %s\n", hex);
		return WhitePixel(dpy, DefaultScreen(dpy));
	}

	if (!XAllocColor(dpy, cmap, &col)) {
		fprintf(stderr, "cupidwm: cannot allocate color %s\n", hex);
		return WhitePixel(dpy, DefaultScreen(dpy));
	}

	/* possibly unsafe BUT i dont think it can cause any problems.
	 * used to make sure borders are opaque with compositor like picom */
	return ((long)col.pixel) | (0xffL << 24);
}

void quit(void)
{
	/* Kill all clients on exit...

	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		for (Client *c = workspaces[ws]; c; c = c->next) {
			XUnmapWindow(dpy, c->win);
			XKillClient(dpy, c->win);
		}
	}
	*/

	XSync(dpy, False);
	XFreeCursor(dpy, cursor_move);
	XFreeCursor(dpy, cursor_normal);
	XFreeCursor(dpy, cursor_resize);
	XCloseDisplay(dpy);
	puts("quitting...");
	running = False;
}

void reload_config(void)
{
	restartwm(NULL);
}

void remove_scratchpad(int n)
{
	if (n < 0 || n >= MAX_SCRATCHPADS || scratchpads[n].client == NULL)
		return;

	Client *c = scratchpads[n].client;

	if (c->win) {
		XMapWindow(dpy, c->win);
		c->mapped = True;
	}

	scratchpads[n].client = NULL;
	scratchpads[n].enabled = False;

	update_net_client_list();
	update_borders();
}

void resize_master_add(void)
{
	/* pick the monitor of the focused window (or 0 if none) */
	int m = focused ? focused->mon : 0;
	m = CLAMP(m, 0, MAX_MONITORS - 1);
	float *mw = &user_config.master_width[m];

	if (*mw < MF_MAX - 0.001f)
		*mw += ((float)user_config.resize_master_amt / 100);

	tile();
	update_borders();
}

void resize_master_sub(void)
{
	/* pick the monitor of the focused window (or 0 if none) */
	int m = focused ? focused->mon : 0;
	m = CLAMP(m, 0, MAX_MONITORS - 1);
	float *mw = &user_config.master_width[m];

	if (*mw > MF_MIN + 0.001f)
		*mw -= ((float)user_config.resize_master_amt / 100);

	tile();
	update_borders();
}

void resize_stack_add(void)
{
	if (!focused || focused->floating || focused == workspaces[current_ws])
		return;

	int bw2 = 2 * user_config.border_width;
	int raw_cur = (focused->custom_stack_height > 0) ? focused->custom_stack_height : (focused->h + bw2);

	int raw_new = raw_cur + user_config.resize_stack_amt;

	/* Calculate maximum allowed height to prevent extending off-screen */
	int mon = CLAMP(focused->mon, 0, n_mons - 1);
	int mon_height = MAX(1, mons[mon].h - mons[mon].reserve_top - mons[mon].reserve_bottom);
	int gaps = user_config.gaps;
	int tile_height = MAX(1, mon_height - 2 * gaps);

	/* Count stack windows (excluding master) */
	int n_stack = 0;
	for (Client *c = workspaces[current_ws]; c; c = c->next) {
		if (c->mapped && !c->floating && !c->fullscreen && c->mon == mon && c != workspaces[current_ws])
			n_stack++;
	}

	/* Maximum height: tile_height minus space for other stack windows (min_height + gap each) */
	int min_stack_height = bw2 + 1;
	int other_stack_space = (n_stack > 1) ? (n_stack - 1) * (min_stack_height + gaps) : 0;
	int max_raw = tile_height - other_stack_space;

	if (raw_new > max_raw)
		raw_new = max_raw;

	focused->custom_stack_height = raw_new;
	tile();
}

void resize_stack_sub(void)
{
	if (!focused || focused->floating || focused == workspaces[current_ws])
		return;

	int bw2 = 2 * user_config.border_width;
	int raw_cur = (focused->custom_stack_height > 0) ? focused->custom_stack_height : (focused->h + bw2);

	int raw_new = raw_cur - user_config.resize_stack_amt;
	int min_raw = bw2 + 1;

	if (raw_new < min_raw)
		raw_new = min_raw;

	focused->custom_stack_height = raw_new;
	tile();
}

void resize_win_down(void)
{
	if (!focused || !focused->floating)
		return;

	int new_h = focused->h + user_config.resize_window_amt;
	int max_h = mons[focused->mon].h - (focused->y - mons[focused->mon].y);
	focused->h = CLAMP(new_h, MIN_WINDOW_SIZE, max_h);
	XResizeWindow(dpy, focused->win, focused->w, focused->h);
}

void resize_win_up(void)
{
	if (!focused || !focused->floating)
		return;

	int new_h = focused->h - user_config.resize_window_amt;
	focused->h = CLAMP(new_h, MIN_WINDOW_SIZE, focused->h);
	XResizeWindow(dpy, focused->win, focused->w, focused->h);
}

void resize_win_right(void)
{
	if (!focused || !focused->floating)
		return;

	int new_w = focused->w + user_config.resize_window_amt;
	int max_w = mons[focused->mon].w - (focused->x - mons[focused->mon].x);
	focused->w = CLAMP(new_w, MIN_WINDOW_SIZE, max_w);
	XResizeWindow(dpy, focused->win, focused->w, focused->h);
}

void resize_win_left(void)
{
	if (!focused || !focused->floating)
		return;

	int new_w = focused->w - user_config.resize_window_amt;
	focused->w = CLAMP(new_w, MIN_WINDOW_SIZE, focused->w);
	XResizeWindow(dpy, focused->win, focused->w, focused->h);
}

void run(void)
{
	running = True;
	XEvent xev;
	time_t last_status_tick = 0;
	int xfd = ConnectionNumber(dpy);

	while (running) {
		long timeout_ms = -1;
		time_t now = time(NULL);

		if (user_config.status_interval_sec > 0) {
			if (last_status_tick == 0)
				timeout_ms = 0;
			else {
				long due_ms = (long)user_config.status_interval_sec * 1000L -
				              (long)(now - last_status_tick) * 1000L;
				timeout_ms = MAX(0, due_ms);
			}
		}

		if (user_config.focus_follows_mouse && drag_mode == DRAG_NONE && !in_ws_switch) {
			const long focus_poll_ms = 120;
			timeout_ms = (timeout_ms < 0) ? focus_poll_ms : MIN(timeout_ms, focus_poll_ms);
		}

		if (timeout_ms < 0) {
			XNextEvent(dpy, &xev);
			xev_case(&xev);
			while (XPending(dpy)) {
				XNextEvent(dpy, &xev);
				xev_case(&xev);
			}
		}
		else {
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(xfd, &fds);

			struct timeval tv;
			tv.tv_sec = timeout_ms / 1000;
			tv.tv_usec = (timeout_ms % 1000) * 1000;

			int selret;
			do {
				selret = select(xfd + 1, &fds, NULL, NULL, &tv);
			} while (selret < 0 && errno == EINTR);

			if (selret > 0 && FD_ISSET(xfd, &fds)) {
				while (XPending(dpy)) {
					XNextEvent(dpy, &xev);
					xev_case(&xev);
				}
			}
		}

		if (user_config.focus_follows_mouse && drag_mode == DRAG_NONE && !in_ws_switch) {
			Window root_ret, child_ret;
			int root_x, root_y, win_x, win_y;
			unsigned int mask;

			if (XQueryPointer(dpy, root, &root_ret, &child_ret,
					  &root_x, &root_y, &win_x, &win_y, &mask) && child_ret != None) {
				Window w = find_toplevel(child_ret);
				if (w && w != root) {
					Client *c = find_client(w);
					if (c && c->mapped && c != focused && c->ws == current_ws)
						set_input_focus(c, True, False);
				}
			}
		}

		now = time(NULL);
		if (user_config.status_interval_sec > 0 &&
		    (last_status_tick == 0 || now - last_status_tick >= user_config.status_interval_sec)) {
			last_status_tick = now;
			updatestatus();
			drawbars();
		}
	}
}

void scan_existing_windows(void)
{
	Window root_return;
	Window parent_return;
	Window *children;
	unsigned int n_children;

	if (XQueryTree(dpy, root, &root_return, &parent_return, &children, &n_children)) {
		for (unsigned int i = 0; i < n_children; i++) {
			XWindowAttributes wa;
			if (!XGetWindowAttributes(dpy, children[i], &wa)
				|| wa.override_redirect || wa.map_state != IsViewable)
				continue;

			XEvent fake_event = {None};
			fake_event.type = MapRequest;
			fake_event.xmaprequest.window = children[i];
			hdl_map_req(&fake_event);
		}
		if (children)
			XFree(children);
	}
}

void select_input(Window w, Mask masks)
{
	XSelectInput(dpy, w, masks);
}

void send_wm_take_focus(Window w)
{
	Atom wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
	Atom wm_take_focus = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	Atom *protos;
	int n;

	if (XGetWMProtocols(dpy, w, &protos, &n)) {
		for (int i = 0; i < n; i++) {
			if (protos[i] == wm_take_focus) {
				XEvent ev = {
				    .xclient = {
						.type = ClientMessage,
						.window = w,
						.message_type = wm_protocols,
						.format = 32}
				};
				ev.xclient.data.l[0] = wm_take_focus;
				ev.xclient.data.l[1] = CurrentTime;
				XSendEvent(dpy, w, False, NoEventMask, &ev);
			}
		}
		XFree(protos);
	}
}

void setup(void)
{
	if ((dpy = XOpenDisplay(NULL)) == NULL) {
		fprintf(stderr, "can't open display.\nquitting...");
		exit(EXIT_FAILURE);
	}
	root = XDefaultRootWindow(dpy);

	setup_atoms();
	other_wm();
	init_defaults();
	update_modifier_masks();
	grab_keys();
	startup_exec();

	cursor_normal = XcursorLibraryLoadCursor(dpy, "left_ptr");
	cursor_move = XcursorLibraryLoadCursor(dpy, "fleur");
	cursor_resize = XcursorLibraryLoadCursor(dpy, "bottom_right_corner");
	XDefineCursor(dpy, root, cursor_normal);

	scr_width = XDisplayWidth(dpy, DefaultScreen(dpy));
	scr_height = XDisplayHeight(dpy, DefaultScreen(dpy));

	update_mons();
	setup_bars();
	updatestatus();

	/* select events wm should look for on root */
	Mask wm_masks = StructureNotifyMask | SubstructureRedirectMask | SubstructureNotifyMask |
	                KeyPressMask | PropertyChangeMask;
	select_input(root, wm_masks);

	/* grab mouse button events on root window */
	Mask root_click_masks = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
	Mask root_swap_masks = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
	Mask root_resize_masks = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
	grab_button(Button1, user_config.modkey, root, True, root_click_masks);
	grab_button(Button1, user_config.modkey | ShiftMask, root, True, root_swap_masks);
	grab_button(Button3, user_config.modkey, root, True, root_resize_masks);
	XSync(dpy, False);

	for (int i = 0; i < LASTEvent; i++)
		evtable[i] = hdl_dummy;
	evtable[ButtonPress] = hdl_button;
	evtable[ButtonRelease] = hdl_button_release;
	evtable[ClientMessage] = hdl_client_msg;
	evtable[ConfigureNotify] = hdl_config_ntf;
	evtable[ConfigureRequest] = hdl_config_req;
	evtable[DestroyNotify] = hdl_destroy_ntf;
	evtable[EnterNotify] = hdl_enter_ntf;
	evtable[Expose] = hdl_expose;
	evtable[KeyPress] = hdl_keypress;
	evtable[MappingNotify] = hdl_mapping_ntf;
	evtable[MapRequest] = hdl_map_req;
	evtable[MotionNotify] = hdl_motion;
	evtable[PropertyNotify] = hdl_property_ntf;
	evtable[UnmapNotify] = hdl_unmap_ntf;
	scan_existing_windows();

	/* prevent child processes from becoming zombies */
	signal(SIGCHLD, SIG_IGN);
}

#include "ewmh.c"

void set_input_focus(Client *c, Bool raise_win, Bool warp)
{
	if (c && c->mapped) {
		focused = c;
		current_mon = CLAMP(c->mon, 0, n_mons - 1);

		/* update remembered focus */
		if (c->ws >= 0 && c->ws < NUM_WORKSPACES)
			ws_focused[c->ws] = c;

		Window w = find_toplevel(c->win);

		XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
		send_wm_take_focus(w);

		if (raise_win) {
			/* always raise in monocle, otherwise respect floating_on_top */
			if (layouts[current_layout].mode == LayoutMonocle || monocle || c->floating || !user_config.floating_on_top)
				XRaiseWindow(dpy, w);
		}
		/* EWMH focus hint */
		XChangeProperty(dpy, root, atoms[ATOM_NET_ACTIVE_WINDOW], XA_WINDOW, 32,
				PropModeReplace, (unsigned char *)&w, 1);

		update_borders();

		if (warp && user_config.warp_cursor)
			warp_cursor(c);
	}
	else {
		/* no client */
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, atoms[ATOM_NET_ACTIVE_WINDOW]);

		focused = NULL;
		ws_focused[current_ws] = NULL;
		update_borders();
	}

	XFlush(dpy);
}

void set_win_scratchpad(int n)
{
	if (n < 0 || n >= MAX_SCRATCHPADS)
		return;

	if (focused == NULL)
		return;

	Client *pad_client = focused;
	Client *previous = scratchpads[n].client;

	if (previous && previous != pad_client) {
		XMapWindow(dpy, previous->win);
		previous->mapped = True;
	}

	scratchpads[n].enabled = False;
	scratchpads[n].client = pad_client;

	XUnmapWindow(dpy, scratchpads[n].client->win);
	scratchpads[n].client->mapped = False;

	if (focused == pad_client) {
		Client *next_focus = NULL;
		for (Client *c = workspaces[current_ws]; c; c = c->next) {
			if (!c->mapped || c == pad_client)
				continue;
			if (c->mon == current_mon) {
				next_focus = c;
				break;
			}
			if (!next_focus)
				next_focus = c;
		}
		set_input_focus(next_focus, True, False);
	}

	update_net_client_list();
	tile();
	update_borders();
}

void reset_opacity(Window w)
{
	Atom atom = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
	XDeleteProperty(dpy, w, atom);
}


void set_opacity(Window w, double opacity)
{
	if (opacity < 0.0)
		opacity = 0.0;

	if (opacity > 1.0)
		opacity = 1.0;

	unsigned long op = (unsigned long)(opacity * 0xFFFFFFFFu);
	Atom atom = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
	XChangeProperty(dpy, w, atom, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&op, 1);
}

void set_wm_state(Window w, long state)
{
	long data[2] = { state, None }; /* state, icon window */
	XChangeProperty(dpy, w, atoms[ATOM_WM_STATE], atoms[ATOM_WM_STATE], 32,
			        PropModeReplace, (unsigned char *)data, 2);
}

int snap_coordinate(int pos, int size, int screen_size, int snap_dist)
{
	if (UDIST(pos, 0) <= snap_dist)
		return 0;
	if (UDIST(pos + size, screen_size) <= snap_dist)
		return screen_size - size;
	return pos;
}

void spawn(const char * const *argv)
{
	if (!argv || !argv[0])
		return;

	int argc = 0;
	while (argv[argc])
		argc++;

	int cmd_count = 1;
	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "|") == 0)
			cmd_count++;
	}

	const char ***commands = malloc(cmd_count * sizeof(char **)); /* *** bruh */
	if (!commands) {
		perror("malloc commands");
		return;
	}

	/* initialize all command pointers to NULL for safe cleanup */
	for (int i = 0; i < cmd_count; i++)
		commands[i] = NULL;

	int cmd_idx = 0;
	int arg_start = 0;
	for (int i = 0; i <= argc; i++) {
		if (!argv[i] || strcmp(argv[i], "|") == 0) {
			int len = i - arg_start;
			const char **cmd_args = malloc((len + 1) * sizeof(char *));

			if (!cmd_args) {
				perror("malloc cmd_args");

				for (int j = 0; j < cmd_idx; j++)
					free(commands[j]);

				free(commands);
				return;
			}

			for (int j = 0; j < len; j++)
				cmd_args[j] = argv[arg_start + j];

			cmd_args[len] = NULL;
			if (cmd_idx >= cmd_count) {
				free(cmd_args);
				for (int j = 0; j < cmd_idx; j++)
					free(commands[j]);
				free(commands);
				fprintf(stderr, "cupidwm: internal command parser bounds error\n");
				return;
			}
			commands[cmd_idx++] = cmd_args;
			arg_start = i + 1;
		}
	}

	int (*pipes)[2] = malloc(sizeof(int[2]) * (cmd_count - 1));
	if (!pipes) {
		perror("malloc pipes");

		for (int j = 0; j < cmd_count; j++)
			free(commands[j]);

		free(commands);
		return;
	}

	for (int i = 0; i < cmd_count - 1; i++) {
		if (pipe(pipes[i]) == -1) {
			perror("pipe");

			for (int j = 0; j < cmd_count; j++)
				free(commands[j]);

			free(commands);
			free(pipes);
			return;
		}
	}

	for (int i = 0; i < cmd_count; i++) {
		if (!commands[i] || !commands[i][0])
			continue;

		pid_t pid = fork();
		if (pid < 0) {
			perror("fork");

			for (int k = 0; k < cmd_count - 1; k++) {
				close(pipes[k][0]);
				close(pipes[k][1]);
			}

			for (int j = 0; j < cmd_count; j++)
				free(commands[j]);

			free(commands);
			free(pipes);
			return;
		}
		if (pid == 0) {
			close(ConnectionNumber(dpy));

			if (i > 0)
				dup2(pipes[i - 1][0], STDIN_FILENO);

			if (i < cmd_count - 1)
				dup2(pipes[i][1], STDOUT_FILENO);

			for (int k = 0; k < cmd_count - 1; k++) {
				close(pipes[k][0]);
				close(pipes[k][1]);
			}

			execvp(commands[i][0], (char* const*)(void*)commands[i]);
			fprintf(stderr, "cupidwm: execvp '%s' failed\n", commands[i][0]);
			exit(EXIT_FAILURE);
		}
	}

	for (int i = 0; i < cmd_count - 1; i++) {
		close(pipes[i][0]);
		close(pipes[i][1]);
	}

	for (int i = 0; i < cmd_count; i++)
		free(commands[i]);

	free(commands);
	free(pipes);
}

void startup_exec(void)
{
	for (size_t i = 0; i < LENGTH(autostart); i++) {
		if (!autostart[i] || !autostart[i][0])
			continue;
		spawn(autostart[i]);
	}
}

void swallow_window(Client *swallower, Client *swallowed)
{
	if (!swallower || !swallowed || swallower->swallowed || swallowed->swallower)
		return;

	XUnmapWindow(dpy, swallower->win);
	swallower->mapped = False;

	swallower->swallowed = swallowed;
	swallowed->swallower = swallower;

	swallowed->floating = swallower->floating;
	if (swallowed->floating) {
		swallowed->x = swallower->x;
		swallowed->y = swallower->y;
		swallowed->w = swallower->w;
		swallowed->h = swallower->h;

		if (swallowed->win)
			XMoveResizeWindow(dpy, swallowed->win, swallowed->x, swallowed->y, swallowed->w, swallowed->h);
	}

	tile();
	update_borders();
}

void swap_clients(Client *a, Client *b)
{
	if (!a || !b || a == b)
		return;

	Client **head = &workspaces[current_ws];
	Client **pa = head, **pb = head;

	while (*pa && *pa != a)
		pa = &(*pa)->next;

	while (*pb && *pb != b)
		pb = &(*pb)->next;

	if (!*pa || !*pb)
		return;

	/* if next to it swap */
	if (*pa == b && *pb == a) {
		Client *tmp = b->next;
		b->next = a;
		a->next = tmp;
		*pa = b;
		return;
	}

	/* full swap */
	Client *ta = *pa;
	Client *tb = *pb;
	Client *ta_next = ta->next;
	Client *tb_next = tb->next;

	*pa = tb;
	tb->next = ta_next == tb ? ta : ta_next;

	*pb = ta;
	ta->next = tb_next == ta ? tb : tb_next;
}

void switch_previous_workspace(void)
{
	change_workspace(previous_workspace);
}

#include "layout.c"

void toggle_scratchpad(int n)
{
	if (n < 0 || n >= MAX_SCRATCHPADS || scratchpads[n].client == NULL)
		return;

	Client *c = scratchpads[n].client;

	if (c->ws != current_ws) {
		/* unlink from old workspace */
		Client **pp = &workspaces[c->ws];
		while (*pp && *pp != c)
			pp = &(*pp)->next;

		if (*pp)
			*pp = c->next;

		/* link to current workspace */
		c->next = workspaces[current_ws];
		workspaces[current_ws] = c;
		c->ws = current_ws;

		long desktop = current_ws;
		XChangeProperty(dpy, c->win, atoms[ATOM_NET_WM_DESKTOP], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&desktop, 1);
	}

	c->mon = CLAMP(focused ? focused->mon : current_mon, 0, n_mons - 1);

	if (scratchpads[n].enabled) {
		XUnmapWindow(dpy, c->win);
		c->mapped = False;
		scratchpads[n].enabled = False;
		focus_prev();
	}
	else {
		XMapWindow(dpy, c->win);
		c->mapped = True;
		scratchpads[n].enabled = True;

		set_input_focus(c, True, True);
	}

	tile();
	update_borders();
	update_net_client_list();
}

void unswallow_window(Client *c)
{
	if (!c || !c->swallower)
		return;

	Client *swallower = c->swallower;
	int ws = swallower->ws;

	/* unlink windows */
	swallower->swallowed = NULL;
	c->swallower = NULL;

	/* mark swallower as visible */
	swallower->mapped = True;

	/* remember it as focused for that workspace */
	if (ws >= 0 && ws < NUM_WORKSPACES)
		ws_focused[ws] = swallower;

	if (ws == current_ws) {
		XMapWindow(dpy, swallower->win);
		set_input_focus(swallower, False, True);
		tile();
		update_borders();
	}
}

void update_borders(void)
{
	for (Client *c = workspaces[current_ws]; c; c = c->next)
		XSetWindowBorder(dpy, c->win, (c == focused ? user_config.border_foc_col : user_config.border_ufoc_col));

	if (focused) {
		Window w = focused->win;
		XChangeProperty(dpy, root, atoms[ATOM_NET_ACTIVE_WINDOW], XA_WINDOW, 32,
				        PropModeReplace, (unsigned char *)&w, 1);
	}

	drawbars();
}


void update_modifier_masks(void)
{
	XModifierKeymap *mod_mapping = XGetModifierMapping(dpy);
	KeyCode num = XKeysymToKeycode(dpy, XK_Num_Lock);
	KeyCode mode = XKeysymToKeycode(dpy, XK_Mode_switch);
	numlock_mask = 0;
	mode_switch_mask = 0;

	int n_masks = 8;
	for (int i = 0; i < n_masks; i++) {
		for (int j = 0; j < mod_mapping->max_keypermod; j++) {
			/* keycode at mod[i][j] */
			KeyCode keycode = mod_mapping->modifiermap[i * mod_mapping->max_keypermod + j];
			if (keycode == num)
				numlock_mask = (1u << i); /* which mod bit == NumLock key */
			if (keycode == mode)
				mode_switch_mask = (1u << i); /* which mod bit == Mode_switch key */
		}
	}
	XFreeModifiermap(mod_mapping);
}

void update_mons(void)
{
	XineramaScreenInfo *info;
	Monitor *old = mons;
	int old_n = n_mons;

	scr_width = XDisplayWidth(dpy, DefaultScreen(dpy));
	scr_height = XDisplayHeight(dpy, DefaultScreen(dpy));

	for (int s = 0; s < ScreenCount(dpy); s++) {
		Window scr_root = RootWindow(dpy, s);
		XDefineCursor(dpy, scr_root, cursor_normal);
	}

	if (XineramaIsActive(dpy)) {
		int query_n = 0;
		info = XineramaQueryScreens(dpy, &query_n);
		if (!info || query_n <= 0) {
			n_mons = 1;
			mons = malloc(sizeof *mons);
			if (!mons) {
				fputs("cupidwm: failed to allocate fallback monitor\n", stderr);
				exit(EXIT_FAILURE);
			}
			mons[0].x = 0;
			mons[0].y = 0;
			mons[0].w = scr_width;
			mons[0].h = scr_height;
			mons[0].reserve_left = 0;
			mons[0].reserve_right = 0;
			mons[0].reserve_top = 0;
			mons[0].reserve_bottom = 0;
			mons[0].bar_y = 0;
			mons[0].barwin = 0;
			if (info)
				XFree(info);
		}
		else {
			n_mons = MIN(query_n, MAX_MONITORS);
			if (query_n > MAX_MONITORS)
				fprintf(stderr, "cupidwm: monitor count %d exceeds MAX_MONITORS=%d, truncating\n", query_n, MAX_MONITORS);

			mons = malloc(sizeof *mons * n_mons);
			if (!mons) {
				fputs("cupidwm: failed to allocate monitors\n", stderr);
				exit(EXIT_FAILURE);
			}
			for (int i = 0; i < n_mons; i++) {
				mons[i].x = info[i].x_org;
				mons[i].y = info[i].y_org;
				mons[i].w = info[i].width;
				mons[i].h = info[i].height;
				mons[i].reserve_left = 0;
				mons[i].reserve_right = 0;
				mons[i].reserve_top = 0;
				mons[i].reserve_bottom = 0;
				mons[i].bar_y = 0;
				mons[i].barwin = 0;
			}
			XFree(info);
		}
	}
	else {
		n_mons = 1;
		mons = malloc(sizeof *mons);
		if (!mons) {
			fputs("cupidwm: failed to allocate monitor\n", stderr);
			exit(EXIT_FAILURE);
		}
		mons[0].x = 0;
		mons[0].y = 0;
		mons[0].w = scr_width;
		mons[0].h = scr_height;
		mons[0].reserve_left = 0;
		mons[0].reserve_right = 0;
		mons[0].reserve_top = 0;
		mons[0].reserve_bottom = 0;
		mons[0].bar_y = 0;
		mons[0].barwin = 0;
	}

	current_mon = CLAMP(current_mon, 0, n_mons - 1);
	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		for (Client *c = workspaces[ws]; c; c = c->next) {
			int guessed = get_monitor_for(c);
			c->mon = CLAMP(guessed, 0, n_mons - 1);
		}
	}
	if (focused)
		focused->mon = CLAMP(focused->mon, 0, n_mons - 1);

	if (old) {
		for (int i = 0; i < old_n; i++) {
			if (old[i].barwin)
				XDestroyWindow(dpy, old[i].barwin);
		}
	}
	free(old);
}





void warp_cursor(Client *c)
{
	if (!c)
		return;

	int center_x = c->x + (c->w / 2);
	int center_y = c->y + (c->h / 2);

	XWarpPointer(dpy, None, root, 0, 0, 0, 0, center_x, center_y);
	XSync(dpy, False);
}


Bool window_should_float(Window w)
{
	const Rule *r = rule_for_window(w);
	return r && r->is_floating;
}

Bool window_should_start_fullscreen(Window w)
{
	const Rule *r = rule_for_window(w);
	return r && r->start_fullscreen;
}

void centrewindowcmd(const Arg *arg) { (void)arg; centre_window(); }
void closewindowcmd(const Arg *arg) { (void)arg; close_focused(); }
void decreasegapscmd(const Arg *arg) { (void)arg; dec_gaps(); }
void focusnextcmd(const Arg *arg) { (void)arg; focus_next(); }
void focusnextmoncmd(const Arg *arg) { (void)arg; focus_next_mon(); }
void focusprevcmd(const Arg *arg) { (void)arg; focus_prev(); }
void focusprevmoncmd(const Arg *arg) { (void)arg; focus_prev_mon(); }
void increasegapscmd(const Arg *arg) { (void)arg; inc_gaps(); }
void killclientcmd(const Arg *arg) { (void)arg; close_focused(); }
void masternextcmd(const Arg *arg) { (void)arg; move_master_next(); }
void masterprevcmd(const Arg *arg) { (void)arg; move_master_prev(); }
void masterincreasecmd(const Arg *arg) { (void)arg; resize_master_add(); }
void masterdecreasecmd(const Arg *arg) { (void)arg; resize_master_sub(); }
void movenextmoncmd(const Arg *arg) { (void)arg; move_next_mon(); }
void moveprevmoncmd(const Arg *arg) { (void)arg; move_prev_mon(); }
void movewindowncmd(const Arg *arg) { (void)arg; move_win_down(); }
void movewinleftcmd(const Arg *arg) { (void)arg; move_win_left(); }
void movewinrightcmd(const Arg *arg) { (void)arg; move_win_right(); }
void movewinupcmd(const Arg *arg) { (void)arg; move_win_up(); }
void prevworkspacecmd(const Arg *arg) { (void)arg; switch_previous_workspace(); }
void quitcmd(const Arg *arg) { (void)arg; quit(); }
void resizewindowncmd(const Arg *arg) { (void)arg; resize_win_down(); }
void resizewinleftcmd(const Arg *arg) { (void)arg; resize_win_left(); }
void resizewinrightcmd(const Arg *arg) { (void)arg; resize_win_right(); }
void resizewinupcmd(const Arg *arg) { (void)arg; resize_win_up(); }
void stackincreasecmd(const Arg *arg) { (void)arg; resize_stack_add(); }
void stackdecreasecmd(const Arg *arg) { (void)arg; resize_stack_sub(); }
void togglefloatingcmd(const Arg *arg) { (void)arg; toggle_floating(); }
void togglefloatingglobalcmd(const Arg *arg) { (void)arg; toggle_floating_global(); }
void togglefullscreencmd(const Arg *arg) { (void)arg; toggle_fullscreen(); }
void createscratchpadcmd(const Arg *arg) { if (arg) set_win_scratchpad((int)arg->ui); }
void removescratchpadcmd(const Arg *arg) { if (arg) remove_scratchpad((int)arg->ui); }
void togglescratchpad(const Arg *arg) { if (arg) toggle_scratchpad((int)arg->ui); }

void spawncmd(const Arg *arg)
{
	if (!arg || !arg->v)
		return;
	spawn((const char *const *)arg->v);
}

void viewws(const Arg *arg)
{
	if (!arg || arg->ui >= NUM_WORKSPACES)
		return;
	change_workspace((int)arg->ui);
	update_net_client_list();
}

void movetows(const Arg *arg)
{
	if (!arg || arg->ui >= NUM_WORKSPACES)
		return;
	move_to_workspace((int)arg->ui);
	update_net_client_list();
}

void setlayoutcmd(const Arg *arg)
{
	int target = arg ? arg->i : -1;
	if (target < 0)
		target = (current_layout + 1) % (int)LENGTH(layouts);
	if (target >= (int)LENGTH(layouts))
		target = 0;

	int mode = layouts[target].mode;
	if (mode == LayoutFloating && !global_floating)
		toggle_floating_global();
	else if (mode != LayoutFloating && global_floating)
		toggle_floating_global();

	current_layout = target;
	monocle = (mode == LayoutMonocle);
	tile();
	update_borders();
}

void movemousecmd(const Arg *arg)
{
	(void)arg;
	if (!focused)
		return;

	if (!focused->floating)
		toggle_floating();

	drag_client = focused;
	drag_start_x = focused->x;
	drag_start_y = focused->y;
	drag_orig_x = focused->x;
	drag_orig_y = focused->y;
	drag_orig_w = focused->w;
	drag_orig_h = focused->h;
	drag_mode = DRAG_MOVE;
	XGrabPointer(dpy, root, True, ButtonReleaseMask | PointerMotionMask,
	             GrabModeAsync, GrabModeAsync, None, cursor_move, CurrentTime);
}

void resizemousecmd(const Arg *arg)
{
	(void)arg;
	if (!focused)
		return;

	if (!focused->floating)
		toggle_floating();

	drag_client = focused;
	drag_start_x = focused->x;
	drag_start_y = focused->y;
	drag_orig_x = focused->x;
	drag_orig_y = focused->y;
	drag_orig_w = focused->w;
	drag_orig_h = focused->h;
	drag_mode = DRAG_RESIZE;
	XGrabPointer(dpy, root, True, ButtonReleaseMask | PointerMotionMask,
	             GrabModeAsync, GrabModeAsync, None, cursor_resize, CurrentTime);
}

void swapmousecmd(const Arg *arg)
{
	(void)arg;
	if (!focused || focused->floating)
		return;

	drag_client = focused;
	drag_start_x = focused->x;
	drag_start_y = focused->y;
	drag_orig_x = focused->x;
	drag_orig_y = focused->y;
	drag_orig_w = focused->w;
	drag_orig_h = focused->h;
	drag_mode = DRAG_SWAP;
	XGrabPointer(dpy, root, True, ButtonReleaseMask | PointerMotionMask,
	             GrabModeAsync, GrabModeAsync, None, cursor_move, CurrentTime);
}

void restartwm(const Arg *arg)
{
	(void)arg;
	if (!saved_argv || saved_argc <= 0 || !saved_argv[0])
		return;

	execvp(saved_argv[0], saved_argv);
	fprintf(stderr, "cupidwm: restart failed for '%s'\n", saved_argv[0]);
}

int xerr(Display *d, XErrorEvent *ee)
{
	/* ignore noise & non fatal errors */
	const struct {
		int req, code;
	} ignore[] = {
		{0, BadWindow},
		{X_GetGeometry, BadDrawable},
		{X_SetInputFocus, BadMatch},
		{X_ConfigureWindow, BadMatch},
	};

	for (size_t i = 0; i < sizeof(ignore) / sizeof(ignore[0]); i++) {
		if ((ignore[i].req == 0 || ignore[i].req == ee->request_code) && (ignore[i].code == ee->error_code))
			return 0;
	}

	return 0;
	(void)d;
	(void)ee;
}

void xev_case(XEvent *xev)
{
	if (xev->type >= 0 && xev->type < LASTEvent)
		evtable[xev->type](xev);
	else
		fprintf(stderr, "cupidwm: invalid event type: %d\n", xev->type);
}

int main(int ac, char **av)
{
	saved_argc = ac;
	saved_argv = av;

	if (ac > 1) {
		if (strcmp(av[1], "-v") == 0 || strcmp(av[1], "--version") == 0) {
			printf("%s\n%s\n%s\n", CUPIDWM_VERSION, CUPIDWM_AUTHOR, CUPIDWM_LICINFO);
			return EXIT_SUCCESS;
		}
		else {
			printf("usage:\n");
			printf("\t[-v || --version]: See the version of cupidwm\n");
			return EXIT_SUCCESS;
		}
	}
	setup();
	puts("cupidwm: starting...");
	run();
	return EXIT_SUCCESS;
}

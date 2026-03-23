/* See LICENSE for more information.
 *
 * cupidwm is a compact X11 window manager with source-based configuration.
 * It provides workspace-driven tiling, floating, monocle, scratchpads,
 * swallowing and custom layouts.
 */

#include <signal.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <regex.h>
#include <sys/select.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

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
#include <X11/extensions/Xrandr.h>
#include <X11/Xcursor/Xcursor.h>

#include "defs.h"

enum {
	DIR_LEFT = 0,
	DIR_RIGHT,
	DIR_UP,
	DIR_DOWN,
};

Client *add_client(Window w, int ws);
void apply_fullscreen(Client *c, Bool on);
void centre_window(void);
void change_workspace(int ws);
int check_parent(pid_t p, pid_t c);
int clean_mask(int mask);
void close_focused(void);
void dec_gaps(void);
Client *find_client(Window w);
Window find_toplevel(Window w);
void focus_next(void);
void focus_prev(void);
void focus_dir(int direction);
void focus_next_mon(void);
void focus_prev_mon(void);
void focus_mon_dir(int direction);
int get_monitor_for(Client *c);
pid_t get_parent_process(pid_t c);
pid_t get_pid(Window w);
int get_workspace_for_window(Window w);
Bool monitor_views_workspace(int mon, int ws);
Bool workspace_is_visible(int ws);
Bool client_should_be_visible(const Client *c);
Bool client_is_visible(const Client *c);
Bool client_is_visible_on_monitor(const Client *c, int mon);
void sync_active_monitor_state(void);
void publish_current_desktop(void);
void publish_desktop_names(void);
void workspace_names_init_defaults(void);
const char *workspace_name_for(int ws);
Bool workspace_name_set(int ws, const char *name);
void workspace_name_reset(int ws);
int layout_index_from_mode(int mode);
Client *first_visible_client(int ws, int mon, Client *skip);
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
void hdl_focus_in(XEvent *xev);
void hdl_keypress(XEvent *xev);
void hdl_mapping_ntf(XEvent *xev);
void hdl_map_req(XEvent *xev);
void hdl_motion(XEvent *xev);
void hdl_property_ntf(XEvent *xev);
void hdl_unmap_ntf(XEvent *xev);
void inc_gaps(void);
void init_defaults(void);
Bool is_child_proc(pid_t pid1, pid_t pid2);
void move_master_next(void);
void move_master_prev(void);
void move_client_to_workspace(Client *moved, int ws, Bool focus_target);
void move_next_mon(void);
void move_prev_mon(void);
void send_to_mon_dir(int direction);
void move_to_workspace(int ws);
void move_dir(int direction);
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
void setup_randr(void);
void setup_atoms(void);
void set_frame_extents(Window w);
void set_allowed_actions(Window w);
void set_input_focus(Client *c, Bool raise_win, Bool warp);
void set_opacity(Window w, double opacity);
void set_win_scratchpad(int n);
void set_wm_state(Window w, long state);
void session_state_save(void);
void session_state_restore(void);
void session_state_clear(void);
int snap_coordinate(int pos, int size, int screen_size, int snap_dist);
void spawn(const char * const *argv);
void startup_exec(void);
void swallow_window(Client *swallower, Client *swallowed);
void swap_clients(Client *a, Client *b);
void swap_dir(int direction);
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
void update_focused_ewmh_state(Client *active);
void warp_cursor(Client *c);
Bool window_has_ewmh_state(Window w, Atom state);
Bool window_is_dock(Window w);
void window_set_ewmh_state(Window w, Atom state, Bool add);
Bool window_should_float(Window w);
Bool window_should_start_fullscreen(Window w);
void ipc_cleanup(void);
int ipc_get_server_fd(void);
void ipc_handle_connection(void);
void ipc_fdset_prepare(fd_set *fds, int *maxfd);
Bool ipc_fdset_has_ready(const fd_set *fds);
int ipc_setup(void);
const char *ipc_resolve_socket_path(const char *configured_path, char *out, size_t outsz);
void ipc_notify_event(const char *event, const char *details);
int xerr(Display *d, XErrorEvent *ee);
void xev_case(XEvent *xev);
void handle_xevent(XEvent *xev);

/* config action wrappers */
void centrewindowcmd(const Arg *arg);
void closewindowcmd(const Arg *arg);
void decreasegapscmd(const Arg *arg);
void focusnextcmd(const Arg *arg);
void focusdircmd(const Arg *arg);
void focusmondircmd(const Arg *arg);
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
void movedircmd(const Arg *arg);
void moveprevmoncmd(const Arg *arg);
void sendmondircmd(const Arg *arg);
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
void swapdircmd(const Arg *arg);
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
void status_set_text_override(const char *text);
void status_clear_text_override(void);
Bool status_text_override_active(void);
int status_segment_count(void);
Bool status_segment_label_at(int index, char *out, size_t outsz);
const char *status_action_command_get(int index, unsigned int button);
Bool status_action_command_set(int index, unsigned int button, const char *cmd);
Bool status_action_command_clear(int index, unsigned int button, Bool all_buttons);
Bool status_dispatch_click_at(int rel_x, unsigned int button);
void status_fifo_setup(void);
int status_fifo_fd(void);
void status_fifo_handle_readable(void);
Client *bar_client_at(Monitor *m, int click_x, int title_x, int title_w);
int collect_bar_clients(int mon, Client **list, int max_clients);
void monitor_workarea(int mon, int *x, int *y, int *w, int *h);
Bool get_window_title(Window w, char *buf, size_t buflen);
Bool monitor_topology_changed(void);
static void apply_client_rule_defaults(Client *c);

#include "config.h"

static Atom atoms[ATOM_COUNT];
static const char *atom_names[ATOM_COUNT] = {
	[ATOM_NET_ACTIVE_WINDOW]             = "_NET_ACTIVE_WINDOW",
	[ATOM_NET_CURRENT_DESKTOP]           = "_NET_CURRENT_DESKTOP",
	[ATOM_NET_SUPPORTED]                 = "_NET_SUPPORTED",
	[ATOM_NET_WM_STATE]                  = "_NET_WM_STATE",
	[ATOM_NET_WM_STATE_FULLSCREEN]       = "_NET_WM_STATE_FULLSCREEN",
	[ATOM_NET_WM_STATE_STICKY]           = "_NET_WM_STATE_STICKY",
	[ATOM_NET_WM_STATE_MAXIMIZED_VERT]   = "_NET_WM_STATE_MAXIMIZED_VERT",
	[ATOM_NET_WM_STATE_MAXIMIZED_HORZ]   = "_NET_WM_STATE_MAXIMIZED_HORZ",
	[ATOM_NET_WM_STATE_SHADED]           = "_NET_WM_STATE_SHADED",
	[ATOM_NET_WM_STATE_SKIP_TASKBAR]     = "_NET_WM_STATE_SKIP_TASKBAR",
	[ATOM_NET_WM_STATE_SKIP_PAGER]       = "_NET_WM_STATE_SKIP_PAGER",
	[ATOM_NET_WM_STATE_HIDDEN]           = "_NET_WM_STATE_HIDDEN",
	[ATOM_NET_WM_STATE_DEMANDS_ATTENTION] = "_NET_WM_STATE_DEMANDS_ATTENTION",
	[ATOM_NET_WM_STATE_FOCUSED]          = "_NET_WM_STATE_FOCUSED",
	[ATOM_WM_STATE]                      = "WM_STATE",
	[ATOM_NET_WM_WINDOW_TYPE]            = "_NET_WM_WINDOW_TYPE",
	[ATOM_NET_WORKAREA]                  = "_NET_WORKAREA",
	[ATOM_WM_DELETE_WINDOW]              = "WM_DELETE_WINDOW",
	[ATOM_NET_WM_STRUT]                  = "_NET_WM_STRUT",
	[ATOM_NET_WM_STRUT_PARTIAL]          = "_NET_WM_STRUT_PARTIAL",
	[ATOM_NET_SUPPORTING_WM_CHECK]       = "_NET_SUPPORTING_WM_CHECK",
	[ATOM_NET_WM_NAME]                   = "_NET_WM_NAME",
	[ATOM_NET_CLOSE_WINDOW]              = "_NET_CLOSE_WINDOW",
	[ATOM_UTF8_STRING]                   = "UTF8_STRING",
	[ATOM_NET_WM_DESKTOP]                = "_NET_WM_DESKTOP",
	[ATOM_NET_CLIENT_LIST]               = "_NET_CLIENT_LIST",
	[ATOM_NET_CLIENT_LIST_STACKING]      = "_NET_CLIENT_LIST_STACKING",
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
	[ATOM_NET_WM_STATE_ABOVE]            = "_NET_WM_STATE_ABOVE",
	[ATOM_NET_WM_STATE_BELOW]            = "_NET_WM_STATE_BELOW",
	[ATOM_NET_MOVERESIZE_WINDOW]         = "_NET_MOVERESIZE_WINDOW",
	[ATOM_NET_REQUEST_FRAME_EXTENTS]     = "_NET_REQUEST_FRAME_EXTENTS",
	[ATOM_NET_RESTACK_WINDOW]            = "_NET_RESTACK_WINDOW",
	[ATOM_NET_WM_ALLOWED_ACTIONS]        = "_NET_WM_ALLOWED_ACTIONS",
	[ATOM_NET_WM_ACTION_MOVE]            = "_NET_WM_ACTION_MOVE",
	[ATOM_NET_WM_ACTION_RESIZE]          = "_NET_WM_ACTION_RESIZE",
	[ATOM_NET_WM_ACTION_MINIMIZE]        = "_NET_WM_ACTION_MINIMIZE",
	[ATOM_NET_WM_ACTION_SHADE]           = "_NET_WM_ACTION_SHADE",
	[ATOM_NET_WM_ACTION_STICK]           = "_NET_WM_ACTION_STICK",
	[ATOM_NET_WM_ACTION_MAXIMIZE_HORZ]   = "_NET_WM_ACTION_MAXIMIZE_HORZ",
	[ATOM_NET_WM_ACTION_MAXIMIZE_VERT]   = "_NET_WM_ACTION_MAXIMIZE_VERT",
	[ATOM_NET_WM_ACTION_FULLSCREEN]      = "_NET_WM_ACTION_FULLSCREEN",
	[ATOM_NET_WM_ACTION_CHANGE_DESKTOP]  = "_NET_WM_ACTION_CHANGE_DESKTOP",
	[ATOM_NET_WM_ACTION_CLOSE]           = "_NET_WM_ACTION_CLOSE",
	[ATOM_WM_PROTOCOLS]                  = "WM_PROTOCOLS",
};

Cursor cursor_normal;
Cursor cursor_move;
Cursor cursor_resize;

Client *workspaces[NUM_WORKSPACES] = {NULL};
WorkspaceState workspace_states[NUM_WORKSPACES];
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
Bool randr_enabled = False;

Mask numlock_mask = 0;
Mask mode_switch_mask = 0;
int randr_event_base = 0;
int randr_error_base = 0;
int randr_major_version = 0;
int randr_minor_version = 0;

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
unsigned long next_client_map_seq = 1;
XFontStruct *bar_font = NULL;
XftFont *bar_xft_font = NULL;
GC bar_gc;
char stext[256] = "";
char status_action_cmd[STATUS_MAX_SEGMENTS][3][STATUS_MAX_ACTION] = {{{0}}};
Bool status_override_active = False;
char workspace_names[NUM_WORKSPACES][WORKSPACE_NAME_MAX] = {{0}};
int saved_argc = 0;
char **saved_argv = NULL;
static char session_state_path_buf[PATH_MAX] = {0};
static int monitor_topology_prev_count = -1;
static unsigned long long monitor_topology_prev_sig = 0;

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

static WorkspaceState *workspace_state_for(int ws)
{
	if (ws < 0 || ws >= NUM_WORKSPACES)
		return NULL;
	return &workspace_states[ws];
}

static void workspace_name_trim_copy(char *dst, size_t dstsz, const char *src)
{
	if (!dst || dstsz == 0)
		return;

	dst[0] = '\0';
	if (!src)
		return;

	while (*src == ' ' || *src == '\t')
		src++;

	size_t len = strlen(src);
	while (len > 0 &&
	       (src[len - 1] == ' ' || src[len - 1] == '\t' ||
	        src[len - 1] == '\n' || src[len - 1] == '\r')) {
		len--;
	}

	if (len >= dstsz)
		len = dstsz - 1;

	if (len > 0)
		memcpy(dst, src, len);
	dst[len] = '\0';
}

static void workspace_name_default(int ws, char *out, size_t outsz)
{
	if (!out || outsz == 0)
		return;

	out[0] = '\0';
	if (ws < 0 || ws >= NUM_WORKSPACES)
		return;

	if (tags[ws] && tags[ws][0])
		workspace_name_trim_copy(out, outsz, tags[ws]);
	if (!out[0])
		snprintf(out, outsz, "%d", ws + 1);
}

static Bool workspace_name_assign(int ws, const char *name, Bool fallback_default, Bool *changed_out)
{
	if (changed_out)
		*changed_out = False;
	if (ws < 0 || ws >= NUM_WORKSPACES)
		return False;

	char normalized[WORKSPACE_NAME_MAX];
	workspace_name_trim_copy(normalized, sizeof(normalized), name);
	if (!normalized[0] && fallback_default)
		workspace_name_default(ws, normalized, sizeof(normalized));
	if (!normalized[0])
		return False;

	Bool changed = strcmp(workspace_names[ws], normalized) != 0;
	if (changed)
		snprintf(workspace_names[ws], sizeof(workspace_names[ws]), "%s", normalized);
	if (changed_out)
		*changed_out = changed;
	return True;
}

int layout_index_from_mode(int mode)
{
	for (int i = 0; i < (int)LENGTH(layouts); i++) {
		if (layouts[i].mode == mode)
			return i;
	}
	return -1;
}

void workspace_names_init_defaults(void)
{
	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		Bool changed = False;
		workspace_name_assign(ws, NULL, True, &changed);
		(void)changed;
	}
}

const char *workspace_name_for(int ws)
{
	if (ws < 0 || ws >= NUM_WORKSPACES)
		return "";
	return workspace_names[ws];
}

void publish_desktop_names(void)
{
	if (!dpy || root == None)
		return;
	if (atoms[ATOM_NET_DESKTOP_NAMES] == None || atoms[ATOM_UTF8_STRING] == None)
		return;

	size_t total = 0;
	for (int ws = 0; ws < NUM_WORKSPACES; ws++)
		total += strlen(workspace_name_for(ws)) + 1;
	if (total == 0)
		total = 1;

	unsigned char *buf = calloc(total, sizeof(unsigned char));
	if (!buf)
		return;

	size_t off = 0;
	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		const char *name = workspace_name_for(ws);
		size_t len = strlen(name);
		if (off + len >= total)
			break;
		memcpy(buf + off, name, len);
		off += len + 1;
	}

	XChangeProperty(dpy, root, atoms[ATOM_NET_DESKTOP_NAMES], atoms[ATOM_UTF8_STRING], 8,
	                PropModeReplace, (unsigned char *)buf, (int)total);
	free(buf);
}

static void workspace_name_sync_state(int ws)
{
	if (ws < 0 || ws >= NUM_WORKSPACES)
		return;

	publish_desktop_names();
	drawbars();
	session_state_save();
	{
		char details[192];
		snprintf(details, sizeof(details), "workspace=%d name=%s", ws + 1, workspace_name_for(ws));
		ipc_notify_event("workspace_name", details);
	}
}

Bool workspace_name_set(int ws, const char *name)
{
	Bool changed = False;
	if (!workspace_name_assign(ws, name, False, &changed))
		return False;
	if (changed)
		workspace_name_sync_state(ws);
	return True;
}

void workspace_name_reset(int ws)
{
	Bool changed = False;
	if (!workspace_name_assign(ws, NULL, True, &changed))
		return;
	if (changed)
		workspace_name_sync_state(ws);
}

static int workspace_layout_for(int ws)
{
	WorkspaceState *state = workspace_state_for(ws);
	int fallback = layout_index_from_mode(LayoutTile);
	if (fallback < 0)
		fallback = 0;
	if (!state)
		return fallback;

	int layout = state->layout;
	if (layout >= 0 && layout < (int)LENGTH(layouts))
		return layout;

	int by_mode = layout_index_from_mode(layout);
	if (by_mode >= 0)
		return by_mode;

	return fallback;
}

static int workspace_gaps_for(int ws)
{
	WorkspaceState *state = workspace_state_for(ws);
	return state ? state->gaps : user_config.gaps;
}

static float *workspace_master_width_for(int ws, int mon)
{
	WorkspaceState *state = workspace_state_for(ws);
	if (!state)
		return NULL;
	mon = CLAMP(mon, 0, MAX_MONITORS - 1);
	return &state->master_width[mon];
}

Bool monitor_views_workspace(int mon, int ws)
{
	if (!mons || n_mons <= 0 || mon < 0 || mon >= n_mons)
		return False;
	if (ws < 0 || ws >= NUM_WORKSPACES)
		return False;
	return mons[mon].view_ws == ws;
}

Bool workspace_is_visible(int ws)
{
	if (!mons || n_mons <= 0 || ws < 0 || ws >= NUM_WORKSPACES)
		return False;

	for (int mon = 0; mon < n_mons; mon++) {
		if (mons[mon].view_ws == ws)
			return True;
	}

	return False;
}

Bool client_is_visible_on_monitor(const Client *c, int mon)
{
	if (!c || c->hidden || c->swallowed)
		return False;
	if (c->sticky)
		return c->mon == mon;
	return c->mon == mon && monitor_views_workspace(mon, c->ws);
}

Bool client_should_be_visible(const Client *c)
{
	if (!c)
		return False;
	return client_is_visible_on_monitor(c, c->mon);
}

Bool client_is_visible(const Client *c)
{
	return c && c->mapped && client_should_be_visible(c);
}

void sync_active_monitor_state(void)
{
	if (n_mons <= 0 || !mons) {
		current_mon = 0;
		current_ws = 0;
		previous_workspace = 0;
		current_layout = 0;
		monocle = (layouts[current_layout].mode == LayoutMonocle);
		return;
	}

	current_mon = CLAMP(current_mon, 0, n_mons - 1);
	current_ws = mons[current_mon].view_ws;
	previous_workspace = mons[current_mon].prev_view_ws;
	current_layout = workspace_layout_for(current_ws);
	current_layout = CLAMP(current_layout, 0, (int)LENGTH(layouts) - 1);
	monocle = (layouts[current_layout].mode == LayoutMonocle);
}

void publish_current_desktop(void)
{
	long current_desktop = current_ws;
	XChangeProperty(dpy, root, atoms[ATOM_NET_CURRENT_DESKTOP], XA_CARDINAL, 32,
	                PropModeReplace, (unsigned char *)&current_desktop, 1);
}

Client *first_visible_client(int ws, int mon, Client *skip)
{
	Client *fallback = NULL;

	if (ws < 0 || ws >= NUM_WORKSPACES)
		return NULL;

	for (Client *c = workspaces[ws]; c; c = c->next) {
		if (!client_is_visible(c) || c == skip)
			continue;
		if (c->mon == mon)
			return c;
		if (!fallback)
			fallback = c;
	}

	return fallback;
}

static void set_monitor_workspace(int mon, int ws)
{
	if (!mons || n_mons <= 0)
		return;

	mon = CLAMP(mon, 0, n_mons - 1);
	if (ws < 0 || ws >= NUM_WORKSPACES)
		return;

	if (mons[mon].view_ws == ws) {
		if (mon == current_mon)
			sync_active_monitor_state();
		return;
	}

	int old_ws = mons[mon].view_ws;
	int swap_mon = -1;
	for (int i = 0; i < n_mons; i++) {
		if (i != mon && mons[i].view_ws == ws) {
			swap_mon = i;
			break;
		}
	}

	mons[mon].prev_view_ws = old_ws;
	mons[mon].view_ws = ws;

	if (swap_mon >= 0) {
		mons[swap_mon].prev_view_ws = mons[swap_mon].view_ws;
		mons[swap_mon].view_ws = old_ws;
	}

	if (mon == current_mon || swap_mon == current_mon)
		sync_active_monitor_state();
}

static void refresh_client_visibility(void)
{
	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		for (Client *c = workspaces[ws]; c; c = c->next) {
			if (!c->mapped)
				continue;
			if (client_should_be_visible(c))
				XMapWindow(dpy, c->win);
			else {
				c->ignore_unmap_events += 2;
				XUnmapWindow(dpy, c->win);
			}
		}
	}
}

static Bool relink_client_to_workspace(Client *c, int ws)
{
	if (!c || ws < 0 || ws >= NUM_WORKSPACES)
		return False;
	if (c->ws == ws)
		return True;

	Client **pp = &workspaces[c->ws];
	while (*pp && *pp != c)
		pp = &(*pp)->next;
	if (!*pp)
		return False;

	*pp = c->next;
	c->next = workspaces[ws];
	workspaces[ws] = c;
	c->ws = ws;
	return True;
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

static unsigned long long mix_u64(unsigned long long h, unsigned long long v)
{
	h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
	return h;
}

static void reset_monitor_slot(Monitor *m, int x, int y, int w, int h)
{
	if (!m)
		return;

	m->x = x;
	m->y = y;
	m->w = MAX(1, w);
	m->h = MAX(1, h);
	m->reserve_left = 0;
	m->reserve_right = 0;
	m->reserve_top = 0;
	m->reserve_bottom = 0;
	m->bar_y = 0;
	m->barwin = 0;
}

static int cmp_monitor_geom(const void *pa, const void *pb)
{
	const Monitor *a = pa;
	const Monitor *b = pb;
	if (a->x != b->x)
		return a->x - b->x;
	if (a->y != b->y)
		return a->y - b->y;
	if (a->w != b->w)
		return a->w - b->w;
	return a->h - b->h;
}

static int sort_and_dedupe_monitors(Monitor *mons, int n)
{
	if (!mons || n <= 0)
		return 0;

	qsort(mons, (size_t)n, sizeof(*mons), cmp_monitor_geom);
	int unique = 1;
	for (int i = 1; i < n; i++) {
		if (cmp_monitor_geom(&mons[i], &mons[unique - 1]) == 0)
			continue;
		mons[unique++] = mons[i];
	}

	return unique;
}

static int query_randr_monitors(Monitor *out, int cap)
{
	if (!out || cap <= 0 || !randr_enabled)
		return 0;

#if RANDR_MAJOR > 1 || (RANDR_MAJOR == 1 && RANDR_MINOR >= 5)
	if (randr_major_version > 1 || (randr_major_version == 1 && randr_minor_version >= 5)) {
		int query_n = 0;
		XRRMonitorInfo *info = XRRGetMonitors(dpy, root, True, &query_n);
		if (!info || query_n <= 0) {
			if (info)
				XRRFreeMonitors(info);
			return 0;
		}

		int n = MIN(query_n, cap);
		if (query_n > cap)
			fprintf(stderr, "cupidwm: RandR monitor count %d exceeds MAX_MONITORS=%d, truncating\n", query_n, cap);

		for (int i = 0; i < n; i++)
			reset_monitor_slot(&out[i], info[i].x, info[i].y, info[i].width, info[i].height);

		XRRFreeMonitors(info);
		return sort_and_dedupe_monitors(out, n);
	}
#endif

	return 0;
}

static int query_randr_crtcs(Monitor *out, int cap)
{
	if (!out || cap <= 0 || !randr_enabled)
		return 0;
	if (randr_major_version < 1 || (randr_major_version == 1 && randr_minor_version < 2))
		return 0;

	XRRScreenResources *res = NULL;
#if RANDR_MAJOR > 1 || (RANDR_MAJOR == 1 && RANDR_MINOR >= 3)
	res = XRRGetScreenResourcesCurrent(dpy, root);
#endif
	if (!res)
		res = XRRGetScreenResources(dpy, root);
	if (!res)
		return 0;

	int n = 0;
	Bool warned = False;
	for (int i = 0; i < res->ncrtc; i++) {
		XRRCrtcInfo *ci = XRRGetCrtcInfo(dpy, res, res->crtcs[i]);
		if (!ci)
			continue;

		Bool active = (ci->mode != None && ci->noutput > 0 && ci->width > 0 && ci->height > 0);
		if (active) {
			if (n < cap)
				reset_monitor_slot(&out[n++], ci->x, ci->y, ci->width, ci->height);
			else if (!warned) {
				fprintf(stderr, "cupidwm: RandR CRTC count exceeds MAX_MONITORS=%d, truncating\n", cap);
				warned = True;
			}
		}

		XRRFreeCrtcInfo(ci);
	}

	XRRFreeScreenResources(res);
	return sort_and_dedupe_monitors(out, n);
}

static int query_xinerama_monitors(Monitor *out, int cap)
{
	if (!out || cap <= 0 || !XineramaIsActive(dpy))
		return 0;

	int query_n = 0;
	XineramaScreenInfo *info = XineramaQueryScreens(dpy, &query_n);
	if (!info || query_n <= 0) {
		if (info)
			XFree(info);
		return 0;
	}

	int n = MIN(query_n, cap);
	if (query_n > cap)
		fprintf(stderr, "cupidwm: monitor count %d exceeds MAX_MONITORS=%d, truncating\n", query_n, cap);

	for (int i = 0; i < n; i++)
		reset_monitor_slot(&out[i], info[i].x_org, info[i].y_org, info[i].width, info[i].height);

	XFree(info);
	return sort_and_dedupe_monitors(out, n);
}

static int query_active_monitors(Monitor *out, int cap, Bool *used_randr)
{
	if (used_randr)
		*used_randr = False;
	if (!out || cap <= 0)
		return 0;

	int n = query_randr_monitors(out, cap);
	if (n > 0) {
		if (used_randr)
			*used_randr = True;
		return n;
	}

	n = query_randr_crtcs(out, cap);
	if (n > 0) {
		if (used_randr)
			*used_randr = True;
		return n;
	}

	n = query_xinerama_monitors(out, cap);
	if (n > 0)
		return n;

	reset_monitor_slot(out, 0, 0, XDisplayWidth(dpy, DefaultScreen(dpy)), XDisplayHeight(dpy, DefaultScreen(dpy)));
	return 1;
}

static unsigned long long monitor_topology_signature(const Monitor *monitor_list, int count,
                                                     int display_width, int display_height)
{
	unsigned long long sig = 1469598103934665603ULL;

	sig = mix_u64(sig, (unsigned long long)(unsigned int)display_width);
	sig = mix_u64(sig, (unsigned long long)(unsigned int)display_height);
	for (int i = 0; i < count; i++) {
		sig = mix_u64(sig, (unsigned long long)(unsigned int)monitor_list[i].x);
		sig = mix_u64(sig, (unsigned long long)(unsigned int)monitor_list[i].y);
		sig = mix_u64(sig, (unsigned long long)(unsigned int)monitor_list[i].w);
		sig = mix_u64(sig, (unsigned long long)(unsigned int)monitor_list[i].h);
	}

	return sig;
}

static void snapshot_monitor_topology(unsigned long long *sig_out, int *count_out)
{
	Monitor discovered[MAX_MONITORS];
	int count = query_active_monitors(discovered, MAX_MONITORS, NULL);
	unsigned long long sig = monitor_topology_signature(discovered, count,
	                                                    XDisplayWidth(dpy, DefaultScreen(dpy)),
	                                                    XDisplayHeight(dpy, DefaultScreen(dpy)));

	if (sig_out)
		*sig_out = sig;
	if (count_out)
		*count_out = count;
}

static void monitor_topology_seed_current(void)
{
	if (!mons || n_mons <= 0) {
		monitor_topology_prev_count = -1;
		monitor_topology_prev_sig = 0;
		return;
	}

	monitor_topology_prev_count = n_mons;
	monitor_topology_prev_sig = monitor_topology_signature(mons, n_mons, scr_width, scr_height);
}

Bool monitor_topology_changed(void)
{
	unsigned long long sig = 0;
	int count = 0;
	snapshot_monitor_topology(&sig, &count);

	if (monitor_topology_prev_count < 0) {
		monitor_topology_prev_count = count;
		monitor_topology_prev_sig = sig;
		return False;
	}

	Bool changed = (count != monitor_topology_prev_count || sig != monitor_topology_prev_sig);
	monitor_topology_prev_count = count;
	monitor_topology_prev_sig = sig;
	return changed;
}

static void session_display_token(char *out, size_t outsz)
{
	if (!out || outsz == 0)
		return;

	out[0] = '\0';
	const char *display = getenv("DISPLAY");
	if (!display || !display[0])
		return;

	size_t j = 0;
	for (size_t i = 0; display[i] && j < outsz - 1; i++) {
		unsigned char ch = (unsigned char)display[i];
		if ((ch >= 'a' && ch <= 'z') ||
		    (ch >= 'A' && ch <= 'Z') ||
		    (ch >= '0' && ch <= '9') ||
		    ch == '-' || ch == '_')
			out[j++] = (char)ch;
		else
			out[j++] = '_';
	}
	out[j] = '\0';
}

static int session_ensure_private_tmp_dir(char *out_dir, size_t outsz)
{
	if (!out_dir || outsz == 0)
		return -1;

	snprintf(out_dir, outsz, "/tmp/cupidwm-%u", (unsigned)getuid());
	if (mkdir(out_dir, 0700) < 0 && errno != EEXIST)
		return -1;

	struct stat st;
	if (stat(out_dir, &st) < 0)
		return -1;
	if (!S_ISDIR(st.st_mode) || st.st_uid != getuid() || (st.st_mode & 077) != 0)
		return -1;

	return 0;
}

static Bool session_join_path(char *out, size_t outsz, const char *base, const char *name)
{
	if (!out || outsz == 0 || !base || !base[0] || !name || !name[0])
		return False;

	size_t base_len = strlen(base);
	while (base_len > 1 && base[base_len - 1] == '/')
		base_len--;

	size_t name_len = strlen(name);
	if (base_len + 1 + name_len + 1 > outsz)
		return False;

	memcpy(out, base, base_len);
	out[base_len] = '/';
	memcpy(out + base_len + 1, name, name_len);
	out[base_len + 1 + name_len] = '\0';
	return True;
}

static const char *session_state_path(void)
{
	if (session_state_path_buf[0])
		return session_state_path_buf;

	char display_tok[64] = {0};
	session_display_token(display_tok, sizeof(display_tok));

	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (runtime_dir && runtime_dir[0]) {
		char name[128] = {0};
		if (display_tok[0])
			snprintf(name, sizeof(name), "cupidwm-%s.session", display_tok);
		else
			snprintf(name, sizeof(name), "cupidwm.session");

		if (!session_join_path(session_state_path_buf, sizeof(session_state_path_buf), runtime_dir, name))
			return NULL;
		return session_state_path_buf;
	}

	char tmp_dir[PATH_MAX] = {0};
	if (session_ensure_private_tmp_dir(tmp_dir, sizeof(tmp_dir)) < 0)
		return NULL;

	char name[128] = {0};
	if (display_tok[0])
		snprintf(name, sizeof(name), "cupidwm-%s.session", display_tok);
	else
		snprintf(name, sizeof(name), "cupidwm.session");

	if (!session_join_path(session_state_path_buf, sizeof(session_state_path_buf), tmp_dir, name))
		return NULL;

	return session_state_path_buf;
}

void session_state_save(void)
{
	const char *path = session_state_path();
	if (!path || !path[0])
		return;

	char tmp_path[PATH_MAX] = {0};
	snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

	FILE *f = fopen(tmp_path, "w");
	if (!f)
		return;

	fprintf(f, "version 1\n");
	fprintf(f, "current_mon %d\n", current_mon);
	fprintf(f, "current_ws %d\n", current_ws);
	fprintf(f, "n_mons %d\n", n_mons);

	for (int i = 0; i < n_mons; i++)
		fprintf(f, "monitor %d %d %d\n", i, mons[i].view_ws, mons[i].prev_view_ws);

	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		fprintf(f, "workspace %d %d\n", ws, workspace_layout_for(ws));
		Window fw = workspace_states[ws].focused ? workspace_states[ws].focused->win : None;
		fprintf(f, "focus %d 0x%lx\n", ws, (unsigned long)fw);
		fprintf(f, "wsname %d %s\n", ws, workspace_name_for(ws));
	}

	for (int i = 0; i < MAX_SCRATCHPADS; i++) {
		Window w = scratchpads[i].client ? scratchpads[i].client->win : None;
		fprintf(f, "scratchpad %d 0x%lx %d\n", i, (unsigned long)w, scratchpads[i].enabled ? 1 : 0);
	}

	fclose(f);
	rename(tmp_path, path);
}

void session_state_clear(void)
{
	const char *path = session_state_path();
	if (!path || !path[0])
		return;
	unlink(path);
}

void session_state_restore(void)
{
	const char *path = session_state_path();
	if (!path || !path[0])
		return;

	FILE *f = fopen(path, "r");
	if (!f)
		return;

	int restore_current_mon = current_mon;
	int restore_current_ws = current_ws;
	int restore_mon_ws[MAX_MONITORS];
	int restore_mon_prev[MAX_MONITORS];
	int restore_ws_layout[NUM_WORKSPACES];
	Window restore_ws_focus[NUM_WORKSPACES];
	char restore_ws_name[NUM_WORKSPACES][WORKSPACE_NAME_MAX];
	Bool restore_ws_name_set[NUM_WORKSPACES];
	Window restore_sp_win[MAX_SCRATCHPADS];
	int restore_sp_enabled[MAX_SCRATCHPADS];

	for (int i = 0; i < MAX_MONITORS; i++) {
		restore_mon_ws[i] = -1;
		restore_mon_prev[i] = -1;
	}
	for (int i = 0; i < NUM_WORKSPACES; i++) {
		restore_ws_layout[i] = -1;
		restore_ws_focus[i] = None;
		restore_ws_name[i][0] = '\0';
		restore_ws_name_set[i] = False;
	}
	for (int i = 0; i < MAX_SCRATCHPADS; i++) {
		restore_sp_win[i] = None;
		restore_sp_enabled[i] = -1;
	}

	char line[512];
	while (fgets(line, sizeof(line), f)) {
		int a = 0, b = 0, c = 0;
		unsigned long w = 0;

		if (sscanf(line, "current_mon %d", &a) == 1) {
			restore_current_mon = a;
			continue;
		}
		if (sscanf(line, "current_ws %d", &a) == 1) {
			restore_current_ws = a;
			continue;
		}
		if (sscanf(line, "monitor %d %d %d", &a, &b, &c) == 3) {
			if (a >= 0 && a < MAX_MONITORS) {
				restore_mon_ws[a] = b;
				restore_mon_prev[a] = c;
			}
			continue;
		}
		if (sscanf(line, "workspace %d %d", &a, &b) == 2) {
			if (a >= 0 && a < NUM_WORKSPACES)
				restore_ws_layout[a] = b;
			continue;
		}
		if (strncmp(line, "wsname ", 7) == 0) {
			char *name_ptr = line + 7;
			char *end = NULL;
			long ws = strtol(name_ptr, &end, 10);
			if (end != name_ptr && ws >= 0 && ws < NUM_WORKSPACES) {
				while (*end == ' ' || *end == '\t')
					end++;
				workspace_name_trim_copy(restore_ws_name[ws], sizeof(restore_ws_name[ws]), end);
				if (restore_ws_name[ws][0])
					restore_ws_name_set[ws] = True;
			}
			continue;
		}
		if (sscanf(line, "focus %d 0x%lx", &a, &w) == 2) {
			if (a >= 0 && a < NUM_WORKSPACES)
				restore_ws_focus[a] = (Window)w;
			continue;
		}
		if (sscanf(line, "scratchpad %d 0x%lx %d", &a, &w, &b) == 3) {
			if (a >= 0 && a < MAX_SCRATCHPADS) {
				restore_sp_win[a] = (Window)w;
				restore_sp_enabled[a] = b;
			}
			continue;
		}
	}

	fclose(f);

	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		if (restore_ws_layout[ws] < 0)
			continue;
		workspace_states[ws].layout = CLAMP(restore_ws_layout[ws], 0, (int)LENGTH(layouts) - 1);
	}
	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		if (!restore_ws_name_set[ws])
			continue;
		Bool changed = False;
		workspace_name_assign(ws, restore_ws_name[ws], False, &changed);
		(void)changed;
	}
	publish_desktop_names();

	for (int i = 0; i < n_mons && i < MAX_MONITORS; i++) {
		if (restore_mon_ws[i] >= 0)
			set_monitor_workspace(i, CLAMP(restore_mon_ws[i], 0, NUM_WORKSPACES - 1));
		if (restore_mon_prev[i] >= 0)
			mons[i].prev_view_ws = CLAMP(restore_mon_prev[i], 0, NUM_WORKSPACES - 1);
	}

	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		Client *fc = NULL;
		if (restore_ws_focus[ws] != None)
			fc = find_client(restore_ws_focus[ws]);
		workspace_states[ws].focused = fc;
		ws_focused[ws] = fc;
	}

	for (int i = 0; i < MAX_SCRATCHPADS; i++) {
		scratchpads[i].client = NULL;
		scratchpads[i].enabled = False;
		if (restore_sp_win[i] == None)
			continue;
		Client *c = find_client(restore_sp_win[i]);
		if (!c)
			continue;
		scratchpads[i].client = c;
		scratchpads[i].enabled = (restore_sp_enabled[i] > 0) ? True : False;
	}

	current_mon = CLAMP(restore_current_mon, 0, MAX(0, n_mons - 1));
	sync_active_monitor_state();
	if (restore_current_ws >= 0 && restore_current_ws < NUM_WORKSPACES)
		set_monitor_workspace(current_mon, restore_current_ws);
	sync_active_monitor_state();

	refresh_client_visibility();
	tile();

	Client *target_focus = ws_focused[current_ws];
	if (!target_focus || !client_is_visible(target_focus))
		target_focus = first_visible_client(current_ws, current_mon, NULL);
	set_input_focus(target_focus, False, True);

	publish_current_desktop();
	update_client_desktop_properties();
	update_net_client_list();
}

Client *add_client(Window w, int ws)
{
	Client *c = malloc(sizeof(Client));
	if (!c) {
		fprintf(stderr, "cupidwm: could not alloc memory for client\n");
		return NULL;
	}

	c->win = w;
	c->map_seq = next_client_map_seq++;
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
	c->modal_forced_floating = False;
	c->fullscreen = False;
	c->hidden = False;
	c->sticky = False;
	c->maximized_horz = False;
	c->maximized_vert = False;
	c->max_forced_floating = False;
	c->max_restore_valid = False;
	c->max_restore_floating = False;
	c->max_restore_x = 0;
	c->max_restore_y = 0;
	c->max_restore_w = 0;
	c->max_restore_h = 0;
	c->mapped = True;
	c->no_focus_on_map = False;
	c->suppress_enter_focus_once = False;
	c->suppress_focus_until_sec = 0;
	c->ignore_unmap_events = 0;
	c->custom_stack_height = 0;

	if (global_floating)
		c->floating = True;

	apply_client_rule_defaults(c);

	/* remember first created client per workspace as a fallback */
	if (!ws_focused[ws])
		ws_focused[ws] = c;

	if (client_should_be_visible(c) && !focused) {
		focused = c;
		current_mon = c->mon;
		sync_active_monitor_state();
	}

	/* associate client with workspace n */
	unsigned long desktop = c->sticky ? 0xFFFFFFFFUL : (unsigned long)ws;
	XChangeProperty(dpy, w, atoms[ATOM_NET_WM_DESKTOP], XA_CARDINAL, 32,
				        PropModeReplace, (unsigned char *)&desktop, 1);
	XRaiseWindow(dpy, w);
	update_net_client_list();
	{
		char details[160];
		snprintf(details, sizeof(details), "win=0x%lx ws=%d mon=%d", (unsigned long)c->win, c->ws + 1, c->mon);
		ipc_notify_event("client_add", details);
	}
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
		XConfigureEvent notify = {
			.type = ConfigureNotify,
			.display = dpy,
			.event = c->win,
			.window = c->win,
			.x = c->x,
			.y = c->y,
			.width = c->w,
			.height = c->h,
			.border_width = 0,
			.above = None,
			.override_redirect = False,
		};
		XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&notify);

		XRaiseWindow(dpy, c->win);
		update_net_client_list();
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
		XConfigureEvent notify = {
			.type = ConfigureNotify,
			.display = dpy,
			.event = c->win,
			.window = c->win,
			.x = c->x,
			.y = c->y,
			.width = c->w,
			.height = c->h,
			.border_width = user_config.border_width,
			.above = None,
			.override_redirect = False,
		};
		XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&notify);

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

void change_workspace(int ws)
{
	if (ws < 0 || ws >= NUM_WORKSPACES || !mons || n_mons <= 0)
		return;
	if (ws == current_ws)
		return;

	/* remember last focus for workspace we are leaving */
	if (focused && focused->ws == current_ws)
		ws_focused[current_ws] = focused;

	in_ws_switch = True;

	Bool visible_scratchpads[MAX_SCRATCHPADS] = {False};
	for (int i = 0; i < MAX_SCRATCHPADS; i++) {
		if (scratchpads[i].client && scratchpads[i].enabled) {
			visible_scratchpads[i] = True;
		}
	}

	int old_ws = current_ws;
	set_monitor_workspace(current_mon, ws);
	sync_active_monitor_state();

	/* move visible scratchpads to new workspace - linked list only, no X calls */
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
			c->mon = current_mon;
		}
	}

	/* Pre-position new workspace windows while they are still unmapped.
	 * tile() only calls XMoveResizeWindow (no map/unmap), so this is
	 * invisible and sets up correct geometry before they become visible. */
	tile();

	/* Grab server only for the map/unmap step to prevent tearing.
	 * The critical section is now as short as possible; all other work
	 * (focus, EWMH properties, session save) runs after the grab is
	 * released so other clients are not frozen longer than necessary. */
	XGrabServer(dpy);

	/* Update EWMH desktop property for moved scratchpads */
	for (int i = 0; i < MAX_SCRATCHPADS; i++) {
		if (visible_scratchpads[i] && scratchpads[i].client) {
			Client *c = scratchpads[i].client;
			unsigned long desktop = c->sticky ? 0xFFFFFFFFUL : (unsigned long)current_ws;
			XChangeProperty(dpy, c->win, atoms[ATOM_NET_WM_DESKTOP], XA_CARDINAL, 32,
			                PropModeReplace, (unsigned char *)&desktop, 1);
		}
	}

	refresh_client_visibility();

	XUngrabServer(dpy);
	XSync(dpy, False);

	/* restore last focused client for this workspace */
	focused = ws_focused[current_ws];

	if (focused && !client_is_visible(focused))
		focused = NULL;

	if (!focused && workspaces[current_ws]) {
		focused = first_visible_client(current_ws, current_mon, NULL);
		if (focused)
			current_mon = CLAMP(focused->mon, 0, n_mons - 1);
		else
			focused = NULL;
	}

	/* try focus scratchpad if no other window available */
	for (int i = 0; i < MAX_SCRATCHPADS; i++) {
		if (!focused && visible_scratchpads[i] && scratchpads[i].client) {
			focused = scratchpads[i].client;
			break;
		}
	}

	/* Focus without warping inside the grab; warp separately now that the
	 * grab is released so warp_cursor does not block all clients. */
	set_input_focus(focused, False, False);
	if (focused && user_config.warp_cursor)
		warp_cursor(focused);

	previous_workspace = old_ws;
	publish_current_desktop();
	update_client_desktop_properties();
	session_state_save();

	in_ws_switch = False;
	{
		char details[96];
		snprintf(details, sizeof(details), "workspace=%d monitor=%d", current_ws + 1, current_mon);
		ipc_notify_event("workspace", details);
	}
}

int check_parent(pid_t p, pid_t c)
{
	/* Cap the ancestry walk to avoid O(depth) /proc reads in the event loop.
	 * Real process trees are shallow; anything beyond 16 hops is not a match. */
	int depth = 0;
	while (p != c && c != 0 && depth < 16) {
		c = get_parent_process(c);
		depth++;
	}

	return (p == c) ? (int)c : 0;
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
	WorkspaceState *state = workspace_state_for(current_ws);
	if (state && state->gaps > 0) {
		state->gaps--;
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
	while ((!client_is_visible_on_monitor(c, current_mon)) && c != start);

	/* if we return to start: */
	if (!client_is_visible_on_monitor(c, current_mon))
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
	} while ((!client_is_visible_on_monitor(c, current_mon)) && c != start);

	/* this stops invisible windows being detected or focused */
	if (!client_is_visible_on_monitor(c, current_mon))
		return;

	focused = c;
	current_mon = c->mon;
	set_input_focus(focused, True, True);
}

static Bool direction_accepts_delta(int direction, int dx, int dy)
{
	switch (direction) {
	case DIR_LEFT:
		return dx < 0;
	case DIR_RIGHT:
		return dx > 0;
	case DIR_UP:
		return dy < 0;
	case DIR_DOWN:
		return dy > 0;
	default:
		return False;
	}
}

static void direction_primary_secondary(int direction, int dx, int dy, long *primary, long *secondary)
{
	if (!primary || !secondary)
		return;

	switch (direction) {
	case DIR_LEFT:
		*primary = (long)(-dx);
		*secondary = (long)llabs((long long)dy);
		break;
	case DIR_RIGHT:
		*primary = (long)dx;
		*secondary = (long)llabs((long long)dy);
		break;
	case DIR_UP:
		*primary = (long)(-dy);
		*secondary = (long)llabs((long long)dx);
		break;
	case DIR_DOWN:
		*primary = (long)dy;
		*secondary = (long)llabs((long long)dx);
		break;
	default:
		*primary = 0;
		*secondary = LONG_MAX;
		break;
	}
}

static Bool client_is_directional_candidate(const Client *c, int mon, Bool tiled_only)
{
	if (!client_is_visible_on_monitor(c, mon))
		return False;
	if (!tiled_only)
		return True;
	return !c->floating && !c->fullscreen;
}

static Client *directional_client_target(Client *base, int mon, int ws, int direction, Bool tiled_only)
{
	if (ws < 0 || ws >= NUM_WORKSPACES || mon < 0 || mon >= n_mons)
		return NULL;

	Client *anchor = base;
	if (!anchor || !client_is_directional_candidate(anchor, mon, tiled_only)) {
		for (Client *c = workspaces[ws]; c; c = c->next) {
			if (!client_is_directional_candidate(c, mon, tiled_only))
				continue;
			anchor = c;
			break;
		}
	}

	if (!anchor)
		return NULL;

	int bx = anchor->x + anchor->w / 2;
	int by = anchor->y + anchor->h / 2;
	long best_score = LONG_MAX;
	Client *best = NULL;

	for (Client *c = workspaces[ws]; c; c = c->next) {
		if (c == anchor || !client_is_directional_candidate(c, mon, tiled_only))
			continue;

		int cx = c->x + c->w / 2;
		int cy = c->y + c->h / 2;
		int dx = cx - bx;
		int dy = cy - by;

		if (!direction_accepts_delta(direction, dx, dy))
			continue;

		long primary = 0;
		long secondary = 0;
		direction_primary_secondary(direction, dx, dy, &primary, &secondary);
		long score = primary * primary * 4 + secondary * secondary;

		if (!best || score < best_score) {
			best = c;
			best_score = score;
		}
	}

	return best;
}

static int directional_monitor_target(int mon, int direction)
{
	if (!mons || n_mons <= 1)
		return -1;

	mon = CLAMP(mon, 0, n_mons - 1);
	int bx = mons[mon].x + mons[mon].w / 2;
	int by = mons[mon].y + mons[mon].h / 2;
	long best_score = LONG_MAX;
	int best = -1;

	for (int i = 0; i < n_mons; i++) {
		if (i == mon)
			continue;

		int cx = mons[i].x + mons[i].w / 2;
		int cy = mons[i].y + mons[i].h / 2;
		int dx = cx - bx;
		int dy = cy - by;

		if (!direction_accepts_delta(direction, dx, dy))
			continue;

		long primary = 0;
		long secondary = 0;
		direction_primary_secondary(direction, dx, dy, &primary, &secondary);
		long score = primary * primary * 4 + secondary * secondary;

		if (best < 0 || score < best_score) {
			best = i;
			best_score = score;
		}
	}

	return best;
}

void focus_dir(int direction)
{
	Client *base = focused;
	if (!base || !client_is_visible_on_monitor(base, current_mon))
		base = first_visible_client(current_ws, current_mon, NULL);

	Client *target = directional_client_target(base, current_mon, current_ws, direction, False);
	if (!target)
		return;

	set_input_focus(target, True, True);
}

void swap_dir(int direction)
{
	if (!focused || !client_is_visible_on_monitor(focused, current_mon) || focused->floating || focused->fullscreen)
		return;

	Client *target = directional_client_target(focused, current_mon, current_ws, direction, True);
	if (!target)
		return;

	swap_clients(focused, target);
	tile();
	set_input_focus(focused, True, False);
}

void move_dir(int direction)
{
	if (!focused || !client_is_visible_on_monitor(focused, current_mon) || focused->floating || focused->fullscreen)
		return;

	Client *target = directional_client_target(focused, current_mon, current_ws, direction, True);
	if (!target)
		return;

	Client **from = &workspaces[current_ws];
	while (*from && *from != focused)
		from = &(*from)->next;

	if (!*from)
		return;

	Client *moving = *from;
	*from = moving->next;

	Client **to = &workspaces[current_ws];
	while (*to && *to != target)
		to = &(*to)->next;

	if (!*to) {
		moving->next = workspaces[current_ws];
		workspaces[current_ws] = moving;
		return;
	}

	moving->next = *to;
	*to = moving;

	tile();
	set_input_focus(moving, True, False);
}

void focus_mon_dir(int direction)
{
	int target_mon = directional_monitor_target(current_mon, direction);
	if (target_mon < 0)
		return;

	int target_ws = mons[target_mon].view_ws;
	Client *target_client = first_visible_client(target_ws, target_mon, NULL);
	if (target_client) {
		current_mon = target_mon;
		sync_active_monitor_state();
		set_input_focus(target_client, True, True);
		return;
	}

	current_mon = target_mon;
	sync_active_monitor_state();
	set_input_focus(NULL, False, False);
	int center_x = mons[target_mon].x + mons[target_mon].w / 2;
	int center_y = mons[target_mon].y + mons[target_mon].h / 2;
	XWarpPointer(dpy, None, root, 0, 0, 0, 0, center_x, center_y);
	XSync(dpy, False);
}

void send_to_mon_dir(int direction)
{
	if (!focused || n_mons <= 1)
		return;

	int origin_mon = CLAMP(focused->mon, 0, n_mons - 1);
	int target_mon = directional_monitor_target(origin_mon, direction);
	if (target_mon < 0 || target_mon == origin_mon)
		return;

	while (focused && focused->mon != target_mon) {
		int current = CLAMP(focused->mon, 0, n_mons - 1);
		int next = (current + 1) % n_mons;
		int prev = (current - 1 + n_mons) % n_mons;

		if (target_mon == next)
			move_next_mon();
		else if (target_mon == prev)
			move_prev_mon();
		else {
			int next_steps = (target_mon - current + n_mons) % n_mons;
			int prev_steps = (current - target_mon + n_mons) % n_mons;
			if (next_steps <= prev_steps)
				move_next_mon();
			else
				move_prev_mon();
		}
	}
}

void focus_next_mon(void)
{
	if (n_mons <= 1)
		return;

	int target_mon = (current_mon + 1) % n_mons;
	int target_ws = mons[target_mon].view_ws;
	Client *target_client = first_visible_client(target_ws, target_mon, NULL);

	if (target_client) {
		/* focus the window on target monitor */
		focused = target_client;
		current_mon = target_mon;
		sync_active_monitor_state();
		set_input_focus(focused, True, True);
	}
	else {
		/* no windows on target monitor, just move cursor to center and update current_mon */
		current_mon = target_mon;
		sync_active_monitor_state();
		set_input_focus(NULL, False, False);
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
	int target_ws = mons[target_mon].view_ws;
	Client *target_client = first_visible_client(target_ws, target_mon, NULL);

	if (target_client) {
		/* focus the window on target monitor */
		focused = target_client;
		current_mon = target_mon;
		sync_active_monitor_state();
		set_input_focus(focused, True, True);
	}
	else {
		current_mon = target_mon;
		sync_active_monitor_state();
		set_input_focus(NULL, False, False);
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

static Bool str_contains_ci(const char *haystack, const char *needle);

static Bool rule_matches_window(const Rule *r, const XClassHint *ch, const char *title)
{
	if (!r)
		return False;

	int mode = r->match_mode;
	if (mode < RuleMatchExact || mode > RuleMatchRegex)
		mode = RuleMatchExact;

	Bool cls_match = True;
	Bool inst_match = True;
	Bool title_match = True;

	if (r->class)
		cls_match = False;
	if (r->instance)
		inst_match = False;
	if (r->title)
		title_match = False;

	if (r->class && ch->res_class) {
		if (mode == RuleMatchExact)
			cls_match = (strcasecmp(ch->res_class, r->class) == 0);
		else if (mode == RuleMatchSubstring)
			cls_match = str_contains_ci(ch->res_class, r->class);
		else {
			regex_t re;
			if (regcomp(&re, r->class, REG_EXTENDED | REG_NOSUB | REG_ICASE) == 0) {
				cls_match = (regexec(&re, ch->res_class, 0, NULL, 0) == 0);
				regfree(&re);
			}
		}
	}

	if (r->instance && ch->res_name) {
		if (mode == RuleMatchExact)
			inst_match = (strcasecmp(ch->res_name, r->instance) == 0);
		else if (mode == RuleMatchSubstring)
			inst_match = str_contains_ci(ch->res_name, r->instance);
		else {
			regex_t re;
			if (regcomp(&re, r->instance, REG_EXTENDED | REG_NOSUB | REG_ICASE) == 0) {
				inst_match = (regexec(&re, ch->res_name, 0, NULL, 0) == 0);
				regfree(&re);
			}
		}
	}

	if (r->title && title) {
		if (mode == RuleMatchExact)
			title_match = (strcasecmp(title, r->title) == 0);
		else if (mode == RuleMatchSubstring)
			title_match = str_contains_ci(title, r->title);
		else {
			regex_t re;
			if (regcomp(&re, r->title, REG_EXTENDED | REG_NOSUB | REG_ICASE) == 0) {
				title_match = (regexec(&re, title, 0, NULL, 0) == 0);
				regfree(&re);
			}
		}
	}

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
		int npath = snprintf(fdpath, sizeof(fdpath), "%s/%s", path, ent->d_name);
		if (npath < 0 || (size_t)npath >= sizeof(fdpath))
			continue;
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

static Bool rule_has_geometry(const Rule *r)
{
	if (!r)
		return False;

	return r->x >= 0 || r->y >= 0 || r->w > 0 || r->h > 0 || r->centered;
}

static void apply_rule_geometry(Client *c, const Rule *r)
{
	if (!c || !r || !rule_has_geometry(r) || !mons || n_mons <= 0)
		return;

	int mon = CLAMP(c->mon, 0, n_mons - 1);
	int work_x = 0, work_y = 0, work_w = 0, work_h = 0;
	monitor_workarea(mon, &work_x, &work_y, &work_w, &work_h);

	if (r->w > 0)
		c->w = MAX(MIN_WINDOW_SIZE, r->w);
	if (r->h > 0)
		c->h = MAX(MIN_WINDOW_SIZE, r->h);

	if (r->centered) {
		c->x = work_x + (work_w - c->w) / 2;
		c->y = work_y + (work_h - c->h) / 2;
	}
	else {
		if (r->x >= 0)
			c->x = work_x + r->x;
		if (r->y >= 0)
			c->y = work_y + r->y;
	}

	if (c->x < work_x)
		c->x = work_x;
	if (c->y < work_y)
		c->y = work_y;
	if (c->x + c->w > work_x + work_w)
		c->x = MAX(work_x, work_x + work_w - c->w);
	if (c->y + c->h > work_y + work_h)
		c->y = MAX(work_y, work_y + work_h - c->h);

	XMoveResizeWindow(dpy, c->win, c->x, c->y, (unsigned int)c->w, (unsigned int)c->h);
}

static void apply_client_rule_defaults(Client *c)
{
	if (!c)
		return;

	const Rule *r = rule_for_window(c->win);
	if (!r)
		return;

	if (r->monitor >= 0 && r->monitor < n_mons)
		c->mon = r->monitor;

	if (r->is_floating)
		c->floating = True;
	if (r->start_fullscreen)
		c->fullscreen = True;

	if (r->sticky)
		c->sticky = True;
	if (r->no_focus_on_map)
		c->no_focus_on_map = True;

	if (r->scratchpad >= 0 && r->scratchpad < MAX_SCRATCHPADS) {
		scratchpads[r->scratchpad].client = c;
		scratchpads[r->scratchpad].enabled = True;
	}

	if (r->skip_taskbar)
		window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_SKIP_TASKBAR], True);
	if (r->skip_pager)
		window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_SKIP_PAGER], True);

	apply_rule_geometry(c, r);
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

void move_client_to_workspace(Client *moved, int ws, Bool focus_target)
{
	if (!moved || ws < 0 || ws >= NUM_WORKSPACES || moved->ws == ws)
		return;

	int from_ws = moved->ws;
	Bool from_current = monitor_views_workspace(moved->mon, from_ws);
	Bool to_current = monitor_views_workspace(moved->mon, ws);
	Bool was_focused = (focused == moved);

	if (from_current && moved->mapped) {
		moved->ignore_unmap_events += 2;
		XUnmapWindow(dpy, moved->win);
		moved->mapped = True;
	}

	Client **pp = &workspaces[from_ws];
	while (*pp && *pp != moved)
		pp = &(*pp)->next;

	if (!*pp)
		return;

	*pp = moved->next;

	moved->next = workspaces[ws];
	workspaces[ws] = moved;
	moved->ws = ws;

	unsigned long desktop = moved->sticky ? 0xFFFFFFFFUL : (unsigned long)ws;
	XChangeProperty(dpy, moved->win, atoms[ATOM_NET_WM_DESKTOP], XA_CARDINAL, 32,
		        PropModeReplace, (unsigned char *)&desktop, 1);

	if (from_ws >= 0 && from_ws < NUM_WORKSPACES && ws_focused[from_ws] == moved)
		ws_focused[from_ws] = NULL;
	ws_focused[ws] = moved;

	if (to_current) {
		if (!moved->mapped) {
			XMapWindow(dpy, moved->win);
			moved->mapped = True;
		}
		if (focus_target)
			set_input_focus(moved, True, True);
	}

	update_net_client_list();

	if (from_current) {
		tile();
		if (was_focused || !focused || !client_is_visible(focused)) {
			Client *next_focus = first_visible_client(current_ws, current_mon, NULL);
			set_input_focus(next_focus, True, False);
		}
		else {
			update_borders();
		}
		session_state_save();
		return;
	}

	if (to_current) {
		tile();
		if (!focus_target)
			update_borders();
	}

	session_state_save();
}

#include "status.c"
#include "bar.c"

#include "input.c"
#include "ipc.c"

void inc_gaps(void)
{
	WorkspaceState *state = workspace_state_for(current_ws);
	if (state)
		state->gaps++;
	tile();
	update_borders();
}

void init_defaults(void)
{
	workspace_names_init_defaults();

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
	user_config.status_allow_external_cmd = status_allow_external_cmd ? True : False;
	user_config.status_external_cmd = status_external_cmd;
	user_config.ipc_enable = ipc_enable ? True : False;
	user_config.ipc_socket_path = ipc_socket_path;

	int default_layout = layout_index_from_mode(LayoutTile);
	if (default_layout < 0)
		default_layout = 0;

	for (int i = 0; i < MAX_MONITORS; i++)
		user_config.master_width[i] = master_width_default;

	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		workspace_states[ws].layout = default_layout;
		workspace_states[ws].gaps = default_gaps;
		workspace_states[ws].focused = NULL;
		for (int mon = 0; mon < MAX_MONITORS; mon++)
			workspace_states[ws].master_width[mon] = master_width_default;
	}

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

	Client *moved = focused;
	int old_mon = moved->mon;
	int old_ws = moved->ws;
	int target_mon = (focused->mon + 1) % n_mons;
	int target_ws = mons[target_mon].view_ws;

	if (!relink_client_to_workspace(moved, target_ws))
		return;

	/* update window's monitor assignment */
	focused->mon = target_mon;
	current_mon = target_mon;
	sync_active_monitor_state();

	unsigned long desktop = focused->sticky ? 0xFFFFFFFFUL : (unsigned long)focused->ws;
	XChangeProperty(dpy, focused->win, atoms[ATOM_NET_WM_DESKTOP], XA_CARDINAL, 32,
	                PropModeReplace, (unsigned char *)&desktop, 1);
	if (old_ws != focused->ws && old_ws >= 0 && old_ws < NUM_WORKSPACES && ws_focused[old_ws] == moved)
		ws_focused[old_ws] = first_visible_client(old_ws, old_mon, moved);
	ws_focused[focused->ws] = focused;
	workspace_states[focused->ws].focused = focused;
	if (old_ws != focused->ws && old_ws >= 0 && old_ws < NUM_WORKSPACES &&
	    workspace_states[old_ws].focused == moved)
		workspace_states[old_ws].focused = ws_focused[old_ws];

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
	refresh_client_visibility();
	tile();
	update_net_client_list();

	/* follow the window with cursor if enabled */
	if (user_config.warp_cursor)
		warp_cursor(focused);

	update_borders();
}

void move_prev_mon(void)
{
	if (!focused || n_mons <= 1)
		return; /* no focused window or only one monitor */

	Client *moved = focused;
	int old_mon = moved->mon;
	int old_ws = moved->ws;
	int target_mon = (focused->mon - 1 + n_mons) % n_mons;
	int target_ws = mons[target_mon].view_ws;

	if (!relink_client_to_workspace(moved, target_ws))
		return;

	/* update window's monitor assignment */
	focused->mon = target_mon;
	current_mon = target_mon;
	sync_active_monitor_state();

	unsigned long desktop = focused->sticky ? 0xFFFFFFFFUL : (unsigned long)focused->ws;
	XChangeProperty(dpy, focused->win, atoms[ATOM_NET_WM_DESKTOP], XA_CARDINAL, 32,
	                PropModeReplace, (unsigned char *)&desktop, 1);
	if (old_ws != focused->ws && old_ws >= 0 && old_ws < NUM_WORKSPACES && ws_focused[old_ws] == moved)
		ws_focused[old_ws] = first_visible_client(old_ws, old_mon, moved);
	ws_focused[focused->ws] = focused;
	workspace_states[focused->ws].focused = focused;
	if (old_ws != focused->ws && old_ws >= 0 && old_ws < NUM_WORKSPACES &&
	    workspace_states[old_ws].focused == moved)
		workspace_states[old_ws].focused = ws_focused[old_ws];

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
	refresh_client_visibility();
	tile();
	update_net_client_list();

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
	if (!moved || moved->ws != current_ws || !client_is_visible_on_monitor(moved, current_mon)) {
		Client *candidate = ws_focused[current_ws];
		if (candidate && candidate->ws == current_ws && client_is_visible_on_monitor(candidate, current_mon))
			moved = candidate;
		else
			moved = NULL;

		if (!moved) {
			for (Client *c = workspaces[current_ws]; c; c = c->next) {
				if (!client_is_visible_on_monitor(c, current_mon))
					continue;
				moved = c;
				break;
			}
		}
	}

	if (!moved)
		return;

	move_client_to_workspace(moved, ws, False);
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

static void destroy_wm_windows(void)
{
	if (!dpy)
		return;

	if (mons) {
		for (int i = 0; i < n_mons; i++) {
			if (!mons[i].barwin)
				continue;
			XDestroyWindow(dpy, mons[i].barwin);
			mons[i].barwin = 0;
		}
	}

	if (wm_check_win) {
		XDestroyWindow(dpy, wm_check_win);
		wm_check_win = None;
	}
}

void quit(void)
{
	if (!running && dpy == NULL)
		return;

	running = False;
	session_state_save();

	/* Kill all clients on exit...

	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		for (Client *c = workspaces[ws]; c; c = c->next) {
			XUnmapWindow(dpy, c->win);
			XKillClient(dpy, c->win);
		}
	}
	*/

	ipc_cleanup();
	if (dpy) {
		destroy_wm_windows();
		XSync(dpy, False);
		if (cursor_move)
			XFreeCursor(dpy, cursor_move);
		if (cursor_normal)
			XFreeCursor(dpy, cursor_normal);
		if (cursor_resize)
			XFreeCursor(dpy, cursor_resize);
		XCloseDisplay(dpy);
		dpy = NULL;
	}
	puts("quitting...");
}

void reload_config(void)
{
	session_state_save();
	update_mons();
	setup_bars();
	update_struts();
	session_state_restore();
	update_borders();
	drawbars();
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
	session_state_save();
}

void resize_master_add(void)
{
	/* pick the monitor of the focused window (or 0 if none) */
	int m = focused ? focused->mon : 0;
	m = CLAMP(m, 0, MAX_MONITORS - 1);
	float *mw = workspace_master_width_for(current_ws, m);
	if (!mw)
		return;

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
	float *mw = workspace_master_width_for(current_ws, m);
	if (!mw)
		return;

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
	time_t last_monitor_probe = 0;
	int xfd = ConnectionNumber(dpy);
	ipc_setup();
	status_fifo_setup();

	while (running) {
		long timeout_ms = -1;
		time_t now = time(NULL);
		Bool handled_xevents = False;

		if (!status_text_override_active() && user_config.status_interval_sec > 0) {
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

		if (ipc_get_server_fd() >= 0)
			ipc_handle_connection();

		if (!running)
			break;

		if (XPending(dpy)) {
			while (running && XPending(dpy)) {
				XNextEvent(dpy, &xev);
				handle_xevent(&xev);
				handled_xevents = True;
			}
		} else {
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(xfd, &fds);
			int maxfd = xfd;

			int fifo_fd = status_fifo_fd();
			ipc_fdset_prepare(&fds, &maxfd);
			if (fifo_fd >= 0) {
				FD_SET(fifo_fd, &fds);
				maxfd = MAX(maxfd, fifo_fd);
			}

			struct timeval tv;
			struct timeval *tvp = NULL;
			if (timeout_ms >= 0) {
				tv.tv_sec = timeout_ms / 1000;
				tv.tv_usec = (timeout_ms % 1000) * 1000;
				tvp = &tv;
			}

			int selret;
			do {
				selret = select(maxfd + 1, &fds, NULL, NULL, tvp);
			} while (selret < 0 && errno == EINTR);

				if (selret > 0) {
					if (ipc_fdset_has_ready(&fds))
						ipc_handle_connection();

					if (fifo_fd >= 0 && FD_ISSET(fifo_fd, &fds))
						status_fifo_handle_readable();

					if (!running)
						break;

					if (FD_ISSET(xfd, &fds)) {
						while (running && XPending(dpy)) {
							XNextEvent(dpy, &xev);
							handle_xevent(&xev);
							handled_xevents = True;
						}
					}
				}
		}

		if (!running)
			break;

		if (handled_xevents)
			update_net_client_list();

		if (!running)
			break;

		if (user_config.focus_follows_mouse && drag_mode == DRAG_NONE && !in_ws_switch) {
			Window root_ret, child_ret;
			int root_x, root_y, win_x, win_y;
			unsigned int mask;

			if (XQueryPointer(dpy, root, &root_ret, &child_ret,
					  &root_x, &root_y, &win_x, &win_y, &mask) && child_ret != None) {
				Window w = find_toplevel(child_ret);
				if (w && w != root) {
					Client *c = find_client(w);
					if (c && client_is_visible(c) && c != focused) {
						if (c->suppress_focus_until_sec > 0) {
							long now_sec = (long)time(NULL);
							if (now_sec <= c->suppress_focus_until_sec)
								goto skip_pointer_focus;
							c->suppress_focus_until_sec = 0;
						}
						set_input_focus(c, True, False);
					}
				}
			}
		}
	skip_pointer_focus: ;

		now = time(NULL);
		if (!randr_enabled && (last_monitor_probe == 0 || now - last_monitor_probe >= 2)) {
			last_monitor_probe = now;
			if (monitor_topology_changed()) {
				update_mons();
				setup_bars();
				update_struts();
				tile();
				update_borders();
			}
		}

		if (!status_text_override_active() && user_config.status_interval_sec > 0 &&
		    (last_status_tick == 0 || now - last_status_tick >= user_config.status_interval_sec)) {
			last_status_tick = now;
			updatestatus();
			drawbars();
		}
	}

	ipc_cleanup();
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

void setup_randr(void)
{
	randr_enabled = False;
	randr_event_base = 0;
	randr_error_base = 0;
	randr_major_version = 0;
	randr_minor_version = 0;

	int major = 1;
	int minor = 2;
	if (!XRRQueryExtension(dpy, &randr_event_base, &randr_error_base))
		return;
	if (!XRRQueryVersion(dpy, &major, &minor))
		return;
	randr_major_version = major;
	randr_minor_version = minor;

	XRRSelectInput(dpy, root,
		               RRScreenChangeNotifyMask |
		               RRCrtcChangeNotifyMask |
	               RROutputChangeNotifyMask |
	               RROutputPropertyNotifyMask);
	randr_enabled = True;
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

	workspace_names_init_defaults();
	setup_atoms();
	setup_randr();
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
	publish_current_desktop();
	monitor_topology_seed_current();
	setup_bars();
	updatestatus();

	/* select events wm should look for on root */
	Mask wm_masks = StructureNotifyMask | SubstructureRedirectMask | SubstructureNotifyMask |
	                KeyPressMask | PropertyChangeMask | PointerMotionMask;
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
	evtable[FocusIn] = hdl_focus_in;
	evtable[Expose] = hdl_expose;
	evtable[KeyPress] = hdl_keypress;
	evtable[MappingNotify] = hdl_mapping_ntf;
	evtable[MapRequest] = hdl_map_req;
	evtable[MotionNotify] = hdl_motion;
	evtable[PropertyNotify] = hdl_property_ntf;
	evtable[UnmapNotify] = hdl_unmap_ntf;
	scan_existing_windows();
	session_state_restore();

	/* prevent child processes from becoming zombies */
	signal(SIGCHLD, SIG_IGN);
}

#include "ewmh.c"

void set_input_focus(Client *c, Bool raise_win, Bool warp)
{
	Window prev_focused = focused ? focused->win : None;
	int prev_ws = current_ws;
	int prev_mon = current_mon;

	if (c && client_is_visible(c)) {
			focused = c;
			current_mon = CLAMP(c->mon, 0, n_mons - 1);
			sync_active_monitor_state();

			/* update remembered focus */
			if (c->ws >= 0 && c->ws < NUM_WORKSPACES) {
				ws_focused[c->ws] = c;
				workspace_states[c->ws].focused = c;
			}

		Window w = find_toplevel(c->win);
		Bool focus_changed = (prev_focused != w || current_ws != prev_ws || current_mon != prev_mon);

		XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
		send_wm_take_focus(w);
		window_set_ewmh_state(c->win, atoms[ATOM_NET_WM_STATE_DEMANDS_ATTENTION], False);
		{
			XWMHints *hints = XGetWMHints(dpy, c->win);
			if (hints) {
				if (hints->flags & XUrgencyHint) {
					hints->flags &= ~XUrgencyHint;
					XSetWMHints(dpy, c->win, hints);
				}
				XFree(hints);
			}
		}

			if (raise_win) {
				/* always raise in monocle, otherwise respect floating_on_top */
				if (layouts[current_layout].mode == LayoutMonocle || monocle || c->floating || !user_config.floating_on_top)
					XRaiseWindow(dpy, w);
			}
			if (focus_changed) {
				publish_current_desktop();
				/* EWMH focus hint */
				XChangeProperty(dpy, root, atoms[ATOM_NET_ACTIVE_WINDOW], XA_WINDOW, 32,
						PropModeReplace, (unsigned char *)&w, 1);
			}
			update_focused_ewmh_state(c);
			if (focus_changed || raise_win)
				update_net_client_list();

			if (focus_changed)
				update_borders();

		if (warp && user_config.warp_cursor)
			warp_cursor(c);
	}
	else {
		/* no client */
		Bool focus_changed = (prev_focused != None || current_ws != prev_ws || current_mon != prev_mon);
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		if (focus_changed)
			XDeleteProperty(dpy, root, atoms[ATOM_NET_ACTIVE_WINDOW]);

		focused = NULL;
		ws_focused[current_ws] = NULL;
		workspace_states[current_ws].focused = NULL;
		update_focused_ewmh_state(NULL);
		if (focus_changed)
			update_net_client_list();
		if (focus_changed)
			update_borders();
	}

	Window now_focused = focused ? focused->win : None;
	if (now_focused != prev_focused || current_ws != prev_ws || current_mon != prev_mon) {
		char details[192];
		snprintf(details, sizeof(details), "focused=0x%lx workspace=%d monitor=%d",
		         (unsigned long)now_focused, current_ws + 1, current_mon);
		ipc_notify_event("focus", details);
	}
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
	session_state_save();
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

			unsigned long desktop = c->sticky ? 0xFFFFFFFFUL : (unsigned long)current_ws;
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
	session_state_save();
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

	if (monitor_views_workspace(swallower->mon, ws)) {
		XMapWindow(dpy, swallower->win);
		set_input_focus(swallower, False, True);
		tile();
		update_borders();
	}
}

void update_borders(void)
{
	for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
		for (Client *c = workspaces[ws]; c; c = c->next) {
			if (!client_is_visible(c))
				continue;
			XSetWindowBorder(dpy, c->win,
			                 (c == focused ? user_config.border_foc_col : user_config.border_ufoc_col));
		}
	}

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
	Monitor *old = mons;
	int old_n = n_mons;
	Monitor discovered[MAX_MONITORS];

	scr_width = XDisplayWidth(dpy, DefaultScreen(dpy));
	scr_height = XDisplayHeight(dpy, DefaultScreen(dpy));

	for (int s = 0; s < ScreenCount(dpy); s++) {
		Window scr_root = RootWindow(dpy, s);
		XDefineCursor(dpy, scr_root, cursor_normal);
	}

	n_mons = query_active_monitors(discovered, MAX_MONITORS, NULL);
	if (n_mons <= 0)
		n_mons = 1;

	mons = malloc(sizeof(*mons) * (size_t)n_mons);
	if (!mons) {
		fputs("cupidwm: failed to allocate monitors\n", stderr);
		exit(EXIT_FAILURE);
	}
	Bool used_workspaces[NUM_WORKSPACES] = {False};
	for (int i = 0; i < n_mons; i++) {
		mons[i] = discovered[i];
		int desired_ws = (old && i < old_n) ? old[i].view_ws : (i % NUM_WORKSPACES);
		desired_ws = CLAMP(desired_ws, 0, NUM_WORKSPACES - 1);
		if (used_workspaces[desired_ws]) {
			for (int ws = 0; ws < NUM_WORKSPACES; ws++) {
				if (!used_workspaces[ws]) {
					desired_ws = ws;
					break;
				}
			}
		}
		used_workspaces[desired_ws] = True;
		mons[i].view_ws = desired_ws;
		mons[i].prev_view_ws = (old && i < old_n) ? CLAMP(old[i].prev_view_ws, 0, NUM_WORKSPACES - 1) : desired_ws;
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

	sync_active_monitor_state();

	if (old) {
		for (int i = 0; i < old_n; i++) {
			if (old[i].barwin)
				XDestroyWindow(dpy, old[i].barwin);
		}
	}
	free(old);
	{
		char details[64];
		snprintf(details, sizeof(details), "monitors=%d", n_mons);
		ipc_notify_event("monitor_change", details);
	}
}





void warp_cursor(Client *c)
{
	if (!c)
		return;

	int center_x = c->x + (c->w / 2);
	int center_y = c->y + (c->h / 2);

	/* XWarpPointer is asynchronous; XFlush in set_input_focus is sufficient. */
	XWarpPointer(dpy, None, root, 0, 0, 0, 0, center_x, center_y);
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
void focusdircmd(const Arg *arg) { if (arg) focus_dir(arg->i); }
void focusmondircmd(const Arg *arg) { if (arg) focus_mon_dir(arg->i); }
void focusnextmoncmd(const Arg *arg) { (void)arg; focus_next_mon(); }
void focusprevcmd(const Arg *arg) { (void)arg; focus_prev(); }
void focusprevmoncmd(const Arg *arg) { (void)arg; focus_prev_mon(); }
void increasegapscmd(const Arg *arg) { (void)arg; inc_gaps(); }
void killclientcmd(const Arg *arg) { (void)arg; close_focused(); }
void masternextcmd(const Arg *arg) { (void)arg; move_master_next(); }
void masterprevcmd(const Arg *arg) { (void)arg; move_master_prev(); }
void masterincreasecmd(const Arg *arg) { (void)arg; resize_master_add(); }
void masterdecreasecmd(const Arg *arg) { (void)arg; resize_master_sub(); }
void movedircmd(const Arg *arg) { if (arg) move_dir(arg->i); }
void movenextmoncmd(const Arg *arg) { (void)arg; move_next_mon(); }
void moveprevmoncmd(const Arg *arg) { (void)arg; move_prev_mon(); }
void sendmondircmd(const Arg *arg) { if (arg) send_to_mon_dir(arg->i); }
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
void swapdircmd(const Arg *arg) { if (arg) swap_dir(arg->i); }
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
	int target = -1;
	if (!arg || arg->i < 0) {
		target = (workspace_layout_for(current_ws) + 1) % (int)LENGTH(layouts);
	}
	else {
		int requested = arg->i;
		int by_mode = layout_index_from_mode(requested);
		if (by_mode >= 0)
			target = by_mode;
		else if (requested >= 0 && requested < (int)LENGTH(layouts))
			target = requested;
	}
	if (target < 0 || target >= (int)LENGTH(layouts))
		target = layout_index_from_mode(LayoutTile);
	if (target < 0 || target >= (int)LENGTH(layouts))
		target = 0;

	int mode = layouts[target].mode;
	if (mode == LayoutFloating && !global_floating)
		toggle_floating_global();
	else if (mode != LayoutFloating && global_floating)
		toggle_floating_global();

	workspace_states[current_ws].layout = target;
	sync_active_monitor_state();
	tile();
	update_borders();
	session_state_save();
	{
		char details[128];
		snprintf(details, sizeof(details), "workspace=%d layout=%s", current_ws + 1,
		         layouts[target].symbol ? layouts[target].symbol : "?");
		ipc_notify_event("layout", details);
	}
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
	int grab = XGrabPointer(dpy, root, True, ButtonReleaseMask | PointerMotionMask,
	             GrabModeAsync, GrabModeAsync, None, cursor_move, CurrentTime);
	if (grab != GrabSuccess)
		cancel_drag();
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
	int grab = XGrabPointer(dpy, root, True, ButtonReleaseMask | PointerMotionMask,
	             GrabModeAsync, GrabModeAsync, None, cursor_resize, CurrentTime);
	if (grab != GrabSuccess)
		cancel_drag();
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
	int grab = XGrabPointer(dpy, root, True, ButtonReleaseMask | PointerMotionMask,
	             GrabModeAsync, GrabModeAsync, None, cursor_move, CurrentTime);
	if (grab != GrabSuccess)
		cancel_drag();
}

void restartwm(const Arg *arg)
{
	(void)arg;
	if (!saved_argv || saved_argc <= 0 || !saved_argv[0])
		return;

	ipc_cleanup();
	if (dpy) {
		destroy_wm_windows();
		XSync(dpy, False);
		if (cursor_move)
			XFreeCursor(dpy, cursor_move);
		if (cursor_normal)
			XFreeCursor(dpy, cursor_normal);
		if (cursor_resize)
			XFreeCursor(dpy, cursor_resize);
		XCloseDisplay(dpy);
		dpy = NULL;
	}

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

void handle_xevent(XEvent *xev)
{
	if (!xev)
		return;

	if (randr_enabled) {
		int rr_screen_change = randr_event_base + RRScreenChangeNotify;
		int rr_notify = randr_event_base + RRNotify;
		if (xev->type == rr_screen_change || xev->type == rr_notify) {
			Bool force_refresh = (xev->type == rr_screen_change);
			/* XRRUpdateConfiguration only accepts RRScreenChangeNotify/root ConfigureNotify. */
			if (force_refresh)
				XRRUpdateConfiguration(xev);
			if (force_refresh || monitor_topology_changed()) {
				update_mons();
				setup_bars();
				update_struts();
				tile();
				update_borders();
			}
			return;
		}
	}

	xev_case(xev);
}

static void print_usage(FILE *out, const char *argv0)
{
	const char *name = (argv0 && argv0[0]) ? argv0 : "cupidwm";
	fprintf(out, "usage: %s [options]\n", name);
	fprintf(out, "  -h, --help               Show this help text\n");
	fprintf(out, "  -v, --version            Print version information and exit\n");
	fprintf(out, "  --print-ipc-socket       Print the resolved IPC socket path and exit\n");
}

int main(int ac, char **av)
{
	saved_argc = ac;
	saved_argv = av;

	if (ac > 1) {
		for (int i = 1; i < ac; i++) {
			if (strcmp(av[i], "-v") == 0 || strcmp(av[i], "--version") == 0) {
				printf("%s\n%s\n%s\n", CUPIDWM_VERSION, CUPIDWM_AUTHOR, CUPIDWM_LICINFO);
				return EXIT_SUCCESS;
			}

			if (strcmp(av[i], "-h") == 0 || strcmp(av[i], "--help") == 0) {
				print_usage(stdout, av[0]);
				return EXIT_SUCCESS;
			}

			if (strcmp(av[i], "--print-ipc-socket") == 0) {
				char socket_path[PATH_MAX] = {0};
				if (!ipc_resolve_socket_path(ipc_socket_path, socket_path, sizeof(socket_path)) ||
				    socket_path[0] == '\0') {
					fprintf(stderr, "cupidwm: failed to resolve IPC socket path\n");
					return EXIT_FAILURE;
				}
				puts(socket_path);
				return EXIT_SUCCESS;
			}

			fprintf(stderr, "cupidwm: unknown option: %s\n", av[i]);
			print_usage(stderr, av[0]);
			return EXIT_FAILURE;
		}
	}

	setup();
	puts("cupidwm: starting...");
	run();
	return EXIT_SUCCESS;
}

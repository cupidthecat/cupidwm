/* See LICENSE for more information on use */
#pragma once

#include <X11/Xlib.h>

#define CUPIDWM_VERSION "cupidwm v1.0"
#define CUPIDWM_AUTHOR "(C) cupidwm contributors 2026"
#define CUPIDWM_LICINFO "See LICENSE for more information"

#define MF_MIN               0.05f
#define MF_MAX               0.95f
#define MAX(a, b)            ((a) > (b) ? (a) : (b))
#define MIN(a, b)            ((a) < (b) ? (a) : (b))
#define UDIST(a, b)          abs((int)(a) - (int)(b))
#define CLAMP(x, lo, hi)     (((x) < (lo)) ? (lo) : ((x) > (hi)) ? (hi) : (x))
#define LENGTH(X)            (sizeof(X) / sizeof((X)[0]))

#define MAX_MONITORS         32
#define MAX_CLIENTS          99
#define MAX_SCRATCHPADS      32
#define MAX_ITEMS            256
#define MIN_WINDOW_SIZE      20
#define PATH_MAX             4096

#define NUM_WORKSPACES       9
#define WORKSPACE_NAMES      \
	"1""\0"               \
	"2""\0"               \
	"3""\0"               \
	"4""\0"               \
	"5""\0"               \
	"6""\0"               \
	"7""\0"               \
	"8""\0"               \
	"9""\0"

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef enum { DRAG_NONE, DRAG_MOVE, DRAG_RESIZE, DRAG_SWAP } DragMode;
typedef void (*EventHandler)(XEvent *);

/* internal bind types */
#define TYPE_WS_CHANGE       0
#define TYPE_WS_MOVE         1
#define TYPE_FUNC            2
#define TYPE_CMD             3
#define TYPE_SP_REMOVE       4
#define TYPE_SP_TOGGLE       5
#define TYPE_SP_CREATE       6

typedef union {
	const char **cmd;
	void (*fn)(void);
	int ws;
	int sp;
} Action;

typedef struct {
	int mods;
	KeySym keysym;
	KeyCode keycode;
	Action action;
	int type;
} Binding;

enum {
	ClkTagBar,
	ClkLtSymbol,
	ClkStatusText,
	ClkWinTitle,
	ClkClientWin,
	ClkRootWin,
};

enum {
	LayoutTile = 0,
	LayoutFloating = 1,
	LayoutMonocle = 2,
	LayoutFibonacci = 3,
	LayoutDwindle = 4,
};

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	int workspace;          /* 0-8, -1 means current workspace */
	Bool is_floating;
	Bool start_fullscreen;
	Bool can_swallow;
	Bool can_be_swallowed;
} Rule;

typedef struct {
	const char *symbol;
	int mode;
} Layout;

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *arg);
	const Arg arg;
} Key;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Client {
	Window win;
	int x, y, h, w;
	int orig_x, orig_y, orig_w, orig_h;
	int custom_stack_height;
	int mon;
	int ws;
	Bool fixed;
	Bool floating;
	Bool prev_floating;
	Bool floating_saved;
	Bool fullscreen;
	Bool mapped;
	int ignore_unmap_events;
	pid_t pid;
	struct Client *next;
	struct Client *swallowed;
	struct Client *swallower;
} Client;

typedef struct {
	int modkey;
	int gaps;
	int border_width;
	long border_foc_col;
	long border_ufoc_col;
	long border_swap_col;
	long bar_bg_col;
	long bar_fg_col;
	long bar_sel_bg_col;
	long bar_sel_fg_col;
	float master_width[MAX_MONITORS];
	int motion_throttle;
	int resize_master_amt;
	int resize_stack_amt;
	int snap_distance;
	int move_window_amt;
	int resize_window_amt;
	Bool new_win_focus;
	Bool focus_follows_mouse;
	Bool warp_cursor;
	Bool floating_on_top;
	Bool new_win_master;
	Bool showbar;
	Bool topbar;
	int bar_height;
	Bool bar_show_tabs;
	Bool bar_click_focus_tabs;
	Bool bar_show_title_fallback;
	int status_interval_sec;
	Bool status_use_root_name;
	Bool status_enable_fallback;
	Bool status_show_disk;
	Bool status_show_disk_total;
	Bool status_show_cpu;
	Bool status_show_ram;
	Bool status_show_battery;
	Bool status_show_time;
	Bool status_ram_show_percent;
	Bool status_battery_show_state;
	const char *status_disk_path;
	const char *status_battery_path;
	const char *status_disk_label;
	const char *status_cpu_label;
	const char *status_ram_label;
	const char *status_battery_label;
	const char *status_time_label;
	const char *status_section_order;
	const char *status_time_format;
	const char *status_separator;
	int n_binds;
	Binding binds[MAX_ITEMS];
	char **should_float[MAX_ITEMS];
	char **start_fullscreen[MAX_ITEMS];
	char **can_swallow[MAX_ITEMS];
	char **can_be_swallowed[MAX_ITEMS];
	char **scratchpads[MAX_SCRATCHPADS];
	char **open_in_workspace[MAX_ITEMS];
	char *to_run[MAX_ITEMS];
} Config;

typedef struct {
	int x, y;
	int w, h;
	int reserve_left, reserve_right;
	int reserve_top, reserve_bottom;
	int bar_y;
	Window barwin;
} Monitor;

typedef struct {
	Client *client;
	Bool enabled;
} Scratchpad;

typedef enum {
	ATOM_NET_ACTIVE_WINDOW,
	ATOM_NET_CURRENT_DESKTOP,
	ATOM_NET_SUPPORTED,
	ATOM_NET_WM_STATE,
	ATOM_NET_WM_STATE_FULLSCREEN,
	ATOM_WM_STATE,
	ATOM_NET_WM_WINDOW_TYPE,
	ATOM_NET_WORKAREA,
	ATOM_WM_DELETE_WINDOW,
	ATOM_NET_WM_STRUT,
	ATOM_NET_WM_STRUT_PARTIAL,
	ATOM_NET_SUPPORTING_WM_CHECK,
	ATOM_NET_WM_NAME,
	ATOM_UTF8_STRING,
	ATOM_NET_WM_DESKTOP,
	ATOM_NET_CLIENT_LIST,
	ATOM_NET_FRAME_EXTENTS,
	ATOM_NET_NUMBER_OF_DESKTOPS,
	ATOM_NET_DESKTOP_NAMES,
	ATOM_NET_WM_PID,
	ATOM_NET_WM_WINDOW_TYPE_DOCK,
	ATOM_NET_WM_WINDOW_TYPE_UTILITY,
	ATOM_NET_WM_WINDOW_TYPE_DIALOG,
	ATOM_NET_WM_WINDOW_TYPE_TOOLBAR,
	ATOM_NET_WM_WINDOW_TYPE_SPLASH,
	ATOM_NET_WM_WINDOW_TYPE_POPUP_MENU,
	ATOM_NET_WM_WINDOW_TYPE_MENU,
	ATOM_NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
	ATOM_NET_WM_WINDOW_TYPE_TOOLTIP,
	ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION,
	ATOM_NET_WM_STATE_MODAL,
	ATOM_WM_PROTOCOLS,
	ATOM_COUNT
} AtomType;

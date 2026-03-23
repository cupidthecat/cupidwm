/* See LICENSE for more information on use */
/* Shared constants, enums, structs, and globals across cupidwm modules. */
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
#define WORKSPACE_NAME_MAX   64
#define STATUS_MAX_SEGMENTS  16
#define STATUS_MAX_LABEL     128
#define STATUS_MAX_ACTION    256

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
	LayoutGrid = 5,
	LayoutColumns = 6,
};

enum {
	RuleMatchExact = 0,
	RuleMatchSubstring = 1,
	RuleMatchRegex = 2,
};

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	int workspace;          /* 0-8, -1 means current workspace */
	int monitor;            /* monitor index, -1 means cursor monitor */
	int x;
	int y;
	int w;
	int h;
	int scratchpad;         /* scratchpad slot, -1 means none */
	int match_mode;         /* RuleMatch* */
	Bool centered;
	Bool is_floating;
	Bool start_fullscreen;
	Bool sticky;
	Bool skip_taskbar;
	Bool skip_pager;
	Bool no_focus_on_map;
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
	unsigned long map_seq;
	int x, y, h, w;
	int basew, baseh, incw, inch;
	int maxw, maxh, minw, minh;
	float mina, maxa;
	int orig_x, orig_y, orig_w, orig_h;
	int max_restore_x, max_restore_y, max_restore_w, max_restore_h;
	int custom_stack_height;
	int mon;
	int ws;
	Bool fixed;
	Bool isurgent;
	Bool neverfocus;
	Bool floating;
	Bool prev_floating;
	Bool floating_saved;
	Bool modal_forced_floating;
	Bool fullscreen;
	Bool hidden;
	Bool sticky;
	Bool maximized_horz;
	Bool maximized_vert;
	Bool max_forced_floating;
	Bool max_restore_valid;
	Bool max_restore_floating;
	Bool mapped;
	Bool no_focus_on_map;
	Bool suppress_enter_focus_once;
	long suppress_focus_until_sec;
	int ignore_unmap_events;
	pid_t pid;
	struct Client *next;
	struct Client *snext;
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
	Bool status_allow_external_cmd;
	const char *status_external_cmd;
	Bool ipc_enable;
	const char *ipc_socket_path;
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
	int view_ws;
	int prev_view_ws;
	int bar_y;
	Window barwin;
} Monitor;

typedef struct {
	Client *client;
	Bool enabled;
} Scratchpad;

typedef struct {
	int layout;
	int gaps;
	int nmaster;
	float master_width[MAX_MONITORS];
	Client *focused;
} WorkspaceState;

typedef enum {
	ATOM_NET_ACTIVE_WINDOW,
	ATOM_NET_CURRENT_DESKTOP,
	ATOM_NET_SUPPORTED,
	ATOM_NET_WM_STATE,
	ATOM_NET_WM_STATE_FULLSCREEN,
	ATOM_NET_WM_STATE_STICKY,
	ATOM_NET_WM_STATE_MAXIMIZED_VERT,
	ATOM_NET_WM_STATE_MAXIMIZED_HORZ,
	ATOM_NET_WM_STATE_SHADED,
	ATOM_NET_WM_STATE_SKIP_TASKBAR,
	ATOM_NET_WM_STATE_SKIP_PAGER,
	ATOM_NET_WM_STATE_HIDDEN,
	ATOM_NET_WM_STATE_DEMANDS_ATTENTION,
	ATOM_NET_WM_STATE_FOCUSED,
	ATOM_WM_STATE,
	ATOM_NET_WM_WINDOW_TYPE,
	ATOM_NET_WORKAREA,
	ATOM_WM_DELETE_WINDOW,
	ATOM_NET_WM_STRUT,
	ATOM_NET_WM_STRUT_PARTIAL,
	ATOM_NET_SUPPORTING_WM_CHECK,
	ATOM_NET_WM_NAME,
	ATOM_NET_CLOSE_WINDOW,
	ATOM_UTF8_STRING,
	ATOM_NET_WM_DESKTOP,
	ATOM_NET_CLIENT_LIST,
	ATOM_NET_CLIENT_LIST_STACKING,
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
	ATOM_NET_WM_STATE_ABOVE,
	ATOM_NET_WM_STATE_BELOW,
	ATOM_NET_MOVERESIZE_WINDOW,
	ATOM_NET_REQUEST_FRAME_EXTENTS,
	ATOM_NET_RESTACK_WINDOW,
	ATOM_NET_WM_ALLOWED_ACTIONS,
	ATOM_NET_WM_ACTION_MOVE,
	ATOM_NET_WM_ACTION_RESIZE,
	ATOM_NET_WM_ACTION_MINIMIZE,
	ATOM_NET_WM_ACTION_SHADE,
	ATOM_NET_WM_ACTION_STICK,
	ATOM_NET_WM_ACTION_MAXIMIZE_HORZ,
	ATOM_NET_WM_ACTION_MAXIMIZE_VERT,
	ATOM_NET_WM_ACTION_FULLSCREEN,
	ATOM_NET_WM_ACTION_CHANGE_DESKTOP,
	ATOM_NET_WM_ACTION_CLOSE,
	ATOM_WM_PROTOCOLS,
	ATOM_COUNT
} AtomType;

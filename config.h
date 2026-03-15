/* See LICENSE for copyright and license details. */

/* appearance */
static const unsigned int borderpx = 3;
static const unsigned int snap = 5;
static const int showbar = 1;
static const int topbar = 1;
static const int barheight = 20;
static const char fontname[] = "undefined-medium";
static const int bar_show_tabs = 1;
static const int bar_click_focus_tabs = 1;
static const int bar_show_title_fallback = 1;

static const int status_interval_sec = 1;
static const int status_use_root_name = 1;
static const int status_enable_fallback = 1;
static const int status_show_disk = 1;
static const int status_show_disk_total = 1;
static const char status_disk_path[] = "/";
static const char status_time_format[] = "%Y-%m-%d %H:%M";
static const char status_separator[] = " | ";

static const char col_border_focused[] = "#000000";
static const char col_border_unfocused[] = "#444444";
static const char col_border_swap[] = "#eeeeee";
static const char col_bar_bg[] = "#111111";
static const char col_bar_fg[] = "#bbbbbb";
static const char col_bar_sel_bg[] = "#005577";
static const char col_bar_sel_fg[] = "#eeeeee";

/* behavior */
static const int motion_throttle = 60;
static const int resize_master_amount = 1;
static const int resize_stack_amount = 20;
static const int move_window_amount = 50;
static const int resize_window_amount = 50;
static const int new_window_focus = 1;
static const int focus_follows_mouse = 1;
static const int warp_cursor_on = 1;
static const int floating_on_top = 1;
static const int new_window_master = 0;
static const int default_gaps = 5;
static const float master_width_default = 0.60f;

/* workspaces */
static const char *tags[NUM_WORKSPACES] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

/* class rules */
static const Rule rules[] = {
	/* class,      instance, title, workspace, is_floating, start_fullscreen, can_swallow, can_be_swallowed */
	{ "pcmanfm",  NULL,     NULL,  -1,        True,        False,            False,       False },
	{ "obs",      NULL,     NULL,  -1,        True,        False,            False,       False },
	{ "mpv",      NULL,     NULL,  -1,        False,       True,             False,       True  },
	{ "vlc",      NULL,     NULL,  -1,        False,       True,             False,       True  },
	{ "st",       NULL,     NULL,  -1,        False,       False,            True,        False },
};

/* layouts */
static const Layout layouts[] = {
	/* symbol, mode */
	{ "[M]", LayoutMonocle },
	{ "[]=", LayoutTile },
	{ "><>", LayoutFloating },
	{ "[@]", LayoutFibonacci },
	{ "[\\]", LayoutDwindle },
};

#define MODKEY Mod4Mask
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static const char *termcmd[] = { "/bin/sh", "-lc", "cd \"$HOME\" && exec st", NULL };
static const char *dmenucmd[] = { "dmenu_run", NULL };
static const char *browsercmd[] = { "firefox", NULL };

static const char *autostart_shell_0[] = { "sh", "-c", "xsetroot -cursor_name left_ptr", NULL };
static const char *const *autostart[] = {
	autostart_shell_0,
	NULL,
};

#define WSKEYS(KEY, WS) \
	{ MODKEY,                       KEY, viewws,      {.ui = WS} }, \
	{ MODKEY|ShiftMask,             KEY, movetows,    {.ui = WS} }

static const Key keys[] = {
	/* modifier                     key         function                 argument */
	{ MODKEY,                       XK_Return,  spawncmd,                {.v = termcmd } },
	{ MODKEY,                       XK_p,       spawncmd,                {.v = dmenucmd } },
	{ MODKEY,                       XK_b,       spawncmd,                {.v = browsercmd } },
	{ MODKEY|ShiftMask,             XK_q,       killclientcmd,           {0} },
	{ MODKEY|ShiftMask,             XK_Escape,  quitcmd,                 {0} },
	{ MODKEY|ShiftMask,             XK_r,       restartwm,               {0} },
	{ MODKEY,                       XK_j,       focusnextcmd,            {0} },
	{ MODKEY,                       XK_k,       focusprevcmd,            {0} },
	{ MODKEY|ShiftMask,             XK_j,       masternextcmd,           {0} },
	{ MODKEY|ShiftMask,             XK_k,       masterprevcmd,           {0} },
	{ MODKEY,                       XK_h,       masterdecreasecmd,       {0} },
	{ MODKEY,                       XK_l,       masterincreasecmd,       {0} },
	{ MODKEY|ControlMask,           XK_h,       stackdecreasecmd,        {0} },
	{ MODKEY|ControlMask,           XK_l,       stackincreasecmd,        {0} },
	{ MODKEY,                       XK_m,       setlayoutcmd,            {.i = LayoutMonocle} },
	{ MODKEY,                       XK_t,       setlayoutcmd,            {.i = LayoutTile} },
	{ MODKEY,                       XK_f,       setlayoutcmd,            {.i = LayoutFloating} },
	{ MODKEY,                       XK_r,       setlayoutcmd,            {.i = LayoutFibonacci} },
	{ MODKEY,                       XK_y,       setlayoutcmd,            {.i = LayoutDwindle} },
	{ MODKEY,                       XK_space,   togglefloatingcmd,       {0} },
	{ MODKEY|ShiftMask,             XK_space,   togglefloatingglobalcmd, {0} },
	{ MODKEY|ShiftMask,             XK_f,       togglefullscreencmd,     {0} },
	{ MODKEY,                       XK_equal,   increasegapscmd,         {0} },
	{ MODKEY,                       XK_minus,   decreasegapscmd,         {0} },
	{ MODKEY,                       XK_comma,   focusprevmoncmd,         {0} },
	{ MODKEY,                       XK_period,  focusnextmoncmd,         {0} },
	{ MODKEY|ShiftMask,             XK_comma,   moveprevmoncmd,          {0} },
	{ MODKEY|ShiftMask,             XK_period,  movenextmoncmd,          {0} },
	{ MODKEY,                       XK_Up,      movewinupcmd,            {0} },
	{ MODKEY,                       XK_Down,    movewindowncmd,          {0} },
	{ MODKEY,                       XK_Left,    movewinleftcmd,          {0} },
	{ MODKEY,                       XK_Right,   movewinrightcmd,         {0} },
	{ MODKEY|ShiftMask,             XK_Up,      resizewinupcmd,          {0} },
	{ MODKEY|ShiftMask,             XK_Down,    resizewindowncmd,        {0} },
	{ MODKEY|ShiftMask,             XK_Left,    resizewinleftcmd,        {0} },
	{ MODKEY|ShiftMask,             XK_Right,   resizewinrightcmd,       {0} },
	{ MODKEY,                       XK_c,       centrewindowcmd,         {0} },
	{ MODKEY,                       XK_Tab,     prevworkspacecmd,        {0} },

	/* scratchpads */
	{ MODKEY|Mod1Mask,              XK_1,       createscratchpadcmd,     {.ui = 0} },
	{ MODKEY|Mod1Mask,              XK_2,       createscratchpadcmd,     {.ui = 1} },
	{ MODKEY|Mod1Mask,              XK_3,       createscratchpadcmd,     {.ui = 2} },
	{ MODKEY|Mod1Mask,              XK_4,       createscratchpadcmd,     {.ui = 3} },
	{ MODKEY|ControlMask,           XK_1,       togglescratchpad,        {.ui = 0} },
	{ MODKEY|ControlMask,           XK_2,       togglescratchpad,        {.ui = 1} },
	{ MODKEY|ControlMask,           XK_3,       togglescratchpad,        {.ui = 2} },
	{ MODKEY|ControlMask,           XK_4,       togglescratchpad,        {.ui = 3} },
	{ MODKEY|Mod1Mask|ShiftMask,    XK_1,       removescratchpadcmd,     {.ui = 0} },
	{ MODKEY|Mod1Mask|ShiftMask,    XK_2,       removescratchpadcmd,     {.ui = 1} },
	{ MODKEY|Mod1Mask|ShiftMask,    XK_3,       removescratchpadcmd,     {.ui = 2} },
	{ MODKEY|Mod1Mask|ShiftMask,    XK_4,       removescratchpadcmd,     {.ui = 3} },

	WSKEYS(XK_1, 0),
	WSKEYS(XK_2, 1),
	WSKEYS(XK_3, 2),
	WSKEYS(XK_4, 3),
	WSKEYS(XK_5, 4),
	WSKEYS(XK_6, 5),
	WSKEYS(XK_7, 6),
	WSKEYS(XK_8, 7),
	WSKEYS(XK_9, 8),
};

static const Button buttons[] = {
	/* click          event mask        button   function            argument */
	{ ClkLtSymbol,    0,                Button1, setlayoutcmd,      {.i = -1} },
	{ ClkTagBar,      0,                Button1, viewws,            {0} },
	{ ClkTagBar,      ShiftMask,        Button1, movetows,          {0} },
	{ ClkClientWin,   MODKEY,           Button1, movemousecmd,      {0} },
	{ ClkClientWin,   MODKEY|ShiftMask, Button1, swapmousecmd,      {0} },
	{ ClkClientWin,   MODKEY,           Button2, togglefloatingcmd, {0} },
	{ ClkClientWin,   MODKEY,           Button3, resizemousecmd,    {0} },
};

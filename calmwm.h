/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2004 Marius Aamodt Eriksen <marius@monkey.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $OpenBSD$
 */

#ifndef _CALMWM_H_
#define _CALMWM_H_

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>

#undef MIN
#undef MAX
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

#define	CONFFILE	".cwmrc"

#define BUTTONMASK	(ButtonPressMask | ButtonReleaseMask)
#define MOUSEMASK	(BUTTONMASK | PointerMotionMask)
#define MENUMASK 	(MOUSEMASK | ButtonMotionMask | ExposureMask)
#define MENUGRABMASK	(MOUSEMASK | ButtonMotionMask | StructureNotifyMask)
#define KEYMASK		(KeyPressMask | ExposureMask)
#define IGNOREMODMASK	(LockMask | Mod2Mask | 0x2000)

/* direction/amount */
#define CWM_UP			0x0001
#define CWM_DOWN		0x0002
#define CWM_LEFT		0x0004
#define CWM_RIGHT		0x0008
#define CWM_BIGAMOUNT		0x0010
#define DIRECTIONMASK	(CWM_UP | CWM_DOWN | CWM_LEFT | CWM_RIGHT)

#define CWM_CLIENT_CYCLE	0x0001
#define CWM_CLIENT_RCYCLE	0x0002
#define CWM_CLIENT_CYCLE_INGRP	0x0004

#define CWM_CLIENT_TILE_HORIZ	0x0001
#define CWM_CLIENT_TILE_VERT	0x0002

#define CWM_MENU_EXEC		0x0001
#define CWM_MENU_EXEC_WM	0x0002

#define CWM_MENU_DUMMY		0x0001
#define CWM_MENU_FILE		0x0002
#define CWM_MENU_LIST		0x0004

#define CWM_GAP			0x0001
#define CWM_NOGAP		0x0002

#define CWM_KEY			0x0001
#define CWM_BTN			0x0002

#define CWM_CONTEXT_NONE	0x0000
#define CWM_CONTEXT_CLIENT	0x0001
#define CWM_CONTEXT_SCREEN	0x0002

#define CWM_QUIT		0x0000
#define CWM_RUNNING		0x0001
#define CWM_EXEC_WM		0x0002

union arg {
	char	*c;
	int	 i;
};
union press {
	KeySym		 keysym;
	unsigned int	 button;
};

enum cursor_font {
	CF_NORMAL,
	CF_MOVE,
	CF_RESIZE,
	CF_QUESTION,
	CF_NITEMS
};
enum color {
	CWM_COLOR_BORDER_ACTIVE,
	CWM_COLOR_BORDER_INACTIVE,
	CWM_COLOR_BORDER_URGENCY,
	CWM_COLOR_BORDER_GROUP,
	CWM_COLOR_BORDER_UNGROUP,
	CWM_COLOR_MENU_FG,
	CWM_COLOR_MENU_BG,
	CWM_COLOR_MENU_FONT,
	CWM_COLOR_MENU_FONT_SEL,
	CWM_COLOR_NITEMS
};

struct geom {
	int		 x;
	int		 y;
	int		 w;
	int		 h;
};

struct gap {
	int		 top;
	int		 bottom;
	int		 left;
	int		 right;
};

struct winname {
	TAILQ_ENTRY(winname)	 entry;
	char			*name;
};
TAILQ_HEAD(winname_q, winname);
TAILQ_HEAD(ignore_q, winname);

struct client_ctx {
	TAILQ_ENTRY(client_ctx)	 entry;
	TAILQ_ENTRY(client_ctx)	 group_entry;
	struct screen_ctx	*sc;
	struct group_ctx	*gc;
	Window			 win;
	Colormap		 colormap;
	unsigned int		 bwidth; /* border width */
	struct geom		 geom, savegeom, fullgeom;
	struct {
		long		 flags;	/* defined hints */
		int		 basew;	/* desired width */
		int		 baseh;	/* desired height */
		int		 minw;	/* minimum width */
		int		 minh;	/* minimum height */
		int		 maxw;	/* maximum width */
		int		 maxh;	/* maximum height */
		int		 incw;	/* width increment progression */
		int		 inch;	/* height increment progression */
		float		 mina;	/* minimum aspect ratio */
		float		 maxa;	/* maximum aspect ratio */
	} hint;
	struct {
		int		 x;	/* x position */
		int		 y;	/* y position */
	} ptr;
	struct {
		int		 h;	/* height */
		int		 w;	/* width */
	} dim;
#define CLIENT_HIDDEN			0x0001
#define CLIENT_IGNORE			0x0002
#define CLIENT_VMAXIMIZED		0x0004
#define CLIENT_HMAXIMIZED		0x0008
#define CLIENT_FREEZE			0x0010
#define CLIENT_GROUP			0x0020
#define CLIENT_UNGROUP			0x0040
#define CLIENT_INPUT			0x0080
#define CLIENT_WM_DELETE_WINDOW		0x0100
#define CLIENT_WM_TAKE_FOCUS		0x0200
#define CLIENT_URGENCY			0x0400
#define CLIENT_FULLSCREEN		0x0800
#define CLIENT_STICKY			0x1000
#define CLIENT_ACTIVE			0x2000

#define CLIENT_HIGHLIGHT		(CLIENT_GROUP | CLIENT_UNGROUP)
#define CLIENT_MAXFLAGS			(CLIENT_VMAXIMIZED | CLIENT_HMAXIMIZED)
#define CLIENT_MAXIMIZED		(CLIENT_VMAXIMIZED | CLIENT_HMAXIMIZED)
	int			 flags;
	int			 stackingorder;
	struct winname_q	 nameq;
	char			*name;
	char			*label;
	char			*matchname;
	XClassHint		ch;
	XWMHints		*wmh;
};
TAILQ_HEAD(client_ctx_q, client_ctx);

struct group_ctx {
	TAILQ_ENTRY(group_ctx)	 entry;
	struct screen_ctx	*sc;
	char			*name;
	int			 num;
	struct client_ctx_q	 clientq;
};
TAILQ_HEAD(group_ctx_q, group_ctx);

struct autogroupwin {
	TAILQ_ENTRY(autogroupwin)	 entry;
	char				*class;
	char				*name;
	int 				 num;
};
TAILQ_HEAD(autogroupwin_q, autogroupwin);

struct region_ctx {
	TAILQ_ENTRY(region_ctx)	 entry;
	int			 num;
	struct geom		 area;
	struct geom		 view; /* viewable area */
	struct geom		 work; /* workable area, gap-applied */
};
TAILQ_HEAD(region_ctx_q, region_ctx);

struct screen_ctx {
	TAILQ_ENTRY(screen_ctx)	 entry;
	int			 which;
	Window			 rootwin;
	int			 cycling;
	int			 hideall;
	int			 snapdist;
	struct geom		 view; /* viewable area */
	struct geom		 work; /* workable area, gap-applied */
	struct gap		 gap;
	struct client_ctx_q	 clientq;
	struct region_ctx_q	 regionq;
	struct group_ctx_q	 groupq;
	struct group_ctx	*group_active;
	struct {
		Window		 win;
		XftDraw		*xftdraw;
	} menu;
	XftColor		 xftcolor[CWM_COLOR_NITEMS];
	XftFont			*xftfont;
};
TAILQ_HEAD(screen_ctx_q, screen_ctx);

struct binding {
	TAILQ_ENTRY(binding)	 entry;
	void			(*callback)(struct client_ctx *, union arg *, int);
	union arg		 argument;
	unsigned int		 modmask;
	union press		 press;
	int			 context;
};
TAILQ_HEAD(keybinding_q, binding);
TAILQ_HEAD(mousebinding_q, binding);

struct cmd {
	TAILQ_ENTRY(cmd)	 entry;
	char			*name;
	char			 path[PATH_MAX];
};
TAILQ_HEAD(cmd_q, cmd);

struct menu {
	TAILQ_ENTRY(menu)	 entry;
	TAILQ_ENTRY(menu)	 resultentry;
#define MENU_MAXENTRY		 200
	char			 text[MENU_MAXENTRY + 1];
	char			 print[MENU_MAXENTRY + 1];
	void			*ctx;
	short			 dummy;
	short			 abort;
};
TAILQ_HEAD(menu_q, menu);

struct conf {
	struct keybinding_q	 keybindingq;
	struct mousebinding_q	 mousebindingq;
	struct autogroupwin_q	 autogroupq;
	struct ignore_q		 ignoreq;
	struct cmd_q		 cmdq;
	int			 ngroups;
	int			 stickygroups;
	int			 nameqlen;
	int			 bwidth;
	int			 mamount;
	int			 snapdist;
	struct gap		 gap;
	char			*color[CWM_COLOR_NITEMS];
	char			 known_hosts[PATH_MAX];
	char			*font;
	char			*wmname;
	Cursor			 cursor[CF_NITEMS];
};

/* MWM hints */
struct mwm_hints {
#define MWM_HINTS_ELEMENTS	3L
#define MWM_FLAGS_STATUS	(1<<3)

#define MWM_FLAGS_FUNCTIONS	(1<<0)
#define MWM_FLAGS_DECORATIONS	(1<<1)
#define MWM_FLAGS_INPUT_MODE	(1<<2)
	unsigned long	flags;

#define MWM_FUNCS_ALL		(1<<0)
#define MWM_FUNCS_RESIZE	(1<<1)
#define MWM_FUNCS_MOVE		(1<<2)
#define MWM_FUNCS_MINIMIZE	(1<<3)
#define MWM_FUNCS_MAXIMIZE	(1<<4)
#define MWM_FUNCS_CLOSE		(1<<5)
	unsigned long	functions;

#define	MWM_DECOR_ALL		(1<<0)
#define	MWM_DECOR_BORDER	(1<<1)
#define MWM_DECOR_RESIZE_HANDLE	(1<<2)
#define MWM_DECOR_TITLEBAR	(1<<3)
#define MWM_DECOR_MENU		(1<<4)
#define MWM_DECOR_MINIMIZE	(1<<5)
#define MWM_DECOR_MAXIMIZE	(1<<6)
	unsigned long	decorations;
};

extern Display				*X_Dpy;
extern Time				 Last_Event_Time;
extern struct screen_ctx_q		 Screenq;
extern struct conf			 Conf;
extern char				*wm_argv;
extern const char			*homedir;
extern int				 HasRandr, Randr_ev;

enum {
	WM_STATE,
	WM_DELETE_WINDOW,
	WM_TAKE_FOCUS,
	WM_PROTOCOLS,
	_MOTIF_WM_HINTS,
	UTF8_STRING,
	WM_CHANGE_STATE,
	CWMH_NITEMS
};
enum {
	_NET_SUPPORTED,
	_NET_SUPPORTING_WM_CHECK,
	_NET_ACTIVE_WINDOW,
	_NET_CLIENT_LIST,
	_NET_CLIENT_LIST_STACKING,
	_NET_NUMBER_OF_DESKTOPS,
	_NET_CURRENT_DESKTOP,
	_NET_DESKTOP_VIEWPORT,
	_NET_DESKTOP_GEOMETRY,
	_NET_VIRTUAL_ROOTS,
	_NET_SHOWING_DESKTOP,
	_NET_DESKTOP_NAMES,
	_NET_WORKAREA,
	_NET_WM_NAME,
	_NET_WM_DESKTOP,
	_NET_CLOSE_WINDOW,
	_NET_WM_STATE,
#define	_NET_WM_STATES_NITEMS	7
	_NET_WM_STATE_STICKY,
	_NET_WM_STATE_MAXIMIZED_VERT,
	_NET_WM_STATE_MAXIMIZED_HORZ,
	_NET_WM_STATE_HIDDEN,
	_NET_WM_STATE_FULLSCREEN,
	_NET_WM_STATE_DEMANDS_ATTENTION,
	_CWM_WM_STATE_FREEZE,
	EWMH_NITEMS
};
enum {
	_NET_WM_STATE_REMOVE,
	_NET_WM_STATE_ADD,
	_NET_WM_STATE_TOGGLE
};
extern Atom				 cwmh[CWMH_NITEMS];
extern Atom				 ewmh[EWMH_NITEMS];

__dead void		 usage(void);

void			 client_applysizehints(struct client_ctx *);
void			 client_config(struct client_ctx *);
struct client_ctx	*client_current(void);
void			 client_cycle(struct screen_ctx *, int);
void			 client_cycle_leave(struct screen_ctx *);
void			 client_delete(struct client_ctx *);
void			 client_draw_border(struct client_ctx *);
struct client_ctx	*client_find(Window);
long			 client_get_wm_state(struct client_ctx *);
void			 client_getsizehints(struct client_ctx *);
void			 client_hide(struct client_ctx *);
void 			 client_htile(struct client_ctx *);
void			 client_lower(struct client_ctx *);
void			 client_map(struct client_ctx *);
void			 client_msg(struct client_ctx *, Atom, Time);
void			 client_move(struct client_ctx *);
int			 client_inbound(struct client_ctx *, int, int);
struct client_ctx	*client_init(Window, struct screen_ctx *, int);
void			 client_ptrsave(struct client_ctx *);
void			 client_ptrwarp(struct client_ctx *);
void			 client_raise(struct client_ctx *);
void			 client_resize(struct client_ctx *, int);
void			 client_send_delete(struct client_ctx *);
void			 client_set_wm_state(struct client_ctx *, long);
void			 client_setactive(struct client_ctx *);
void			 client_setname(struct client_ctx *);
int			 client_snapcalc(int, int, int, int, int);
void			 client_toggle_freeze(struct client_ctx *);
void			 client_toggle_fullscreen(struct client_ctx *);
void			 client_toggle_hidden(struct client_ctx *);
void			 client_toggle_hmaximize(struct client_ctx *);
void			 client_toggle_maximize(struct client_ctx *);
void			 client_toggle_sticky(struct client_ctx *);
void			 client_toggle_vmaximize(struct client_ctx *);
void			 client_transient(struct client_ctx *);
void			 client_unhide(struct client_ctx *);
void			 client_urgency(struct client_ctx *);
void 			 client_vtile(struct client_ctx *);
void			 client_wm_hints(struct client_ctx *);

void			 group_alltoggle(struct screen_ctx *);
void			 group_assign(struct group_ctx *, struct client_ctx *);
int			 group_autogroup(struct client_ctx *);
void			 group_cycle(struct screen_ctx *, int);
void			 group_hide(struct group_ctx *);
void			 group_hidetoggle(struct screen_ctx *, int);
int			 group_holds_only_hidden(struct group_ctx *);
int			 group_holds_only_sticky(struct group_ctx *);
void			 group_init(struct screen_ctx *, int);
void			 group_movetogroup(struct client_ctx *, int);
void			 group_only(struct screen_ctx *, int);
int			 group_restore(struct client_ctx *);
void			 group_show(struct group_ctx *);
void			 group_toggle_membership_enter(struct client_ctx *);
void			 group_toggle_membership_leave(struct client_ctx *);
void			 group_update_names(struct screen_ctx *);

void			 search_match_client(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_exec(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_exec_path(struct menu_q *,
			     struct menu_q *, char *);
void			 search_match_path_any(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_text(struct menu_q *, struct menu_q *,
			     char *);
void			 search_print_client(struct menu *, int);
void			 search_print_cmd(struct menu *, int);
void			 search_print_group(struct menu *, int);

struct region_ctx	*region_find(struct screen_ctx *, int, int);
struct geom		 screen_apply_gap(struct screen_ctx *, struct geom);
struct screen_ctx	*screen_find(Window);
struct geom		 screen_area(struct screen_ctx *, int, int, int);
void			 screen_init(int);
void			 screen_update_geometry(struct screen_ctx *);
void			 screen_updatestackingorder(struct screen_ctx *);
void			 screen_assert_clients_within(struct screen_ctx *);

void			 kbfunc_client_cycle(struct client_ctx *, union arg *, int);
void			 kbfunc_client_delete(struct client_ctx *, union arg *, int);
void			 kbfunc_client_hide(struct client_ctx *, union arg *, int);
void			 kbfunc_menu_client_label(struct client_ctx *, union arg *, int);
void			 kbfunc_client_lower(struct client_ctx *, union arg *, int);
void			 kbfunc_client_move(struct client_ctx *, union arg *, int);
void			 kbfunc_client_movetogroup(struct client_ctx *,
			     union arg *, int);
void			 kbfunc_client_raise(struct client_ctx *, union arg *, int);
void			 kbfunc_client_rcycle(struct client_ctx *, union arg *, int);
void			 kbfunc_client_resize(struct client_ctx *, union arg *, int);
void 			 kbfunc_client_tile(struct client_ctx *, union arg *, int);
void			 kbfunc_client_toggle_group(struct client_ctx *,
			     union arg *, int);
void			 kbfunc_client_toggle_freeze(struct client_ctx *,
    			     union arg *, int);
void			 kbfunc_client_toggle_fullscreen(struct client_ctx *,
			     union arg *, int);
void			 kbfunc_client_toggle_hmaximize(struct client_ctx *,
			     union arg *, int);
void			 kbfunc_client_toggle_maximize(struct client_ctx *,
			     union arg *, int);
void			 kbfunc_client_toggle_sticky(struct client_ctx *,
    			     union arg *, int);
void			 kbfunc_client_toggle_vmaximize(struct client_ctx *,
			     union arg *, int);
void			 kbfunc_cwm_status(struct client_ctx *, union arg *, int);
void			 kbfunc_exec(struct client_ctx *, union arg *, int);
void			 kbfunc_exec_lock(struct client_ctx *, union arg *, int);
void			 kbfunc_exec_term(struct client_ctx *, union arg *, int);
void			 kbfunc_group_alltoggle(struct client_ctx *,
			     union arg *, int);
void			 kbfunc_group_cycle(struct client_ctx *, union arg *, int);
void			 kbfunc_group_only(struct client_ctx *, union arg *, int);
void			 kbfunc_group_toggle(struct client_ctx *, union arg *, int);
void			 kbfunc_menu_exec(struct client_ctx *, union arg *, int);
void			 kbfunc_menu_client(struct client_ctx *, union arg *, int);
void			 kbfunc_menu_cmd(struct client_ctx *, union arg *, int);
void			 kbfunc_menu_group(struct client_ctx *, union arg *, int);
void			 kbfunc_menu_ssh(struct client_ctx *, union arg *, int);
void			 kbfunc_ptrmove(struct client_ctx *, union arg *, int);

void			 mousefunc_client_move(struct client_ctx *,
    			    union arg *, int);
void			 mousefunc_client_resize(struct client_ctx *,
    			    union arg *, int);

void			 menu_windraw(struct screen_ctx *, Window,
			     const char *, ...);
struct menu  		*menu_filter(struct screen_ctx *, struct menu_q *,
			     const char *, const char *, int,
			     void (*)(struct menu_q *, struct menu_q *, char *),
			     void (*)(struct menu *, int));
void			 menuq_add(struct menu_q *, void *, const char *, ...);
void			 menuq_clear(struct menu_q *);

int			 parse_config(const char *, struct conf *);

void			 conf_atoms(void);
void			 conf_autogroup(struct conf *, int, const char *,
			     const char *);
int			 conf_bind_kbd(struct conf *, const char *,
    			     const char *);
int			 conf_bind_mouse(struct conf *, const char *,
    			     const char *);
void			 conf_clear(struct conf *);
void			 conf_client(struct client_ctx *);
int			 conf_cmd_add(struct conf *, const char *,
			     const char *);
void			 conf_cursor(struct conf *);
void			 conf_grab_kbd(Window);
void			 conf_grab_mouse(Window);
void			 conf_init(struct conf *);
void			 conf_ignore(struct conf *, const char *);
void			 conf_screen(struct screen_ctx *);

void			 xev_process(void);

int			 xu_getprop(Window, Atom, Atom, long, unsigned char **);
int			 xu_getstrprop(Window, Atom, char **);
void			 xu_ptr_getpos(Window, int *, int *);
void			 xu_ptr_setpos(Window, int, int);
void 			 xu_xorcolor(XftColor, XftColor, XftColor *);

void			 xu_ewmh_net_supported(struct screen_ctx *);
void			 xu_ewmh_net_supported_wm_check(struct screen_ctx *);
void			 xu_ewmh_net_desktop_geometry(struct screen_ctx *);
void			 xu_ewmh_net_workarea(struct screen_ctx *);
void			 xu_ewmh_net_client_list(struct screen_ctx *);
void			 xu_ewmh_net_client_list_stacking(struct screen_ctx *);
void			 xu_ewmh_net_active_window(struct screen_ctx *, Window);
Window			 xu_ewmh_get_net_active_window(struct screen_ctx *);
void			 xu_ewmh_net_wm_desktop_viewport(struct screen_ctx *);
void			 xu_ewmh_net_wm_number_of_desktops(struct screen_ctx *);
void			 xu_ewmh_net_showing_desktop(struct screen_ctx *);
void			 xu_ewmh_net_virtual_roots(struct screen_ctx *);
void			 xu_ewmh_net_current_desktop(struct screen_ctx *);
void			 xu_ewmh_net_desktop_names(struct screen_ctx *);

void			 xu_ewmh_net_wm_desktop(struct client_ctx *);
Atom 			*xu_ewmh_get_net_wm_state(struct client_ctx *, int *);
void 			 xu_ewmh_handle_net_wm_state_msg(struct client_ctx *,
			     int, Atom , Atom);
void 			 xu_ewmh_set_net_wm_state(struct client_ctx *);
void 			 xu_ewmh_restore_net_wm_state(struct client_ctx *);

char			*u_argv(char * const *);
void			 u_exec(char *);
void			 u_spawn(char *);

void			*xcalloc(size_t, size_t);
void			*xmalloc(size_t);
void			*xreallocarray(void *, size_t, size_t);
char			*xstrdup(const char *);
int			 xasprintf(char **, const char *, ...)
			    __attribute__((__format__ (printf, 2, 3)))
			    __attribute__((__nonnull__ (2)));

#endif /* _CALMWM_H_ */

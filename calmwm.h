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

#include <sys/param.h>
#include <stdio.h>
#include "queue.h"

/* prototypes for portable-included functions */
char *fgetln(FILE *, size_t *);
long long strtonum(const char *, long long, long long, const char **);
void *reallocarray(void *, size_t, size_t);


#ifdef strlcat
#define HAVE_STRLCAT
#else
size_t strlcat(char *, const char *, size_t);
#endif
#ifdef strlcpy
#define HAVE_STRLCPY
#else
size_t strlcpy(char *, const char *, size_t);
#endif

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>

#define LOG_DEBUG0(...)	log_debug(0, __func__, __VA_ARGS__)
#define LOG_DEBUG1(...)	log_debug(1, __func__, __VA_ARGS__)
#define LOG_DEBUG2(...)	log_debug(2, __func__, __VA_ARGS__)
#define LOG_DEBUG3(...)	log_debug(3, __func__, __VA_ARGS__)

#undef MIN
#undef MAX
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

#define BUTTONMASK	(ButtonPressMask | ButtonReleaseMask)
#define MOUSEMASK	(BUTTONMASK | PointerMotionMask)
#define IGNOREMODMASK	(LockMask | Mod2Mask | 0x2000)

/* direction/amount */
#define CWM_UP			0x0001
#define CWM_DOWN		0x0002
#define CWM_LEFT		0x0004
#define CWM_RIGHT		0x0008
#define CWM_BIGAMOUNT		0x0010
#define CWM_UP_BIG		(CWM_UP | CWM_BIGAMOUNT)
#define CWM_DOWN_BIG		(CWM_DOWN | CWM_BIGAMOUNT)
#define CWM_LEFT_BIG		(CWM_LEFT | CWM_BIGAMOUNT)
#define CWM_RIGHT_BIG		(CWM_RIGHT | CWM_BIGAMOUNT)
#define CWM_UP_RIGHT		(CWM_UP | CWM_RIGHT)
#define CWM_UP_LEFT		(CWM_UP | CWM_LEFT)
#define CWM_DOWN_RIGHT		(CWM_DOWN | CWM_RIGHT)
#define CWM_DOWN_LEFT		(CWM_DOWN | CWM_LEFT)

#define CWM_CYCLE_FORWARD	0x0001
#define CWM_CYCLE_REVERSE	0x0002
#define CWM_CYCLE_INGROUP	0x0004
#define CWM_CYCLE_INCLASS	0x0008

enum cwm_status {
	CWM_QUIT,
	CWM_RUNNING,
	CWM_EXEC_WM
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
TAILQ_HEAD(name_q, winname);
TAILQ_HEAD(ignore_q, winname);

struct client_ctx {
	TAILQ_ENTRY(client_ctx)	 entry;
	struct screen_ctx	*sc;
	struct group_ctx	*gc;
	Window			 win;
	Colormap		 colormap;
	int			 bwidth; /* border width */
	int			 obwidth; /* original border width */
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
#define CLIENT_SKIP_PAGER		0x4000
#define CLIENT_SKIP_TASKBAR		0x8000

#define CLIENT_SKIP_CYCLE		(CLIENT_HIDDEN | CLIENT_IGNORE | \
					 CLIENT_SKIP_TASKBAR | CLIENT_SKIP_PAGER)
#define CLIENT_HIGHLIGHT		(CLIENT_GROUP | CLIENT_UNGROUP)
#define CLIENT_MAXFLAGS			(CLIENT_VMAXIMIZED | CLIENT_HMAXIMIZED)
#define CLIENT_MAXIMIZED		(CLIENT_VMAXIMIZED | CLIENT_HMAXIMIZED)
	int			 flags;
	int			 stackingorder;
	struct name_q		 nameq;
	char			*name;
	char			*label;
	char			*res_class; /* class hint */
	char			*res_name; /* class hint */
	int			 initial_state; /* wm hint */
};
TAILQ_HEAD(client_q, client_ctx);

struct group_ctx {
	TAILQ_ENTRY(group_ctx)	 entry;
	struct screen_ctx	*sc;
	char			*name;
	int			 num;
};
TAILQ_HEAD(group_q, group_ctx);

struct autogroup {
	TAILQ_ENTRY(autogroup)	 entry;
	char			*class;
	char			*name;
	int 			 num;
};
TAILQ_HEAD(autogroup_q, autogroup);

struct region_ctx {
	TAILQ_ENTRY(region_ctx)	 entry;
	int			 num;
	struct geom		 view; /* viewable area */
	struct geom		 work; /* workable area, gap-applied */
};
TAILQ_HEAD(region_q, region_ctx);

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
	struct client_q		 clientq;
	struct region_q		 regionq;
	struct group_q		 groupq;
	struct group_ctx	*group_active;
	struct group_ctx	*group_last;
	Colormap		 colormap;
	Visual			*visual;
	struct {
		Window		 win;
		XftDraw		*xftdraw;
	} prop;
	XftColor		 xftcolor[CWM_COLOR_NITEMS];
	XftFont			*xftfont;
};
TAILQ_HEAD(screen_q, screen_ctx);

struct cargs {
	char		*cmd;
	int		 flag;
	enum {
		CWM_XEV_KEY,
		CWM_XEV_BTN
	} xev;
};
enum context {
	CWM_CONTEXT_NONE = 0,
	CWM_CONTEXT_CC,
	CWM_CONTEXT_SC
};
struct bind_ctx {
	TAILQ_ENTRY(bind_ctx)	 entry;
	void			(*callback)(void *, struct cargs *);
	struct cargs		*cargs;
	enum context		 context;
	unsigned int		 modmask;
	union {
		KeySym		 keysym;
		unsigned int	 button;
	} press;
};
TAILQ_HEAD(keybind_q, bind_ctx);
TAILQ_HEAD(mousebind_q, bind_ctx);

struct cmd_ctx {
	TAILQ_ENTRY(cmd_ctx)	 entry;
	char			*name;
	char			*path;
};
TAILQ_HEAD(cmd_q, cmd_ctx);
TAILQ_HEAD(wm_q, cmd_ctx);

#define CWM_MENU_DUMMY		0x0001
#define CWM_MENU_FILE		0x0002
#define CWM_MENU_LIST		0x0004
#define CWM_MENU_WINDOW_ALL	0x0008
#define CWM_MENU_WINDOW_HIDDEN	0x0010

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
	struct keybind_q	 keybindq;
	struct mousebind_q	 mousebindq;
	struct autogroup_q	 autogroupq;
	struct ignore_q		 ignoreq;
	struct cmd_q		 cmdq;
	struct wm_q		 wmq;
	int			 ngroups;
	int			 stickygroups;
	int			 nameqlen;
	int			 bwidth;
	int			 mamount;
	int			 snapdist;
	int			 htile;
	int			 vtile;
	struct gap		 gap;
	char			*color[CWM_COLOR_NITEMS];
	char			*font;
	char			*wmname;
	Cursor			 cursor[CF_NITEMS];
	int			 xrandr;
	int			 xrandr_event_base;
	char			*conf_file;
	char			*known_hosts;
	char			*wm_argv;
	int			 debug;
};

/* MWM hints */
struct mwm_hints {
#define MWM_HINTS_ELEMENTS	5L

#define MWM_HINTS_FUNCTIONS	(1L << 0)
#define MWM_HINTS_DECORATIONS	(1L << 1)
#define MWM_HINTS_INPUT_MODE	(1L << 2)
#define MWM_HINTS_STATUS	(1L << 3)
	unsigned long	flags;

#define MWM_FUNC_ALL		(1L << 0)
#define MWM_FUNC_RESIZE		(1L << 1)
#define MWM_FUNC_MOVE		(1L << 2)
#define MWM_FUNC_MINIMIZE	(1L << 3)
#define MWM_FUNC_MAXIMIZE	(1L << 4)
#define MWM_FUNC_CLOSE		(1L << 5)
	unsigned long	functions;

#define	MWM_DECOR_ALL		(1L << 0)
#define	MWM_DECOR_BORDER	(1L << 1)
#define MWM_DECOR_RESIZEH	(1L << 2)
#define MWM_DECOR_TITLE		(1L << 3)
#define MWM_DECOR_MENU		(1L << 4)
#define MWM_DECOR_MINIMIZE	(1L << 5)
#define MWM_DECOR_MAXIMIZE	(1L << 6)
	unsigned long	decorations;

#define MWM_INPUT_MODELESS			0
#define MWM_INPUT_PRIMARY_APPLICATION_MODAL	1
#define MWM_INPUT_SYSTEM_MODAL			2
#define MWM_INPUT_FULL_APPLICATION_MODAL	3
	long		inputMode;

#define MWM_TEAROFF_WINDOW	(1L << 0)
	unsigned long	status;
};

enum cwmh {
	WM_STATE,
	WM_DELETE_WINDOW,
	WM_TAKE_FOCUS,
	WM_PROTOCOLS,
	_MOTIF_WM_HINTS,
	UTF8_STRING,
	WM_CHANGE_STATE,
	CWMH_NITEMS
};
enum ewmh {
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
#define	_NET_WM_STATES_NITEMS	9
	_NET_WM_STATE_STICKY,
	_NET_WM_STATE_MAXIMIZED_VERT,
	_NET_WM_STATE_MAXIMIZED_HORZ,
	_NET_WM_STATE_HIDDEN,
	_NET_WM_STATE_FULLSCREEN,
	_NET_WM_STATE_DEMANDS_ATTENTION,
	_NET_WM_STATE_SKIP_PAGER,
	_NET_WM_STATE_SKIP_TASKBAR,
	_CWM_WM_STATE_FREEZE,
	EWMH_NITEMS
};
enum net_wm_state {
	_NET_WM_STATE_REMOVE,
	_NET_WM_STATE_ADD,
	_NET_WM_STATE_TOGGLE
};

extern Display				*X_Dpy;
extern Time				 Last_Event_Time;
extern Atom				 cwmh[CWMH_NITEMS];
extern Atom				 ewmh[EWMH_NITEMS];
extern struct screen_q			 Screenq;
extern struct conf			 Conf;

void			 usage(void);

void			 client_apply_sizehints(struct client_ctx *);
void			 client_close(struct client_ctx *);
void			 client_config(struct client_ctx *);
struct client_ctx	*client_current(struct screen_ctx *);
void			 client_draw_border(struct client_ctx *);
struct client_ctx	*client_find(Window);
void			 client_get_sizehints(struct client_ctx *);
void			 client_hide(struct client_ctx *);
void 			 client_htile(struct client_ctx *);
int			 client_inbound(struct client_ctx *, int, int);
struct client_ctx	*client_init(Window, struct screen_ctx *);
void			 client_lower(struct client_ctx *);
void			 client_move(struct client_ctx *);
void			 client_mtf(struct client_ctx *);
struct client_ctx	*client_next(struct client_ctx *);
struct client_ctx	*client_prev(struct client_ctx *);
void			 client_ptr_inbound(struct client_ctx *, int);
void			 client_ptr_save(struct client_ctx *);
void			 client_ptr_warp(struct client_ctx *);
void			 client_raise(struct client_ctx *);
void			 client_remove(struct client_ctx *);
void			 client_resize(struct client_ctx *, int);
void			 client_set_active(struct client_ctx *);
void			 client_set_name(struct client_ctx *);
void			 client_show(struct client_ctx *);
int			 client_snapcalc(int, int, int, int, int);
void			 client_toggle_hidden(struct client_ctx *);
void			 client_toggle_hmaximize(struct client_ctx *);
void			 client_toggle_fullscreen(struct client_ctx *);
void			 client_toggle_freeze(struct client_ctx *);
void			 client_toggle_maximize(struct client_ctx *);
void			 client_toggle_skip_pager(struct client_ctx *);
void			 client_toggle_skip_taskbar(struct client_ctx *);
void			 client_toggle_sticky(struct client_ctx *);
void			 client_toggle_vmaximize(struct client_ctx *);
void			 client_transient(struct client_ctx *);
void			 client_urgency(struct client_ctx *);
void 			 client_vtile(struct client_ctx *);
void			 client_wm_hints(struct client_ctx *);

void			 group_assign(struct group_ctx *, struct client_ctx *);
int			 group_autogroup(struct client_ctx *);
void			 group_cycle(struct screen_ctx *, int);
void			 group_hide(struct group_ctx *);
int			 group_holds_only_hidden(struct group_ctx *);
int			 group_holds_only_sticky(struct group_ctx *);
void			 group_init(struct screen_ctx *, int, const char *);
void			 group_movetogroup(struct client_ctx *, int);
void			 group_only(struct screen_ctx *, int);
void			 group_close(struct screen_ctx *, int);
int			 group_restore(struct client_ctx *);
void			 group_show(struct group_ctx *);
void			 group_toggle(struct screen_ctx *, int);
void			 group_toggle_all(struct screen_ctx *);
void			 group_toggle_membership(struct client_ctx *);
void			 group_update_names(struct screen_ctx *);

void			 search_match_client(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_cmd(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_exec(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_group(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_path(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_text(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_wm(struct menu_q *, struct menu_q *,
			     char *);
void			 search_print_client(struct menu *, int);
void			 search_print_cmd(struct menu *, int);
void			 search_print_group(struct menu *, int);
void			 search_print_text(struct menu *, int);
void			 search_print_wm(struct menu *, int);

struct region_ctx	*region_find(struct screen_ctx *, int, int);
void			 screen_assert_clients_within(struct screen_ctx *);
struct geom		 screen_area(struct screen_ctx *, int, int, int);
struct screen_ctx	*screen_find(Window);
void			 screen_init(int);
void			 screen_prop_win_create(struct screen_ctx *, Window);
void			 screen_prop_win_destroy(struct screen_ctx *);
void			 screen_prop_win_draw(struct screen_ctx *,
			     const char *, ...)
			    __attribute__((__format__ (printf, 2, 3)))
			    __attribute__((__nonnull__ (2)));
void			 screen_update_geometry(struct screen_ctx *);
void			 screen_updatestackingorder(struct screen_ctx *);

void			 kbfunc_cwm_status(void *, struct cargs *);
void			 kbfunc_ptrmove(void *, struct cargs *);
void			 kbfunc_client_snap(void *, struct cargs *);
void			 kbfunc_client_move(void *, struct cargs *);
void			 kbfunc_client_resize(void *, struct cargs *);
void			 kbfunc_client_close(void *, struct cargs *);
void			 kbfunc_client_lower(void *, struct cargs *);
void			 kbfunc_client_raise(void *, struct cargs *);
void			 kbfunc_client_hide(void *, struct cargs *);
void			 kbfunc_client_toggle_freeze(void *, struct cargs *);
void			 kbfunc_client_toggle_sticky(void *, struct cargs *);
void			 kbfunc_client_toggle_fullscreen(void *,
			      struct cargs *);
void			 kbfunc_client_toggle_maximize(void *, struct cargs *);
void			 kbfunc_client_toggle_hmaximize(void *, struct cargs *);
void			 kbfunc_client_toggle_vmaximize(void *, struct cargs *);
void 			 kbfunc_client_htile(void *, struct cargs *);
void 			 kbfunc_client_vtile(void *, struct cargs *);
void			 kbfunc_client_cycle(void *, struct cargs *);
void			 kbfunc_client_toggle_group(void *, struct cargs *);
void			 kbfunc_client_movetogroup(void *, struct cargs *);
void			 kbfunc_group_toggle(void *, struct cargs *);
void			 kbfunc_group_only(void *, struct cargs *);
void			 kbfunc_group_last(void *, struct cargs *);
void			 kbfunc_group_close(void *, struct cargs *);
void			 kbfunc_group_cycle(void *, struct cargs *);
void			 kbfunc_group_toggle_all(void *, struct cargs *);
void			 kbfunc_menu_client(void *, struct cargs *);
void			 kbfunc_menu_cmd(void *, struct cargs *);
void			 kbfunc_menu_group(void *, struct cargs *);
void			 kbfunc_menu_wm(void *, struct cargs *);
void			 kbfunc_menu_exec(void *, struct cargs *);
void			 kbfunc_menu_ssh(void *, struct cargs *);
void			 kbfunc_client_menu_label(void *, struct cargs *);
void			 kbfunc_exec_cmd(void *, struct cargs *);
void			 kbfunc_exec_lock(void *, struct cargs *);
void			 kbfunc_exec_term(void *, struct cargs *);

struct menu  		*menu_filter(struct screen_ctx *, struct menu_q *,
			     const char *, const char *, int,
			     void (*)(struct menu_q *, struct menu_q *, char *),
			     void (*)(struct menu *, int));
void			 menuq_add(struct menu_q *, void *, const char *, ...)
			    __attribute__((__format__ (printf, 3, 4)));
void			 menuq_clear(struct menu_q *);

int			 parse_config(const char *, struct conf *);

void			 conf_autogroup(struct conf *, int, const char *,
			     const char *);
int			 conf_bind_key(struct conf *, const char *,
    			     const char *);
int			 conf_bind_mouse(struct conf *, const char *,
    			     const char *);
void			 conf_clear(struct conf *);
void			 conf_client(struct client_ctx *);
void			 conf_cmd_add(struct conf *, const char *,
			     const char *);
void			 conf_wm_add(struct conf *, const char *,
			     const char *);
void			 conf_cursor(struct conf *);
void			 conf_grab_kbd(Window);
void			 conf_grab_mouse(Window);
void			 conf_init(struct conf *);
void			 conf_ignore(struct conf *, const char *);
void			 conf_screen(struct screen_ctx *);
void			 conf_group(struct screen_ctx *);

void			 xev_process(void);

int			 xu_get_prop(Window, Atom, Atom, long, unsigned char **);
int			 xu_get_strprop(Window, Atom, char **);
void			 xu_ptr_get(Window, int *, int *);
void			 xu_ptr_set(Window, int, int);
void			 xu_get_wm_state(Window, long *);
void			 xu_set_wm_state(Window, long);
void			 xu_send_clientmsg(Window, Atom, Time);
void 			 xu_xorcolor(XftColor, XftColor, XftColor *);

void			 xu_atom_init(void);
void			 xu_ewmh_net_supported(struct screen_ctx *);
void			 xu_ewmh_net_supported_wm_check(struct screen_ctx *);
void			 xu_ewmh_net_desktop_geometry(struct screen_ctx *);
void			 xu_ewmh_net_desktop_viewport(struct screen_ctx *);
void			 xu_ewmh_net_workarea(struct screen_ctx *);
void			 xu_ewmh_net_client_list(struct screen_ctx *);
void			 xu_ewmh_net_client_list_stacking(struct screen_ctx *);
void			 xu_ewmh_net_active_window(struct screen_ctx *, Window);
void			 xu_ewmh_net_number_of_desktops(struct screen_ctx *);
void			 xu_ewmh_net_showing_desktop(struct screen_ctx *);
void			 xu_ewmh_net_virtual_roots(struct screen_ctx *);
void			 xu_ewmh_net_current_desktop(struct screen_ctx *);
void			 xu_ewmh_net_desktop_names(struct screen_ctx *);
int			 xu_ewmh_get_net_wm_desktop(struct client_ctx *, long *);
void			 xu_ewmh_set_net_wm_desktop(struct client_ctx *);
Atom 			*xu_ewmh_get_net_wm_state(struct client_ctx *, int *);
void 			 xu_ewmh_handle_net_wm_state_msg(struct client_ctx *,
			     int, Atom, Atom);
void 			 xu_ewmh_set_net_wm_state(struct client_ctx *);
void 			 xu_ewmh_restore_net_wm_state(struct client_ctx *);

char			*u_argv(char * const *);
void			 u_exec(char *);
void			 u_spawn(char *);
void			 log_debug(int, const char *, const char *, ...)
			    __attribute__((__format__ (printf, 3, 4)))
			    __attribute__((__nonnull__ (3)));

void			*xcalloc(size_t, size_t);
void			*xmalloc(size_t);
void			*xreallocarray(void *, size_t, size_t);
char			*xstrdup(const char *);
int			 xasprintf(char **, const char *, ...)
			    __attribute__((__format__ (printf, 2, 3)))
			    __attribute__((__nonnull__ (2)));
int			 xvasprintf(char **, const char *, va_list)
			    __attribute__((__nonnull__ (2)));

#endif /* _CALMWM_H_ */

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

#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xinerama.h>
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
#define	WMNAME	 	"CWM"

#define ChildMask	(SubstructureRedirectMask|SubstructureNotifyMask)
#define ButtonMask	(ButtonPressMask|ButtonReleaseMask)
#define MouseMask	(ButtonMask|PointerMotionMask)
#define KeyMask		(KeyPressMask|ExposureMask)
#define MenuMask 	(ButtonMask|ButtonMotionMask|ExposureMask| \
			PointerMotionMask)
#define MenuGrabMask	(ButtonMask|ButtonMotionMask|StructureNotifyMask|\
			PointerMotionMask)
#define SearchMask	(KeyPressMask|ExposureMask)

/* kb movement */
#define CWM_MOVE		0x0001
#define CWM_RESIZE		0x0002
#define CWM_PTRMOVE		0x0004
#define CWM_BIGMOVE		0x0008
#define CWM_UP			0x0010
#define CWM_DOWN		0x0020
#define CWM_LEFT		0x0040
#define CWM_RIGHT		0x0080

/* exec */
#define	CWM_EXEC_PROGRAM	0x0001
#define	CWM_EXEC_WM		0x0002

/* client cycle */
#define CWM_CYCLE		0x0001
#define CWM_RCYCLE		0x0002

/* group cycle */
#define CWM_CYCLEGROUP		0x0001
#define CWM_RCYCLEGROUP		0x0002

#define KBTOGROUP(X) ((X) - 1)

union arg {
	char	*c;
	int	 i;
};

enum cwmcolor {
	CWM_COLOR_BORDOR_ACTIVE,
	CWM_COLOR_BORDER_INACTIVE,
	CWM_COLOR_BORDER_GROUP,
	CWM_COLOR_BORDER_UNGROUP,
	CWM_COLOR_FG_MENU,
	CWM_COLOR_BG_MENU,
	CWM_COLOR_MAX
};

struct color {
	unsigned long	 pixel;
	char		*name;
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

struct client_ctx {
	TAILQ_ENTRY(client_ctx) entry;
	TAILQ_ENTRY(client_ctx) group_entry;
	TAILQ_ENTRY(client_ctx) mru_entry;
	struct screen_ctx	*sc;
	Window			 win;
	XSizeHints		*size;
	Colormap		 cmap;
	u_int			 bwidth; /* border width */
	struct {
		int		 x;	/* x position */
		int		 y;	/* y position */
		int		 width;	/* width */
		int		 height;/* height */
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
	} geom, savegeom;
	struct {
		int		 x;	/* x position */
		int		 y;	/* y position */
	} ptr;
#define CLIENT_PROTO_DELETE		 0x0001
#define CLIENT_PROTO_TAKEFOCUS		 0x0002
	int			 xproto;
#define CLIENT_HIDDEN			0x0001
#define CLIENT_IGNORE			0x0002
#define CLIENT_DOMAXIMIZE		0x0004
#define CLIENT_MAXIMIZED		0x0008
#define CLIENT_DOVMAXIMIZE		0x0010
#define CLIENT_VMAXIMIZED		0x0020
#define CLIENT_DOHMAXIMIZE		0x0040
#define CLIENT_HMAXIMIZED		0x0080
#define CLIENT_FREEZE			0x0100
	int			 flags;
	int			 state;
	int			 active;
	int			 stackingorder;
#define CLIENT_HIGHLIGHT_GROUP		0x0001
#define CLIENT_HIGHLIGHT_UNGROUP	0x0002
	int			 highlight;
	struct winname_q	 nameq;
#define CLIENT_MAXNAMEQLEN		5
	int			 nameqlen;
	char			*name;
	char			*label;
	char			*matchname;
	struct group_ctx	*group;
	char			*app_class;
	char			*app_name;
};
TAILQ_HEAD(client_ctx_q, client_ctx);
TAILQ_HEAD(cycle_entry_q, client_ctx);

struct winmatch {
	TAILQ_ENTRY(winmatch)	entry;
#define WIN_MAXTITLELEN		256
	char			title[WIN_MAXTITLELEN];
};
TAILQ_HEAD(winmatch_q, winmatch);

struct group_ctx {
	TAILQ_ENTRY(group_ctx)	 entry;
	struct client_ctx_q	 clients;
	int			 shortcut;
	int			 hidden;
	int			 nhidden;
	int			 highstack;
};
TAILQ_HEAD(group_ctx_q, group_ctx);

struct autogroupwin {
	TAILQ_ENTRY(autogroupwin)	 entry;
	char				*class;
	char				*name;
	int 				 num;
};
TAILQ_HEAD(autogroupwin_q, autogroupwin);

struct screen_ctx {
	TAILQ_ENTRY(screen_ctx)	 entry;
	u_int			 which;
	Window			 rootwin;
	Window			 menuwin;
	struct color		 color[CWM_COLOR_MAX];
	GC			 gc;
	int			 altpersist;
	int			 xmax;
	int			 ymax;
	struct gap		 gap;
	struct cycle_entry_q	 mruq;
	XftColor		 xftcolor;
	XftDraw			*xftdraw;
	XftFont			*font;
	u_int			 fontheight;
	int			 xinerama_no;
	XineramaScreenInfo	*xinerama;
#define CALMWM_NGROUPS		 9
	struct group_ctx	 groups[CALMWM_NGROUPS];
	struct group_ctx_q	 groupq;
	int			 group_hideall;
	int			 group_nonames;
	struct group_ctx	*group_active;
	char 			**group_names;
};
TAILQ_HEAD(screen_ctx_q, screen_ctx);

struct keybinding {
	TAILQ_ENTRY(keybinding)	 entry;
	void			(*callback)(struct client_ctx *, union arg *);
	union arg		 argument;
	int			 modmask;
	int			 keysym;
	int			 keycode;
#define KBFLAG_NEEDCLIENT	 0x0001
	int			 flags;
};
TAILQ_HEAD(keybinding_q, keybinding);

struct mousebinding {
	TAILQ_ENTRY(mousebinding)	entry;
	void			 	(*callback)(struct client_ctx *, void *);
	int				modmask;
	int			 	button;
#define MOUSEBIND_CTX_ROOT		0x0001
#define MOUSEBIND_CTX_WIN		0x0002
	int				context;
};
TAILQ_HEAD(mousebinding_q, mousebinding);

struct cmd {
	TAILQ_ENTRY(cmd)	entry;
	int			flags;
	char			image[MAXPATHLEN];
#define CMD_MAXLABELLEN		256
	char			label[CMD_MAXLABELLEN];
};
TAILQ_HEAD(cmd_q, cmd);

struct menu {
	TAILQ_ENTRY(menu)	 entry;
	TAILQ_ENTRY(menu)	 resultentry;
#define MENU_MAXENTRY		 50
	char			 text[MENU_MAXENTRY + 1];
	char			 print[MENU_MAXENTRY + 1];
	void			*ctx;
	short			 dummy;
	short			 abort;
};
TAILQ_HEAD(menu_q, menu);

struct conf {
	struct keybinding_q	 keybindingq;
	struct autogroupwin_q	 autogroupq;
	struct winmatch_q	 ignoreq;
	char			 conf_path[MAXPATHLEN];
	struct cmd_q		 cmdq;
	struct mousebinding_q	 mousebindingq;
#define	CONF_STICKY_GROUPS		0x0001
	int			 flags;
#define CONF_BWIDTH			1
	int			 bwidth;
#define	CONF_MAMOUNT			1
	int			 mamount;
	struct gap		 gap;
#define CONF_COLOR_ACTIVEBORDER		"#CCCCCC"
#define CONF_COLOR_INACTIVEBORDER	"#666666"
#define CONF_COLOR_GROUPBORDER		"blue"
#define CONF_COLOR_UNGROUPBORDER	"red"
#define CONF_COLOR_MENUFG		"black"
#define CONF_COLOR_MENUBG		"white"
	struct color		 color[CWM_COLOR_MAX];
	char			 termpath[MAXPATHLEN];
	char			 lockpath[MAXPATHLEN];
#define	DEFAULTFONTNAME			"sans-serif:pixelsize=14:bold"
	char			*DefaultFontName;
};

/* MWM hints */
struct mwm_hints {
	u_long	flags;
	u_long	functions;
	u_long	decorations;
};
#define MWM_NUMHINTS		3
#define	PROP_MWM_HINTS_ELEMENTS	3
#define	MWM_HINTS_DECORATIONS	(1<<1)
#define	MWM_DECOR_ALL		(1<<0)
#define	MWM_DECOR_BORDER	(1<<1)

__dead void		 usage(void);

void			 client_applysizehints(struct client_ctx *);
struct client_ctx	*client_current(void);
struct client_ctx	*client_cycle(struct screen_ctx *, int);
int			 client_delete(struct client_ctx *);
void			 client_draw_border(struct client_ctx *);
struct client_ctx	*client_find(Window);
void			 client_freeze(struct client_ctx *);
void			 client_getsizehints(struct client_ctx *);
void			 client_hide(struct client_ctx *);
void			 client_horizmaximize(struct client_ctx *);
void			 client_leave(struct client_ctx *);
void			 client_lower(struct client_ctx *);
void			 client_map(struct client_ctx *);
void			 client_maximize(struct client_ctx *);
void			 client_move(struct client_ctx *);
void			 client_mtf(struct client_ctx *);
struct client_ctx	*client_new(Window, struct screen_ctx *, int);
void			 client_ptrsave(struct client_ctx *);
void			 client_ptrwarp(struct client_ctx *);
void			 client_raise(struct client_ctx *);
void			 client_resize(struct client_ctx *);
void			 client_send_delete(struct client_ctx *);
void			 client_setactive(struct client_ctx *, int);
void			 client_setname(struct client_ctx *);
void			 client_unhide(struct client_ctx *);
void			 client_vertmaximize(struct client_ctx *);
void			 client_warp(struct client_ctx *);

void			 group_alltoggle(struct screen_ctx *);
void			 group_autogroup(struct client_ctx *);
void			 group_client_delete(struct client_ctx *);
void			 group_cycle(struct screen_ctx *, int);
void			 group_hidetoggle(struct screen_ctx *, int);
void			 group_init(struct screen_ctx *);
void			 group_make_autogroup(struct conf *, char *, int);
void			 group_menu(XButtonEvent *);
void			 group_movetogroup(struct client_ctx *, int);
void			 group_only(struct screen_ctx *, int);
void			 group_sticky(struct client_ctx *);
void			 group_sticky_toggle_enter(struct client_ctx *);
void			 group_sticky_toggle_exit(struct client_ctx *);
void			 group_update_names(struct screen_ctx *);

void			 search_match_client(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_exec(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_text(struct menu_q *, struct menu_q *,
			     char *);
void			 search_print_client(struct menu *, int);

XineramaScreenInfo	*screen_find_xinerama(struct screen_ctx *, int, int);
struct screen_ctx	*screen_fromroot(Window);
void			 screen_init_xinerama(struct screen_ctx *);
void			 screen_update_geometry(struct screen_ctx *, int, int);
void			 screen_updatestackingorder(struct screen_ctx *);

void			 kbfunc_client_cycle(struct client_ctx *, union arg *);
void			 kbfunc_client_cyclegroup(struct client_ctx *,
			     union arg *);
void			 kbfunc_client_delete(struct client_ctx *, union arg *);
void			 kbfunc_client_freeze(struct client_ctx *, union arg *);
void			 kbfunc_client_group(struct client_ctx *, union arg *);
void			 kbfunc_client_grouponly(struct client_ctx *,
			     union arg *);
void			 kbfunc_client_grouptoggle(struct client_ctx *,
			     union arg *);
void			 kbfunc_client_hide(struct client_ctx *, union arg *);
void			 kbfunc_client_hmaximize(struct client_ctx *,
			     union arg *);
void			 kbfunc_client_label(struct client_ctx *, union arg *);
void			 kbfunc_client_lower(struct client_ctx *, union arg *);
void			 kbfunc_client_maximize(struct client_ctx *,
			     union arg *);
void			 kbfunc_client_movetogroup(struct client_ctx *,
			     union arg *);
void			 kbfunc_client_nogroup(struct client_ctx *,
			     union arg *);
void			 kbfunc_client_raise(struct client_ctx *, union arg *);
void			 kbfunc_client_rcycle(struct client_ctx *, union arg *);
void			 kbfunc_client_search(struct client_ctx *, union arg *);
void			 kbfunc_client_vmaximize(struct client_ctx *,
			     union arg *);
void			 kbfunc_cmdexec(struct client_ctx *, union arg *);
void			 kbfunc_exec(struct client_ctx *, union arg *);
void			 kbfunc_lock(struct client_ctx *, union arg *);
void			 kbfunc_menu_search(struct client_ctx *, union arg *);
void			 kbfunc_moveresize(struct client_ctx *, union arg *);
void			 kbfunc_quit_wm(struct client_ctx *, union arg *);
void			 kbfunc_reload(struct client_ctx *, union arg *);
void			 kbfunc_ssh(struct client_ctx *, union arg *);
void			 kbfunc_term(struct client_ctx *, union arg *);

void			 mousefunc_menu_cmd(struct client_ctx *, void *);
void			 mousefunc_menu_group(struct client_ctx *, void *);
void			 mousefunc_menu_unhide(struct client_ctx *, void *);
void			 mousefunc_window_grouptoggle(struct client_ctx *,
			    void *);
void			 mousefunc_window_hide(struct client_ctx *, void *);
void			 mousefunc_window_lower(struct client_ctx *, void *);
void			 mousefunc_window_move(struct client_ctx *, void *);
void			 mousefunc_window_resize(struct client_ctx *, void *);

struct menu  		*menu_filter(struct screen_ctx *, struct menu_q *,
			     char *, char *, int,
			     void (*)(struct menu_q *, struct menu_q *, char *),
			     void (*)(struct menu *, int));
void			 menu_init(struct screen_ctx *);

int			 parse_config(const char *, struct conf *);

void			 conf_bindname(struct conf *, char *, char *);
void			 conf_clear(struct conf *);
void			 conf_client(struct client_ctx *);
void			 conf_cmd_add(struct conf *, char *, char *, int);
void			 conf_color(struct conf *, struct screen_ctx *);
void			 conf_font(struct conf *, struct screen_ctx *);
void			 conf_gap(struct conf *, struct screen_ctx *);
void			 conf_grab(struct conf *, struct keybinding *);
void			 conf_grab_mouse(struct client_ctx *);
void			 conf_init(struct conf *);
void			 conf_mousebind(struct conf *, char *, char *);
void			 conf_reload(struct conf *);
void			 conf_setup(struct conf *, const char *);
void			 conf_ungrab(struct conf *, struct keybinding *);

int			 font_ascent(struct screen_ctx *);
int			 font_descent(struct screen_ctx *);
void			 font_draw(struct screen_ctx *, const char *, int,
			     Drawable, int, int);
u_int			 font_height(struct screen_ctx *);
void			 font_init(struct screen_ctx *);
int			 font_width(struct screen_ctx *, const char *, int);
XftFont			*font_make(struct screen_ctx *, const char *);

void			 xev_loop(void);

void			 xu_btn_grab(Window, int, u_int);
void			 xu_btn_ungrab(Window, int, u_int);
void			 xu_configure(struct client_ctx *);
void			 xu_freecolor(struct screen_ctx *, unsigned long);
void			 xu_getatoms(void);
unsigned long		 xu_getcolor(struct screen_ctx *, char *);
int			 xu_getprop(Window, Atom, Atom, long, u_char **);
int			 xu_getstate(struct client_ctx *, int *);
int			 xu_getstrprop(Window, Atom, char **);
void			 xu_key_grab(Window, int, int);
void			 xu_key_ungrab(Window, int, int);
void			 xu_ptr_getpos(Window, int *, int *);
int			 xu_ptr_grab(Window, int, Cursor);
int			 xu_ptr_regrab(int, Cursor);
void			 xu_ptr_setpos(Window, int, int);
void			 xu_ptr_ungrab(void);
void			 xu_sendmsg(Window, Atom, long);
void			 xu_setstate(struct client_ctx *, int);
void			 xu_setwmname(struct screen_ctx *);

void			 u_exec(char *);
int			 u_spawn(char *);

void			*xcalloc(size_t, size_t);
void			 xfree(void *);
void			*xmalloc(size_t);
char			*xstrdup(const char *);

/* Externs */
extern Display				*X_Dpy;

extern Cursor				 Cursor_move;
extern Cursor				 Cursor_resize;
extern Cursor				 Cursor_select;
extern Cursor				 Cursor_default;
extern Cursor				 Cursor_question;

extern struct screen_ctx_q		 Screenq;
extern struct client_ctx_q		 Clientq;
extern struct conf			 Conf;

extern int				 HasXinerama, HasRandr, Randr_ev;

#define	WM_STATE			 cwm_atoms[0]
#define WM_DELETE_WINDOW		 cwm_atoms[1]
#define WM_TAKE_FOCUS			 cwm_atoms[2]
#define WM_PROTOCOLS			 cwm_atoms[3]
#define _MOTIF_WM_HINTS			 cwm_atoms[4]
#define	UTF8_STRING			 cwm_atoms[5]
/*
 * please make all hints below this point netwm hints, starting with
 * _NET_SUPPORTED. If you change other hints make sure you update
 * CWM_NETWM_START
 */
#define	_NET_SUPPORTED			 cwm_atoms[6]
#define	_NET_SUPPORTING_WM_CHECK	 cwm_atoms[7]
#define	_NET_WM_NAME			 cwm_atoms[8]
#define	_NET_ACTIVE_WINDOW		 cwm_atoms[9]
#define	_NET_CLIENT_LIST		 cwm_atoms[10]
#define	_NET_NUMBER_OF_DESKTOPS		 cwm_atoms[11]
#define	_NET_CURRENT_DESKTOP		 cwm_atoms[12]
#define	_NET_DESKTOP_VIEWPORT		 cwm_atoms[13]
#define	_NET_DESKTOP_GEOMETRY		 cwm_atoms[14]
#define	_NET_VIRTUAL_ROOTS		 cwm_atoms[15]
#define	_NET_SHOWING_DESKTOP		 cwm_atoms[16]
#define	_NET_DESKTOP_NAMES		 cwm_atoms[17]
#define	_NET_WM_DESKTOP			 cwm_atoms[18]
#define	_NET_WORKAREA			 cwm_atoms[19]
#define CWM_NO_ATOMS			 20
#define CWM_NETWM_START			 6

extern Atom				 cwm_atoms[CWM_NO_ATOMS];

#endif /* _CALMWM_H_ */

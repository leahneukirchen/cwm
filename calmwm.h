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
 * $Id$
 */

#ifndef _CALMWM_H_
#define _CALMWM_H_

#define CALMWM_MAXNAMELEN 256

#undef MIN
#undef MAX
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

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

struct client_ctx;

TAILQ_HEAD(cycle_entry_q, client_ctx);

struct screen_ctx {
	TAILQ_ENTRY(screen_ctx)	entry;

	u_int		 which;
	Window		 rootwin;
	Window		 menuwin;

	struct color	 color[CWM_COLOR_MAX];
	GC		 gc;

	int		 altpersist;

	int		 xmax;
	int		 ymax;

	struct cycle_entry_q mruq;

	XftDraw		*xftdraw;
	XftColor	 xftcolor;

	int		 xinerama_no;
	XineramaScreenInfo *xinerama;
};

TAILQ_HEAD(screen_ctx_q, screen_ctx);

#define CLIENT_PROTO_DELETE	0x01
#define CLIENT_PROTO_TAKEFOCUS	0x02

#define CLIENT_MAXNAMEQLEN	5

#define CLIENT_HIDDEN		0x01
#define CLIENT_IGNORE		0x02
#define CLIENT_DOMAXIMIZE	0x04
#define CLIENT_MAXIMIZED	0x08
#define CLIENT_DOVMAXIMIZE	0x10
#define CLIENT_VMAXIMIZED	0x20
#define CLIENT_DOHMAXIMIZE	0x40
#define CLIENT_HMAXIMIZED	0x80

#define CLIENT_HIGHLIGHT_GROUP		1
#define CLIENT_HIGHLIGHT_UNGROUP	2

struct winname {
	TAILQ_ENTRY(winname)	 entry;
	char			*name;
};

TAILQ_HEAD(winname_q, winname);

struct client_ctx {
	TAILQ_ENTRY(client_ctx) entry;
	TAILQ_ENTRY(client_ctx) searchentry;
	TAILQ_ENTRY(client_ctx) group_entry;
	TAILQ_ENTRY(client_ctx) mru_entry;

	struct screen_ctx	*sc;
	Window			 win;
	XSizeHints		*size;

	Colormap		 cmap;

	u_int			 bwidth;
	struct {
		int		 x, y, width, height, basew, baseh,
				 minw, minh, maxw, maxh, incw, inch;
		float		 mina, maxa;
	} geom, savegeom;

	struct {
		int		 x,y;
	} ptr;

	int			 xproto;

	int			 flags;
	int			 state;
	char			*name;
	struct winname_q	 nameq;
	size_t			 nameqlen;

	char			*label;
	int			 active;
	int			 highlight;

	char			*matchname;
	struct group_ctx	*group;

	int			stackingorder;

	char			*app_class;
	char			*app_name;
	char			*app_cliarg;
};

TAILQ_HEAD(client_ctx_q, client_ctx);

extern const char *shortcut_to_name[];

struct group_ctx {
	TAILQ_ENTRY(group_ctx)	 entry;
	struct client_ctx_q	 clients;
	const char		*name;
	int			 shortcut;
	int			 hidden;
	int			 nhidden;
	int			 highstack;
};

TAILQ_HEAD(group_ctx_q, group_ctx);

/* Autogroups */
struct autogroupwin {
	TAILQ_ENTRY(autogroupwin) entry;

	char	*class;
	char	*name;
	char	*group;
};

TAILQ_HEAD(autogroupwin_q, autogroupwin);

#define CWM_MOVE	0x01
#define CWM_RESIZE	0x02
#define CWM_PTRMOVE	0x04
#define CWM_BIGMOVE	0x08
#define CWM_UP		0x10
#define CWM_DOWN	0x20
#define CWM_LEFT	0x40
#define CWM_RIGHT	0x80

/*
 * Match a window.
 */
#define CONF_MAX_WINTITLE	256
struct winmatch {
	TAILQ_ENTRY(winmatch)	entry;
	char			title[CONF_MAX_WINTITLE];
};

TAILQ_HEAD(winmatch_q, winmatch);

/* for cwm_exec */
#define	CWM_EXEC_PROGRAM	0x1
#define	CWM_EXEC_WM		0x2
/* for alt-tab */
#define CWM_CYCLE		0x0
#define CWM_RCYCLE		0x1
/* for group cycle */
#define CWM_CYCLEGROUP		0x0
#define CWM_RCYCLEGROUP		0x1

#define KBFLAG_NEEDCLIENT 0x01

#define KBTOGROUP(X) ((X) - 1)

union arg {
	char	*c;
	int	 i;
};

struct keybinding {
	TAILQ_ENTRY(keybinding)	 entry;
	void			(*callback)(struct client_ctx *, union arg *);
	union arg		 argument;
	int			 modmask;
	int			 keysym;
	int			 keycode;
	int			 flags;
};

struct cmd {
	TAILQ_ENTRY(cmd)	entry;
	int			flags;
	char			image[MAXPATHLEN];
	char			label[256];
	/* (argv) */
};

struct mousebinding {
	int				modmask;
	int			 	button;
	int				context;
	void			 	(*callback)(struct client_ctx *, void *);
	TAILQ_ENTRY(mousebinding)	entry;
};

#define MOUSEBIND_CTX_ROOT	1
#define MOUSEBIND_CTX_WIN	2

TAILQ_HEAD(keybinding_q, keybinding);
TAILQ_HEAD(cmd_q, cmd);
TAILQ_HEAD(mousebinding_q, mousebinding);

/* Global configuration */
struct conf {
	struct keybinding_q	 keybindingq;
	struct autogroupwin_q	 autogroupq;
	struct winmatch_q	 ignoreq;
	char			 conf_path[MAXPATHLEN];
	struct cmd_q		 cmdq;
	struct mousebinding_q	 mousebindingq;

#define	CONF_STICKY_GROUPS	 0x0001
	int			 flags;
#define CONF_BWIDTH		 1
	int			 bwidth;
#define	CONF_MAMOUNT		 1
	int			 mamount;

#define CONF_COLOR_ACTIVEBORDER		"#CCCCCC"
#define CONF_COLOR_INACTIVEBORDER	"#666666"
#define CONF_COLOR_GROUPBORDER		"blue"
#define CONF_COLOR_UNGROUPBORDER	"red"
#define CONF_COLOR_MENUFG		"black"
#define CONF_COLOR_MENUBG		"white"
	struct color		 color[CWM_COLOR_MAX];

	char			 termpath[MAXPATHLEN];
	char			 lockpath[MAXPATHLEN];

#define	DEFAULTFONTNAME		 "sans-serif:pixelsize=14:bold"
	char			*DefaultFontName;
	XftFont			*DefaultFont;
	u_int			 FontHeight;
	int			 gap_top, gap_bottom, gap_left, gap_right;
};

/* Menu stuff */

#define MENU_MAXENTRY 50

struct menu {
	TAILQ_ENTRY(menu)	entry;
	TAILQ_ENTRY(menu)	resultentry;

	char			 text[MENU_MAXENTRY + 1];
	char			 print[MENU_MAXENTRY + 1];
	void			*ctx;
	short			 dummy;
};

TAILQ_HEAD(menu_q, menu);

enum ctltype {
	CTL_NONE = -1,
	CTL_ERASEONE = 0, CTL_WIPE, CTL_UP, CTL_DOWN, CTL_RETURN,
	CTL_ABORT, CTL_ALL
};

/* MWM hints */

struct mwm_hints {
	u_long	flags;
	u_long	functions;
	u_long	decorations;
};

#define MWM_NUMHINTS 3

#define	PROP_MWM_HINTS_ELEMENTS	3
#define	MWM_HINTS_DECORATIONS	(1 << 1)
#define	MWM_DECOR_ALL		(1 << 0)
#define	MWM_DECOR_BORDER	(1 << 1)

int			 input_keycodetrans(KeyCode, u_int, enum ctltype *,
			     char *);

__dead void		 usage(void);

struct client_ctx	*client_find(Window);
struct client_ctx	*client_new(Window, struct screen_ctx *, int);
int			 client_delete(struct client_ctx *);
void			 client_setactive(struct client_ctx *, int);
void			 client_resize(struct client_ctx *);
void			 client_lower(struct client_ctx *);
void			 client_raise(struct client_ctx *);
void			 client_move(struct client_ctx *);
void			 client_leave(struct client_ctx *);
void			 client_send_delete(struct client_ctx *);
struct client_ctx	*client_current(void);
void			 client_hide(struct client_ctx *);
void			 client_unhide(struct client_ctx *);
void			 client_setname(struct client_ctx *);
void			 client_warp(struct client_ctx *);
void			 client_ptrwarp(struct client_ctx *);
void			 client_ptrsave(struct client_ctx *);
void			 client_draw_border(struct client_ctx *);
void			 client_maximize(struct client_ctx *);
void			 client_vertmaximize(struct client_ctx *);
void			 client_horizmaximize(struct client_ctx *);
void			 client_map(struct client_ctx *);
void			 client_mtf(struct client_ctx *);
struct client_ctx	*client_cycle(int);
void			 client_getsizehints(struct client_ctx *);
void			 client_applysizehints(struct client_ctx *);

struct menu  		*menu_filter(struct menu_q *, char *, char *, int,
			     void (*)(struct menu_q *, struct menu_q *, char *),
			     void (*)(struct menu *, int));
void			 menu_init(struct screen_ctx *);

/* XXX should be xu_ */
void			  xev_reconfig(struct client_ctx *);

void			 xev_loop(void);

void			 xu_getatoms(void);
int			 xu_ptr_grab(Window, int, Cursor);
void			 xu_btn_grab(Window, int, u_int);
int			 xu_ptr_regrab(int, Cursor);
void			 xu_btn_ungrab(Window, int, u_int);
void			 xu_ptr_ungrab(void);
void			 xu_ptr_setpos(Window, int, int);
void			 xu_ptr_getpos(Window, int *, int *);
void			 xu_key_grab(Window, int, int);
void			 xu_key_ungrab(Window, int, int);
void			 xu_sendmsg(struct client_ctx *, Atom, long);
int			 xu_getprop(struct client_ctx *, Atom, Atom, long,
			     u_char **);
char			*xu_getstrprop(struct client_ctx *, Atom atm);
void			 xu_setstate(struct client_ctx *, int);
int			 xu_getstate(struct client_ctx *, int *);
unsigned long		 xu_getcolor(struct screen_ctx *, char *);
void			 xu_freecolor(struct screen_ctx *, unsigned long);
void			 xu_setwmname(struct screen_ctx *);

int			 u_spawn(char *);
void			 u_exec(char *);

void			 xfree(void *);
void			*xmalloc(size_t);
void			*xcalloc(size_t, size_t);
char			*xstrdup(const char *);

struct screen_ctx	*screen_fromroot(Window);
struct screen_ctx	*screen_current(void);
void			 screen_updatestackingorder(void);
void			 screen_init_xinerama(struct screen_ctx *);
XineramaScreenInfo	*screen_find_xinerama(struct screen_ctx *, int, int);

void			 conf_setup(struct conf *, const char *);
void			 conf_client(struct client_ctx *);
void			 conf_grab(struct conf *, struct keybinding *);
void			 conf_ungrab(struct conf *, struct keybinding *);
void			 conf_bindname(struct conf *, char *, char *);
void			 conf_mousebind(struct conf *, char *, char *);
void			 conf_grab_mouse(struct client_ctx *);
void			 conf_reload(struct conf *);
void			 conf_font(struct conf *);
void			 conf_color(struct conf *);
void			 conf_init(struct conf *);
void			 conf_clear(struct conf *);
void			 conf_cmd_add(struct conf *, char *, char *, int);

int			 parse_config(const char *, struct conf *);

void			 kbfunc_client_lower(struct client_ctx *, union arg *);
void			 kbfunc_client_raise(struct client_ctx *, union arg *);
void			 kbfunc_client_search(struct client_ctx *, union arg *);
void			 kbfunc_client_hide(struct client_ctx *, union arg *);
void			 kbfunc_client_cycle(struct client_ctx *, union arg *);
void			 kbfunc_client_rcycle(struct client_ctx *, union arg *);
void			 kbfunc_cmdexec(struct client_ctx *, union arg *);
void			 kbfunc_client_label(struct client_ctx *, union arg *);
void			 kbfunc_client_delete(struct client_ctx *, union arg *);
void			 kbfunc_client_group(struct client_ctx *, union arg *);
void			 kbfunc_client_grouponly(struct client_ctx *,
			     union arg *);
void			 kbfunc_client_cyclegroup(struct client_ctx *,
			     union arg *);
void			 kbfunc_client_nogroup(struct client_ctx *,
			     union arg *);
void			 kbfunc_client_grouptoggle(struct client_ctx *,
			     union arg *);
void			 kbfunc_client_movetogroup(struct client_ctx *,
			     union arg *);
void			 kbfunc_client_maximize(struct client_ctx *,
			     union arg *);
void			 kbfunc_client_vmaximize(struct client_ctx *,
			     union arg *);
void			 kbfunc_client_hmaximize(struct client_ctx *,
			     union arg *);
void			 kbfunc_reload(struct client_ctx *, union arg *);
void			 kbfunc_quit_wm(struct client_ctx *, union arg *);
void			 kbfunc_moveresize(struct client_ctx *, union arg *);
void			 kbfunc_menu_search(struct client_ctx *, union arg *);
void			 kbfunc_exec(struct client_ctx *, union arg *);
void			 kbfunc_ssh(struct client_ctx *, union arg *);
void			 kbfunc_term(struct client_ctx *, union arg *);
void			 kbfunc_lock(struct client_ctx *, union arg *);

void			 mousefunc_window_resize(struct client_ctx *, void *);
void			 mousefunc_window_move(struct client_ctx *, void *);
void			 mousefunc_window_grouptoggle(struct client_ctx *,
			    void *);
void			 mousefunc_window_lower(struct client_ctx *, void *);
void			 mousefunc_window_hide(struct client_ctx *, void *);
void			 mousefunc_menu_group(struct client_ctx *, void *);
void			 mousefunc_menu_unhide(struct client_ctx *, void *);
void			 mousefunc_menu_cmd(struct client_ctx *, void *);

void			 search_match_client(struct menu_q *, struct menu_q *,
			     char *);
void			 search_print_client(struct menu *, int);
void			 search_match_text(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_exec(struct menu_q *, struct menu_q *,
			     char *);

void			 group_init(void);
void			 group_hidetoggle(int);
void			 group_only(int);
void			 group_cycle(int);
void			 group_sticky(struct client_ctx *);
void			 group_client_delete(struct client_ctx *);
void			 group_menu(XButtonEvent *);
void			 group_alltoggle(void);
void			 group_sticky_toggle_enter(struct client_ctx *);
void			 group_sticky_toggle_exit(struct client_ctx *);
void			 group_autogroup(struct client_ctx *);
void			 group_movetogroup(struct client_ctx *, int);

void			 font_init(struct screen_ctx *);
int			 font_width(const char *, int);
void			 font_draw(struct screen_ctx *, const char *, int,
			     Drawable, int, int);
XftFont			*font_make(struct screen_ctx *, const char *);

#define font_ascent()	Conf.DefaultFont->ascent
#define font_descent()	Conf.DefaultFont->descent
#define	font_height()	Conf.FontHeight

/* Externs */

extern Display				*X_Dpy;

extern Cursor				 Cursor_move;
extern Cursor				 Cursor_resize;
extern Cursor				 Cursor_select;
extern Cursor				 Cursor_default;
extern Cursor				 Cursor_question;

extern struct screen_ctx_q		 Screenq;
extern struct screen_ctx		*curscreen;

extern struct client_ctx_q		 Clientq;

extern int				 HasXinerama, HasRandr, Randr_ev;
extern struct conf			 Conf;

#define	WM_STATE			 cwm_atoms[0]
#define WM_DELETE_WINDOW		 cwm_atoms[1]
#define WM_TAKE_FOCUS			 cwm_atoms[2]
#define WM_PROTOCOLS			 cwm_atoms[3]
#define _MOTIF_WM_HINTS			 cwm_atoms[4]
#define	_CWM_GRP			 cwm_atoms[5]
#define	UTF8_STRING			 cwm_atoms[6]
/*
 * please make all hints below this point netwm hints, starting with
 * _NET_SUPPORTED. If you change other hints make sure you update
 * CWM_NETWM_START
 */
#define	_NET_SUPPORTED			 cwm_atoms[7]
#define	_NET_SUPPORTING_WM_CHECK	 cwm_atoms[8]
#define	_NET_WM_NAME			 cwm_atoms[9]
#define	_NET_ACTIVE_WINDOW		 cwm_atoms[10]
#define _NET_CLIENT_LIST		 cwm_atoms[11]
#define CWM_NO_ATOMS			 12
#define CWM_NETWM_START			 7

extern Atom				 cwm_atoms[CWM_NO_ATOMS];

#endif /* _CALMWM_H_ */

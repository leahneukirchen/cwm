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

#define ChildMask	(SubstructureRedirectMask|SubstructureNotifyMask)
#define ButtonMask	(ButtonPressMask|ButtonReleaseMask)
#define MouseMask	(ButtonMask|PointerMotionMask)

struct client_ctx;

TAILQ_HEAD(cycle_entry_q, client_ctx);

struct screen_ctx {
	TAILQ_ENTRY(screen_ctx)	entry;

	u_int		 which;
	Window		 rootwin;
	Window		 menuwin;
	Colormap	 colormap;
	XColor		 bgcolor, fgcolor, fccolor, redcolor, cyancolor,
			 whitecolor, blackcolor;
	char		*display;
	unsigned long	 blackpixl, whitepixl, redpixl, bluepixl, cyanpixl;
	GC		 gc;

	Pixmap		 gray, blue, red;

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

#define CLIENT_HIGHLIGHT_BLUE	1
#define CLIENT_HIGHLIGHT_RED	2


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

	Window			 pwin;

	u_int			 bwidth;
	struct {
		int		 x, y, width, height;
		int		 min_dx, min_dy;
	} geom, savegeom;

	struct {
		int		 x,y;
	} ptr;

	int			 beepbeep;

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

static char *shortcut_to_name[] = {
	"nogroup", "one", "two", "three",
	"four", "five", "six", "seven",
	"eight", "nine"
};

struct group_ctx {
	TAILQ_ENTRY(group_ctx)	 entry;
	struct client_ctx_q	 clients;
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

/* NULL/0 values indicate match any. */
struct xevent {
	TAILQ_ENTRY(xevent)	 entry;
	Window			*xev_win;
	Window			*xev_root;
	int			 xev_type;
	void			 (*xev_cb)(struct xevent *, XEvent *);
	void			*xev_arg;
};

TAILQ_HEAD(xevent_q, xevent);

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

struct keybinding {
	int			 modmask;
	int			 keysym;
	int			 keycode;
	int			 flags;
	void			 (*callback)(struct client_ctx *, void *);
	void			*argument;
	TAILQ_ENTRY(keybinding)	 entry;
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

int			 x_errorhandler(Display *, XErrorEvent *);
void			 x_setup(void);
char			*x_screenname(int);
void			 x_setupscreen(struct screen_ctx *, u_int);
__dead void		 usage(void);

struct client_ctx	*client_find(Window);
void			 client_setup(void);
struct client_ctx	*client_new(Window, struct screen_ctx *, int);
int			 client_delete(struct client_ctx *, int, int);
void			 client_setactive(struct client_ctx *, int);
void			 client_gravitate(struct client_ctx *, int);
void			 client_resize(struct client_ctx *);
void			 client_lower(struct client_ctx *);
void			 client_raise(struct client_ctx *);
void			 client_move(struct client_ctx *);
void			 client_leave(struct client_ctx *);
void			 client_send_delete(struct client_ctx *);
struct client_ctx	*client_current(void);
void			 client_hide(struct client_ctx *);
void			 client_unhide(struct client_ctx *);
void			 client_nocurrent(void);
void			 client_setname(struct client_ctx *);
void			 client_warp(struct client_ctx *);
void			 client_ptrwarp(struct client_ctx *);
void			 client_ptrsave(struct client_ctx *);
void			 client_draw_border(struct client_ctx *);
void			 client_update(struct client_ctx *);
void			 client_placecalc(struct client_ctx *);
void			 client_maximize(struct client_ctx *);
void			 client_vertmaximize(struct client_ctx *);
u_long			 client_bg_pixel(struct client_ctx *);
Pixmap			 client_bg_pixmap(struct client_ctx *);
void			 client_map(struct client_ctx *);
void			 client_mtf(struct client_ctx *);
struct client_ctx	*client_cycle(int);
struct client_ctx	*client_mrunext(struct client_ctx *);
struct client_ctx	*client_mruprev(struct client_ctx *);
void			 client_gethints(struct client_ctx *);
void			 client_freehints(struct client_ctx *);
void			 client_do_shape(struct client_ctx *);

struct menu  		*menu_filter(struct menu_q *, char *, char *, int,
			     void (*)(struct menu_q *, struct menu_q *, char *),
			     void (*)(struct menu *, int));
void			 menu_init(struct screen_ctx *);

void			 xev_handle_maprequest(struct xevent *, XEvent *);
void			 xev_handle_unmapnotify(struct xevent *, XEvent *);
void			 xev_handle_destroynotify(struct xevent *, XEvent *);
void			 xev_handle_configurerequest(struct xevent *, XEvent *);
void			 xev_handle_propertynotify(struct xevent *, XEvent *);
void			 xev_handle_enternotify(struct xevent *, XEvent *);
void			 xev_handle_leavenotify(struct xevent *, XEvent *);
void			 xev_handle_buttonpress(struct xevent *, XEvent *);
void			 xev_handle_buttonrelease(struct xevent *, XEvent *);
void			 xev_handle_keypress(struct xevent *, XEvent *);
void			 xev_handle_keyrelease(struct xevent *, XEvent *);
void			 xev_handle_expose(struct xevent *, XEvent *);
void			 xev_handle_clientmessage(struct xevent *, XEvent *);
void			 xev_handle_shape(struct xevent *, XEvent *);
void			 xev_handle_randr(struct xevent *, XEvent *);
void			 xev_handle_mapping(struct xevent *, XEvent *);

#define XEV_QUICK(a, b, c, d, e) do {		\
	xev_register(xev_new(a, b, c, d, e));	\
} while (0)

/* XXX should be xu_ */
void			  xev_reconfig(struct client_ctx *);

void			 xev_init(void);
struct xevent		*xev_new(Window *, Window *, int,
			     void (*)(struct xevent *, XEvent *), void *);
void			 xev_register(struct xevent *);
void			 xev_loop(void);

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

int			 u_spawn(char *);
void			 u_exec(char *);

void			 grab_sweep(struct client_ctx *);
void			 grab_drag(struct client_ctx *);

void			 xfree(void *);
void			*xmalloc(size_t);
void			*xcalloc(size_t, size_t);
char			*xstrdup(const char *);

#define XMALLOC(p, t) ((p) = (t *)xmalloc(sizeof * (p)))
#define XCALLOC(p, t) ((p) = (t *)xcalloc(1, sizeof * (p)))

struct screen_ctx	*screen_fromroot(Window);
struct screen_ctx	*screen_current(void);
void			 screen_updatestackingorder(void);
void			 screen_init_xinerama(struct screen_ctx *);

void			 conf_setup(struct conf *, const char *);
void			 conf_client(struct client_ctx *);
void			 conf_grab(struct conf *, struct keybinding *);
void			 conf_ungrab(struct conf *, struct keybinding *);
void			 conf_bindname(struct conf *, char *, char *);
void			 conf_unbind(struct conf *, struct keybinding *);
void			 conf_mousebind(struct conf *, char *, char *);
void			 conf_mouseunbind(struct conf *, struct mousebinding *);
void			 conf_grab_mouse(struct client_ctx *);
void			 conf_reload(struct conf *);
void			 conf_font(struct conf *);

void			 kbfunc_client_lower(struct client_ctx *, void *);
void			 kbfunc_client_raise(struct client_ctx *, void *);
void			 kbfunc_client_search(struct client_ctx *, void *);
void			 kbfunc_client_hide(struct client_ctx *, void *);
void			 kbfunc_client_cycle(struct client_ctx *, void *);
void			 kbfunc_client_rcycle(struct client_ctx *, void *);
void			 kbfunc_cmdexec(struct client_ctx *, void *);
void			 kbfunc_client_label(struct client_ctx *, void *);
void			 kbfunc_client_delete(struct client_ctx *, void *);
void			 kbfunc_client_group(struct client_ctx *, void *);
void			 kbfunc_client_cyclegroup(struct client_ctx *, void *);
void			 kbfunc_client_nogroup(struct client_ctx *, void *);
void			 kbfunc_client_grouptoggle(struct client_ctx *, void *);
void			 kbfunc_client_maximize(struct client_ctx *, void *);
void			 kbfunc_client_vmaximize(struct client_ctx *, void *);
void			 kbfunc_reload(struct client_ctx *, void *);
void			 kbfunc_quit_wm(struct client_ctx *, void *);
void			 kbfunc_moveresize(struct client_ctx *, void *);
void			 kbfunc_menu_search(struct client_ctx *, void *);
void			 kbfunc_exec(struct client_ctx *, void *);
void			 kbfunc_ssh(struct client_ctx *, void *);
void			 kbfunc_term(struct client_ctx *, void *);
void			 kbfunc_lock(struct client_ctx *, void *);

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
void			 group_cycle(int);
void			 group_sticky(struct client_ctx *);
void			 group_client_delete(struct client_ctx *);
void			 group_menu(XButtonEvent *);
void			 group_alltoggle(void);
void			 group_sticky_toggle_enter(struct client_ctx *);
void			 group_sticky_toggle_exit(struct client_ctx *);
void			 group_autogroup(struct client_ctx *);

void			 font_init(struct screen_ctx *);
int			 font_width(const char *, int);
void			 font_draw(struct screen_ctx *, const char *, int,
			     Drawable, int, int);
XftFont			*font_make(struct screen_ctx *, const char *);

#define font_ascent()	Conf.DefaultFont->ascent
#define font_descent()	Conf.DefaultFont->descent
#define	font_height()	Conf.FontHeight

#define CCTOSC(cc) (cc->sc)

/* Externs */

extern Display				*X_Dpy;

extern Cursor				 Cursor_move;
extern Cursor				 Cursor_resize;
extern Cursor				 Cursor_select;
extern Cursor				 Cursor_default;
extern Cursor				 Cursor_question;

extern struct screen_ctx_q		 Screenq;
extern struct screen_ctx		*curscreen;
extern u_int				 Nscreens;

extern struct client_ctx_q		 Clientq;

extern int				 Doshape, Shape_ev;
extern int				 Doshape, Shape_ev;
extern int				 HasXinerama, HasRandr, Randr_ev;
extern struct conf			 Conf;

#endif /* _CALMWM_H_ */

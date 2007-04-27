/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2004 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id$
 */

#ifndef _CALMWM_H_
#define _CALMWM_H_

#define CALMWM_MAXNAMELEN 256

#include "hash.h"

#undef MIN
#undef MAX
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

enum conftype {
	CONF_BWIDTH, CONF_IGNORE, CONF_NOTIFIER,
};

#define ChildMask	(SubstructureRedirectMask|SubstructureNotifyMask)
#define ButtonMask	(ButtonPressMask|ButtonReleaseMask)
#define MouseMask	(ButtonMask|PointerMotionMask)

struct client_ctx;

TAILQ_HEAD(cycle_entry_q, client_ctx);

/* #define CYCLE_FOREACH_MRU(cy, ctx) TAILQ_FOREACH((ctx),  */

struct screen_ctx;

struct fontdesc {
	const char           *name;
	XftFont              *fn;
	struct screen_ctx    *sc;
	HASH_ENTRY(fontdesc)  node;
};

int fontdesc_cmp(struct fontdesc *a, struct fontdesc *b);

HASH_HEAD(fonthash, fontdesc, 16);
HASH_PROTOTYPE(fonthash, fontdesc, node, fontdesc_cmp);

struct screen_ctx {
	TAILQ_ENTRY(screen_ctx)	entry;

	u_int		 which;
	Window		 rootwin;
	Window		 menuwin;
	Window		 searchwin;
	Window		 groupwin;
	Window		 infowin;
	Colormap	 colormap;
	GC		 invcg;
	XColor		 bgcolor, fgcolor, fccolor, redcolor, cyancolor,
			 whitecolor, blackcolor;
	char		*display;
	unsigned long	 blackpixl, whitepixl, redpixl, bluepixl, cyanpixl;
	GC		 gc, invgc, hlgc;

	Pixmap		 gray, blue, red;

	int		 altpersist;

        FILE            *notifier;

	struct cycle_entry_q mruq;

	struct client_ctx* cycle_client;

	struct fonthash  fonthash;
	XftDraw         *xftdraw;
	XftColor         xftcolor;
};

TAILQ_HEAD(screen_ctx_q, screen_ctx);

#define CLIENT_PROTO_DELETE     0x01
#define CLIENT_PROTO_TAKEFOCUS  0x02

#define CLIENT_MAXNAMEQLEN 5

#define CLIENT_HIDDEN  0x01
#define CLIENT_IGNORE  0x02
#define CLIENT_INQUEUE 0x04	/* tmp used by search code */
#define CLIENT_MAXIMIZED 0x08

#define CLIENT_HIGHLIGHT_BLUE 1
#define CLIENT_HIGHLIGHT_RED 2 


struct winname {
	TAILQ_ENTRY(winname) entry;
	char *name;
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
	struct group_ctx        *group;
	int                      groupcommit;

	int                      stackingorder;

	char                    *app_class;
	char                    *app_name;
	char                    *app_cliarg;
};

TAILQ_HEAD(client_ctx_q, client_ctx);

struct group_ctx {
	TAILQ_ENTRY(group_ctx) entry;
	struct client_ctx_q  clients;
	char                *name;
	int                  shortcut;
	int                  hidden;
	int                  nhidden;
	int                  highstack;
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
	TAILQ_ENTRY(xevent) entry;
	Window  *xev_win;
	Window  *xev_root;
	int     xev_type;
	void  (*xev_cb)(struct xevent *, XEvent *);
	void   *xev_arg;
};

TAILQ_HEAD(xevent_q, xevent);

/* Keybindings */
enum kbtype {
	KB_DELETE, KB_NEWTERM0, KB_NEWTERM1, KB_HIDE,
	KB_LOWER, KB_RAISE, KB_SEARCH, KB_CYCLE, KB_LABEL,
	KB_GROUPSELECT, KB_VERTMAXIMIZE,

	/* Group numbers need to be in order. */
	KB_GROUP_1, KB_GROUP_2, KB_GROUP_3, KB_GROUP_4, KB_GROUP_5,
	KB_GROUP_6, KB_GROUP_7, KB_GROUP_8, KB_GROUP_9, KB_NOGROUP,
	KB_NEXTGROUP, KB_PREVGROUP,

	KB_MOVE_WEST, KB_MOVE_EAST, KB_MOVE_NORTH, KB_MOVE_SOUTH,

	KB__LAST
};

#define KBFLAG_NEEDCLIENT 0x01
#define KBFLAG_FINDCLIENT 0x02

#define KBTOGROUP(X) ((X) - 1)

struct keybinding {
	int modmask;
	int keysym;
        int keycode;
	int flags;
        void (*callback)(struct client_ctx *, void *);
        void *argument;
	TAILQ_ENTRY(keybinding) entry;
};

struct cmd {
	TAILQ_ENTRY(cmd) entry;
	int flags;
#define CMD_STATIC	0x01		/* static configuration in conf.c */
	char image[MAXPATHLEN];
	char label[256];
	/* (argv) */
};

TAILQ_HEAD(keybinding_q, keybinding);
TAILQ_HEAD(cmd_q, cmd);

/* Global configuration */
struct conf {
	struct keybinding_q keybindingq;
	struct autogroupwin_q autogroupq;
	char	      menu_path[MAXPATHLEN];
	struct cmd_q  cmdq;

	int	      flags;
#define CONF_STICKY_GROUPS	0x0001

	char          termpath[MAXPATHLEN];
	char          lockpath[MAXPATHLEN];
};

/* Menu stuff */

#define MENU_MAXENTRY 50

struct menu {
	TAILQ_ENTRY(menu) entry;
	TAILQ_ENTRY(menu) resultentry;

	char  text[MENU_MAXENTRY + 1];
	char  print[MENU_MAXENTRY + 1];
	void *ctx;
	short lasthit;
};

TAILQ_HEAD(menu_q, menu);

enum ctltype {
	CTL_NONE = -1,
	CTL_ERASEONE = 0, CTL_WIPE, CTL_UP, CTL_DOWN, CTL_RETURN,
	CTL_ABORT, CTL_ALL
};

/* MWM hints */

struct mwm_hints {
	u_long  flags;
	u_long  functions;
	u_long  decorations;
};

#define MWM_NUMHINTS 3

#define PROP_MWM_HINTS_ELEMENTS	3
#define MWM_HINTS_DECORATIONS	(1 << 1)
#define MWM_DECOR_ALL		(1 << 0)
#define MWM_DECOR_BORDER	(1 << 1)

int input_keycodetrans(KeyCode, u_int, enum ctltype *, char *, int);

int   x_errorhandler(Display *, XErrorEvent *);
void  x_setup(void);
char *x_screenname(int);
void  x_loop(void);
int   x_setupscreen(struct screen_ctx *, u_int);

struct client_ctx *client_find(Window);
void               client_setup(void);
struct client_ctx *client_new(Window, struct screen_ctx *, int);
int                client_delete(struct client_ctx *, int, int);
void               client_setactive(struct client_ctx *, int);
void               client_gravitate(struct client_ctx *, int);
void               client_resize(struct client_ctx *);
void               client_lower(struct client_ctx *);
void               client_raise(struct client_ctx *);
void               client_move(struct client_ctx *);
void               client_leave(struct client_ctx *);
void               client_send_delete(struct client_ctx *);
struct client_ctx *client_current(void);
void               client_hide(struct client_ctx *);
void               client_unhide(struct client_ctx *);
void               client_nocurrent(void);
void               client_setname(struct client_ctx *);
void               client_warp(struct client_ctx *);
void               client_ptrwarp(struct client_ctx *);
void               client_ptrsave(struct client_ctx *);
void               client_draw_border(struct client_ctx *);
void               client_update(struct client_ctx *);
void               client_cycle(struct client_ctx *);
void               client_placecalc(struct client_ctx *);
void               client_maximize(struct client_ctx *);
void               client_vertmaximize(struct client_ctx *);
u_long             client_bg_pixel(struct client_ctx *);
Pixmap             client_bg_pixmap(struct client_ctx *);
void               client_map(struct client_ctx *cc);
void               client_mtf(struct client_ctx *cc);
struct client_ctx *client_cyclenext(struct client_ctx *cc, int reverse);
void               client_cycleinfo(struct client_ctx *cc);
void               client_altrelease();
struct client_ctx *client_mrunext(struct client_ctx *cc);
struct client_ctx *client_mruprev(struct client_ctx *cc);
void               client_gethints(struct client_ctx *cc);
void               client_freehints(struct client_ctx *cc);

void xev_handle_maprequest(struct xevent *, XEvent *);
void xev_handle_unmapnotify(struct xevent *, XEvent *);
void xev_handle_destroynotify(struct xevent *, XEvent *);
void xev_handle_configurerequest(struct xevent *, XEvent *);
void xev_handle_propertynotify(struct xevent *, XEvent *);
void xev_handle_enternotify(struct xevent *, XEvent *);
void xev_handle_leavenotify(struct xevent *, XEvent *);
void xev_handle_buttonpress(struct xevent *, XEvent *);
void xev_handle_buttonrelease(struct xevent *, XEvent *);
void xev_handle_keypress(struct xevent *, XEvent *);
void xev_handle_keyrelease(struct xevent *, XEvent *);
void xev_handle_expose(struct xevent *, XEvent *);
void xev_handle_clientmessage(struct xevent *, XEvent *);

#define XEV_QUICK(a, b, c, d, e) do {		\
	xev_register(xev_new(a, b, c, d, e));	\
} while (0)

void xev_reconfig(struct client_ctx *);	/* XXX should be xu_ */

void           xev_init(void);
struct xevent *xev_new(Window *, Window *, int, void (*)(struct xevent *, XEvent *), void *);
void           xev_register(struct xevent *);
void           xev_loop(void);

int   xu_ptr_grab(Window, int, Cursor);
int   xu_btn_grab(Window, int, u_int);
int   xu_ptr_regrab(int, Cursor);
void  xu_btn_ungrab(Window, int, u_int);
void  xu_ptr_ungrab(void);
void  xu_ptr_setpos(Window, int, int);
void  xu_ptr_getpos(Window, int *, int *);
void  xu_key_grab(Window, int, int);
void  xu_sendmsg(struct client_ctx *, Atom, long);
int   xu_getprop(struct client_ctx *, Atom, Atom, long, u_char **);
char *xu_getstrprop(struct client_ctx *, Atom atm);
void  xu_setstate(struct client_ctx *, int);
int   xu_getstate(struct client_ctx *, int *);
void  xu_key_grab_keycode(Window, int, int);

int dirent_exists(char *);
int dirent_isdir(char *);
int dirent_islink(char *);
int u_spawn(char *);

int   grab_sweep(struct client_ctx *);
int   grab_drag(struct client_ctx *);
void  grab_menuinit(struct screen_ctx *);
void *grab_menu(XButtonEvent *, struct menu_q *);
void  grab_label(struct client_ctx *);

void  xfree(void *);
void *xmalloc(size_t);
void *xcalloc(size_t);
char *xstrdup(const char *);

#define XMALLOC(p, t) ((p) = (t *)xmalloc(sizeof * (p)))
#define XCALLOC(p, t) ((p) = (t *)xcalloc(sizeof * (p)))

void               screen_init(void);
struct screen_ctx *screen_fromroot(Window);
struct screen_ctx *screen_current(void);
void               screen_updatestackingorder(void);
void               screen_infomsg(char *);

void  conf_setup(struct conf *);
int   conf_get_int(struct client_ctx *, enum conftype);
void  conf_client(struct client_ctx *);
void  conf_bindkey(struct conf *, void (*)(struct client_ctx *, void *),
          int, int, int, void *);
void  conf_parsekeys(struct conf *, char *);
void  conf_parsesettings(struct conf *, char *);
void  conf_parseignores(struct conf *, char *);
void  conf_parseautogroups(struct conf *, char *);
void  conf_cmd_clear(struct conf *);
int   conf_cmd_changed(char *);
void  conf_cmd_populate(struct conf *, char *);
void  conf_cmd_refresh(struct conf *c);
char *conf_get_str(struct client_ctx *, enum conftype);

void kbfunc_client_lower(struct client_ctx *, void *);
void kbfunc_client_raise(struct client_ctx *, void *);
void kbfunc_client_search(struct client_ctx *, void *);
void kbfunc_client_hide(struct client_ctx *, void *);
void kbfunc_client_cycle(struct client_ctx *, void *);
void kbfunc_client_rcycle(struct client_ctx *cc, void *arg);
void kbfunc_cmdexec(struct client_ctx *, void *);
void kbfunc_client_label(struct client_ctx *, void *);
void kbfunc_client_delete(struct client_ctx *, void *);
void kbfunc_client_groupselect(struct client_ctx *, void *);
void kbfunc_client_group(struct client_ctx *, void *);
void kbfunc_client_nextgroup(struct client_ctx *, void *);
void kbfunc_client_prevgroup(struct client_ctx *, void *);
void kbfunc_client_nogroup(struct client_ctx *, void *);
void kbfunc_client_maximize(struct client_ctx *, void *);
void kbfunc_client_vmaximize(struct client_ctx *, void *);
void kbfunc_menu_search(struct client_ctx *, void *);
void kbfunc_term(struct client_ctx *cc, void *arg);
void kbfunc_lock(struct client_ctx *cc, void *arg);

void draw_outline(struct client_ctx *);

void  search_init(struct screen_ctx *);
struct menu *search_start(struct menu_q *menuq,
    void (*match)(struct menu_q *, struct menu_q *, char *),
    void (*rank)(struct menu_q *, char *),
    void (*print)(struct menu *mi, int),
    char *);
void  search_match_client(struct menu_q *, struct menu_q *, char *);
void  search_print_client(struct menu *mi, int list);
void  search_match_text(struct menu_q *, struct menu_q *, char *);
void  search_rank_text(struct menu_q *, char *);

void group_init(void);
int  group_new(void);
int  group_select(int);
void group_enter(void);
void group_exit(int);
void group_click(struct client_ctx *);
void group_display_init(struct screen_ctx *);
void group_display_draw(struct screen_ctx *);
void group_display_keypress(KeyCode);
void group_hidetoggle(int);
void group_slide(int);
void group_sticky(struct client_ctx *);
void group_client_delete(struct client_ctx *);
void group_menu(XButtonEvent *);
void group_namemode(void);
void group_alltoggle(void);
void group_deletecurrent(void);
void group_done(void);
void group_sticky_toggle_enter(struct client_ctx *);
void group_sticky_toggle_exit(struct client_ctx *);
void group_autogroup(struct client_ctx *);

void notification_init(struct screen_ctx *);

struct client_ctx *geographic_west(struct client_ctx *);

Cursor cursor_bigarrow();

void font_init(struct screen_ctx *sc);
struct fontdesc *font_get(struct screen_ctx *sc, const char *name);
int font_width(struct fontdesc *fdp, const char *text, int len);
void font_draw(struct fontdesc *fdp, const char *text, int len,
    Drawable d, int x, int y);
int font_ascent(struct fontdesc *fdp);
int font_descent(struct fontdesc *fdp);
struct fontdesc *font_getx(struct screen_ctx *sc, const char *name);

#define CCTOSC(cc) (cc->sc)

/* Externs */

extern Display				*G_dpy;
extern XFontStruct			*G_font;

extern Cursor				 G_cursor_move;
extern Cursor				 G_cursor_resize;
extern Cursor				 G_cursor_select;
extern Cursor				 G_cursor_default;
extern Cursor				 G_cursor_question;

extern struct screen_ctx_q		 G_screenq;
extern struct screen_ctx		*G_curscreen;
extern u_int				 G_nscreens;

extern struct client_ctx_q		 G_clientq;

extern int				 G_doshape, G_shape_ev;
extern struct conf			 G_conf;

extern int G_groupmode;
extern struct fontdesc                  *DefaultFont;


#endif /* _CALMWM_H_ */

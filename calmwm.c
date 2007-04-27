/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2004 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id$
 */

#include "headers.h"
#include "calmwm.h"

Display				*G_dpy;
XFontStruct			*G_font;

Cursor				 G_cursor_move;
Cursor				 G_cursor_resize;
Cursor				 G_cursor_select;
Cursor				 G_cursor_default;
Cursor				 G_cursor_question;

struct screen_ctx_q		 G_screenq;
struct screen_ctx		*G_curscreen;
u_int				 G_nscreens;

struct client_ctx_q		 G_clientq;

int				 G_doshape, G_shape_ev;
int				 G_starting;
struct conf			 G_conf;
struct fontdesc                 *DefaultFont;
char                            *DefaultFontName;

/* From TWM */
#define gray_width 2
#define gray_height 2
static char gray_bits[] = {0x02, 0x01};

/* List borrowed from 9wm/rio */
char *tryfonts[] = {
	"9x15bold",
	"blit",
	"*-lucidatypewriter-bold-*-14-*-75-*",
	"*-lucidatypewriter-medium-*-12-*-75-*",
	"fixed",
	"*",
	NULL,
};

static void _sigchld_cb(int);

int
main(int argc, char **argv)
{
	int ch;
	int conf_flags = 0;

	DefaultFontName = "sans-serif:pixelsize=14:bold";

	while ((ch = getopt(argc, argv, "sf:")) != -1) {
		switch (ch) {
		case 's':
			conf_flags |= CONF_STICKY_GROUPS;
			break;
		case 'f':
			DefaultFontName = xstrdup(optarg);
			break;
		default:
			errx(1, "Unknown option '%c'", ch);
		}
	}

	/* Ignore a few signals. */
        if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
                err(1, "signal");

        if (signal(SIGCHLD, _sigchld_cb) == SIG_ERR)
                err(1, "signal");

	group_init();

	G_starting = 1;
	conf_setup(&G_conf);
	G_conf.flags |= conf_flags;
	client_setup();
	x_setup();
	G_starting = 0;

	xev_init();
	XEV_QUICK(NULL, NULL, MapRequest, xev_handle_maprequest, NULL);
	XEV_QUICK(NULL, NULL, UnmapNotify, xev_handle_unmapnotify, NULL);
	XEV_QUICK(NULL, NULL, ConfigureRequest,
	    xev_handle_configurerequest, NULL);
	XEV_QUICK(NULL, NULL, PropertyNotify, xev_handle_propertynotify, NULL);
	XEV_QUICK(NULL, NULL, EnterNotify, xev_handle_enternotify, NULL);
	XEV_QUICK(NULL, NULL, LeaveNotify, xev_handle_leavenotify, NULL);
	XEV_QUICK(NULL, NULL, ButtonPress, xev_handle_buttonpress, NULL);
	XEV_QUICK(NULL, NULL, ButtonRelease, xev_handle_buttonrelease, NULL);
	XEV_QUICK(NULL, NULL, KeyPress, xev_handle_keypress, NULL);
	XEV_QUICK(NULL, NULL, KeyRelease, xev_handle_keyrelease, NULL);
	XEV_QUICK(NULL, NULL, Expose, xev_handle_expose, NULL);
	XEV_QUICK(NULL, NULL, DestroyNotify, xev_handle_destroynotify, NULL);
	XEV_QUICK(NULL, NULL, ClientMessage, xev_handle_clientmessage, NULL);

	xev_loop();

	return (0);
}

void
x_setup(void)
{
	int i;
	struct screen_ctx *sc;
	char *fontname;

	TAILQ_INIT(&G_screenq);

	if ((G_dpy = XOpenDisplay("")) == NULL)
		errx(1, "%s:%d XOpenDisplay()", __FILE__, __LINE__);

	XSetErrorHandler(x_errorhandler);

	G_doshape = XShapeQueryExtension(G_dpy, &G_shape_ev, &i);

	i = 0;
	while ((fontname = tryfonts[i++]) != NULL) {
		if ((G_font = XLoadQueryFont(G_dpy, fontname)) != NULL)
			break;
	}

	if (fontname == NULL)
		errx(1, "Couldn't load any fonts.");

	G_nscreens = ScreenCount(G_dpy);
	for (i = 0; i < (int)G_nscreens; i++) {
		XMALLOC(sc, struct screen_ctx);
		x_setupscreen(sc, i);
		TAILQ_INSERT_TAIL(&G_screenq, sc, entry);
	}

	G_cursor_move = XCreateFontCursor(G_dpy, XC_fleur);
	G_cursor_resize = XCreateFontCursor(G_dpy, XC_bottom_right_corner);
	/* (used to be) XCreateFontCursor(G_dpy, XC_hand1); */
	G_cursor_select = XCreateFontCursor(G_dpy, XC_hand1);
/* 	G_cursor_select = cursor_bigarrow(G_curscreen); */
	G_cursor_default = XCreateFontCursor(G_dpy, XC_X_cursor);
/* 	G_cursor_default = cursor_bigarrow(G_curscreen); */
	G_cursor_question = XCreateFontCursor(G_dpy, XC_question_arrow);
}

int
x_setupscreen(struct screen_ctx *sc, u_int which)
{
	XColor tmp;
	XGCValues gv, gv1/* , gv2 */;
	Window *wins, w0, w1;
	u_int nwins, i = 0;
	XWindowAttributes winattr;
	XSetWindowAttributes rootattr;
	struct keybinding *kb;

	sc->display = x_screenname(which);
	sc->which = which;
	sc->rootwin = RootWindow(G_dpy, which);
	XAllocNamedColor(G_dpy, DefaultColormap(G_dpy, which),
	    "black", &sc->fgcolor, &tmp);
	XAllocNamedColor(G_dpy, DefaultColormap(G_dpy, which),
	    "#00cc00", &sc->bgcolor, &tmp);
	XAllocNamedColor(G_dpy,DefaultColormap(G_dpy, which),
	    "blue", &sc->fccolor, &tmp);
	XAllocNamedColor(G_dpy, DefaultColormap(G_dpy, which),
	    "red", &sc->redcolor, &tmp);
	XAllocNamedColor(G_dpy, DefaultColormap(G_dpy, which),
	    "#00ccc8", &sc->cyancolor, &tmp);
	XAllocNamedColor(G_dpy, DefaultColormap(G_dpy, which),
	    "white", &sc->whitecolor, &tmp);
	XAllocNamedColor(G_dpy, DefaultColormap(G_dpy, which),
	    "black", &sc->blackcolor, &tmp);

	TAILQ_FOREACH(kb, &G_conf.keybindingq, entry)
		xu_key_grab(sc->rootwin, kb->modmask, kb->keysym);

	/* Special -- for alt state. */
/* 	xu_key_grab(sc->rootwin, 0, XK_Alt_L); */
/* 	xu_key_grab(sc->rootwin, 0, XK_Alt_R); */

	sc->blackpixl = BlackPixel(G_dpy, sc->which);
	sc->whitepixl = WhitePixel(G_dpy, sc->which);
	sc->bluepixl = sc->fccolor.pixel;
	sc->redpixl = sc->redcolor.pixel;
	sc->cyanpixl = sc->cyancolor.pixel;

        sc->gray = XCreatePixmapFromBitmapData(G_dpy, sc->rootwin,
            gray_bits, gray_width, gray_height,
            sc->blackpixl, sc->whitepixl, DefaultDepth(G_dpy, sc->which));

        sc->blue = XCreatePixmapFromBitmapData(G_dpy, sc->rootwin,
            gray_bits, gray_width, gray_height,
            sc->bluepixl, sc->whitepixl, DefaultDepth(G_dpy, sc->which));

        sc->red = XCreatePixmapFromBitmapData(G_dpy, sc->rootwin,
            gray_bits, gray_width, gray_height,
	    sc->redpixl, sc->whitepixl, DefaultDepth(G_dpy, sc->which));

	gv.foreground = sc->blackpixl^sc->whitepixl;
	gv.background = sc->whitepixl;
	gv.function = GXxor;
	gv.line_width = 1;
	gv.subwindow_mode = IncludeInferiors;
	gv.font = G_font->fid;

	sc->gc = XCreateGC(G_dpy, sc->rootwin,
	    GCForeground|GCBackground|GCFunction|
	    GCLineWidth|GCSubwindowMode|GCFont, &gv);

#ifdef notyet
	gv2.foreground = sc->blackpixl^sc->cyanpixl;
	gv2.background = sc->cyanpixl;
	gv2.function = GXxor;
	gv2.line_width = 1;
	gv2.subwindow_mode = IncludeInferiors;
	gv2.font = G_font->fid;
#endif

	sc->hlgc = XCreateGC(G_dpy, sc->rootwin,
	    GCForeground|GCBackground|GCFunction|
	    GCLineWidth|GCSubwindowMode|GCFont, &gv);

	gv1.function = GXinvert;
	gv1.subwindow_mode = IncludeInferiors;
	gv1.line_width = 1;
	gv1.font = G_font->fid;

	sc->invgc = XCreateGC(G_dpy, sc->rootwin,
	    GCFunction|GCSubwindowMode|GCLineWidth|GCFont, &gv1);

	font_init(sc);
	DefaultFont = font_getx(sc, DefaultFontName);

	/*
	 * XXX - this should *really* be in screen_init().  ordering
	 * problem.
	 */
	TAILQ_INIT(&sc->mruq);

	/* Initialize menu window. */
	grab_menuinit(sc);
	search_init(sc);

	/* Deal with existing clients. */
	XQueryTree(G_dpy, sc->rootwin, &w0, &w1, &wins, &nwins);	

	for (i = 0; i < nwins; i++) {
		XGetWindowAttributes(G_dpy, wins[i], &winattr);
		if (winattr.override_redirect ||
		    winattr.map_state != IsViewable) {
			char *name;
			XFetchName(G_dpy, wins[i], &name);
			continue;
		}
		client_new(wins[i], sc, winattr.map_state != IsUnmapped);
	}
	XFree(wins);

	G_curscreen = sc;	/* XXX */
	screen_init();
	screen_updatestackingorder();

	rootattr.event_mask = ChildMask|PropertyChangeMask|EnterWindowMask|
	    LeaveWindowMask|ColormapChangeMask|ButtonMask;

	/* Set the root cursor to a nice obnoxious arrow :-) */
/* 	rootattr.cursor = cursor_bigarrow(sc); */

	XChangeWindowAttributes(G_dpy, sc->rootwin,
	    /* CWCursor| */CWEventMask, &rootattr);

	XSync(G_dpy, False);

	return (0);
}

char *
x_screenname(int which)
{
	char *cp, *dstr, *sn;
	size_t snlen;

	if (which > 9)
		errx(1, "Can't handle more than 9 screens.  If you need it, "
		    "tell <marius@monkey.org>.  It's a trivial fix.");

	dstr = xstrdup(DisplayString(G_dpy));

	if ((cp = rindex(dstr, ':')) == NULL)
		return (NULL);

	if ((cp = index(cp, '.')) != NULL)
		*cp = '\0';

	snlen = strlen(dstr) + 3; /* string, dot, number, null */
	sn = (char *)xmalloc(snlen);
	snprintf(sn, snlen, "%s.%d", dstr, which);
	free(dstr);

	return (sn);
}

int
x_errorhandler(Display *dpy, XErrorEvent *e)
{
#ifdef DEBUG
	{
		char msg[80], number[80], req[80];

		XGetErrorText(G_dpy, e->error_code, msg, sizeof(msg));
		snprintf(number, sizeof(number), "%d", e->request_code);
		XGetErrorDatabaseText(G_dpy, "XRequest", number,
		    "<unknown>", req, sizeof(req));

		warnx("%s(0x%x): %s", req, (u_int)e->resourceid, msg);
	}
#endif

	if (G_starting && 
	    e->error_code == BadAccess &&                                       
	    e->request_code == X_GrabKey)                        
		errx(1, "root window unavailable - perhaps another "
		    "wm is running?");                                      

	return (0);
}

static void
_sigchld_cb(int which)
{
	pid_t pid;
	int status;

/* 	Collect dead children. */
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0 ||
            (pid < 0 && errno == EINTR))
		;
}

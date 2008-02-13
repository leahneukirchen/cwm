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

#include "headers.h"
#include "calmwm.h"

Display				*X_Dpy;

Cursor				 Cursor_move;
Cursor				 Cursor_resize;
Cursor				 Cursor_select;
Cursor				 Cursor_default;
Cursor				 Cursor_question;

struct screen_ctx_q		 Screenq;
struct screen_ctx		*Curscreen;
u_int				 Nscreens;

struct client_ctx_q		 Clientq;

int				 Doshape, Shape_ev;
int				 Starting;
struct conf			 Conf;
struct fontdesc                 *DefaultFont;
char                            *DefaultFontName;

/* From TWM */
#define gray_width 2
#define gray_height 2
static char gray_bits[] = {0x02, 0x01};


static void _sigchld_cb(int);

int
main(int argc, char **argv)
{
	int ch;
	int conf_flags = 0;

	char *display_name = NULL;

	DefaultFontName = "sans-serif:pixelsize=14:bold";

	while ((ch = getopt(argc, argv, "d:sf:")) != -1) {
		switch (ch) {
		case 'd':
			display_name = optarg;
			break;
		case 's':
			conf_flags |= CONF_STICKY_GROUPS;
			break;
		case 'f':
			DefaultFontName = xstrdup(optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Ignore a few signals. */
        if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
                err(1, "signal");

        if (signal(SIGCHLD, _sigchld_cb) == SIG_ERR)
                err(1, "signal");

	group_init();

	Starting = 1;
	conf_setup(&Conf);
	Conf.flags |= conf_flags;
	client_setup();
	x_setup(display_name);
	Starting = 0;

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
x_setup(char *display_name)
{
	int i;
	struct screen_ctx *sc;
	char *fontname;

	TAILQ_INIT(&Screenq);

	if ((X_Dpy = XOpenDisplay(display_name)) == NULL)
		errx(1, "unable to open display \"%s\"",
		    XDisplayName(display_name));

	XSetErrorHandler(x_errorhandler);

	Doshape = XShapeQueryExtension(X_Dpy, &Shape_ev, &i);

	Nscreens = ScreenCount(X_Dpy);
	for (i = 0; i < (int)Nscreens; i++) {
		XMALLOC(sc, struct screen_ctx);
		x_setupscreen(sc, i);
		TAILQ_INSERT_TAIL(&Screenq, sc, entry);
	}

	Cursor_move = XCreateFontCursor(X_Dpy, XC_fleur);
	Cursor_resize = XCreateFontCursor(X_Dpy, XC_bottom_right_corner);
	Cursor_select = XCreateFontCursor(X_Dpy, XC_hand1);
	Cursor_default = XCreateFontCursor(X_Dpy, XC_X_cursor);
	Cursor_question = XCreateFontCursor(X_Dpy, XC_question_arrow);
}

void
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
	sc->rootwin = RootWindow(X_Dpy, which);
	XAllocNamedColor(X_Dpy, DefaultColormap(X_Dpy, which),
	    "black", &sc->fgcolor, &tmp);
	XAllocNamedColor(X_Dpy, DefaultColormap(X_Dpy, which),
	    "#00cc00", &sc->bgcolor, &tmp);
	XAllocNamedColor(X_Dpy,DefaultColormap(X_Dpy, which),
	    "blue", &sc->fccolor, &tmp);
	XAllocNamedColor(X_Dpy, DefaultColormap(X_Dpy, which),
	    "red", &sc->redcolor, &tmp);
	XAllocNamedColor(X_Dpy, DefaultColormap(X_Dpy, which),
	    "#00ccc8", &sc->cyancolor, &tmp);
	XAllocNamedColor(X_Dpy, DefaultColormap(X_Dpy, which),
	    "white", &sc->whitecolor, &tmp);
	XAllocNamedColor(X_Dpy, DefaultColormap(X_Dpy, which),
	    "black", &sc->blackcolor, &tmp);

	TAILQ_FOREACH(kb, &Conf.keybindingq, entry)
		xu_key_grab(sc->rootwin, kb->modmask, kb->keysym);

	sc->blackpixl = BlackPixel(X_Dpy, sc->which);
	sc->whitepixl = WhitePixel(X_Dpy, sc->which);
	sc->bluepixl = sc->fccolor.pixel;
	sc->redpixl = sc->redcolor.pixel;
	sc->cyanpixl = sc->cyancolor.pixel;

        sc->gray = XCreatePixmapFromBitmapData(X_Dpy, sc->rootwin,
            gray_bits, gray_width, gray_height,
            sc->blackpixl, sc->whitepixl, DefaultDepth(X_Dpy, sc->which));

        sc->blue = XCreatePixmapFromBitmapData(X_Dpy, sc->rootwin,
            gray_bits, gray_width, gray_height,
            sc->bluepixl, sc->whitepixl, DefaultDepth(X_Dpy, sc->which));

        sc->red = XCreatePixmapFromBitmapData(X_Dpy, sc->rootwin,
            gray_bits, gray_width, gray_height,
	    sc->redpixl, sc->whitepixl, DefaultDepth(X_Dpy, sc->which));

	gv.foreground = sc->blackpixl^sc->whitepixl;
	gv.background = sc->whitepixl;
	gv.function = GXxor;
	gv.line_width = 1;
	gv.subwindow_mode = IncludeInferiors;

	sc->gc = XCreateGC(X_Dpy, sc->rootwin,
	    GCForeground|GCBackground|GCFunction|
	    GCLineWidth|GCSubwindowMode, &gv);

	sc->hlgc = XCreateGC(X_Dpy, sc->rootwin,
	    GCForeground|GCBackground|GCFunction|
	    GCLineWidth|GCSubwindowMode, &gv);

	gv1.function = GXinvert;
	gv1.subwindow_mode = IncludeInferiors;
	gv1.line_width = 1;

	sc->invgc = XCreateGC(X_Dpy, sc->rootwin,
	    GCFunction|GCSubwindowMode|GCLineWidth, &gv1);

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
	XQueryTree(X_Dpy, sc->rootwin, &w0, &w1, &wins, &nwins);	

	for (i = 0; i < nwins; i++) {
		XGetWindowAttributes(X_Dpy, wins[i], &winattr);
		if (winattr.override_redirect ||
		    winattr.map_state != IsViewable) {
			char *name;
			XFetchName(X_Dpy, wins[i], &name);
			continue;
		}
		client_new(wins[i], sc, winattr.map_state != IsUnmapped);
	}
	XFree(wins);

	Curscreen = sc;	/* XXX */
	screen_init();
	screen_updatestackingorder();

	rootattr.event_mask = ChildMask|PropertyChangeMask|EnterWindowMask|
	    LeaveWindowMask|ColormapChangeMask|ButtonMask;

	XChangeWindowAttributes(X_Dpy, sc->rootwin,
	    /* CWCursor| */CWEventMask, &rootattr);

	XSync(X_Dpy, False);

	return;
}

char *
x_screenname(int which)
{
	char *cp, *dstr, *sn;
	size_t snlen;

	if (which > 9)
		errx(1, "Can't handle more than 9 screens.  If you need it, "
		    "tell <marius@monkey.org>.  It's a trivial fix.");

	dstr = xstrdup(DisplayString(X_Dpy));

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

		XGetErrorText(X_Dpy, e->error_code, msg, sizeof(msg));
		snprintf(number, sizeof(number), "%d", e->request_code);
		XGetErrorDatabaseText(X_Dpy, "XRequest", number,
		    "<unknown>", req, sizeof(req));

		warnx("%s(0x%x): %s", req, (u_int)e->resourceid, msg);
	}
#endif

	if (Starting && 
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

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-s] [-d display] [-f fontname] \n", __progname);
	exit(1);
}

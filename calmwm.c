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

struct client_ctx_q		 Clientq;

int				 HasXinerama, HasRandr, Randr_ev;
int				 Starting;
struct conf			 Conf;

static void	sigchld_cb(int);
static void	dpy_init(const char *);
static void	x_setup(void);
static void	x_setupscreen(struct screen_ctx *, u_int);
static void	x_teardown(void);

int
main(int argc, char **argv)
{
	const char	*conf_file = NULL;
	char		*display_name = NULL;
	int		 ch;

	while ((ch = getopt(argc, argv, "c:d:")) != -1) {
		switch (ch) {
		case 'c':
			conf_file = optarg;
			break;
		case 'd':
			display_name = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (signal(SIGCHLD, sigchld_cb) == SIG_ERR)
		err(1, "signal");

	group_init();

	Starting = 1;
	dpy_init(display_name);
	bzero(&Conf, sizeof(Conf));
	conf_setup(&Conf, conf_file);
	client_setup();
	xu_getatoms();
	x_setup();
	Starting = 0;

	xev_loop();

	x_teardown();

	return (0);
}

void
dpy_init(const char *dpyname)
{
	int	i;

	if ((X_Dpy = XOpenDisplay(dpyname)) == NULL)
		errx(1, "unable to open display \"%s\"",
		    XDisplayName(dpyname));

	XSetErrorHandler(x_errorhandler);

	HasRandr = XRRQueryExtension(X_Dpy, &Randr_ev, &i);

	TAILQ_INIT(&Screenq);
}

void
x_setup(void)
{
	struct screen_ctx	*sc;
	struct keybinding	*kb;
	int			 i;

	for (i = 0; i < ScreenCount(X_Dpy); i++) {
		XCALLOC(sc, struct screen_ctx);
		x_setupscreen(sc, i);
		TAILQ_INSERT_TAIL(&Screenq, sc, entry);
	}

	/*
	 * XXX key grabs weren't done before, since Screenq was empty,
	 * do them here for now (this needs changing).
	 */
	TAILQ_FOREACH(kb, &Conf.keybindingq, entry)
		conf_grab(&Conf, kb);

	Cursor_move = XCreateFontCursor(X_Dpy, XC_fleur);
	Cursor_resize = XCreateFontCursor(X_Dpy, XC_bottom_right_corner);
	Cursor_select = XCreateFontCursor(X_Dpy, XC_hand1);
	Cursor_default = XCreateFontCursor(X_Dpy, XC_X_cursor);
	Cursor_question = XCreateFontCursor(X_Dpy, XC_question_arrow);
}

void
x_teardown(void)
{
	struct screen_ctx	*sc;

	TAILQ_FOREACH(sc, &Screenq, entry)
		XFreeGC(X_Dpy, sc->gc);

	XCloseDisplay(X_Dpy);
}

void
x_setupscreen(struct screen_ctx *sc, u_int which)
{
	Window			*wins, w0, w1;
	XWindowAttributes	 winattr;
	XSetWindowAttributes	 rootattr;
	int			 fake;
	u_int			 nwins, i;

	Curscreen = sc;

	sc->which = which;
	sc->rootwin = RootWindow(X_Dpy, sc->which);
	sc->xmax = DisplayWidth(X_Dpy, sc->which);
	sc->ymax = DisplayHeight(X_Dpy, sc->which);

	conf_color(&Conf);

	font_init(sc);
	conf_font(&Conf);

	TAILQ_INIT(&sc->mruq);

	/* Initialize menu window. */
	menu_init(sc);

	/* Deal with existing clients. */
	XQueryTree(X_Dpy, sc->rootwin, &w0, &w1, &wins, &nwins);

	for (i = 0; i < nwins; i++) {
		XGetWindowAttributes(X_Dpy, wins[i], &winattr);
		if (winattr.override_redirect ||
		    winattr.map_state != IsViewable)
			continue;
		client_new(wins[i], sc, winattr.map_state != IsUnmapped);
	}
	XFree(wins);

	screen_updatestackingorder();

	rootattr.event_mask = ChildMask|PropertyChangeMask|EnterWindowMask|
	    LeaveWindowMask|ColormapChangeMask|ButtonMask;

	XChangeWindowAttributes(X_Dpy, sc->rootwin,
	    CWEventMask, &rootattr);

	if (XineramaQueryExtension(X_Dpy, &fake, &fake) == 1 &&
	    ((HasXinerama = XineramaIsActive(X_Dpy)) == 1))
		HasXinerama = 1;
	if (HasRandr)
		XRRSelectInput(X_Dpy, sc->rootwin, RRScreenChangeNotifyMask);
	/*
	 * initial setup of xinerama screens, if we're using RandR then we'll
	 * redo this whenever the screen changes since a CTRC may have been
	 * added or removed
	 */
	screen_init_xinerama(sc);

	XSync(X_Dpy, False);

	return;
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
sigchld_cb(int which)
{
	pid_t	 pid;
	int	 save_errno = errno;
	int	 status;

	/* Collect dead children. */
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0 ||
	    (pid < 0 && errno == EINTR))
		;

	errno = save_errno;
}

__dead void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-c file] [-d display]\n", __progname);
	exit(1);
}

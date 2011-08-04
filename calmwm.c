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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "calmwm.h"

Display				*X_Dpy;

Cursor				 Cursor_default;
Cursor				 Cursor_move;
Cursor				 Cursor_normal;
Cursor				 Cursor_question;
Cursor				 Cursor_resize;

struct screen_ctx_q		 Screenq = TAILQ_HEAD_INITIALIZER(Screenq);
struct client_ctx_q		 Clientq = TAILQ_HEAD_INITIALIZER(Clientq);

int				 HasXinerama, HasRandr, Randr_ev;
struct conf			 Conf;

static void	sigchld_cb(int);
static void	dpy_init(const char *);
static int	x_errorhandler(Display *, XErrorEvent *);
static int	x_wmerrorhandler(Display *, XErrorEvent *);
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

	dpy_init(display_name);

	bzero(&Conf, sizeof(Conf));
	conf_setup(&Conf, conf_file);
	xu_getatoms();
	x_setup();

	xev_loop();

	x_teardown();

	return (0);
}

static void
dpy_init(const char *dpyname)
{
	int	i;

	XSetErrorHandler(x_errorhandler);

	if ((X_Dpy = XOpenDisplay(dpyname)) == NULL)
		errx(1, "unable to open display \"%s\"",
		    XDisplayName(dpyname));

	XSetErrorHandler(x_wmerrorhandler);
	XSelectInput(X_Dpy, DefaultRootWindow(X_Dpy), SubstructureRedirectMask);
	XSync(X_Dpy, False);
	XSetErrorHandler(x_errorhandler);

	HasRandr = XRRQueryExtension(X_Dpy, &Randr_ev, &i);
}

static void
x_setup(void)
{
	struct screen_ctx	*sc;
	struct keybinding	*kb;
	int			 i;

	Cursor_default = XCreateFontCursor(X_Dpy, XC_X_cursor);
	Cursor_move = XCreateFontCursor(X_Dpy, XC_fleur);
	Cursor_normal = XCreateFontCursor(X_Dpy, XC_left_ptr);
	Cursor_question = XCreateFontCursor(X_Dpy, XC_question_arrow);
	Cursor_resize = XCreateFontCursor(X_Dpy, XC_bottom_right_corner);

	for (i = 0; i < ScreenCount(X_Dpy); i++) {
		sc = xcalloc(1, sizeof(*sc));
		x_setupscreen(sc, i);
		TAILQ_INSERT_TAIL(&Screenq, sc, entry);
	}

	/*
	 * XXX key grabs weren't done before, since Screenq was empty,
	 * do them here for now (this needs changing).
	 */
	TAILQ_FOREACH(kb, &Conf.keybindingq, entry)
		conf_grab(&Conf, kb);
}

static void
x_teardown(void)
{
	struct screen_ctx	*sc;

	TAILQ_FOREACH(sc, &Screenq, entry)
		XFreeGC(X_Dpy, sc->gc);

	XCloseDisplay(X_Dpy);
}

static void
x_setupscreen(struct screen_ctx *sc, u_int which)
{
	Window			*wins, w0, w1;
	XWindowAttributes	 winattr;
	XSetWindowAttributes	 rootattr;
	int			 fake;
	u_int			 nwins, i;
	sc->which = which;
	sc->rootwin = RootWindow(X_Dpy, sc->which);

	conf_gap(&Conf, sc);
	screen_update_geometry(sc, DisplayWidth(X_Dpy, sc->which),
	    DisplayHeight(X_Dpy, sc->which));

	conf_color(&Conf, sc);

	group_init(sc);
	font_init(sc);
	conf_font(&Conf, sc);

	TAILQ_INIT(&sc->mruq);

	/* Initialize menu window. */
	menu_init(sc);

	xu_setwmname(sc);

	rootattr.cursor = Cursor_normal;
	rootattr.event_mask = CHILDMASK|PropertyChangeMask|EnterWindowMask|
	    LeaveWindowMask|ColormapChangeMask|BUTTONMASK;

	XChangeWindowAttributes(X_Dpy, sc->rootwin,
	    CWEventMask|CWCursor, &rootattr);

	/* Deal with existing clients. */
	XQueryTree(X_Dpy, sc->rootwin, &w0, &w1, &wins, &nwins);

	for (i = 0; i < nwins; i++) {
		XGetWindowAttributes(X_Dpy, wins[i], &winattr);
		if (winattr.override_redirect ||
		    winattr.map_state != IsViewable)
			continue;
		(void)client_new(wins[i], sc, winattr.map_state != IsUnmapped);
	}
	XFree(wins);

	screen_updatestackingorder(sc);

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
}

static int
x_wmerrorhandler(Display *dpy, XErrorEvent *e)
{
	errx(1, "root window unavailable - perhaps another wm is running?");

	return (0);
}
static int
x_errorhandler(Display *dpy, XErrorEvent *e)
{
#if DEBUG
	char msg[80], number[80], req[80];

	XGetErrorText(X_Dpy, e->error_code, msg, sizeof(msg));
	(void)snprintf(number, sizeof(number), "%d", e->request_code);
	XGetErrorDatabaseText(X_Dpy, "XRequest", number,
	    "<unknown>", req, sizeof(req));

	warnx("%s(0x%x): %s", req, (u_int)e->resourceid, msg);
#endif
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

	(void)fprintf(stderr, "usage: %s [-c file] [-d display]\n",
	    __progname);
	exit(1);
}

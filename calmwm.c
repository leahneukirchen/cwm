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

#include <sys/types.h>
#include "queue.h"
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

Display			*X_Dpy;
Time			 Last_Event_Time = CurrentTime;
Atom			 cwmh[CWMH_NITEMS];
Atom			 ewmh[EWMH_NITEMS];
struct screen_q		 Screenq = TAILQ_HEAD_INITIALIZER(Screenq);
struct conf		 Conf;
volatile sig_atomic_t	 cwm_status;

void	usage(void);
static void	sighdlr(int);
static int	x_errorhandler(Display *, XErrorEvent *);
static int	x_init(const char *);
static void	x_teardown(void);
static int	x_wmerrorhandler(Display *, XErrorEvent *);

int
main(int argc, char **argv)
{
	char		*display_name = NULL;
	char		*fallback;
	int		 ch, xfd, nflag = 0;
	struct pollfd	 pfd[1];

	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		warnx("no locale support");
	mbtowc(NULL, NULL, MB_CUR_MAX);

	conf_init(&Conf);

	fallback = u_argv(argv);
	Conf.wm_argv = u_argv(argv);
	while ((ch = getopt(argc, argv, "c:d:nv")) != -1) {
		switch (ch) {
		case 'c':
			free(Conf.conf_file);
			Conf.conf_file = xstrdup(optarg);
			break;
		case 'd':
			display_name = optarg;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'v':
			Conf.debug++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (signal(SIGCHLD, sighdlr) == SIG_ERR ||
	    signal(SIGHUP, sighdlr) == SIG_ERR ||
	    signal(SIGINT, sighdlr) == SIG_ERR ||
	    signal(SIGTERM, sighdlr) == SIG_ERR)
		err(1, "signal");

	if (parse_config(Conf.conf_file, &Conf) == -1) {
		warnx("error parsing config file");
		if (nflag)
			return 1;
	}
	if (nflag)
		return 0;

	xfd = x_init(display_name);
	cwm_status = CWM_RUNNING;

#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1)
		err(1, "pledge");
#endif

	memset(&pfd, 0, sizeof(pfd));
	pfd[0].fd = xfd;
	pfd[0].events = POLLIN;
	while (cwm_status == CWM_RUNNING) {
		xev_process();
		if (poll(pfd, 1, -1) == -1) {
			if (errno != EINTR)
				warn("poll");
		}
	}
	x_teardown();
	if (cwm_status == CWM_EXEC_WM) {
		u_exec(Conf.wm_argv);
		warnx("'%s' failed to start, starting fallback", Conf.wm_argv);
		u_exec(fallback);
	}

	return 0;
}

static int
x_init(const char *dpyname)
{
	int	i;

	if ((X_Dpy = XOpenDisplay(dpyname)) == NULL)
		errx(1, "unable to open display \"%s\"", XDisplayName(dpyname));

	XSetErrorHandler(x_wmerrorhandler);
	XSelectInput(X_Dpy, DefaultRootWindow(X_Dpy), SubstructureRedirectMask);
	XSync(X_Dpy, False);
	XSetErrorHandler(x_errorhandler);

	Conf.xrandr = XRRQueryExtension(X_Dpy, &Conf.xrandr_event_base, &i);

	xu_atom_init();
	conf_cursor(&Conf);

	for (i = 0; i < ScreenCount(X_Dpy); i++)
		screen_init(i);

	return ConnectionNumber(X_Dpy);
}

static void
x_teardown(void)
{
	struct screen_ctx	*sc;
	unsigned int		 i;

	conf_clear(&Conf);

	TAILQ_FOREACH(sc, &Screenq, entry) {
		for (i = 0; i < CWM_COLOR_NITEMS; i++)
			XftColorFree(X_Dpy, DefaultVisual(X_Dpy, sc->which),
			    DefaultColormap(X_Dpy, sc->which),
			    &sc->xftcolor[i]);
		XftFontClose(X_Dpy, sc->xftfont);
		XUngrabKey(X_Dpy, AnyKey, AnyModifier, sc->rootwin);
	}
	XUngrabPointer(X_Dpy, CurrentTime);
	XUngrabKeyboard(X_Dpy, CurrentTime);
	for (i = 0; i < CF_NITEMS; i++)
		XFreeCursor(X_Dpy, Conf.cursor[i]);
	XSync(X_Dpy, False);
	XSetInputFocus(X_Dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XCloseDisplay(X_Dpy);
}

static int
x_wmerrorhandler(Display *dpy, XErrorEvent *e)
{
	errx(1, "root window unavailable - perhaps another wm is running?");
	return 0;
}

static int
x_errorhandler(Display *dpy, XErrorEvent *e)
{
#ifdef DEBUG
	char msg[80], number[80], req[80];

	XGetErrorText(X_Dpy, e->error_code, msg, sizeof(msg));
	(void)snprintf(number, sizeof(number), "%d", e->request_code);
	XGetErrorDatabaseText(X_Dpy, "XRequest", number,
	    "<unknown>", req, sizeof(req));

	warnx("%s(0x%x): %s", req, (unsigned int)e->resourceid, msg);
#endif
	return 0;
}

static void
sighdlr(int sig)
{
	pid_t	 pid;
	int	 save_errno = errno, status;

	switch (sig) {
	case SIGCHLD:
		/* Collect dead children. */
		while ((pid = waitpid(-1, &status, WNOHANG)) > 0 ||
		    (pid < 0 && errno == EINTR))
			;
		break;
	case SIGHUP:
		cwm_status = CWM_EXEC_WM;
		break;
	case SIGINT:
	case SIGTERM:
		cwm_status = CWM_QUIT;
		break;
	}

	errno = save_errno;
}

void
usage(void)
{
	extern char	*__progname;

	(void)fprintf(stderr, "usage: %s [-nv] [-c file] [-d display]\n",
	    __progname);
	exit(1);
}

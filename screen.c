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

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

void
screen_init(int which)
{
	struct screen_ctx	*sc;
	Window			*wins, w0, w1;
	XSetWindowAttributes	 rootattr;
	unsigned int		 nwins, i;

	sc = xcalloc(1, sizeof(*sc));

	TAILQ_INIT(&sc->mruq);

	sc->which = which;
	sc->rootwin = RootWindow(X_Dpy, sc->which);
	conf_screen(sc);

	xu_ewmh_net_supported(sc);
	xu_ewmh_net_supported_wm_check(sc);

	screen_update_geometry(sc);
	group_init(sc);

	rootattr.cursor = Conf.cursor[CF_NORMAL];
	rootattr.event_mask = SubstructureRedirectMask|SubstructureNotifyMask|
	    PropertyChangeMask|EnterWindowMask|LeaveWindowMask|
	    ColormapChangeMask|BUTTONMASK;

	XChangeWindowAttributes(X_Dpy, sc->rootwin,
	    CWEventMask|CWCursor, &rootattr);

	/* Deal with existing clients. */
	if (XQueryTree(X_Dpy, sc->rootwin, &w0, &w1, &wins, &nwins)) {
		for (i = 0; i < nwins; i++)
			(void)client_init(wins[i], sc);

		XFree(wins);
	}
	screen_updatestackingorder(sc);

	if (HasRandr)
		XRRSelectInput(X_Dpy, sc->rootwin, RRScreenChangeNotifyMask);

	TAILQ_INSERT_TAIL(&Screenq, sc, entry);

	XSync(X_Dpy, False);
}

struct screen_ctx *
screen_fromroot(Window rootwin)
{
	struct screen_ctx	*sc;

	TAILQ_FOREACH(sc, &Screenq, entry)
		if (sc->rootwin == rootwin)
			return (sc);

	/* XXX FAIL HERE */
	return (TAILQ_FIRST(&Screenq));
}

void
screen_updatestackingorder(struct screen_ctx *sc)
{
	Window			*wins, w0, w1;
	struct client_ctx	*cc;
	unsigned int		 nwins, i, s;

	if (!XQueryTree(X_Dpy, sc->rootwin, &w0, &w1, &wins, &nwins))
		return;

	for (s = 0, i = 0; i < nwins; i++) {
		/* Skip hidden windows */
		if ((cc = client_find(wins[i])) == NULL ||
		    cc->flags & CLIENT_HIDDEN)
			continue;

		cc->stackingorder = s++;
	}

	XFree(wins);
}

/*
 * Find which xinerama screen the coordinates (x,y) is on.
 */
struct geom
screen_find_xinerama(struct screen_ctx *sc, int x, int y, int flags)
{
	XineramaScreenInfo	*info;
	struct geom		 geom;
	int			 i;

	geom = sc->work;

	if (sc->xinerama == NULL)
		return (geom);

	for (i = 0; i < sc->xinerama_no; i++) {
		info = &sc->xinerama[i];
		if (x >= info->x_org && x < info->x_org + info->width &&
		    y >= info->y_org && y < info->y_org + info->height) {
			geom.x = info->x_org;
			geom.y = info->y_org;
			geom.w = info->width;
			geom.h = info->height;
			break;
		}
	}
	if (flags & CWM_GAP) {
		geom.x += sc->gap.left;
		geom.y += sc->gap.top;
		geom.w -= (sc->gap.left + sc->gap.right);
		geom.h -= (sc->gap.top + sc->gap.bottom);
	}
	return (geom);
}

void
screen_update_geometry(struct screen_ctx *sc)
{
	XineramaScreenInfo	*info = NULL;
	int			 info_no = 0;

	sc->view.x = 0;
	sc->view.y = 0;
	sc->view.w = DisplayWidth(X_Dpy, sc->which);
	sc->view.h = DisplayHeight(X_Dpy, sc->which);

	sc->work.x = sc->view.x + sc->gap.left;
	sc->work.y = sc->view.y + sc->gap.top;
	sc->work.w = sc->view.w - (sc->gap.left + sc->gap.right);
	sc->work.h = sc->view.h - (sc->gap.top + sc->gap.bottom);

	/* RandR event may have a CTRC added or removed. */
	if (XineramaIsActive(X_Dpy))
		info = XineramaQueryScreens(X_Dpy, &info_no);
	if (sc->xinerama != NULL)
		XFree(sc->xinerama);
	sc->xinerama = info;
	sc->xinerama_no = info_no;

	xu_ewmh_net_desktop_geometry(sc);
	xu_ewmh_net_workarea(sc);
}

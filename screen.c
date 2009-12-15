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

#include "calmwm.h"

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
	u_int			 nwins, i, s;

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

void
screen_init_xinerama(struct screen_ctx *sc)
{
	XineramaScreenInfo	*info;
	int			 no;

	if (HasXinerama == 0 || XineramaIsActive(X_Dpy) == 0) {
		HasXinerama = 0;
		sc->xinerama_no = 0;
	}

	info = XineramaQueryScreens(X_Dpy, &no);
	if (info == NULL) {
		/* Is xinerama actually off, instead of a malloc failure? */
		if (sc->xinerama == NULL)
			HasXinerama = 0;
		return;
	}

	if (sc->xinerama != NULL)
		XFree(sc->xinerama);
	sc->xinerama = info;
	sc->xinerama_no = no;
}

/*
 * Find which xinerama screen the coordinates (x,y) is on.
 */
XineramaScreenInfo *
screen_find_xinerama(struct screen_ctx *sc, int x, int y)
{
	XineramaScreenInfo	*info;
	int			 i;

	for (i = 0; i < sc->xinerama_no; i++) {
		info = &sc->xinerama[i];
		if (x > info->x_org && x < info->x_org + info->width &&
		    y > info->y_org && y < info->y_org + info->height)
			return (info);
	}
	return (NULL);
}

void
screen_update_geometry(struct screen_ctx *sc, int width, int height)
{
	long	 geom[2], workareas[CALMWM_NGROUPS][4];
	int	 i;

	sc->xmax = geom[0] = width;
	sc->ymax = geom[1] = height;
	XChangeProperty(X_Dpy, sc->rootwin, _NET_DESKTOP_GEOMETRY,
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)geom , 2);

	/* x, y, width, height. XXX gap */
	for (i = 0; i < CALMWM_NGROUPS; i++) {
		workareas[i][0] = 0;
		workareas[i][1] = 0;
		workareas[i][2] = width;
		workareas[i][3] = height;
	}

	XChangeProperty(X_Dpy, sc->rootwin, _NET_WORKAREA,
	    XA_CARDINAL, 32, PropModeReplace,
	    (unsigned char *)workareas, CALMWM_NGROUPS * 4);
}

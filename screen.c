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

#include <err.h>
#include <errno.h>
#include <limits.h>
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

	sc = xmalloc(sizeof(*sc));

	TAILQ_INIT(&sc->clientq);
	TAILQ_INIT(&sc->regionq);
	TAILQ_INIT(&sc->groupq);

	sc->which = which;
	sc->rootwin = RootWindow(X_Dpy, sc->which);
	sc->cycling = 0;
	sc->hideall = 0;

	conf_screen(sc);

	xu_ewmh_net_supported(sc);
	xu_ewmh_net_supported_wm_check(sc);

	screen_update_geometry(sc);

	for (i = 0; i < CALMWM_NGROUPS; i++)
		group_init(sc, i);

	xu_ewmh_net_desktop_names(sc);
	xu_ewmh_net_wm_desktop_viewport(sc);
	xu_ewmh_net_wm_number_of_desktops(sc);
	xu_ewmh_net_showing_desktop(sc);
	xu_ewmh_net_virtual_roots(sc);

	rootattr.cursor = Conf.cursor[CF_NORMAL];
	rootattr.event_mask = SubstructureRedirectMask |
	    SubstructureNotifyMask | PropertyChangeMask | EnterWindowMask |
	    LeaveWindowMask | ColormapChangeMask | BUTTONMASK;

	XChangeWindowAttributes(X_Dpy, sc->rootwin,
	    (CWEventMask | CWCursor), &rootattr);

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
screen_find(Window win)
{
	struct screen_ctx	*sc;

	TAILQ_FOREACH(sc, &Screenq, entry) {
		if (sc->rootwin == win)
			return(sc);
	}
	warnx("screen_find failure win 0x%lu\n", win);
	return(NULL);
}

void
screen_updatestackingorder(struct screen_ctx *sc)
{
	Window			*wins, w0, w1;
	struct client_ctx	*cc;
	unsigned int		 nwins, i, s;

	if (XQueryTree(X_Dpy, sc->rootwin, &w0, &w1, &wins, &nwins)) {
		for (s = 0, i = 0; i < nwins; i++) {
			/* Skip hidden windows */
			if ((cc = client_find(wins[i])) == NULL ||
			    cc->flags & CLIENT_HIDDEN)
				continue;

			cc->stackingorder = s++;
		}
		XFree(wins);
	}
}

struct region_ctx *
region_find(struct screen_ctx *sc, int x, int y)
{
	struct region_ctx	*rc;

	TAILQ_FOREACH(rc, &sc->regionq, entry) {
		if ((x >= rc->view.x) && (x < (rc->view.x + rc->view.w)) &&
		    (y >= rc->view.y) && (y < (rc->view.y + rc->view.h))) {
			break;
		}
	}
	return(rc);
}

struct geom
screen_area(struct screen_ctx *sc, int x, int y, int flags)
{
	struct region_ctx	*rc;
	struct geom		 area = sc->work;

	TAILQ_FOREACH(rc, &sc->regionq, entry) {
		if ((x >= rc->area.x) && (x < (rc->area.x + rc->area.w)) &&
		    (y >= rc->area.y) && (y < (rc->area.y + rc->area.h))) {
			area = rc->area;
			break;
		}
	}
	if (flags & CWM_GAP)
		area = screen_apply_gap(sc, area);
	return(area);
}

void
screen_update_geometry(struct screen_ctx *sc)
{
	struct region_ctx	*rc;

	sc->view.x = 0;
	sc->view.y = 0;
	sc->view.w = DisplayWidth(X_Dpy, sc->which);
	sc->view.h = DisplayHeight(X_Dpy, sc->which);
	sc->work = screen_apply_gap(sc, sc->view);

	while ((rc = TAILQ_FIRST(&sc->regionq)) != NULL) {
		TAILQ_REMOVE(&sc->regionq, rc, entry);
		free(rc);
	}

	if (HasRandr) {
		XRRScreenResources *sr;
		XRRCrtcInfo *ci;
		int i;

		sr = XRRGetScreenResources(X_Dpy, sc->rootwin);
		for (i = 0, ci = NULL; i < sr->ncrtc; i++) {
			ci = XRRGetCrtcInfo(X_Dpy, sr, sr->crtcs[i]);
			if (ci == NULL)
				continue;
			if (ci->noutput == 0) {
				XRRFreeCrtcInfo(ci);
				continue;
			}

			rc = xmalloc(sizeof(*rc));
			rc->num = i;
			rc->area.x = ci->x;
			rc->area.y = ci->y;
			rc->area.w = ci->width;
			rc->area.h = ci->height;
			rc->view.x = ci->x;
			rc->view.y = ci->y;
			rc->view.w = ci->width;
			rc->view.h = ci->height;
			rc->work = screen_apply_gap(sc, rc->view);
			TAILQ_INSERT_TAIL(&sc->regionq, rc, entry);

			XRRFreeCrtcInfo(ci);
		}
		XRRFreeScreenResources(sr);
	} else {
		rc = xmalloc(sizeof(*rc));
		rc->num = 0;
		rc->view.x = 0;
		rc->view.y = 0;
		rc->view.w = DisplayWidth(X_Dpy, sc->which);
		rc->view.h = DisplayHeight(X_Dpy, sc->which);
		rc->work = screen_apply_gap(sc, rc->view);
		TAILQ_INSERT_TAIL(&sc->regionq, rc, entry);
	}

	xu_ewmh_net_desktop_geometry(sc);
	xu_ewmh_net_workarea(sc);
}

struct geom
screen_apply_gap(struct screen_ctx *sc, struct geom geom)
{
	geom.x += sc->gap.left;
	geom.y += sc->gap.top;
	geom.w -= (sc->gap.left + sc->gap.right);
	geom.h -= (sc->gap.top + sc->gap.bottom);

	return(geom);
}

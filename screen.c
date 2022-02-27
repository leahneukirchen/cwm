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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

static struct geom screen_apply_gap(struct screen_ctx *, struct geom);
static void screen_scan(struct screen_ctx *);

void
screen_init(int which)
{
	struct screen_ctx	*sc;
	XSetWindowAttributes	 attr;

	sc = xmalloc(sizeof(*sc));

	TAILQ_INIT(&sc->clientq);
	TAILQ_INIT(&sc->regionq);
	TAILQ_INIT(&sc->groupq);

	sc->which = which;
	sc->rootwin = RootWindow(X_Dpy, sc->which);
	sc->colormap = DefaultColormap(X_Dpy, sc->which);
	sc->visual = DefaultVisual(X_Dpy, sc->which);
	sc->cycling = 0;
	sc->hideall = 0;

	conf_screen(sc);

	xu_ewmh_net_supported(sc);
	xu_ewmh_net_supported_wm_check(sc);

	conf_group(sc);
	sc->group_last = sc->group_active;
	screen_update_geometry(sc);

	xu_ewmh_net_desktop_names(sc);
	xu_ewmh_net_number_of_desktops(sc);
	xu_ewmh_net_showing_desktop(sc);
	xu_ewmh_net_virtual_roots(sc);

	attr.cursor = Conf.cursor[CF_NORMAL];
	attr.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
	    EnterWindowMask | PropertyChangeMask | ButtonPressMask;
	XChangeWindowAttributes(X_Dpy, sc->rootwin, (CWEventMask | CWCursor), &attr);

	if (Conf.xrandr)
		XRRSelectInput(X_Dpy, sc->rootwin, RRScreenChangeNotifyMask);

	screen_scan(sc);
	screen_updatestackingorder(sc);

	TAILQ_INSERT_TAIL(&Screenq, sc, entry);

	XSync(X_Dpy, False);
}

static void
screen_scan(struct screen_ctx *sc)
{
	struct client_ctx	 *cc, *active = NULL;
	Window			*wins, w0, w1, rwin, cwin;
	unsigned int		 nwins, i, mask;
	int			 rx, ry, wx, wy;

	XQueryPointer(X_Dpy, sc->rootwin, &rwin, &cwin,
	    &rx, &ry, &wx, &wy, &mask);

	if (XQueryTree(X_Dpy, sc->rootwin, &w0, &w1, &wins, &nwins)) {
		for (i = 0; i < nwins; i++) {
			if ((cc = client_init(wins[i], sc)) != NULL)
				if (cc->win == cwin)
					active = cc;
		}
		XFree(wins);
	}
	if (active)
		client_set_active(active);
}

struct screen_ctx *
screen_find(Window win)
{
	struct screen_ctx	*sc;

	TAILQ_FOREACH(sc, &Screenq, entry) {
		if (sc->rootwin == win)
			return sc;
	}
	warnx("%s: failure win 0x%lx", __func__, win);
	return NULL;
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
	return rc;
}

struct geom
screen_area(struct screen_ctx *sc, int x, int y, int apply_gap)
{
	struct region_ctx	*rc;
	struct geom		 area = sc->view;

	TAILQ_FOREACH(rc, &sc->regionq, entry) {
		if ((x >= rc->view.x) && (x < (rc->view.x + rc->view.w)) &&
		    (y >= rc->view.y) && (y < (rc->view.y + rc->view.h))) {
			area = rc->view;
			break;
		}
	}
	if (apply_gap)
		area = screen_apply_gap(sc, area);
	return area;
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

	if (Conf.xrandr) {
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
	xu_ewmh_net_desktop_viewport(sc);
	xu_ewmh_net_workarea(sc);
}

static struct geom
screen_apply_gap(struct screen_ctx *sc, struct geom geom)
{
	geom.x += sc->gap.left;
	geom.y += sc->gap.top;
	geom.w -= (sc->gap.left + sc->gap.right);
	geom.h -= (sc->gap.top + sc->gap.bottom);

	return geom;
}

/* Bring back clients which are beyond the screen. */
void
screen_assert_clients_within(struct screen_ctx *sc)
{
	struct client_ctx	*cc;
	int			 top, left, right, bottom;

	TAILQ_FOREACH(cc, &sc->clientq, entry) {
		if (cc->sc != sc)
			continue;
		top = cc->geom.y;
		left = cc->geom.x;
		right = cc->geom.x + cc->geom.w + (cc->bwidth * 2) - 1;
		bottom = cc->geom.y + cc->geom.h + (cc->bwidth * 2) - 1;
		if ((top > sc->view.h || left > sc->view.w) ||
		    (bottom < 0 || right < 0)) {
			cc->geom.x = sc->gap.left;
			cc->geom.y = sc->gap.top;
			client_move(cc);
		}
	}
}

void
screen_prop_win_create(struct screen_ctx *sc, Window win)
{
	sc->prop.win = XCreateSimpleWindow(X_Dpy, win, 0, 0, 1, 1, 0,
	    sc->xftcolor[CWM_COLOR_MENU_BG].pixel,
	    sc->xftcolor[CWM_COLOR_MENU_BG].pixel);
	sc->prop.xftdraw = XftDrawCreate(X_Dpy, sc->prop.win,
	    sc->visual, sc->colormap);

	XMapWindow(X_Dpy, sc->prop.win);
}

void
screen_prop_win_destroy(struct screen_ctx *sc)
{
	XftDrawDestroy(sc->prop.xftdraw);
	XDestroyWindow(X_Dpy, sc->prop.win);
}

void
screen_prop_win_draw(struct screen_ctx *sc, const char *fmt, ...)
{
	va_list			 ap;
	char			*text;
	XGlyphInfo		 extents;

	va_start(ap, fmt);
	xvasprintf(&text, fmt, ap);
	va_end(ap);

	XftTextExtentsUtf8(X_Dpy, sc->xftfont, (const FcChar8*)text,
	    strlen(text), &extents);
	XResizeWindow(X_Dpy, sc->prop.win, extents.xOff, sc->xftfont->height);
	XClearWindow(X_Dpy, sc->prop.win);
	XftDrawStringUtf8(sc->prop.xftdraw, &sc->xftcolor[CWM_COLOR_MENU_FONT],
	    sc->xftfont, 0, sc->xftfont->ascent + 1,
	    (const FcChar8*)text, strlen(text));

	free(text);
}

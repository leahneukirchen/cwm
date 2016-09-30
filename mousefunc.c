/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2004 Marius Aamodt Eriksen <marius@monkey.org>
 * Copyright (c) 2008 rivo nurges <rix@estpak.ee>
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
#include <sys/queue.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

void
mousefunc_client_resize(struct client_ctx *cc, union arg *arg)
{
	XEvent			 ev;
	Time			 ltime = 0;
	struct screen_ctx	*sc = cc->sc;

	if (cc->flags & CLIENT_FREEZE)
		return;

	client_raise(cc);
	client_ptrsave(cc);

	if (XGrabPointer(X_Dpy, cc->win, False, MOUSEMASK,
	    GrabModeAsync, GrabModeAsync, None, Conf.cursor[CF_RESIZE],
	    CurrentTime) != GrabSuccess)
		return;

	xu_ptr_setpos(cc->win, cc->geom.w, cc->geom.h);

	for (;;) {
		XWindowEvent(X_Dpy, cc->win, MOUSEMASK, &ev);

		switch (ev.type) {
		case MotionNotify:
			/* not more than 60 times / second */
			if ((ev.xmotion.time - ltime) <= (1000 / 60))
				continue;
			ltime = ev.xmotion.time;

			cc->geom.w = ev.xmotion.x;
			cc->geom.h = ev.xmotion.y;
			client_applysizehints(cc);
			client_resize(cc, 1);
			menu_windraw(sc, cc->win,
			    "%4d x %-4d", cc->dim.w, cc->dim.h);
			break;
		case ButtonRelease:
			client_resize(cc, 1);
			XUnmapWindow(X_Dpy, sc->menu.win);
			XReparentWindow(X_Dpy, sc->menu.win, sc->rootwin, 0, 0);
			XUngrabPointer(X_Dpy, CurrentTime);

			/* Make sure the pointer stays within the window. */
			if (cc->ptr.x > cc->geom.w)
				cc->ptr.x = cc->geom.w - cc->bwidth;
			if (cc->ptr.y > cc->geom.h)
				cc->ptr.y = cc->geom.h - cc->bwidth;
			client_ptrwarp(cc);
			return;
		}
	}
	/* NOTREACHED */
}

void
mousefunc_client_move(struct client_ctx *cc, union arg *arg)
{
	XEvent			 ev;
	Time			 ltime = 0;
	struct screen_ctx	*sc = cc->sc;
	struct geom		 area;
	int			 px, py;

	client_raise(cc);

	if (cc->flags & CLIENT_FREEZE)
		return;

	if (XGrabPointer(X_Dpy, cc->win, False, MOUSEMASK,
	    GrabModeAsync, GrabModeAsync, None, Conf.cursor[CF_MOVE],
	    CurrentTime) != GrabSuccess)
		return;

	xu_ptr_getpos(cc->win, &px, &py);

	for (;;) {
		XWindowEvent(X_Dpy, cc->win, MOUSEMASK, &ev);

		switch (ev.type) {
		case MotionNotify:
			/* not more than 60 times / second */
			if ((ev.xmotion.time - ltime) <= (1000 / 60))
				continue;
			ltime = ev.xmotion.time;

			cc->geom.x = ev.xmotion.x_root - px - cc->bwidth;
			cc->geom.y = ev.xmotion.y_root - py - cc->bwidth;

			area = screen_area(sc,
			    cc->geom.x + cc->geom.w / 2,
			    cc->geom.y + cc->geom.h / 2, CWM_GAP);
			cc->geom.x += client_snapcalc(cc->geom.x,
			    cc->geom.x + cc->geom.w + (cc->bwidth * 2),
			    area.x, area.x + area.w, sc->snapdist);
			cc->geom.y += client_snapcalc(cc->geom.y,
			    cc->geom.y + cc->geom.h + (cc->bwidth * 2),
			    area.y, area.y + area.h, sc->snapdist);
			client_move(cc);
			menu_windraw(sc, cc->win,
			    "%4d, %-4d", cc->geom.x, cc->geom.y);
			break;
		case ButtonRelease:
			client_move(cc);
			XUnmapWindow(X_Dpy, sc->menu.win);
			XReparentWindow(X_Dpy, sc->menu.win, sc->rootwin, 0, 0);
			XUngrabPointer(X_Dpy, CurrentTime);
			return;
		}
	}
	/* NOTREACHED */
}

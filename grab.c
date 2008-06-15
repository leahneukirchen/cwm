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

static int	_sweepcalc(struct client_ctx *, int, int, int, int);

#define ADJUST_HEIGHT(cc, dy)	((cc->geom.height - cc->geom.min_dy)/ dy)
#define ADJUST_WIDTH(cc, dx)	((cc->geom.width - cc->geom.min_dx)/ dx)

void
grab_sweep_draw(struct client_ctx *cc, int dx, int dy)
{
	struct screen_ctx *sc = CCTOSC(cc);
	int x0 = cc->geom.x, y0 = cc->geom.y;
	char asize[10];	/* fits "nnnnxnnnn\0" */
	int wide, height, wide_size, wide_name;

	snprintf(asize, sizeof(asize), "%dx%d",
	    ADJUST_WIDTH(cc, dx), ADJUST_HEIGHT(cc, dy));
	wide_size = font_width(asize, strlen(asize)) + 4;
	wide_name = font_width(cc->name, strlen(cc->name)) + 4;
	wide = MAX(wide_size, wide_name);
	height = font_ascent() + font_descent() + 1;

	XMoveResizeWindow(X_Dpy, sc->menuwin, x0, y0, wide, height * 2);
	XMapWindow(X_Dpy, sc->menuwin);
	XReparentWindow(X_Dpy, sc->menuwin, cc->win, 0, 0);
	XClearWindow(X_Dpy, sc->menuwin);
	font_draw(sc, cc->name, strlen(cc->name), sc->menuwin,
	    2, font_ascent() + 1);
	font_draw(sc, asize, strlen(asize), sc->menuwin,
	    wide/2 - wide_size/2, height + font_ascent() + 1);
}

void
grab_sweep(struct client_ctx *cc)
{
	XEvent ev;
	struct screen_ctx *sc = CCTOSC(cc);
	int x0 = cc->geom.x, y0 = cc->geom.y;
	int dx, dy;

	dx = MAX(1, cc->size->width_inc);
	dy = MAX(1, cc->size->height_inc);

	client_raise(cc);
	client_ptrsave(cc);

	if (xu_ptr_grab(sc->rootwin, MouseMask, Cursor_resize) < 0)
		return;

	xu_ptr_setpos(cc->win, cc->geom.width, cc->geom.height);
	grab_sweep_draw(cc, dx, dy);

	for (;;) {
		/* Look for changes in ptr position. */
		XMaskEvent(X_Dpy, MouseMask|ExposureMask, &ev);

		switch (ev.type) {
		case Expose:
			client_draw_border(cc);
			break;
		case MotionNotify:
			if (_sweepcalc(cc, x0, y0, ev.xmotion.x, ev.xmotion.y))
				/* Recompute window output */
				grab_sweep_draw(cc, dx, dy);

			XMoveResizeWindow(X_Dpy, cc->pwin,
			    cc->geom.x - cc->bwidth,
			    cc->geom.y - cc->bwidth,
			    cc->geom.width + cc->bwidth*2,
			    cc->geom.height + cc->bwidth*2);
			XMoveResizeWindow(X_Dpy, cc->win,
			    cc->bwidth, cc->bwidth,
			    cc->geom.width, cc->geom.height);

			client_do_shape(cc);
			break;
		case ButtonRelease:
			XUnmapWindow(X_Dpy, sc->menuwin);
			XReparentWindow(X_Dpy, sc->menuwin, sc->rootwin, 0, 0);
			xu_ptr_ungrab();

			/* Make sure the pointer stays within the window. */
			if (cc->ptr.x > cc->geom.width)
				cc->ptr.x = cc->geom.width - cc->bwidth;
			if (cc->ptr.y > cc->geom.height)
				cc->ptr.y = cc->geom.height - cc->bwidth;
			client_ptrwarp(cc);

			return;
		}
	}
	/* NOTREACHED */
}

void
grab_drag(struct client_ctx *cc)
{
	int x0 = cc->geom.x, y0 = cc->geom.y, xm, ym;
	struct screen_ctx *sc = CCTOSC(cc);
	XEvent ev;

	client_raise(cc);

	if (xu_ptr_grab(sc->rootwin, MouseMask, Cursor_move) < 0)
		return;

	xu_ptr_getpos(sc->rootwin, &xm, &ym);

	for (;;) {
		XMaskEvent(X_Dpy, MouseMask|ExposureMask, &ev);

		switch (ev.type) {
		case Expose:
			client_draw_border(cc);
			break;
		case MotionNotify:
			cc->geom.x = x0 + (ev.xmotion.x - xm);
			cc->geom.y = y0 + (ev.xmotion.y - ym);

			XMoveWindow(X_Dpy, cc->pwin,
			    cc->geom.x - cc->bwidth, cc->geom.y - cc->bwidth);

			break;
		case ButtonRelease:
			xu_ptr_ungrab();
			return;
		}
	}
	/* NOTREACHED */
}

static int
_sweepcalc(struct client_ctx *cc, int x0, int y0, int motionx, int motiony)
{
	int width, height;

	width = cc->geom.width;
	height = cc->geom.height;

	cc->geom.width = abs(x0 - motionx);
	cc->geom.height = abs(y0 - motiony);

	if (cc->size->flags & PResizeInc) {
		cc->geom.width -=
		    (cc->geom.width - cc->geom.min_dx) % cc->size->width_inc;
		cc->geom.height -=
		    (cc->geom.height - cc->geom.min_dy) % cc->size->height_inc;
	}

	if (cc->size->flags & PMinSize) {
		cc->geom.width = MAX(cc->geom.width, cc->size->min_width);
		cc->geom.height = MAX(cc->geom.height, cc->size->min_height);
	}

	if (cc->size->flags & PMaxSize) {
		cc->geom.width = MIN(cc->geom.width, cc->size->max_width);
		cc->geom.height = MIN(cc->geom.height, cc->size->max_height);
	}

	cc->geom.x = x0 <= motionx ? x0 : x0 - cc->geom.width;
	cc->geom.y = y0 <= motiony ? y0 : y0 - cc->geom.height;

	return (width != cc->geom.width || height != cc->geom.height);
}

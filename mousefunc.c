/*
 *  calmwm - the calm window manager
 *
 *  Copyright (c) 2004 Marius Aamodt Eriksen <marius@monkey.org>
 *  Copyright (c) 2008 rivo nurges <rix@estpak.ee>
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

static int	_mousefunc_sweep_calc(struct client_ctx *, int, int, int, int);
static void	_mousefunc_sweep_draw(struct client_ctx *, int, int);

#define ADJUST_HEIGHT(cc, dy)	((cc->geom.height - cc->geom.min_dy) / dy)
#define ADJUST_WIDTH(cc, dx)	((cc->geom.width - cc->geom.min_dx) / dx)

static int
_mousefunc_sweep_calc(struct client_ctx *cc, int x, int y, int mx, int my)
{
	int	 width = cc->geom.width, height = cc->geom.height;

	cc->geom.width = abs(x - mx);
	cc->geom.height = abs(y - my);

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

	cc->geom.x = x <= mx ? x : x - cc->geom.width;
	cc->geom.y = y <= my ? y : y - cc->geom.height;

	return (width != cc->geom.width || height != cc->geom.height);
}

static void
_mousefunc_sweep_draw(struct client_ctx *cc, int dx, int dy)
{
	struct screen_ctx	*sc = CCTOSC(cc);
	char			 asize[10]; /* fits "nnnnxnnnn\0" */
	int			 wide, height, wide_size, wide_name;
	int			 x = cc->geom.x, y = cc->geom.y;

	snprintf(asize, sizeof(asize), "%dx%d",
	    ADJUST_WIDTH(cc, dx), ADJUST_HEIGHT(cc, dy));
	wide_size = font_width(asize, strlen(asize)) + 4;
	wide_name = font_width(cc->name, strlen(cc->name)) + 4;
	wide = MAX(wide_size, wide_name);
	height = font_ascent() + font_descent() + 1;

	XMoveResizeWindow(X_Dpy, sc->menuwin, x, y, wide, height * 2);
	XMapWindow(X_Dpy, sc->menuwin);
	XReparentWindow(X_Dpy, sc->menuwin, cc->win, 0, 0);
	XClearWindow(X_Dpy, sc->menuwin);
	font_draw(sc, cc->name, strlen(cc->name), sc->menuwin,
	    2, font_ascent() + 1);
	font_draw(sc, asize, strlen(asize), sc->menuwin,
	    wide / 2 - wide_size / 2, height + font_ascent() + 1);
}

void
mousefunc_window_resize(struct client_ctx *cc, void *arg)
{
	XEvent			 ev;
	struct screen_ctx	*sc = CCTOSC(cc);
	int			 dx, dy;
	int			 x = cc->geom.x, y = cc->geom.y;

	dx = MAX(1, cc->size->width_inc);
	dy = MAX(1, cc->size->height_inc);

	client_raise(cc);
	client_ptrsave(cc);

	if (xu_ptr_grab(sc->rootwin, MouseMask, Cursor_resize) < 0)
		return;

	xu_ptr_setpos(cc->win, cc->geom.width, cc->geom.height);
	_mousefunc_sweep_draw(cc, dx, dy);

	for (;;) {
		XMaskEvent(X_Dpy, MouseMask|ExposureMask, &ev);

		switch (ev.type) {
		case Expose:
			client_draw_border(cc);
			break;
		case MotionNotify:
			if (_mousefunc_sweep_calc(cc, x, y,
			    ev.xmotion.x, ev.xmotion.y))
				/* Recompute window output */
				_mousefunc_sweep_draw(cc, dx, dy);

			XMoveResizeWindow(X_Dpy, cc->win,
			    cc->geom.x - cc->bwidth,
			    cc->geom.y - cc->bwidth,
			    cc->geom.width + cc->bwidth * 2,
			    cc->geom.height + cc->bwidth * 2);
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
mousefunc_window_move(struct client_ctx *cc, void *arg)
{
	XEvent			 ev;
	struct screen_ctx	*sc = CCTOSC(cc);
	int			 mx, my;
	int			 x = cc->geom.x, y = cc->geom.y;

	client_raise(cc);

	if (xu_ptr_grab(sc->rootwin, MouseMask, Cursor_move) < 0)
		return;

	xu_ptr_getpos(sc->rootwin, &mx, &my);

	for (;;) {
		XMaskEvent(X_Dpy, MouseMask|ExposureMask, &ev);

		switch (ev.type) {
		case Expose:
			client_draw_border(cc);
			break;
		case MotionNotify:
			cc->geom.x = x + (ev.xmotion.x - mx);
			cc->geom.y = y + (ev.xmotion.y - my);

			XMoveWindow(X_Dpy, cc->win,
			    cc->geom.x - cc->bwidth, cc->geom.y - cc->bwidth);

			break;
		case ButtonRelease:
			xu_ptr_ungrab();
			return;
		}
	}
	/* NOTREACHED */
}

void
mousefunc_window_grouptoggle(struct client_ctx *cc, void *arg)
{
	group_sticky_toggle_enter(cc);
}

void
mousefunc_window_lower(struct client_ctx *cc, void *arg)
{
	client_ptrsave(cc);
	client_lower(cc);
}

void
mousefunc_window_hide(struct client_ctx *cc, void *arg)
{
	client_hide(cc);
}

void
mousefunc_menu_group(struct client_ctx *cc, void *arg)
{
	group_menu(arg);
}

void
mousefunc_menu_unhide(struct client_ctx *cc, void *arg)
{
	struct client_ctx	*old_cc;
	struct menu		*mi;
	struct menu_q		 menuq;
	char			*wname;

	old_cc = client_current();

	TAILQ_INIT(&menuq);
	TAILQ_FOREACH(cc, &Clientq, entry)
		if (cc->flags & CLIENT_HIDDEN) {
			if (cc->label != NULL)
				wname = cc->label;
			else
				wname = cc->name;

			if (wname == NULL)
				continue;

			XCALLOC(mi, struct menu);
			strlcpy(mi->text, wname, sizeof(mi->text));
			mi->ctx = cc;
			TAILQ_INSERT_TAIL(&menuq, mi, entry);
		}

	if (TAILQ_EMPTY(&menuq))
		return;

	mi = menu_filter(&menuq, NULL, NULL, 0, NULL, NULL);
	if (mi != NULL) {
		cc = (struct client_ctx *)mi->ctx;
		client_unhide(cc);

		if (old_cc != NULL)
			client_ptrsave(old_cc);
		client_ptrwarp(cc);
	} else {
		while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
			TAILQ_REMOVE(&menuq, mi, entry);
			xfree(mi);
		}
	}
}

void
mousefunc_menu_cmd(struct client_ctx *cc, void *arg)
{
	struct menu	*mi;
	struct menu_q	 menuq;
	struct cmd	*cmd;

	TAILQ_INIT(&menuq);
	TAILQ_FOREACH(cmd, &Conf.cmdq, entry) {
		XCALLOC(mi, struct menu);
		strlcpy(mi->text, cmd->label, sizeof(mi->text));
		mi->ctx = cmd;
		TAILQ_INSERT_TAIL(&menuq, mi, entry);
	}
	if (TAILQ_EMPTY(&menuq))
		return;

	mi = menu_filter(&menuq, NULL, NULL, 0, NULL, NULL);
	if (mi != NULL)
		u_spawn(((struct cmd *)mi->ctx)->image);
	else
		while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
			TAILQ_REMOVE(&menuq, mi, entry);
			xfree(mi);
		}
}

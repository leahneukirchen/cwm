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

#include <sys/param.h>
#include <sys/queue.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "calmwm.h"

static int	mousefunc_sweep_calc(struct client_ctx *, int, int, int, int);
static void	mousefunc_sweep_draw(struct client_ctx *);

static int
mousefunc_sweep_calc(struct client_ctx *cc, int x, int y, int mx, int my)
{
	int	 width = cc->geom.width, height = cc->geom.height;

	cc->geom.width = abs(x - mx) - cc->bwidth;
	cc->geom.height = abs(y - my) - cc->bwidth;

	client_applysizehints(cc);

	cc->geom.x = x <= mx ? x : x - cc->geom.width;
	cc->geom.y = y <= my ? y : y - cc->geom.height;

	return (width != cc->geom.width || height != cc->geom.height);
}

static void
mousefunc_sweep_draw(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	char			 asize[10]; /* fits "nnnnxnnnn\0" */
	int			 width, width_size, width_name;

	(void)snprintf(asize, sizeof(asize), "%dx%d",
	    (cc->geom.width - cc->hint.basew) / cc->hint.incw,
	    (cc->geom.height - cc->hint.baseh) / cc->hint.inch);
	width_size = font_width(sc, asize, strlen(asize)) + 4;
	width_name = font_width(sc, cc->name, strlen(cc->name)) + 4;
	width = MAX(width_size, width_name);

	XReparentWindow(X_Dpy, sc->menuwin, cc->win, 0, 0);
	XMoveResizeWindow(X_Dpy, sc->menuwin, 0, 0, width, font_height(sc) * 2);
	XMapWindow(X_Dpy, sc->menuwin);
	XClearWindow(X_Dpy, sc->menuwin);
	font_draw(sc, cc->name, strlen(cc->name), sc->menuwin,
	    2, font_ascent(sc) + 1);
	font_draw(sc, asize, strlen(asize), sc->menuwin,
	    width / 2 - width_size / 2, font_height(sc) + font_ascent(sc) + 1);
}

void
mousefunc_window_resize(struct client_ctx *cc, void *arg)
{
	XEvent			 ev;
	Time			 time = 0;
	struct screen_ctx	*sc = cc->sc;
	int			 x = cc->geom.x, y = cc->geom.y;

	if (cc->flags & CLIENT_FREEZE)
		return;

	client_raise(cc);
	client_ptrsave(cc);

	if (xu_ptr_grab(cc->win, MOUSEMASK, Cursor_resize) < 0)
		return;

	xu_ptr_setpos(cc->win, cc->geom.width, cc->geom.height);
	mousefunc_sweep_draw(cc);

	for (;;) {
		XMaskEvent(X_Dpy, MOUSEMASK|ExposureMask, &ev);

		switch (ev.type) {
		case Expose:
			client_draw_border(cc);
			break;
		case MotionNotify:
			if (mousefunc_sweep_calc(cc, x, y,
			    ev.xmotion.x_root, ev.xmotion.y_root))
				/* Recompute window output */
				mousefunc_sweep_draw(cc);

			/* don't resize more than 60 times / second */
			if ((ev.xmotion.time - time) > (1000 / 60)) {
				time = ev.xmotion.time;
				client_resize(cc);
			}
			break;
		case ButtonRelease:
			if (time)
				client_resize(cc);
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
	Time			 time = 0;
	int			 px, py;

	client_raise(cc);

	if (cc->flags & CLIENT_FREEZE)
		return;

	if (xu_ptr_grab(cc->win, MOUSEMASK, Cursor_move) < 0)
		return;

	xu_ptr_getpos(cc->win, &px, &py);

	for (;;) {
		XMaskEvent(X_Dpy, MOUSEMASK|ExposureMask, &ev);

		switch (ev.type) {
		case Expose:
			client_draw_border(cc);
			break;
		case MotionNotify:
			cc->geom.x = ev.xmotion.x_root - px - cc->bwidth;
			cc->geom.y = ev.xmotion.y_root - py - cc->bwidth;

			cc->geom.x += client_snapcalc(cc->geom.x,
			    cc->geom.width, cc->sc->xmax,
			    cc->bwidth, Conf.snapdist);
			cc->geom.y += client_snapcalc(cc->geom.y,
			    cc->geom.height, cc->sc->ymax,
			    cc->bwidth, Conf.snapdist);

			/* don't move more than 60 times / second */
			if ((ev.xmotion.time - time) > (1000 / 60)) {
				time = ev.xmotion.time;
				client_move(cc);
			}
			break;
		case ButtonRelease:
			if (time)
				client_move(cc);
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
mousefunc_window_raise(struct client_ctx *cc, void *arg)
{
	client_raise(cc);
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
	struct screen_ctx	*sc;
	struct client_ctx	*old_cc;
	struct menu		*mi;
	struct menu_q		 menuq;
	char			*wname;

	sc = cc->sc;
	old_cc = client_current();

	TAILQ_INIT(&menuq);
	TAILQ_FOREACH(cc, &Clientq, entry)
		if (cc->flags & CLIENT_HIDDEN) {
			wname = (cc->label) ? cc->label : cc->name;
			if (wname == NULL)
				continue;

			mi = xcalloc(1, sizeof(*mi));
			(void)strlcpy(mi->text, wname, sizeof(mi->text));
			mi->ctx = cc;
			TAILQ_INSERT_TAIL(&menuq, mi, entry);
		}

	if (TAILQ_EMPTY(&menuq))
		return;

	mi = menu_filter(sc, &menuq, NULL, NULL, 0, NULL, NULL);
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
	struct screen_ctx	*sc;
	struct menu		*mi;
	struct menu_q		 menuq;
	struct cmd		*cmd;

	sc = cc->sc;

	TAILQ_INIT(&menuq);
	TAILQ_FOREACH(cmd, &Conf.cmdq, entry) {
		mi = xcalloc(1, sizeof(*mi));
		(void)strlcpy(mi->text, cmd->label, sizeof(mi->text));
		mi->ctx = cmd;
		TAILQ_INSERT_TAIL(&menuq, mi, entry);
	}
	if (TAILQ_EMPTY(&menuq))
		return;

	mi = menu_filter(sc, &menuq, NULL, NULL, 0, NULL, NULL);
	if (mi != NULL)
		u_spawn(((struct cmd *)mi->ctx)->image);
	else
		while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
			TAILQ_REMOVE(&menuq, mi, entry);
			xfree(mi);
		}
}

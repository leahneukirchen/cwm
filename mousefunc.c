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
#include "queue.h"

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

static void	mousefunc_sweep_draw(struct client_ctx *);

static void
mousefunc_sweep_draw(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	char			 s[14]; /* fits " nnnn x nnnn \0" */

	(void)snprintf(s, sizeof(s), " %4d x %-4d ", cc->dim.w, cc->dim.h);

	XReparentWindow(X_Dpy, sc->menuwin, cc->win, 0, 0);
	XMoveResizeWindow(X_Dpy, sc->menuwin, 0, 0,
	    xu_xft_width(sc->xftfont, s, strlen(s)), sc->xftfont->height);
	XMapWindow(X_Dpy, sc->menuwin);
	XClearWindow(X_Dpy, sc->menuwin);

	xu_xft_draw(sc, s, CWM_COLOR_MENU_FONT, 0, sc->xftfont->ascent + 1);
}

void
mousefunc_client_resize(struct client_ctx *cc, union arg *arg)
{
	XEvent			 ev;
	Time			 ltime = 0;
	struct screen_ctx	*sc = cc->sc;
	int			 x = cc->geom.x, y = cc->geom.y;

	if (cc->flags & CLIENT_FREEZE)
		return;

	client_raise(cc);
	client_ptrsave(cc);

	if (xu_ptr_grab(cc->win, MOUSEMASK, Conf.cursor[CF_RESIZE]) < 0)
		return;

	xu_ptr_setpos(cc->win, cc->geom.w, cc->geom.h);
	mousefunc_sweep_draw(cc);

	for (;;) {
		XMaskEvent(X_Dpy, MOUSEMASK, &ev);

		switch (ev.type) {
		case MotionNotify:
			/* not more than 60 times / second */
			if ((ev.xmotion.time - ltime) <= (1000 / 60))
				continue;
			ltime = ev.xmotion.time;

			cc->geom.w = abs(x - ev.xmotion.x_root) - cc->bwidth;
			cc->geom.h = abs(y - ev.xmotion.y_root) - cc->bwidth;
			cc->geom.x = (x <= ev.xmotion.x_root) ?
				x : x - cc->geom.w;
			cc->geom.y = (y <= ev.xmotion.y_root) ?
				y : y - cc->geom.h;
			client_applysizehints(cc);
			client_resize(cc, 1);
			mousefunc_sweep_draw(cc);
			break;
		case ButtonRelease:
			client_resize(cc, 1);
			XUnmapWindow(X_Dpy, sc->menuwin);
			XReparentWindow(X_Dpy, sc->menuwin, sc->rootwin, 0, 0);
			xu_ptr_ungrab();

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

	if (xu_ptr_grab(cc->win, MOUSEMASK, Conf.cursor[CF_MOVE]) < 0)
		return;

	xu_ptr_getpos(cc->win, &px, &py);

	for (;;) {
		XMaskEvent(X_Dpy, MOUSEMASK, &ev);

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
			break;
		case ButtonRelease:
			client_move(cc);
			xu_ptr_ungrab();
			return;
		}
	}
	/* NOTREACHED */
}

void
mousefunc_menu_group(struct client_ctx *cc, union arg *arg)
{
	struct screen_ctx	*sc = cc->sc;
	struct group_ctx	*gc;
	struct menu		*mi;
	struct menu_q		 menuq;

	TAILQ_INIT(&menuq);
	TAILQ_FOREACH(gc, &sc->groupq, entry) {
		if (group_holds_only_sticky(gc))
			continue;
		menuq_add(&menuq, gc, "%d %s", gc->num, gc->name);
	}

	if ((mi = menu_filter(sc, &menuq, NULL, NULL, CWM_MENU_LIST,
	    NULL, search_print_group)) != NULL) {
		gc = (struct group_ctx *)mi->ctx;
		(group_holds_only_hidden(gc)) ?
		    group_show(gc) : group_hide(gc);
	}

	menuq_clear(&menuq);
}

void
mousefunc_menu_client(struct client_ctx *cc, union arg *arg)
{
	struct screen_ctx	*sc = cc->sc;
	struct client_ctx	*old_cc;
	struct menu		*mi;
	struct menu_q		 menuq;

	old_cc = client_current();

	TAILQ_INIT(&menuq);
	TAILQ_FOREACH(cc, &sc->clientq, entry) {
		if (cc->flags & CLIENT_HIDDEN) {
			menuq_add(&menuq, cc, NULL);
		}
	}

	if ((mi = menu_filter(sc, &menuq, NULL, NULL, CWM_MENU_LIST,
	    NULL, search_print_client)) != NULL) {
		cc = (struct client_ctx *)mi->ctx;
		client_unhide(cc);
		if (old_cc != NULL)
			client_ptrsave(old_cc);
		client_ptrwarp(cc);
	}

	menuq_clear(&menuq);
}

void
mousefunc_menu_cmd(struct client_ctx *cc, union arg *arg)
{
	struct screen_ctx	*sc = cc->sc;
	struct cmd		*cmd;
	struct menu		*mi;
	struct menu_q		 menuq;

	TAILQ_INIT(&menuq);
	TAILQ_FOREACH(cmd, &Conf.cmdq, entry) {
		if ((strcmp(cmd->name, "lock") == 0) ||
		    (strcmp(cmd->name, "term") == 0))
			continue;
		menuq_add(&menuq, cmd, NULL);
	}

	if ((mi = menu_filter(sc, &menuq, NULL, NULL, CWM_MENU_LIST,
	    NULL, search_print_cmd)) != NULL)
		u_spawn(((struct cmd *)mi->ctx)->path);

	menuq_clear(&menuq);
}

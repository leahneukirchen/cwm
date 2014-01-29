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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

static void	mousefunc_sweep_calc(struct client_ctx *, int, int, int, int);
static void	mousefunc_sweep_draw(struct client_ctx *);

static void
mousefunc_sweep_calc(struct client_ctx *cc, int x, int y, int mx, int my)
{
	cc->geom.w = abs(x - mx) - cc->bwidth;
	cc->geom.h = abs(y - my) - cc->bwidth;

	client_applysizehints(cc);

	cc->geom.x = x <= mx ? x : x - cc->geom.w;
	cc->geom.y = y <= my ? y : y - cc->geom.h;
}

static void
mousefunc_sweep_draw(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	char			 s[14]; /* fits " nnnn x nnnn \0" */

	(void)snprintf(s, sizeof(s), " %4d x %-4d ",
	    (cc->geom.w - cc->hint.basew) / cc->hint.incw,
	    (cc->geom.h - cc->hint.baseh) / cc->hint.inch);

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
			mousefunc_sweep_calc(cc, x, y,
			    ev.xmotion.x_root, ev.xmotion.y_root);

			/* don't resize more than 60 times / second */
			if ((ev.xmotion.time - ltime) > (1000 / 60)) {
				ltime = ev.xmotion.time;
				client_resize(cc, 1);
				mousefunc_sweep_draw(cc);
			}
			break;
		case ButtonRelease:
			if (ltime)
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
	struct geom		 xine;
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
			cc->geom.x = ev.xmotion.x_root - px - cc->bwidth;
			cc->geom.y = ev.xmotion.y_root - py - cc->bwidth;

			xine = screen_find_xinerama(sc,
			    cc->geom.x + cc->geom.w / 2,
			    cc->geom.y + cc->geom.h / 2, CWM_GAP);
			cc->geom.x += client_snapcalc(cc->geom.x,
			    cc->geom.x + cc->geom.w + (cc->bwidth * 2),
			    xine.x, xine.x + xine.w, sc->snapdist);
			cc->geom.y += client_snapcalc(cc->geom.y,
			    cc->geom.y + cc->geom.h + (cc->bwidth * 2),
			    xine.y, xine.y + xine.h, sc->snapdist);

			/* don't move more than 60 times / second */
			if ((ev.xmotion.time - ltime) > (1000 / 60)) {
				ltime = ev.xmotion.time;
				client_move(cc);
			}
			break;
		case ButtonRelease:
			if (ltime)
				client_move(cc);
			xu_ptr_ungrab();
			return;
		}
	}
	/* NOTREACHED */
}

void
mousefunc_client_grouptoggle(struct client_ctx *cc, union arg *arg)
{
	group_sticky_toggle_enter(cc);
}

void
mousefunc_client_lower(struct client_ctx *cc, union arg *arg)
{
	client_ptrsave(cc);
	client_lower(cc);
}

void
mousefunc_client_raise(struct client_ctx *cc, union arg *arg)
{
	client_raise(cc);
}

void
mousefunc_client_hide(struct client_ctx *cc, union arg *arg)
{
	client_hide(cc);
}

void
mousefunc_client_cyclegroup(struct client_ctx *cc, union arg *arg)
{
	group_cycle(cc->sc, arg->i);
}

void
mousefunc_menu_group(struct client_ctx *cc, union arg *arg)
{
	group_menu(cc->sc);
}

void
mousefunc_menu_unhide(struct client_ctx *cc, union arg *arg)
{
	struct screen_ctx	*sc = cc->sc;
	struct client_ctx	*old_cc;
	struct menu		*mi;
	struct menu_q		 menuq;
	char			*wname;

	old_cc = client_current();

	TAILQ_INIT(&menuq);
	TAILQ_FOREACH(cc, &Clientq, entry)
		if (cc->flags & CLIENT_HIDDEN) {
			wname = (cc->label) ? cc->label : cc->name;
			if (wname == NULL)
				continue;

			menuq_add(&menuq, cc, "(%d) %s",
			    cc->group ? cc->group->shortcut : 0, wname);
		}

	if (TAILQ_EMPTY(&menuq))
		return;

	if ((mi = menu_filter(sc, &menuq, NULL, NULL, 0,
	    NULL, NULL)) != NULL) {
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
	TAILQ_FOREACH(cmd, &Conf.cmdq, entry)
		menuq_add(&menuq, cmd, "%s", cmd->name);

	if (TAILQ_EMPTY(&menuq))
		return;

	if ((mi = menu_filter(sc, &menuq, NULL, NULL, 0,
	    NULL, NULL)) != NULL)
		u_spawn(((struct cmd *)mi->ctx)->path);

	menuq_clear(&menuq);
}

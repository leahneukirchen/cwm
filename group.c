/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2004 Andy Adamson <dros@monkey.org>
 * Copyright (c) 2004,2005 Marius Aamodt Eriksen <marius@monkey.org>
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

static struct group_ctx	*group_next(struct group_ctx *);
static struct group_ctx	*group_prev(struct group_ctx *);
static void		 group_restack(struct group_ctx *);
static void		 group_setactive(struct group_ctx *);

const char *num_to_name[] = {
	"nogroup", "one", "two", "three", "four", "five", "six",
	"seven", "eight", "nine"
};

void
group_assign(struct group_ctx *gc, struct client_ctx *cc)
{
	if (cc->gc != NULL)
		TAILQ_REMOVE(&cc->gc->clientq, cc, group_entry);

	if ((gc != NULL) && (gc->num == 0))
		gc = NULL;

	cc->gc = gc;

	if (cc->gc != NULL)
		TAILQ_INSERT_TAIL(&gc->clientq, cc, group_entry);

	xu_ewmh_net_wm_desktop(cc);
}

void
group_hide(struct group_ctx *gc)
{
	struct client_ctx	*cc;

	screen_updatestackingorder(gc->sc);

	TAILQ_FOREACH(cc, &gc->clientq, group_entry) {
		if (!(cc->flags & CLIENT_STICKY))
			client_hide(cc);
	}
}

void
group_show(struct group_ctx *gc)
{
	struct client_ctx	*cc;

	TAILQ_FOREACH(cc, &gc->clientq, group_entry) {
		if (!(cc->flags & CLIENT_STICKY))
			client_unhide(cc);
	}

	group_restack(gc);
	group_setactive(gc);
}

static void
group_restack(struct group_ctx *gc)
{
	struct client_ctx	*cc;
	Window			*winlist;
	int			 i, lastempty = -1;
	int			 nwins = 0, highstack = 0;

	TAILQ_FOREACH(cc, &gc->clientq, group_entry) {
		if (cc->stackingorder > highstack)
			highstack = cc->stackingorder;
	}
	winlist = xreallocarray(NULL, (highstack + 1), sizeof(*winlist));

	/* Invert the stacking order for XRestackWindows(). */
	TAILQ_FOREACH(cc, &gc->clientq, group_entry) {
		winlist[highstack - cc->stackingorder] = cc->win;
		nwins++;
	}

	/* Un-sparseify */
	for (i = 0; i <= highstack; i++) {
		if (!winlist[i] && lastempty == -1)
			lastempty = i;
		else if (winlist[i] && lastempty != -1) {
			winlist[lastempty] = winlist[i];
			if (++lastempty == i)
				lastempty = -1;
		}
	}

	XRestackWindows(X_Dpy, winlist, nwins);
	free(winlist);
}

void
group_init(struct screen_ctx *sc, int num)
{
	struct group_ctx	*gc;

	gc = xmalloc(sizeof(*gc));
	gc->sc = sc;
	gc->name = xstrdup(num_to_name[num]);
	gc->num = num;
	TAILQ_INIT(&gc->clientq);

	TAILQ_INSERT_TAIL(&sc->groupq, gc, entry);

	if (num == 1)
		group_setactive(gc);
}

void
group_setactive(struct group_ctx *gc)
{
	struct screen_ctx	*sc = gc->sc;

	sc->group_active = gc;

	xu_ewmh_net_current_desktop(sc);
}

void
group_movetogroup(struct client_ctx *cc, int idx)
{
	struct screen_ctx	*sc = cc->sc;
	struct group_ctx	*gc;

	if (idx < 0 || idx >= Conf.ngroups)
		errx(1, "%s: index out of range (%d)", __func__, idx);

	TAILQ_FOREACH(gc, &sc->groupq, entry) {
		if (gc->num == idx)
			break;
	}

	if (cc->gc == gc)
		return;
	if (gc->num != 0 && group_holds_only_hidden(gc))
		client_hide(cc);
	group_assign(gc, cc);
}

void
group_toggle_membership_enter(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	struct group_ctx	*gc = sc->group_active;

	if (gc == cc->gc) {
		group_assign(NULL, cc);
		cc->flags |= CLIENT_UNGROUP;
	} else {
		group_assign(gc, cc);
		cc->flags |= CLIENT_GROUP;
	}

	client_draw_border(cc);
}

void
group_toggle_membership_leave(struct client_ctx *cc)
{
	cc->flags &= ~CLIENT_HIGHLIGHT;
	client_draw_border(cc);
}

int
group_holds_only_sticky(struct group_ctx *gc)
{
	struct client_ctx	*cc;

	TAILQ_FOREACH(cc, &gc->clientq, group_entry) {
		if (!(cc->flags & CLIENT_STICKY))
			return(0);
	}
	return(1);
}

int
group_holds_only_hidden(struct group_ctx *gc)
{
	struct client_ctx	*cc;

	TAILQ_FOREACH(cc, &gc->clientq, group_entry) {
		if (!(cc->flags & (CLIENT_HIDDEN | CLIENT_STICKY)))
			return(0);
	}
	return(1);
}

void
group_hidetoggle(struct screen_ctx *sc, int idx)
{
	struct group_ctx	*gc;

	if (idx < 0 || idx >= Conf.ngroups)
		errx(1, "%s: index out of range (%d)", __func__, idx);

	TAILQ_FOREACH(gc, &sc->groupq, entry) {
		if (gc->num == idx)
			break;
	}

	if (group_holds_only_hidden(gc))
		group_show(gc);
	else {
		group_hide(gc);
		/* make clients stick to empty group */
		if (TAILQ_EMPTY(&gc->clientq))
			group_setactive(gc);
	}
}

void
group_only(struct screen_ctx *sc, int idx)
{
	struct group_ctx	*gc;

	if (idx < 0 || idx >= Conf.ngroups)
		errx(1, "%s: index out of range (%d)", __func__, idx);

	TAILQ_FOREACH(gc, &sc->groupq, entry) {
		if (gc->num == idx)
			group_show(gc);
		else
			group_hide(gc);
	}
}

void
group_cycle(struct screen_ctx *sc, int flags)
{
	struct group_ctx	*newgc, *oldgc, *showgroup = NULL;

	oldgc = sc->group_active;

	newgc = oldgc;
	for (;;) {
		newgc = (flags & CWM_CYCLE_REVERSE) ? group_prev(newgc) :
		    group_next(newgc);

		if (newgc == oldgc)
			break;

		if (!group_holds_only_sticky(newgc) && showgroup == NULL)
			showgroup = newgc;
		else if (!group_holds_only_hidden(newgc))
			group_hide(newgc);
	}

	if (showgroup == NULL)
		return;

	group_hide(oldgc);

	if (group_holds_only_hidden(showgroup))
		group_show(showgroup);
	else
		group_setactive(showgroup);
}

static struct group_ctx *
group_next(struct group_ctx *gc)
{
	struct screen_ctx	*sc = gc->sc;
	struct group_ctx	*newgc;

	return(((newgc = TAILQ_NEXT(gc, entry)) != NULL) ?
	    newgc : TAILQ_FIRST(&sc->groupq));
}

static struct group_ctx *
group_prev(struct group_ctx *gc)
{
	struct screen_ctx	*sc = gc->sc;
	struct group_ctx	*newgc;

	return(((newgc = TAILQ_PREV(gc, group_q, entry)) != NULL) ?
	    newgc : TAILQ_LAST(&sc->groupq, group_q));
}

void
group_alltoggle(struct screen_ctx *sc)
{
	struct group_ctx	*gc;

	TAILQ_FOREACH(gc, &sc->groupq, entry) {
		if (sc->hideall)
			group_show(gc);
		else
			group_hide(gc);
	}
	sc->hideall = !sc->hideall;
}

int
group_restore(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	struct group_ctx	*gc;
	int			 num;
	long			*grpnum;

	if (xu_getprop(cc->win, ewmh[_NET_WM_DESKTOP], XA_CARDINAL, 1L,
	    (unsigned char **)&grpnum) <= 0)
		return(0);

	num = (*grpnum == -1) ? 0 : *grpnum;
	num = MIN(num, (Conf.ngroups - 1));
	XFree(grpnum);

	TAILQ_FOREACH(gc, &sc->groupq, entry) {
		if (gc->num == num) {
			group_assign(gc, cc);
			return(1);
		}
	}
	return(0);
}

int
group_autogroup(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	struct autogroup	*ag;
	struct group_ctx	*gc;
	int			 num = -1, both_match = 0;

	if (cc->ch.res_class == NULL || cc->ch.res_name == NULL)
		return(0);

	TAILQ_FOREACH(ag, &Conf.autogroupq, entry) {
		if (strcmp(ag->class, cc->ch.res_class) == 0) {
			if ((ag->name != NULL) &&
			    (strcmp(ag->name, cc->ch.res_name) == 0)) {
				num = ag->num;
				both_match = 1;
			} else if (ag->name == NULL && !both_match)
				num = ag->num;
		}
	}

	TAILQ_FOREACH(gc, &sc->groupq, entry) {
		if (gc->num == num) {
			group_assign(gc, cc);
			return(1);
		}
	}
	return(0);
}

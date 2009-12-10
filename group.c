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
 * $Id$
 */

#include "headers.h"
#include "calmwm.h"

static void		 group_add(struct group_ctx *, struct client_ctx *);
static void		 group_remove(struct client_ctx *);
static void		 group_hide(struct screen_ctx *, struct group_ctx *);
static void		 group_show(struct screen_ctx *, struct group_ctx *);
static void		 group_fix_hidden_state(struct group_ctx *);

const char *shortcut_to_name[] = {
	"nogroup", "one", "two", "three", "four", "five", "six",
	"seven", "eight", "nine"
};

static void
group_add(struct group_ctx *gc, struct client_ctx *cc)
{
	if (cc == NULL || gc == NULL)
		errx(1, "group_add: a ctx is NULL");

	if (cc->group == gc)
		return;

	if (cc->group != NULL)
		TAILQ_REMOVE(&cc->group->clients, cc, group_entry);

	XChangeProperty(X_Dpy, cc->win, _CWM_GRP, XA_STRING,
	    8, PropModeReplace, gc->name, strlen(gc->name));

	TAILQ_INSERT_TAIL(&gc->clients, cc, group_entry);
	cc->group = gc;
}

static void
group_remove(struct client_ctx *cc)
{
	if (cc == NULL || cc->group == NULL)
		errx(1, "group_remove: a ctx is NULL");

	XChangeProperty(X_Dpy, cc->win, _CWM_GRP, XA_STRING, 8,
	    PropModeReplace, shortcut_to_name[0],
	    strlen(shortcut_to_name[0]));

	TAILQ_REMOVE(&cc->group->clients, cc, group_entry);
	cc->group = NULL;
}

static void
group_hide(struct screen_ctx *sc, struct group_ctx *gc)
{
	struct client_ctx	*cc;

	screen_updatestackingorder(sc);

	gc->nhidden = 0;
	gc->highstack = 0;
	TAILQ_FOREACH(cc, &gc->clients, group_entry) {
		client_hide(cc);
		gc->nhidden++;
		if (cc->stackingorder > gc->highstack)
			gc->highstack = cc->stackingorder;
	}
	gc->hidden = 1;		/* XXX: equivalent to gc->nhidden > 0 */
}

static void
group_show(struct screen_ctx *sc, struct group_ctx *gc)
{
	struct client_ctx	*cc;
	Window			*winlist;
	u_int			 i;
	int			 lastempty = -1;

	winlist = (Window *) xcalloc(sizeof(*winlist), (gc->highstack + 1));

	/*
	 * Invert the stacking order as XRestackWindows() expects them
	 * top-to-bottom.
	 */
	TAILQ_FOREACH(cc, &gc->clients, group_entry) {
		winlist[gc->highstack - cc->stackingorder] = cc->win;
		client_unhide(cc);
	}

	/* Un-sparseify */
	for (i = 0; i <= gc->highstack; i++) {
		if (!winlist[i] && lastempty == -1)
			lastempty = i;
		else if (winlist[i] && lastempty != -1) {
			winlist[lastempty] = winlist[i];
			if (++lastempty == i)
				lastempty = -1;
		}
	}

	XRestackWindows(X_Dpy, winlist, gc->nhidden);
	xfree(winlist);

	gc->hidden = 0;
	sc->group_active = gc;
}

void
group_init(struct screen_ctx *sc)
{
	int	 i;

	TAILQ_INIT(&sc->groupq);
	sc->group_hideall = 0;
	sc->group_active = NULL;

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		TAILQ_INIT(&sc->groups[i].clients);
		sc->groups[i].hidden = 0;
		sc->groups[i].shortcut = i + 1;
		sc->groups[i].name = shortcut_to_name[sc->groups[i].shortcut];
		TAILQ_INSERT_TAIL(&sc->groupq, &sc->groups[i], entry);
	}

	sc->group_active = &sc->groups[0];
}

void
group_movetogroup(struct client_ctx *cc, int idx)
{
	struct screen_ctx	*sc = cc->sc;

	if (idx < 0 || idx >= CALMWM_NGROUPS)
		err(1, "group_movetogroup: index out of range (%d)", idx);

	if(sc->group_active != &sc->groups[idx])
		client_hide(cc);
	group_add(&sc->groups[idx], cc);
}

/*
 * Colouring for groups upon add/remove.
 */
void
group_sticky_toggle_enter(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	struct group_ctx	*gc;

	gc = sc->group_active;

	if (gc == cc->group) {
		group_remove(cc);
		cc->highlight = CLIENT_HIGHLIGHT_UNGROUP;
	} else {
		group_add(gc, cc);
		cc->highlight = CLIENT_HIGHLIGHT_GROUP;
	}

	client_draw_border(cc);
}

void
group_sticky_toggle_exit(struct client_ctx *cc)
{
	cc->highlight = 0;
	client_draw_border(cc);
}

/*
 * if group_hidetoggle would produce no effect, toggle the group's hidden state
 */
static void
group_fix_hidden_state(struct group_ctx *gc)
{
	struct client_ctx	*cc;
	int			 same = 0;

	TAILQ_FOREACH(cc, &gc->clients, group_entry) {
		if (gc->hidden == ((cc->flags & CLIENT_HIDDEN) ? 1 : 0))
			same++;
	}

	if (same == 0)
		gc->hidden = !gc->hidden;
}

void
group_hidetoggle(struct screen_ctx *sc, int idx)
{
	struct group_ctx	*gc;

	if (idx < 0 || idx >= CALMWM_NGROUPS)
		err(1, "group_hidetoggle: index out of range (%d)", idx);

	gc = &sc->groups[idx];

	group_fix_hidden_state(gc);

	if (gc->hidden)
		group_show(sc, gc);
	else {
		group_hide(sc, gc);
		/* XXX wtf? */
		if (TAILQ_EMPTY(&gc->clients))
			sc->group_active = gc;
	}
}

void
group_only(struct screen_ctx *sc, int idx)
{
	int	 i;

	if (idx < 0 || idx >= CALMWM_NGROUPS)
		err(1, "group_only: index out of range (%d)", idx);

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		if (i == idx)
			group_show(sc, &sc->groups[i]);
		else
			group_hide(sc, &sc->groups[i]);
	}
}

/*
 * Cycle through active groups.  If none exist, then just stay put.
 */
void
group_cycle(struct screen_ctx *sc, int reverse)
{
	struct group_ctx	*gc, *showgroup = NULL;

	assert(sc->group_active != NULL);

	gc = sc->group_active;
	for (;;) {
		gc = reverse ? TAILQ_PREV(gc, group_ctx_q, entry) :
		    TAILQ_NEXT(gc, entry);
		if (gc == NULL)
			gc = reverse ? TAILQ_LAST(&sc->groupq, group_ctx_q) :
			    TAILQ_FIRST(&sc->groupq);
		if (gc == sc->group_active)
			break;

		if (!TAILQ_EMPTY(&gc->clients) && showgroup == NULL)
			showgroup = gc;
		else if (!gc->hidden)
			group_hide(sc, gc);
	}

	if (showgroup == NULL)
		return;

	group_hide(sc, sc->group_active);

	if (showgroup->hidden)
		group_show(sc, showgroup);
	else
		sc->group_active = showgroup;
}

/* called when a client is deleted */
void
group_client_delete(struct client_ctx *cc)
{
	if (cc->group == NULL)
		return;

	TAILQ_REMOVE(&cc->group->clients, cc, group_entry);
	cc->group = NULL; /* he he */
}

void
group_menu(XButtonEvent *e)
{
	struct screen_ctx	*sc;
	struct group_ctx	*gc;
	struct menu		*mi;
	struct menu_q		 menuq;
	int			 i;

	sc = screen_fromroot(e->root);
	TAILQ_INIT(&menuq);

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		gc = &sc->groups[i];

		if (TAILQ_EMPTY(&gc->clients))
			continue;

		mi = xcalloc(1, sizeof(*mi));
		if (gc->hidden)
			snprintf(mi->text, sizeof(mi->text), "%d: [%s]",
			    gc->shortcut, shortcut_to_name[gc->shortcut]);
		else
			snprintf(mi->text, sizeof(mi->text), "%d: %s",
			    gc->shortcut, shortcut_to_name[gc->shortcut]);
		mi->ctx = gc;
		TAILQ_INSERT_TAIL(&menuq, mi, entry);
	}

	if (TAILQ_EMPTY(&menuq))
		return;

	mi = menu_filter(sc, &menuq, NULL, NULL, 0, NULL, NULL);

	if (mi == NULL || mi->ctx == NULL)
		goto cleanup;

	gc = (struct group_ctx *)mi->ctx;

	(gc->hidden) ? group_show(sc, gc) : group_hide(sc, gc);

cleanup:
	while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
		TAILQ_REMOVE(&menuq, mi, entry);
		xfree(mi);
	}
}

void
group_alltoggle(struct screen_ctx *sc)
{
	int	 i;

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		if (sc->group_hideall)
			group_show(sc, &sc->groups[i]);
		else
			group_hide(sc, &sc->groups[i]);
	}

	sc->group_hideall = (!sc->group_hideall);
}

void
group_autogroup(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	struct autogroupwin	*aw;
	struct group_ctx	*gc;
	unsigned char		*grpstr = NULL;
	char			 group[CALMWM_MAXNAMELEN];

	if (cc->app_class == NULL || cc->app_name == NULL)
		return;
	if (xu_getprop(cc, _CWM_GRP,  XA_STRING,
	    (CALMWM_MAXNAMELEN - 1)/sizeof(long), &grpstr) > 0) {
		strlcpy(group, grpstr, sizeof(group));
		XFree(grpstr);
	} else {
		TAILQ_FOREACH(aw, &Conf.autogroupq, entry) {
			if (strcmp(aw->class, cc->app_class) == 0 &&
			    (aw->name == NULL ||
			    strcmp(aw->name, cc->app_name) == 0)) {
				strlcpy(group, aw->group, sizeof(group));
				break;
			}
		}
	}

	if (strncmp("nogroup", group, 7) == 0)
		return;

	TAILQ_FOREACH(gc, &sc->groupq, entry) {
		if (strcmp(shortcut_to_name[gc->shortcut], group) == 0) {
			group_add(gc, cc);
			return;
		}
	}

	if (Conf.flags & CONF_STICKY_GROUPS)
		group_add(sc->group_active, cc);

}

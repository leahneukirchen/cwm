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

#define CALMWM_NGROUPS 9

static void		 group_add(struct group_ctx *, struct client_ctx *);
static void		 group_remove(struct client_ctx *);
static void		 group_hide(struct group_ctx *);
static void		 group_show(struct group_ctx *);
static void		 group_fix_hidden_state(struct group_ctx *);

struct group_ctx	*Group_active = NULL;
struct group_ctx	 Groups[CALMWM_NGROUPS];
int			 Grouphideall = 0;
struct group_ctx_q	 Groupq;

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
group_hide(struct group_ctx *gc)
{
	struct client_ctx	*cc;

	screen_updatestackingorder();

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
group_show(struct group_ctx *gc)
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
	Group_active = gc;
}

void
group_init(void)
{
	int	 i;

	TAILQ_INIT(&Groupq);

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		TAILQ_INIT(&Groups[i].clients);
		Groups[i].hidden = 0;
		Groups[i].shortcut = i + 1;
		Groups[i].name = shortcut_to_name[Groups[i].shortcut];
		TAILQ_INSERT_TAIL(&Groupq, &Groups[i], entry);
	}

	Group_active = &Groups[0];
}

void
group_movetogroup(struct client_ctx *cc, int idx)
{
	if (idx < 0 || idx >= CALMWM_NGROUPS)
		err(1, "group_movetogroup: index out of range (%d)", idx);

	if(Group_active != &Groups[idx])
		client_hide(cc);
	group_add(&Groups[idx], cc);
}

/*
 * Colouring for groups upon add/remove.
 */
void
group_sticky_toggle_enter(struct client_ctx *cc)
{
	struct group_ctx	*gc;

	gc = Group_active;

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
group_hidetoggle(int idx)
{
	struct group_ctx	*gc;

	if (idx < 0 || idx >= CALMWM_NGROUPS)
		err(1, "group_hidetoggle: index out of range (%d)", idx);

	gc = &Groups[idx];

	group_fix_hidden_state(gc);

	if (gc->hidden)
		group_show(gc);
	else {
		group_hide(gc);
		if (TAILQ_EMPTY(&gc->clients))
			Group_active = gc;
	}
}

void
group_only(int idx)
{
	int	 i;

	if (idx < 0 || idx >= CALMWM_NGROUPS)
		err(1, "group_only: index out of range (%d)", idx);

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		if (i == idx)
			group_show(&Groups[i]);
		else
			group_hide(&Groups[i]);
	}
}

/*
 * Cycle through active groups.  If none exist, then just stay put.
 */
void
group_cycle(int reverse)
{
	struct group_ctx	*gc, *showgroup = NULL;

	assert(Group_active != NULL);

	gc = Group_active;
	for (;;) {
		gc = reverse ? TAILQ_PREV(gc, group_ctx_q, entry) :
		    TAILQ_NEXT(gc, entry);
		if (gc == NULL)
			gc = reverse ? TAILQ_LAST(&Groupq, group_ctx_q) :
			    TAILQ_FIRST(&Groupq);
		if (gc == Group_active)
			break;

		if (!TAILQ_EMPTY(&gc->clients) && showgroup == NULL)
			showgroup = gc;
		else if (!gc->hidden)
			group_hide(gc);
	}

	if (showgroup == NULL)
		return;

	group_hide(Group_active);

	if (showgroup->hidden)
		group_show(showgroup);
	else
		Group_active = showgroup;
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
	struct group_ctx	*gc;
	struct menu		*mi;
	struct menu_q		 menuq;
	int			 i;

	TAILQ_INIT(&menuq);

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		gc = &Groups[i];

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

	mi = menu_filter(&menuq, NULL, NULL, 0, NULL, NULL);

	if (mi == NULL || mi->ctx == NULL)
		goto cleanup;

	gc = (struct group_ctx *)mi->ctx;

	(gc->hidden) ? group_show(gc) : group_hide(gc);

cleanup:
	while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
		TAILQ_REMOVE(&menuq, mi, entry);
		xfree(mi);
	}
}

void
group_alltoggle(void)
{
	int	 i;

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		if (Grouphideall)
			group_show(&Groups[i]);
		else
			group_hide(&Groups[i]);
	}

	Grouphideall = (Grouphideall) ? 0 : 1;
}

void
group_autogroup(struct client_ctx *cc)
{
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

	TAILQ_FOREACH(gc, &Groupq, entry) {
		if (strcmp(shortcut_to_name[gc->shortcut], group) == 0) {
			group_add(gc, cc);
			return;
		}
	}

	if (Conf.flags & CONF_STICKY_GROUPS)
		group_add(Group_active, cc);

}

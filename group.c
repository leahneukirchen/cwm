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

int                 Groupmode = 0;
int                 Groupnamemode = 0;
struct group_ctx   *Group_active = NULL;
struct group_ctx   *Group_current = NULL;
struct group_ctx    Groups[CALMWM_NGROUPS];
char                Group_name[256];
int                 Groupfocusset = 0;
Window              Groupfocuswin;
int                 Groupfocusrevert;
int                 Grouphideall = 0;
struct group_ctx_q  Groupq;

#define GroupMask (KeyPressMask|ExposureMask)

static char *shortcut_to_name[] = {
	"XXX", "one", "two", "three",
	"four", "five", "six", "seven",
	"eight", "nine",
};

static void
_group_add(struct group_ctx *gc, struct client_ctx *cc)
{
	if (cc == NULL || gc == NULL)
	  	errx(1, "_group_add: a ctx is NULL");

	if (cc->group == gc)
		return;

	if (cc->group != NULL)
		TAILQ_REMOVE(&cc->group->clients, cc, group_entry);

	TAILQ_INSERT_TAIL(&gc->clients, cc, group_entry);
	cc->group = gc;
	cc->groupcommit = 0;
}

static void
_group_remove(struct client_ctx *cc)
{
	if (cc == NULL || cc->group == NULL)
	  	errx(1, "_group_remove: a ctx is NULL");

	TAILQ_REMOVE(&cc->group->clients, cc, group_entry);
	cc->group = NULL;
	cc->groupcommit = 0;
	cc->highlight = 0;
	client_draw_border(cc);
}

static void
_group_commit(struct group_ctx *gc)
{
  	struct client_ctx *cc;

	if (gc == NULL)
		errx(1, "_group_commit: ctx is null");

	TAILQ_FOREACH(cc, &gc->clients, group_entry)
		cc->groupcommit = 1;
}

static void
_group_purge(struct group_ctx *gc)
{
	struct client_ctx *cc;

	if (gc == NULL)
		errx(1, "_group_commit: ctx is null");

	TAILQ_FOREACH(cc, &gc->clients, group_entry)
		if (cc->groupcommit == 0)
			_group_remove(cc);
}


static void
_group_hide(struct group_ctx *gc)
{
  	struct client_ctx *cc;

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
_group_show(struct group_ctx *gc)
{
	struct client_ctx *cc;
	Window *winlist;
	u_int i;
	int lastempty = -1;

	winlist = (Window *) xcalloc(sizeof(*winlist) * (gc->highstack + 1));

	/*
	 * Invert the stacking order as XRestackWindows() expects them
	 * top-to-bottom.
	 */
	TAILQ_FOREACH(cc, &gc->clients, group_entry) {
		winlist[gc->highstack - cc->stackingorder] = cc->pwin;
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



static void
_group_destroy(struct group_ctx *gc)
{
	struct client_ctx *cc;

	if (gc->name != NULL) {
		xfree(gc->name);
		gc->name = NULL;
	}

	while ((cc = TAILQ_FIRST(&gc->clients)) != NULL) {
		TAILQ_REMOVE(&gc->clients, cc, group_entry);
		cc->group = NULL;
		cc->groupcommit = 0;
		cc->highlight = 0;
		client_draw_border(cc);
	}
}

void
group_init(void)
{
  	int i;

	TAILQ_INIT(&Groupq);

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		TAILQ_INIT(&Groups[i].clients);
		Groups[i].hidden = 0;
		Groups[i].shortcut = i + 1;
		TAILQ_INSERT_TAIL(&Groupq, &Groups[i], entry);
	}

	Group_current = Group_active = &Groups[0];
}

/* 
 * manipulate the 'current group'
 */

#if 0
/* set current group to the first empty group
 * returns 0 on success, -1 if there are no empty groups
 */
int
group_new(void)
{
  	int i;

	for (i=0; i < CALMWM_NGROUPS; i++) {
		if (TAILQ_EMPTY(&Groups[i].clients)) {
			Group_current = &Groups[i];			

			return (0);
		}
	}

	return (-1);
}
#endif

/* change the current group */
int
group_select(int idx)
{
	struct group_ctx *gc = Group_current;
	struct client_ctx *cc;

	if (idx < 0 || idx >= CALMWM_NGROUPS)
		return (-1);

	TAILQ_FOREACH(cc, &gc->clients, group_entry) {
		cc->highlight = 0;
		client_draw_border(cc);
	}

	_group_commit(gc);
	Group_current = &Groups[idx];

	group_display_draw(screen_current());
	return (0);
}

/* enter group mode */
void
group_enter(void)
{
  	if (Groupmode != 0)
		errx(1, "group_enter called twice");

	if (Group_current == NULL)
		Group_current = &Groups[0];

	/* setup input buffer */
	Group_name[0] = '\0';

  	Groupmode = 1;

	group_display_init(screen_current());
	group_display_draw(screen_current());
}

/* exit group mode */
void
group_exit(int commit)
{
	struct group_ctx *gc = Group_current;
	struct client_ctx *cc;

  	if (Groupmode != 1)
		errx(1, "group_exit called twice");

	TAILQ_FOREACH(cc, &gc->clients, group_entry) {
		cc->highlight = 0;
		client_draw_border(cc);
	}

	if (commit) {
		_group_commit(gc);
	} else {
	  	/* abort */
	  	_group_purge(gc);
		if (!TAILQ_EMPTY(&gc->clients))
	  		_group_destroy(gc);
	}

	XUnmapWindow(X_Dpy, screen_current()->groupwin);

	if (Groupnamemode) {
		XSetInputFocus(X_Dpy, Groupfocuswin, Groupfocusrevert,
		    CurrentTime);
		Groupfocusset = 0;
	}

  	Groupmode = Groupnamemode = 0;
}

void
group_click(struct client_ctx *cc)
{
	struct group_ctx *gc = Group_current;

	if (gc == cc->group)
		_group_remove(cc);
	else 
		_group_add(gc, cc);
	group_display_draw(screen_current());
}


/* Used to add a newly mapped window to the active group */

void
group_sticky(struct client_ctx *cc)
{
	_group_add(Group_active, cc);
}

void
group_sticky_toggle_enter(struct client_ctx *cc)
{
	struct group_ctx *gc = Group_active;

	if (gc == cc->group) {
		_group_remove(cc);
		cc->highlight = CLIENT_HIGHLIGHT_RED;
	} else {
		_group_add(gc, cc);
		cc->highlight = CLIENT_HIGHLIGHT_BLUE;
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
 * selection list display 
 */

void
group_display_init(struct screen_ctx *sc)
{
	sc->groupwin = XCreateSimpleWindow(X_Dpy, sc->rootwin, 0, 0,
	    1, 1, 1, sc->blackpixl, sc->whitepixl);
}

void
group_display_draw(struct screen_ctx *sc)
{
	struct group_ctx *gc = Group_current;
	int x, y, dx, dy, fontheight, titlelen;
	struct client_ctx *cc;
	char titlebuf[1024];
	struct fontdesc *font = DefaultFont;

	snprintf(titlebuf, sizeof(titlebuf), "Editing group %d", gc->shortcut);

	x = y = 0;

	fontheight = font_ascent(font) + font_descent(font) + 1;
	dx = titlelen = font_width(font, titlebuf, strlen(titlebuf));
	dy = fontheight;

	TAILQ_FOREACH(cc, &gc->clients, group_entry) {
		cc->highlight = CLIENT_HIGHLIGHT_BLUE;
		client_draw_border(cc);
	}

	XMoveResizeWindow(X_Dpy, sc->groupwin, x, y, dx, dy);

	/* XXX */
	XSelectInput(X_Dpy, sc->groupwin, GroupMask);

	XMapRaised(X_Dpy, sc->groupwin);
	XClearWindow(X_Dpy, sc->groupwin);
	font_draw(font, titlebuf, strlen(titlebuf), sc->groupwin,
	    0, font_ascent(font) + 1);
}

void 
group_display_keypress(KeyCode k)
{
	struct group_ctx * gc = Group_current;
	char chr;
	enum ctltype ctl;
	int len;

	if (!Groupnamemode)
		return;

	if (input_keycodetrans(k, 0, &ctl, &chr, 1) < 0)
		goto out;

	switch (ctl) {
	case CTL_ERASEONE:
	  	if ((len = strlen(Group_name)) > 0)
			Group_name[len - 1] = '\0';
		break;
	case CTL_RETURN:
		if (gc->name != NULL)
			xfree(gc->name);

		gc->name = xstrdup(Group_name);

		group_exit(1);
		return;
	default:
		break;
	}

	if (chr != '\0')
		snprintf(Group_name, sizeof(Group_name), "%s%c", 
		    Group_name, chr);

out:
	group_display_draw(screen_current());
}

/* if group_hidetoggle would produce no effect, toggle the group's hidden state
 */
void
_group_fix_hidden_state(struct group_ctx *gc)
{
	struct client_ctx *cc;	
	int same = 0;

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
	struct group_ctx *gc;
#ifdef notyet
	char buf[128];
#endif

	if (idx < 0 || idx >= CALMWM_NGROUPS)
		err(1, "group_hidetoggle: index out of range (%d)", idx);

	gc = &Groups[idx];

	_group_fix_hidden_state(gc);

	if (gc->hidden)
		_group_show(gc);
	else {
		_group_hide(gc);
		if (TAILQ_EMPTY(&gc->clients))
			Group_active = gc;
	}

#ifdef notyet
	snprintf(buf, sizeof(buf), "Group %d", idx + 1);
	screen_infomsg(buf);
#endif
}

#define GROUP_NEXT(gc, fwd) (fwd) ?					\
	TAILQ_NEXT(gc, entry) : TAILQ_PREV(gc, group_ctx_q, entry)

/*
 * Jump to the next/previous active group.  If none exist, then just
 * stay put.
 */
void
group_slide(int fwd)
{
	struct group_ctx *gc, *showgroup = NULL;

	assert(Group_active != NULL);

	gc = Group_active;
	for (;;) {
		gc = GROUP_NEXT(gc, fwd);
		if (gc == NULL)
			gc = fwd ? TAILQ_FIRST(&Groupq) :
			    TAILQ_LAST(&Groupq, group_ctx_q);
		if (gc == Group_active)
			break;

		if (!TAILQ_EMPTY(&gc->clients) && showgroup == NULL)
			showgroup = gc;
		else if (!gc->hidden)
			_group_hide(gc);
	}

	if (showgroup == NULL)
		return;

	_group_hide(Group_active);

	if (showgroup->hidden)
		_group_show(showgroup);
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
	cc->groupcommit = 0;
}

void
group_menu(XButtonEvent *e)
{
  	struct menu_q menuq;
	struct menu  *mi;
	int i;
	struct group_ctx *gc;

	TAILQ_INIT(&menuq);

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		gc = &Groups[i];

		if (TAILQ_EMPTY(&gc->clients))
			continue;

		if (gc->name == NULL)
			gc->name = xstrdup(shortcut_to_name[gc->shortcut]);

		XCALLOC(mi, struct menu);
		if (gc->hidden)
			snprintf(mi->text, sizeof(mi->text), "%d: [%s]",
			   gc->shortcut, gc->name); 
		else
			snprintf(mi->text, sizeof(mi->text), "%d: %s",
			   gc->shortcut, gc->name); 
		mi->ctx = gc;
		TAILQ_INSERT_TAIL(&menuq, mi, entry);
	}

	if (TAILQ_EMPTY(&menuq))
		return;

	mi = (struct menu *)grab_menu(e, &menuq);

	if (mi == NULL || mi->ctx == NULL)
		goto cleanup;

	gc = (struct group_ctx *)mi->ctx;

	if (gc->hidden)
		_group_show(gc);
	else
		_group_hide(gc);

 cleanup:
	while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
		TAILQ_REMOVE(&menuq, mi, entry);
		xfree(mi);
	}
}

void
group_namemode(void)
{
	Groupnamemode = 1;

	group_display_draw(screen_current());
}

void
group_alltoggle(void)
{
	int i;

	for (i=0; i < CALMWM_NGROUPS; i++) {
		if (Grouphideall)
			_group_show(&Groups[i]);
		else
		  	_group_hide(&Groups[i]);
	}

	if (Grouphideall)
		Grouphideall = 0;
	else
		Grouphideall = 1;
}

void
group_deletecurrent(void)
{
	_group_destroy(Group_current);
	XUnmapWindow(X_Dpy, screen_current()->groupwin);

  	Groupmode = Groupnamemode = 0;
}

void
group_done(void)
{
	struct group_ctx *gc = Group_current;

	if (gc->name != NULL)
		xfree(gc->name);

	gc->name = xstrdup(shortcut_to_name[gc->shortcut]);

	group_exit(1);
}

void
group_autogroup(struct client_ctx *cc)
{
	struct autogroupwin *aw;
	struct group_ctx *gc;
	char group[CALMWM_MAXNAMELEN];

	if (cc->app_class == NULL || cc->app_name == NULL)
		return;

	TAILQ_FOREACH(aw, &Conf.autogroupq, entry) {
		if (strcmp(aw->class, cc->app_class) == 0 &&
		    (aw->name == NULL || strcmp(aw->name, cc->app_name) == 0)) {
			strlcpy(group, aw->group, sizeof(group));
			break;
		}
	}

	TAILQ_FOREACH(gc, &Groupq, entry) {
		if (strcmp(shortcut_to_name[gc->shortcut], group) == 0)
			_group_add(gc, cc);
	}

}

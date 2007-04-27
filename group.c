/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2004 Andy Adamson <dros@monkey.org>
 * Copyright (c) 2004,2005 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id$
 */

#include "headers.h"
#include "calmwm.h"

#define CALMWM_NGROUPS 9

int                 G_groupmode = 0;
int                 G_groupnamemode = 0;
struct group_ctx   *G_group_active = NULL;
struct group_ctx   *G_group_current = NULL;
struct group_ctx    G_groups[CALMWM_NGROUPS];
char                G_group_name[256];
int                 G_groupfocusset = 0;
Window              G_groupfocuswin;
int                 G_groupfocusrevert;
int                 G_grouphideall = 0;
struct group_ctx_q  G_groupq;

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

	XRestackWindows(G_dpy, winlist, gc->nhidden);
	xfree(winlist);

	gc->hidden = 0;
	G_group_active = gc;
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

	TAILQ_INIT(&G_groupq);

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		TAILQ_INIT(&G_groups[i].clients);
		G_groups[i].hidden = 0;
		G_groups[i].shortcut = i + 1;
		TAILQ_INSERT_TAIL(&G_groupq, &G_groups[i], entry);
	}

	G_group_current = G_group_active = &G_groups[0];
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
		if (TAILQ_EMPTY(&G_groups[i].clients)) {
			G_group_current = &G_groups[i];			

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
	struct group_ctx *gc = G_group_current;
	struct client_ctx *cc;

	if (idx < 0 || idx >= CALMWM_NGROUPS)
		return (-1);

	TAILQ_FOREACH(cc, &gc->clients, group_entry) {
		cc->highlight = 0;
		client_draw_border(cc);
	}

	_group_commit(gc);
	G_group_current = &G_groups[idx];

	group_display_draw(screen_current());
	return (0);
}

/* enter group mode */
void
group_enter(void)
{
  	if (G_groupmode != 0)
		errx(1, "group_enter called twice");

	if (G_group_current == NULL)
		G_group_current = &G_groups[0];

	/* setup input buffer */
	G_group_name[0] = '\0';

  	G_groupmode = 1;

	group_display_init(screen_current());
	group_display_draw(screen_current());
}

/* exit group mode */
void
group_exit(int commit)
{
	struct group_ctx *gc = G_group_current;
	struct client_ctx *cc;

  	if (G_groupmode != 1)
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

	XUnmapWindow(G_dpy, screen_current()->groupwin);

	if (G_groupnamemode) {
		XSetInputFocus(G_dpy, G_groupfocuswin, G_groupfocusrevert,
		    CurrentTime);
		G_groupfocusset = 0;
	}

  	G_groupmode = G_groupnamemode = 0;
}

void
group_click(struct client_ctx *cc)
{
	struct group_ctx *gc = G_group_current;

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
	_group_add(G_group_active, cc);
}

void
group_sticky_toggle_enter(struct client_ctx *cc)
{
	struct group_ctx *gc = G_group_active;

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
	sc->groupwin = XCreateSimpleWindow(G_dpy, sc->rootwin, 0, 0,
	    1, 1, 1, sc->blackpixl, sc->whitepixl);
}

void
group_display_draw(struct screen_ctx *sc)
{
	struct group_ctx *gc = G_group_current;
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

	XMoveResizeWindow(G_dpy, sc->groupwin, x, y, dx, dy);

	/* XXX */
	XSelectInput(G_dpy, sc->groupwin, GroupMask);

	XMapRaised(G_dpy, sc->groupwin);
	XClearWindow(G_dpy, sc->groupwin);
	font_draw(font, titlebuf, strlen(titlebuf), sc->groupwin,
	    0, font_ascent(font) + 1);
}

void 
group_display_keypress(KeyCode k)
{
	struct group_ctx * gc = G_group_current;
	char chr;
	enum ctltype ctl;
	int len;

	if (!G_groupnamemode)
		return;

	if (input_keycodetrans(k, 0, &ctl, &chr, 1) < 0)
		goto out;

	switch (ctl) {
	case CTL_ERASEONE:
	  	if ((len = strlen(G_group_name)) > 0)
			G_group_name[len - 1] = '\0';
		break;
	case CTL_RETURN:
		if (gc->name != NULL)
			xfree(gc->name);

		gc->name = xstrdup(G_group_name);

		group_exit(1);
		return;
	default:
		break;
	}

	if (chr != '\0')
		snprintf(G_group_name, sizeof(G_group_name), "%s%c", 
		    G_group_name, chr);

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

	gc = &G_groups[idx];

	_group_fix_hidden_state(gc);

	if (gc->hidden)
		_group_show(gc);
	else {
		_group_hide(gc);
		if (TAILQ_EMPTY(&gc->clients))
			G_group_active = gc;
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

	assert(G_group_active != NULL);

	gc = G_group_active;
	for (;;) {
		gc = GROUP_NEXT(gc, fwd);
		if (gc == NULL)
			gc = fwd ? TAILQ_FIRST(&G_groupq) :
			    TAILQ_LAST(&G_groupq, group_ctx_q);
		if (gc == G_group_active)
			break;

		if (!TAILQ_EMPTY(&gc->clients) && showgroup == NULL)
			showgroup = gc;
		else if (!gc->hidden)
			_group_hide(gc);
	}

	if (showgroup == NULL)
		return;

	_group_hide(G_group_active);

	if (showgroup->hidden)
		_group_show(showgroup);
	else
		G_group_active = showgroup;
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
		gc = &G_groups[i];

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
	G_groupnamemode = 1;

	group_display_draw(screen_current());
}

void
group_alltoggle(void)
{
	int i;

	for (i=0; i < CALMWM_NGROUPS; i++) {
		if (G_grouphideall)
			_group_show(&G_groups[i]);
		else
		  	_group_hide(&G_groups[i]);
	}

	if (G_grouphideall)
		G_grouphideall = 0;
	else
		G_grouphideall = 1;
}

void
group_deletecurrent(void)
{
	_group_destroy(G_group_current);
	XUnmapWindow(G_dpy, screen_current()->groupwin);

  	G_groupmode = G_groupnamemode = 0;
}

void
group_done(void)
{
	struct group_ctx *gc = G_group_current;

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

	TAILQ_FOREACH(aw, &G_conf.autogroupq, entry) {
		if (strcmp(aw->class, cc->app_class) == 0 &&
		    (aw->name == NULL || strcmp(aw->name, cc->app_name) == 0)) {
			strlcpy(group, aw->group, sizeof(group));
			break;
		}
	}

	TAILQ_FOREACH(gc, &G_groupq, entry) {
		if (strcmp(shortcut_to_name[gc->shortcut], group) == 0)
			_group_add(gc, cc);
	}

}

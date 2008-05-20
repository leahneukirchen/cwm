/*
 * Copyright (c) 2008 Owain G. Ainsworth <oga@openbsd.org>
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
 */

#include "headers.h"
#include "calmwm.h"

#define KeyMask (KeyPressMask|ExposureMask)

struct menu_ctx {
	char			 searchstr[MENU_MAXENTRY + 1];
	char			 dispstr[MENU_MAXENTRY*2 + 1];
	char			 promptstr[MENU_MAXENTRY + 1];
	int			 list;
	int			 listing;
	int			 changed;
	int			 noresult;
	int			 x;
	int			 y; /* location */
    	void (*match)(struct menu_q *, struct menu_q *, char *);
    	void (*print)(struct menu *, int);
};
static struct menu	*menu_handle_key(XEvent *, struct menu_ctx *,
			     struct menu_q *, struct menu_q *);
static void		 menu_draw(struct screen_ctx *, struct menu_ctx *,
			     struct menu_q *, struct menu_q *);

struct menu *
menu_filter(struct menu_q *menuq, char *prompt, char *initial, int dummy,
    void (*match)(struct menu_q *, struct menu_q *, char *),
    void (*print)(struct menu *, int))
{
	struct screen_ctx	*sc = screen_current();
	struct menu_ctx		 mc;
	struct menu_q		 resultq;
	struct menu		*mi = NULL;
	XEvent			 e;
	Window			 focuswin;
	int			 dx, dy, focusrevert;
	char			 endchar = '«';
	struct fontdesc		*font = DefaultFont;

	TAILQ_INIT(&resultq);

	bzero(&mc, sizeof(mc));

	xu_ptr_getpos(sc->rootwin, &mc.x, &mc.y);

	if (prompt == NULL)
		prompt = "search";

	if (initial != NULL)
		strlcpy(mc.searchstr, initial, sizeof(mc.searchstr));
	else
		mc.searchstr[0] = '\0';

	mc.match = match;
	mc.print = print;

	snprintf(mc.promptstr, sizeof(mc.promptstr), "%s»", prompt);
	snprintf(mc.dispstr, sizeof(mc.dispstr), "%s%s%c", mc.promptstr,
	    mc.searchstr, endchar);
	dx = font_width(font, mc.dispstr, strlen(mc.dispstr));
	dy = sc->fontheight;

	XMoveResizeWindow(X_Dpy, sc->menuwin, mc.x, mc.y, dx, dy);
	XSelectInput(X_Dpy, sc->menuwin, KeyMask);
	XMapRaised(X_Dpy, sc->menuwin);

	if (xu_ptr_grab(sc->menuwin, 0, Cursor_question) < 0) {
		XUnmapWindow(X_Dpy, sc->menuwin);
		return (NULL);
	}

	XGetInputFocus(X_Dpy, &focuswin, &focusrevert);
	XSetInputFocus(X_Dpy, sc->menuwin, RevertToPointerRoot, CurrentTime);

	for (;;) {
		mc.changed = 0;

		XWindowEvent(X_Dpy, sc->menuwin, KeyMask, &e);

		switch (e.type) {
		case KeyPress:
			if ((mi = menu_handle_key(&e, &mc, menuq, &resultq))
			    != NULL)
				goto out;
			/* FALLTHROUGH */
		case Expose:
			menu_draw(sc, &mc, menuq, &resultq);
			break;
		}
	}
out:
	if (dummy == 0 && mi->dummy) { /* no match */
		xfree (mi);
		mi = NULL;
		xu_ptr_ungrab();
		XSetInputFocus(X_Dpy, focuswin, focusrevert, CurrentTime);
	}

	XUnmapWindow(X_Dpy, sc->menuwin);

	return (mi);
}

static struct menu *
menu_handle_key(XEvent *e, struct menu_ctx *mc, struct menu_q *menuq,
    struct menu_q *resultq)
{
	struct menu	*mi;
	enum ctltype	 ctl;
	char		 chr;
	size_t		 len;

	if (input_keycodetrans(e->xkey.keycode, e->xkey.state,
	    &ctl, &chr) < 0)
		return (NULL);

	switch (ctl) {
	case CTL_ERASEONE:
		if ((len = strlen(mc->searchstr)) > 0) {
			mc->searchstr[len - 1] = '\0';
			mc->changed = 1;
		}
		break;
	case CTL_UP:
		mi = TAILQ_LAST(resultq, menu_q);
		if (mi == NULL)
			break;

		TAILQ_REMOVE(resultq, mi, resultentry);
		TAILQ_INSERT_HEAD(resultq, mi, resultentry);
		break;
	case CTL_DOWN:
		mi = TAILQ_FIRST(resultq);
		if (mi == NULL)
			break;

		TAILQ_REMOVE(resultq, mi, resultentry);
		TAILQ_INSERT_TAIL(resultq, mi, resultentry);
		break;
	case CTL_RETURN:
		/*
		 * Return whatever the cursor is currently on. Else
		 * even if dummy is zero, we need to return something.
		 */
		if ((mi = TAILQ_FIRST(resultq)) == NULL) {
			mi = xmalloc(sizeof *mi);
			(void)strlcpy(mi->text,
			    mc->searchstr, sizeof(mi->text));
			mi->dummy = 1;
		}
		return (mi);
	case CTL_WIPE:
		mc->searchstr[0] = '\0';
		mc->changed = 1;
		break;
	case CTL_ALL:
		mc->list = !mc->list;
		break;
	case CTL_ABORT:
		mi = xmalloc(sizeof *mi);
		mi->text[0] = '\0';
		mi->dummy = 1;
		return (mi);
	default:
		break;
	}

	if (chr != '\0') {
		char str[2];

		str[0] = chr;
		str[1] = '\0';
		mc->changed = 1;
		strlcat(mc->searchstr, str, sizeof(mc->searchstr));
	}

	mc->noresult = 0;
	if (mc->changed && strlen(mc->searchstr) > 0) {
		(*mc->match)(menuq, resultq, mc->searchstr);
		/* If menuq is empty, never show we've failed */
		mc->noresult = TAILQ_EMPTY(resultq) && !TAILQ_EMPTY(menuq);
	} else if (mc->changed)
		TAILQ_INIT(resultq);

	 if (!mc->list && mc->listing && !mc->changed) {
		TAILQ_INIT(resultq);
		mc->listing = 0;
	}

	return (NULL);
}

static void
menu_draw(struct screen_ctx *sc, struct menu_ctx *mc, struct menu_q *menuq,
    struct menu_q *resultq)
{
	struct menu	*mi;
	char		 endchar = '«';
	int		 n = 0;
	int		 dx, dy;
	int		 xsave, ysave;
	int		 warp;
	struct fontdesc	*font = DefaultFont;

	if (mc->list) {
		if (TAILQ_EMPTY(resultq) && mc->list) {
			/* Copy them all over. */
			TAILQ_FOREACH(mi, menuq, entry)
				TAILQ_INSERT_TAIL(resultq, mi,
				    resultentry);

			mc->listing = 1;
		} else if (mc->changed)
			mc->listing = 0;
	}

	snprintf(mc->dispstr, sizeof(mc->dispstr), "%s%s%c",
	    mc->promptstr, mc->searchstr, endchar);
	dx = font_width(font, mc->dispstr, strlen(mc->dispstr));
	dy = sc->fontheight;

	TAILQ_FOREACH(mi, resultq, resultentry) {
		char *text;

		if (mc->print != NULL) {
			(*mc->print)(mi, mc->listing);
			text = mi->print;
		} else {
			mi->print[0] = '\0';
			text = mi->text;
		}

		dx = MAX(dx, font_width(font, text,
		    MIN(strlen(text), MENU_MAXENTRY)));
		dy += sc->fontheight;
	}

	xsave = mc->x;
	ysave = mc->y;
	if (mc->x < 0)
		mc->x = 0;
	else if (mc->x + dx >= sc->xmax)
		mc->x = sc->xmax - dx;

	if (mc->y + dy >= sc->ymax)
		mc->y = sc->ymax - dy;
	/* never hide the top of the menu */
        if (mc->y < 0)
                mc->y = 0;

	if (mc->x != xsave || mc->y != ysave)
		xu_ptr_setpos(sc->rootwin, mc->x, mc->y);

	XClearWindow(X_Dpy, sc->menuwin);
	XMoveResizeWindow(X_Dpy, sc->menuwin, mc->x, mc->y, dx, dy);

	font_draw(font, mc->dispstr, strlen(mc->dispstr), sc->menuwin,
	    0, font_ascent(font) + 1);

	n = 1;
	TAILQ_FOREACH(mi, resultq, resultentry) {
		char *text = mi->print[0] != '\0' ?
		    mi->print : mi->text;

		font_draw(font, text,
		    MIN(strlen(text), MENU_MAXENTRY),
		    sc->menuwin,
		    0, n*sc->fontheight + font_ascent(font) + 1);
		n++;
	}

	if (n > 1)
		XFillRectangle(X_Dpy, sc->menuwin, sc->gc,
		    0, sc->fontheight, dx, sc->fontheight);

	if (mc->noresult)
		XFillRectangle(X_Dpy, sc->menuwin, sc->gc,
		    0, 0, dx, sc->fontheight);

}

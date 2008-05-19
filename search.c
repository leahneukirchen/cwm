/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2004 Marius Aamodt Eriksen <marius@monkey.org>
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

#define SearchMask (KeyPressMask|ExposureMask)

static int	_strsubmatch(char *, char *, int);

void
search_init(struct screen_ctx *sc)
{
	sc->searchwin = XCreateSimpleWindow(X_Dpy, sc->rootwin, 0, 0,
	    1, 1, 1, sc->blackpixl, sc->whitepixl);
}

/*
 * Input: list of items,
 * Output: choose one
 * so, exactly like menus
 */

struct menu *
search_start(struct menu_q *menuq,
    void (*match)(struct menu_q *, struct menu_q *, char *),
    void (*print)(struct menu *mi, int print),
    char *prompt, int dummy)
{
	struct screen_ctx *sc = screen_current();
	int x, y, dx, dy, fontheight,
	    focusrevert, mutated, xmax, ymax, warp, added, beobnoxious = 0;
	XEvent e;
	char searchstr[MENU_MAXENTRY + 1];
	char dispstr[MENU_MAXENTRY*2 + 1];
	char promptstr[MENU_MAXENTRY + 1];
	Window focuswin;
	struct menu *mi = NULL, *dummy_mi = NULL;
	struct menu_q resultq;
	char chr;
	enum ctltype ctl;
	size_t len;
	u_int n;
	static int list = 0;
	int listing = 0;
	char endchar = '«';
	struct fontdesc *font = DefaultFont;

	if (prompt == NULL)
		prompt = "search";

	TAILQ_INIT(&resultq);

	xmax = DisplayWidth(X_Dpy, sc->which);
	ymax = DisplayHeight(X_Dpy, sc->which);

	xu_ptr_getpos(sc->rootwin, &x, &y);

	searchstr[0] = '\0';

	snprintf(promptstr, sizeof(promptstr), "%s »", prompt);
	dy = fontheight = font_ascent(font) + font_descent(font) + 1;
	snprintf(dispstr, sizeof(dispstr), "%s%c", promptstr, endchar);
	dx = font_width(font, dispstr, strlen(dispstr));

	XMoveResizeWindow(X_Dpy, sc->searchwin, x, y, dx, dy);
	XSelectInput(X_Dpy, sc->searchwin, SearchMask);
	XMapRaised(X_Dpy, sc->searchwin);

	/*
	 * TODO: eventually, the mouse should be able to select
	 * results as well.  Right now we grab it only to set a fancy
	 * cursor.
	 */
	if (xu_ptr_grab(sc->searchwin, 0, Cursor_question) < 0) {
		XUnmapWindow(X_Dpy, sc->searchwin);
		return (NULL);
	}

	XGetInputFocus(X_Dpy, &focuswin, &focusrevert);
	XSetInputFocus(X_Dpy, sc->searchwin, RevertToPointerRoot, CurrentTime);

	for (;;) {
		added = mutated = 0;

		XMaskEvent(X_Dpy, SearchMask, &e);

		switch (e.type) {
		case KeyPress:
			if (input_keycodetrans(e.xkey.keycode, e.xkey.state,
			    &ctl, &chr) < 0)
				continue;

			switch (ctl) {
			case CTL_ERASEONE:
				if ((len = strlen(searchstr)) > 0) {
					searchstr[len - 1] = '\0';
					mutated = 1;
				}
				break;
			case CTL_UP:
				mi = TAILQ_LAST(&resultq, menu_q);
				if (mi == NULL)
					break;

				TAILQ_REMOVE(&resultq, mi, resultentry);
				TAILQ_INSERT_HEAD(&resultq, mi, resultentry);
				break;
			case CTL_DOWN:
				mi = TAILQ_FIRST(&resultq);
				if (mi == NULL)
					break;

				TAILQ_REMOVE(&resultq, mi, resultentry);
				TAILQ_INSERT_TAIL(&resultq, mi, resultentry);
				break;
			case CTL_RETURN:
				/* This is just picking the match the
				 * cursor is over. */
				if ((mi = TAILQ_FIRST(&resultq)) != NULL) {
					goto found;
				} else if (dummy) {
					dummy_mi = xmalloc(sizeof *dummy_mi);
					(void) strlcpy(dummy_mi->text,
					    searchstr, sizeof(dummy_mi->text));
					dummy_mi->dummy = 1;
					goto found;
				}
				goto out;
			case CTL_WIPE:
				searchstr[0] = '\0';
				mutated = 1;
				break;
			case CTL_ALL:
				list = !list;
				break;
			case CTL_ABORT:
				goto out;
			default:
				break;
			}

			if (chr != '\0') {
				char str[2];

				str[0] = chr;
				str[1] = '\0';
				mutated = 1;
				added =
				    strlcat(searchstr, str, sizeof(searchstr));
			}

			beobnoxious = 0;
			if (mutated && strlen(searchstr) > 0) {
				(*match)(menuq, &resultq, searchstr);
				beobnoxious = TAILQ_EMPTY(&resultq);
			} else if (mutated)
				TAILQ_INIT(&resultq);


			 if (!list && listing && !mutated) {
				TAILQ_INIT(&resultq);
				listing = 0;
			}

		case Expose:
			if (list) {
				if (TAILQ_EMPTY(&resultq) && list) {
					/* Copy them all over. */
					TAILQ_FOREACH(mi, menuq, entry)
						TAILQ_INSERT_TAIL(&resultq, mi,
						    resultentry);

					listing = 1;
				} else if (mutated)
					listing = 0;
			}

			snprintf(dispstr, sizeof(dispstr), "%s%s%c",
			    promptstr, searchstr, endchar);
			dx = font_width(font, dispstr, strlen(dispstr));
			dy = fontheight;

			TAILQ_FOREACH(mi, &resultq, resultentry) {
				char *text;

				if (print != NULL) {
					(*print)(mi, listing);
					text = mi->print;
				} else {
					mi->print[0] = '\0';
					text = mi->text;
				}

				dx = MAX(dx, font_width(font, text,
				    MIN(strlen(text), MENU_MAXENTRY)));
				dy += fontheight;
			}

			/*
			 * Calculate new geometry.
			 *
			 * XXX - put this into a util function -- it's
			 * used elsewhere, too.
			 */
			warp = 0;
			if (x < 0) {
				x = 0;
				warp = 1;
			}
			if (x + dx >= xmax) {
				x = xmax - dx;
				warp = 1;
			}

			if (y < 0) {
				y = 0;
				warp = 1;
			}
			if (y + dy >= ymax) {
				y = ymax - dy;
				/* If the menu is too high, never hide the
				 * top of the menu.
				 */
				if (y < 0)
					y = 0;
				warp = 1;
			}

			if (warp)
				xu_ptr_setpos(sc->rootwin, x, y);

			XClearWindow(X_Dpy, sc->searchwin);
			XMoveResizeWindow(X_Dpy, sc->searchwin, x, y, dx, dy);

			font_draw(font, dispstr, strlen(dispstr), sc->searchwin,
			    0, font_ascent(font) + 1);

			n = 1;
			TAILQ_FOREACH(mi, &resultq, resultentry) {
				char *text = mi->print[0] != '\0' ?
				    mi->print : mi->text;

				font_draw(font, text,
				    MIN(strlen(text), MENU_MAXENTRY),
				    sc->searchwin,
				    0, n*fontheight + font_ascent(font) + 1);
				n++;
			}

			if (n > 1)
				XFillRectangle(X_Dpy, sc->searchwin, sc->gc,
				    0, fontheight, dx, fontheight);

			if (beobnoxious)
				XFillRectangle(X_Dpy, sc->searchwin, sc->gc,
				    0, 0, dx, fontheight);

			break;
		}
	}

out:
	/* (if no match) */
	xu_ptr_ungrab();
	XSetInputFocus(X_Dpy, focuswin, focusrevert, CurrentTime);

found:
	XUnmapWindow(X_Dpy, sc->searchwin);

	if (dummy && dummy_mi != NULL)
		return (dummy_mi);
	return (mi);
}

/*
 * Match: label, title, class.
 */

void
search_match_client(struct menu_q *menuq, struct menu_q *resultq, char *search)
{
	struct winname *wn;
	struct menu *mi, *tierp[4], *before = NULL;
	int ntiers = sizeof(tierp)/sizeof(tierp[0]);

	TAILQ_INIT(resultq);

	memset(tierp, 0, sizeof(tierp));

	/*
	 * In order of rank:
	 *
	 *   1. Look through labels.
	 *   2. Look at title history, from present to past.
	 *   3. Look at window class name.
	 */

	TAILQ_FOREACH(mi, menuq, entry) {
		int tier = -1, t;
		struct client_ctx *cc = mi->ctx;

		/* First, try to match on labels. */
		if (cc->label != NULL && _strsubmatch(search, cc->label, 0)) {
			cc->matchname = cc->label;
			tier = 0;
		}

		/* Then, on window names. */
		if (tier < 0) {
			TAILQ_FOREACH_REVERSE(wn, &cc->nameq, winname_q, entry)
				if (_strsubmatch(search, wn->name, 0)) {
					cc->matchname = wn->name;
					tier = 2;
					break;
				}
		}

		/*
		 * See if there is a match on the window class
		 * name.
		 */

		if (tier < 0 && _strsubmatch(search, cc->app_class, 0)) {
			cc->matchname = cc->app_class;
			tier = 3;
		}

		if (tier < 0)
			continue;

		/*
		 * De-rank a client one tier if it's the current
		 * window.  Furthermore, this is denoted by a "!" when
		 * printing the window name in the search menu.
		 */
		if (cc == client_current() && tier < ntiers - 1)
			tier++;

		/*
		 * Clients that are hidden get ranked one up.
		 */
		if (cc->flags & CLIENT_HIDDEN && tier > 0)
			tier--;

		assert(tier < ntiers);

		/*
		 * If you have a tierp, insert after it, and make it
		 * the new tierp.  If you don't have a tierp, find the
		 * first nonzero tierp above you, insert after it.
		 * Always make your current tierp the newly inserted
		 * entry.
		 */

		for (t = tier; t >= 0 && ((before = tierp[t]) == NULL); t--)
			;

		if (before == NULL)
			TAILQ_INSERT_HEAD(resultq, mi, resultentry);
		else
			TAILQ_INSERT_AFTER(resultq, before, mi, resultentry);

		tierp[tier] = mi;
	}
}

void
search_print_client(struct menu *mi, int list)
{
	struct client_ctx *cc = mi->ctx;
	char flag = ' ';

	if (cc == client_current())
		flag = '!';
	else if (cc->flags & CLIENT_HIDDEN)
		flag = '&';

	if (list)
		cc->matchname = TAILQ_FIRST(&cc->nameq)->name;

	snprintf(mi->print, sizeof(mi->print), "%c%s", flag, cc->matchname);

	if (!list && cc->matchname != cc->name &&
	    strlen(mi->print) < sizeof(mi->print) - 1) {
		int diff = sizeof(mi->print) - 1 - strlen(mi->print);
		const char *marker = "";
		char buf[MENU_MAXENTRY + 1];

		/* One for the ':' */
		diff -= 1;

		if (strlen(cc->name) > diff) {
			marker = "..";
			diff -= 2;
		} else {
			diff = strlen(cc->name);
		}

		strlcpy(buf, mi->print, sizeof(buf));
		snprintf(mi->print, sizeof(mi->print),
		    "%s:%.*s%s", buf, diff, cc->name, marker);
	}
}

void
search_match_text(struct menu_q *menuq, struct menu_q *resultq, char *search)
{
	struct menu *mi;

	TAILQ_INIT(resultq);

	TAILQ_FOREACH(mi, menuq, entry)
		if (_strsubmatch(search, mi->text, 0))
			TAILQ_INSERT_TAIL(resultq, mi, resultentry);
}

void
search_match_exec(struct menu_q *menuq, struct menu_q *resultq, char *search)
{
	struct menu *mi;

	TAILQ_INIT(resultq);

	TAILQ_FOREACH(mi, menuq, entry)
		if (_strsubmatch(search, mi->text, 1))
			TAILQ_INSERT_TAIL(resultq, mi, resultentry);
}

static int
_strsubmatch(char *sub, char *str, int zeroidx)
{
	size_t len, sublen;
	u_int n, flen;

	if (sub == NULL || str == NULL)
		return (0);

	len = strlen(str);
	sublen = strlen(sub);

	if (sublen > len)
		return (0);

	if (!zeroidx)
		flen = len - sublen;
	else
		flen = 0;
	for (n = 0; n <= flen; n++)
		if (strncasecmp(sub, str + n, sublen) == 0)
			return (1);

	return (0);
}

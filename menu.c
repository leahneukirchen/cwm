/*
 * calmwm - the calm window manager
 *
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
 *
 * $OpenBSD$
 */

#include <sys/types.h>
#include "queue.h"

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

#define PROMPT_SCHAR	"\xc2\xbb"
#define PROMPT_ECHAR	"\xc2\xab"

#define MENUMASK 	(MOUSEMASK | ButtonMotionMask | KeyPressMask | \
			 ExposureMask)
#define MENUGRABMASK	(MOUSEMASK | ButtonMotionMask | StructureNotifyMask)

enum ctltype {
	CTL_NONE = -1,
	CTL_ERASEONE = 0, CTL_WIPE, CTL_UP, CTL_DOWN, CTL_RETURN,
	CTL_TAB, CTL_ABORT, CTL_ALL
};

struct menu_ctx {
	struct screen_ctx	*sc;
	Window			 win;
	XftDraw			*xftdraw;
	struct geom		 geom;
	char			 searchstr[MENU_MAXENTRY + 1];
	char			 dispstr[MENU_MAXENTRY*2 + 1];
	char			 promptstr[MENU_MAXENTRY + 1];
	int			 list;
	int			 listing;
	int			 changed;
	int			 prev;
	int			 entry;
	int			 num;
	int 			 flags;
	void (*match)(struct menu_q *, struct menu_q *, char *);
	void (*print)(struct menu *, int);
};
static struct menu	*menu_handle_key(XEvent *, struct menu_ctx *,
			     struct menu_q *, struct menu_q *);
static void		 menu_handle_move(struct menu_ctx *,
			     struct menu_q *, int, int);
static struct menu	*menu_handle_release(struct menu_ctx *,
			     struct menu_q *, int, int);
static void		 menu_draw(struct menu_ctx *, struct menu_q *,
			     struct menu_q *);
static void 		 menu_draw_entry(struct menu_ctx *, struct menu_q *,
			     int, int);
static int		 menu_calc_entry(struct menu_ctx *, int, int);
static struct menu	*menu_complete_path(struct menu_ctx *);
static int		 menu_keycode(XKeyEvent *, enum ctltype *, char *);

struct menu *
menu_filter(struct screen_ctx *sc, struct menu_q *menuq, const char *prompt,
    const char *initial, int flags,
    void (*match)(struct menu_q *, struct menu_q *, char *),
    void (*print)(struct menu *, int))
{
	struct menu_ctx		 mc;
	struct menu_q		 resultq;
	struct menu		*mi = NULL;
	XEvent			 e;
	Window			 focuswin;
	int			 focusrevert, xsave, ysave, xcur, ycur;

	TAILQ_INIT(&resultq);

	xu_ptr_get(sc->rootwin, &xsave, &ysave);

	(void)memset(&mc, 0, sizeof(mc));
	mc.sc = sc;
	mc.flags = flags;
	mc.match = match;
	mc.print = print;
	mc.entry = mc.prev = -1;
	mc.geom.x = xsave;
	mc.geom.y = ysave;

	if (mc.flags & CWM_MENU_LIST)
		mc.list = 1;

	(void)strlcpy(mc.promptstr, prompt, sizeof(mc.promptstr));
	if (initial != NULL)
		(void)strlcpy(mc.searchstr, initial, sizeof(mc.searchstr));
	else
		mc.searchstr[0] = '\0';

	mc.win = XCreateSimpleWindow(X_Dpy, sc->rootwin, 0, 0, 1, 1,
	    Conf.bwidth,
	    sc->xftcolor[CWM_COLOR_MENU_FG].pixel,
	    sc->xftcolor[CWM_COLOR_MENU_BG].pixel);
	mc.xftdraw = XftDrawCreate(X_Dpy, mc.win,
	    sc->visual, sc->colormap);

	XSelectInput(X_Dpy, mc.win, MENUMASK);
	XMapRaised(X_Dpy, mc.win);

	if (XGrabPointer(X_Dpy, mc.win, False, MENUGRABMASK,
	    GrabModeAsync, GrabModeAsync, None, Conf.cursor[CF_QUESTION],
	    CurrentTime) != GrabSuccess) {
		XftDrawDestroy(mc.xftdraw);
		XDestroyWindow(X_Dpy, mc.win);
		return NULL;
	}

	XGetInputFocus(X_Dpy, &focuswin, &focusrevert);
	XSetInputFocus(X_Dpy, mc.win, RevertToPointerRoot, CurrentTime);

	/* make sure keybindings don't remove keys from the menu stream */
	XGrabKeyboard(X_Dpy, mc.win, True,
	    GrabModeAsync, GrabModeAsync, CurrentTime);

	for (;;) {
		mc.changed = 0;

		XWindowEvent(X_Dpy, mc.win, MENUMASK, &e);

		switch (e.type) {
		case KeyPress:
			if ((mi = menu_handle_key(&e, &mc, menuq, &resultq))
			    != NULL)
				goto out;
			/* FALLTHROUGH */
		case Expose:
			menu_draw(&mc, menuq, &resultq);
			break;
		case MotionNotify:
			menu_handle_move(&mc, &resultq,
			    e.xbutton.x, e.xbutton.y);
			break;
		case ButtonRelease:
			if ((mi = menu_handle_release(&mc, &resultq,
			    e.xbutton.x, e.xbutton.y)) != NULL)
				goto out;
			break;
		default:
			break;
		}
	}
out:
	if ((mc.flags & CWM_MENU_DUMMY) == 0 && mi->dummy) {
	       	/* no mouse based match */
		free(mi);
		mi = NULL;
	}

	XftDrawDestroy(mc.xftdraw);
	XDestroyWindow(X_Dpy, mc.win);

	XSetInputFocus(X_Dpy, focuswin, focusrevert, CurrentTime);
	/* restore if user didn't move */
	xu_ptr_get(sc->rootwin, &xcur, &ycur);
	if (xcur == mc.geom.x && ycur == mc.geom.y)
		xu_ptr_set(sc->rootwin, xsave, ysave);

	XUngrabPointer(X_Dpy, CurrentTime);
	XUngrabKeyboard(X_Dpy, CurrentTime);

	return mi;
}

static struct menu *
menu_complete_path(struct menu_ctx *mc)
{
	struct screen_ctx	*sc = mc->sc;
	struct menu		*mi, *mr;
	struct menu_q		 menuq;
	int			 mflags = (CWM_MENU_DUMMY);

	mr = xcalloc(1, sizeof(*mr));

	TAILQ_INIT(&menuq);

	if ((mi = menu_filter(sc, &menuq, mc->searchstr, NULL, mflags,
	    search_match_path, search_print_text)) != NULL) {
		mr->abort = mi->abort;
		mr->dummy = mi->dummy;
		if (mi->text[0] != '\0')
			snprintf(mr->text, sizeof(mr->text), "%s \"%s\"",
			    mc->searchstr, mi->text);
		else if (!mr->abort)
			strlcpy(mr->text, mc->searchstr, sizeof(mr->text));
	}
	
	menuq_clear(&menuq);

	return mr;
}

static struct menu *
menu_handle_key(XEvent *e, struct menu_ctx *mc, struct menu_q *menuq,
    struct menu_q *resultq)
{
	struct menu	*mi;
	enum ctltype	 ctl;
	char		 chr[32];
	size_t		 len;
	int 		 clen, i;
	wchar_t 	 wc;

	if (menu_keycode(&e->xkey, &ctl, chr) < 0)
		return NULL;

	switch (ctl) {
	case CTL_ERASEONE:
		if ((len = strlen(mc->searchstr)) > 0) {
			clen = 1;
			while (mbtowc(&wc, &mc->searchstr[len-clen], MB_CUR_MAX) == -1)
				clen++;
			for (i = 1; i <= clen; i++)
				mc->searchstr[len - i] = '\0';
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
			mi = xmalloc(sizeof(*mi));
			(void)strlcpy(mi->text,
			    mc->searchstr, sizeof(mi->text));
			mi->dummy = 1;
		}
		mi->abort = 0;
		return mi;
	case CTL_WIPE:
		mc->searchstr[0] = '\0';
		mc->changed = 1;
		break;
	case CTL_TAB:
		if ((mi = TAILQ_FIRST(resultq)) != NULL) {
			/*
			 * - We are in exec_path menu mode
			 * - It is equal to the input
			 * We got a command, launch the file menu
			 */
			if ((mc->flags & CWM_MENU_FILE) &&
			    (strncmp(mc->searchstr, mi->text,
					strlen(mi->text))) == 0)
				return menu_complete_path(mc);

			/*
			 * Put common prefix of the results into searchstr
			 */
			(void)strlcpy(mc->searchstr,
					mi->text, sizeof(mc->searchstr));
			while ((mi = TAILQ_NEXT(mi, resultentry)) != NULL) {
				i = 0;
				while (tolower(mc->searchstr[i]) ==
					       tolower(mi->text[i]))
					i++;
				mc->searchstr[i] = '\0';
			}
			mc->changed = 1;
		}
		break;
	case CTL_ALL:
		mc->list = !mc->list;
		break;
	case CTL_ABORT:
		mi = xmalloc(sizeof(*mi));
		mi->text[0] = '\0';
		mi->dummy = 1;
		mi->abort = 1;
		return mi;
	default:
		break;
	}

	if (chr[0] != '\0') {
		mc->changed = 1;
		(void)strlcat(mc->searchstr, chr, sizeof(mc->searchstr));
	}

	if (mc->changed) {
		if (mc->searchstr[0] != '\0')
			(*mc->match)(menuq, resultq, mc->searchstr);
	} else if (!mc->list && mc->listing) {
		TAILQ_INIT(resultq);
		mc->listing = 0;
	}

	return NULL;
}

static void
menu_draw(struct menu_ctx *mc, struct menu_q *menuq, struct menu_q *resultq)
{
	struct screen_ctx	*sc = mc->sc;
	struct menu		*mi;
	struct geom		 area;
	int			 n, xsave, ysave;
	XGlyphInfo		 extents;

	if (mc->list) {
		if (TAILQ_EMPTY(resultq)) {
			/* Copy them all over. */
			TAILQ_FOREACH(mi, menuq, entry)
				TAILQ_INSERT_TAIL(resultq, mi, resultentry);

			mc->listing = 1;
		} else if (mc->changed)
			mc->listing = 0;
	}

	(void)snprintf(mc->dispstr, sizeof(mc->dispstr), "%s%s%s%s",
	    mc->promptstr, PROMPT_SCHAR, mc->searchstr, PROMPT_ECHAR);
	XftTextExtentsUtf8(X_Dpy, sc->xftfont,
	    (const FcChar8*)mc->dispstr, strlen(mc->dispstr), &extents);
	mc->geom.w = extents.xOff;
	mc->geom.h = sc->xftfont->ascent + sc->xftfont->descent;
	mc->num = 1;

	TAILQ_FOREACH(mi, resultq, resultentry) {
		(*mc->print)(mi, mc->listing);
		XftTextExtentsUtf8(X_Dpy, sc->xftfont,
		    (const FcChar8*)mi->print,
		    MIN(strlen(mi->print), MENU_MAXENTRY), &extents);
		mc->geom.w = MAX(mc->geom.w, extents.xOff);
		mc->geom.h += sc->xftfont->ascent + sc->xftfont->descent;
		mc->num++;
	}

	area = screen_area(sc, mc->geom.x, mc->geom.y, 1);
	area.w += area.x - Conf.bwidth * 2;
	area.h += area.y - Conf.bwidth * 2;

	xsave = mc->geom.x;
	ysave = mc->geom.y;

	/* Never hide the top, or left side, of the menu. */
	if (mc->geom.x + mc->geom.w >= area.w)
		mc->geom.x = area.w - mc->geom.w;
	if (mc->geom.x < area.x) {
		mc->geom.x = area.x;
		mc->geom.w = MIN(mc->geom.w, (area.w - area.x));
	}
	if (mc->geom.y + mc->geom.h >= area.h)
		mc->geom.y = area.h - mc->geom.h;
	if (mc->geom.y < area.y) {
		mc->geom.y = area.y;
		mc->geom.h = MIN(mc->geom.h, (area.h - area.y));
	}

	if (mc->geom.x != xsave || mc->geom.y != ysave)
		xu_ptr_set(sc->rootwin, mc->geom.x, mc->geom.y);

	XClearWindow(X_Dpy, mc->win);
	XMoveResizeWindow(X_Dpy, mc->win, mc->geom.x, mc->geom.y,
	    mc->geom.w, mc->geom.h);

	n = 1;
	XftDrawStringUtf8(mc->xftdraw,
	    &sc->xftcolor[CWM_COLOR_MENU_FONT], sc->xftfont,
	    0, sc->xftfont->ascent,
	    (const FcChar8*)mc->dispstr, strlen(mc->dispstr));

	TAILQ_FOREACH(mi, resultq, resultentry) {
		int y = n * (sc->xftfont->ascent + sc->xftfont->descent);

		/* Stop drawing when menu doesn't fit inside the screen. */
		if (mc->geom.y + y >= area.h)
			break;

		XftDrawStringUtf8(mc->xftdraw,
		    &sc->xftcolor[CWM_COLOR_MENU_FONT], sc->xftfont,
		    0, y + sc->xftfont->ascent,
		    (const FcChar8*)mi->print, strlen(mi->print));
		n++;
	}
	if (n > 1)
		menu_draw_entry(mc, resultq, 1, 1);
}

static void
menu_draw_entry(struct menu_ctx *mc, struct menu_q *resultq,
    int entry, int active)
{
	struct screen_ctx	*sc = mc->sc;
	struct menu		*mi;
	int			 color, i = 1, y;

	TAILQ_FOREACH(mi, resultq, resultentry)
		if (entry == i++)
			break;
	if (mi == NULL)
		return;

	y = entry * (sc->xftfont->ascent + sc->xftfont->descent);
	color = (active) ? CWM_COLOR_MENU_FG : CWM_COLOR_MENU_BG;
	XftDrawRect(mc->xftdraw, &sc->xftcolor[color], 0, y,
	    mc->geom.w, sc->xftfont->ascent + sc->xftfont->descent);
	color = (active) ? CWM_COLOR_MENU_FONT_SEL : CWM_COLOR_MENU_FONT;
	XftDrawStringUtf8(mc->xftdraw,
	    &sc->xftcolor[color], sc->xftfont, 0, y + sc->xftfont->ascent,
	    (const FcChar8*)mi->print, strlen(mi->print));
}

static void
menu_handle_move(struct menu_ctx *mc, struct menu_q *resultq, int x, int y)
{
	mc->prev = mc->entry;
	mc->entry = menu_calc_entry(mc, x, y);

	if (mc->prev == mc->entry)
		return;

	if (mc->prev != -1)
		menu_draw_entry(mc, resultq, mc->prev, 0);
	if (mc->entry != -1) {
		XChangeActivePointerGrab(X_Dpy, MENUGRABMASK,
		    Conf.cursor[CF_NORMAL], CurrentTime);
		menu_draw_entry(mc, resultq, mc->entry, 1);
	}
}

static struct menu *
menu_handle_release(struct menu_ctx *mc, struct menu_q *resultq, int x, int y)
{
	struct menu		*mi;
	int			 entry, i = 1;

	entry = menu_calc_entry(mc, x, y);

	TAILQ_FOREACH(mi, resultq, resultentry)
		if (entry == i++)
			break;
	if (mi == NULL) {
		mi = xmalloc(sizeof(*mi));
		mi->text[0] = '\0';
		mi->dummy = 1;
	}
	return mi;
}

static int
menu_calc_entry(struct menu_ctx *mc, int x, int y)
{
	struct screen_ctx	*sc = mc->sc;
	int			 entry;

	entry = y / (sc->xftfont->ascent + sc->xftfont->descent);

	/* in bounds? */
	if (x < 0 || x > mc->geom.w || y < 0 ||
	    y > (sc->xftfont->ascent + sc->xftfont->descent) * mc->num ||
	    entry < 0 || entry >= mc->num)
		entry = -1;

	if (entry == 0)
		entry = -1;

	return entry;
}

static int
menu_keycode(XKeyEvent *ev, enum ctltype *ctl, char *chr)
{
	KeySym		 ks;

	*ctl = CTL_NONE;
	chr[0] = '\0';

	ks = XkbKeycodeToKeysym(X_Dpy, ev->keycode, 0,
	     (ev->state & ShiftMask) ? 1 : 0);

	/* Look for control characters. */
	switch (ks) {
	case XK_BackSpace:
		*ctl = CTL_ERASEONE;
		break;
	case XK_KP_Enter:
	case XK_Return:
		*ctl = CTL_RETURN;
		break;
	case XK_Tab:
		*ctl = CTL_TAB;
		break;
	case XK_Up:
		*ctl = CTL_UP;
		break;
	case XK_Down:
		*ctl = CTL_DOWN;
		break;
	case XK_Escape:
		*ctl = CTL_ABORT;
		break;
	}

	if (*ctl == CTL_NONE && (ev->state & ControlMask)) {
		switch (ks) {
		case XK_s:
		case XK_S:
			/* Emacs "next" */
			*ctl = CTL_DOWN;
			break;
		case XK_r:
		case XK_R:
			/* Emacs "previous" */
			*ctl = CTL_UP;
			break;
		case XK_u:
		case XK_U:
			*ctl = CTL_WIPE;
			break;
		case XK_h:
		case XK_H:
			*ctl = CTL_ERASEONE;
			break;
		case XK_a:
		case XK_A:
			*ctl = CTL_ALL;
			break;
		case XK_bracketleft:
			*ctl = CTL_ABORT;
			break;
		}
	}

	if (*ctl == CTL_NONE && (ev->state & Mod1Mask)) {
		switch (ks) {
		case XK_j:
		case XK_J:
			/* Vi "down" */
			*ctl = CTL_DOWN;
			break;
		case XK_k:
		case XK_K:
			/* Vi "up" */
			*ctl = CTL_UP;
			break;
		}
	}

	if (*ctl != CTL_NONE)
		return 0;

	if (XLookupString(ev, chr, 32, &ks, NULL) < 0)
		return -1;

	return 0;
}

void
menuq_add(struct menu_q *mq, void *ctx, const char *fmt, ...)
{
	va_list		 ap;
	struct menu	*mi;

	mi = xcalloc(1, sizeof(*mi));
	mi->ctx = ctx;

	va_start(ap, fmt);
	if (fmt != NULL)
		(void)vsnprintf(mi->text, sizeof(mi->text), fmt, ap);
	else
		mi->text[0] = '\0';
	va_end(ap);

	TAILQ_INSERT_TAIL(mq, mi, entry);
}

void
menuq_clear(struct menu_q *mq)
{
	struct menu	*mi;

	while ((mi = TAILQ_FIRST(mq)) != NULL) {
		TAILQ_REMOVE(mq, mi, entry);
		free(mi);
	}
}

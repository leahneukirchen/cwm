/*
 * calmwm - the calm window manager
 *
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

#include <sys/param.h>
#include <sys/queue.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

static struct client_ctx	*client_mrunext(struct client_ctx *);
static struct client_ctx	*client_mruprev(struct client_ctx *);
static void			 client_mtf(struct client_ctx *);
static void			 client_none(struct screen_ctx *);
static void			 client_placecalc(struct client_ctx *);
static void			 client_update(struct client_ctx *);
static void			 client_getmwmhints(struct client_ctx *);
static int			 client_inbound(struct client_ctx *, int, int);

struct client_ctx	*_curcc = NULL;

struct client_ctx *
client_find(Window win)
{
	struct client_ctx	*cc;

	TAILQ_FOREACH(cc, &Clientq, entry)
		if (cc->win == win)
			return (cc);

	return (NULL);
}

struct client_ctx *
client_new(Window win, struct screen_ctx *sc, int mapped)
{
	struct client_ctx	*cc;
	XClassHint		 xch;
	XWindowAttributes	 wattr;
	XWMHints		*wmhints;
	int			 state;

	if (win == None)
		return (NULL);

	cc = xcalloc(1, sizeof(*cc));

	XGrabServer(X_Dpy);

	cc->state = mapped ? NormalState : IconicState;
	cc->sc = sc;
	cc->win = win;
	cc->size = XAllocSizeHints();

	client_getsizehints(cc);

	TAILQ_INIT(&cc->nameq);
	client_setname(cc);

	conf_client(cc);

	if (XGetClassHint(X_Dpy, cc->win, &xch)) {
		cc->app_name = xch.res_name;
		cc->app_class = xch.res_class;
	}
	client_getmwmhints(cc);

	/* Saved pointer position */
	cc->ptr.x = -1;
	cc->ptr.y = -1;

	XGetWindowAttributes(X_Dpy, cc->win, &wattr);
	cc->geom.x = wattr.x;
	cc->geom.y = wattr.y;
	cc->geom.w = wattr.width;
	cc->geom.h = wattr.height;
	cc->colormap = wattr.colormap;

	if (wattr.map_state != IsViewable) {
		client_placecalc(cc);
		if ((wmhints = XGetWMHints(X_Dpy, cc->win)) != NULL) {
			if (wmhints->flags & StateHint)
				xu_setstate(cc, wmhints->initial_state);

			XFree(wmhints);
		}
		client_move(cc);
	}
	client_draw_border(cc);

	if (xu_getstate(cc, &state) < 0)
		state = NormalState;

	XSelectInput(X_Dpy, cc->win, ColormapChangeMask | EnterWindowMask |
	    PropertyChangeMask | KeyReleaseMask);

	XAddToSaveSet(X_Dpy, cc->win);

	client_transient(cc);

	/* Notify client of its configuration. */
	xu_configure(cc);

	(state == IconicState) ? client_hide(cc) : client_unhide(cc);
	xu_setstate(cc, cc->state);

	TAILQ_INSERT_TAIL(&sc->mruq, cc, mru_entry);
	TAILQ_INSERT_TAIL(&Clientq, cc, entry);

	xu_ewmh_net_client_list(sc);

	client_update(cc);

	if (mapped)
		group_autogroup(cc);

	XSync(X_Dpy, False);
	XUngrabServer(X_Dpy);

	return (cc);
}

void
client_delete(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	struct winname		*wn;

	group_client_delete(cc);

	XGrabServer(X_Dpy);
	xu_setstate(cc, WithdrawnState);
	XRemoveFromSaveSet(X_Dpy, cc->win);

	XSync(X_Dpy, False);
	XUngrabServer(X_Dpy);

	TAILQ_REMOVE(&sc->mruq, cc, mru_entry);
	TAILQ_REMOVE(&Clientq, cc, entry);

	xu_ewmh_net_client_list(sc);

	if (cc == client_current())
		client_none(sc);

	XFree(cc->size);
	if (cc->app_name != NULL)
		XFree(cc->app_name);
	if (cc->app_class != NULL)
		XFree(cc->app_class);

	while ((wn = TAILQ_FIRST(&cc->nameq)) != NULL) {
		TAILQ_REMOVE(&cc->nameq, wn, entry);
		free(wn->name);
		free(wn);
	}

	free(cc);
}

void
client_leave(struct client_ctx *cc)
{
	struct screen_ctx	*sc;

	if (cc == NULL)
		cc = client_current();
	if (cc == NULL)
		return;

	sc = cc->sc;
	xu_btn_ungrab(sc->rootwin, AnyModifier, Button1);
}

void
client_setactive(struct client_ctx *cc, int fg)
{
	struct screen_ctx	*sc;

	if (cc == NULL)
		cc = client_current();
	if (cc == NULL)
		return;

	sc = cc->sc;

	if (fg) {
		XInstallColormap(X_Dpy, cc->colormap);
		XSetInputFocus(X_Dpy, cc->win,
		    RevertToPointerRoot, CurrentTime);
		conf_grab_mouse(cc);
		/*
		 * If we're in the middle of alt-tabbing, don't change
		 * the order please.
		 */
		if (!sc->cycling)
			client_mtf(cc);
	} else
		client_leave(cc);

	if (fg && cc != client_current()) {
		client_setactive(NULL, 0);
		_curcc = cc;
		xu_ewmh_net_active_window(sc, cc->win);
	}

	cc->active = fg;
	client_draw_border(cc);
}

/*
 * set when there is no active client
 */
static void
client_none(struct screen_ctx *sc)
{
	Window none = None;

	xu_ewmh_net_active_window(sc, none);

	_curcc = NULL;
}

struct client_ctx *
client_current(void)
{
	return (_curcc);
}

void
client_freeze(struct client_ctx *cc)
{
	if (cc->flags & CLIENT_FREEZE)
		cc->flags &= ~CLIENT_FREEZE;
	else
		cc->flags |= CLIENT_FREEZE;
}

void
client_maximize(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	struct geom		 xine;

	if (cc->flags & CLIENT_FREEZE)
		return;

	if ((cc->flags & CLIENT_MAXFLAGS) == CLIENT_MAXIMIZED) {
		cc->flags &= ~CLIENT_MAXIMIZED;
		cc->geom = cc->savegeom;
		cc->bwidth = Conf.bwidth;
		goto resize;
	}

	if ((cc->flags & CLIENT_VMAXIMIZED) == 0) {
		cc->savegeom.h = cc->geom.h;
		cc->savegeom.y = cc->geom.y;
	}

	if ((cc->flags & CLIENT_HMAXIMIZED) == 0) {
		cc->savegeom.w = cc->geom.w;
		cc->savegeom.x = cc->geom.x;
	}

	/*
	 * pick screen that the middle of the window is on.
	 * that's probably more fair than if just the origin of
	 * a window is poking over a boundary
	 */
	xine = screen_find_xinerama(sc,
	    cc->geom.x + cc->geom.w / 2,
	    cc->geom.y + cc->geom.h / 2);

	cc->geom = xine;
	cc->bwidth = 0;
	cc->flags |= CLIENT_MAXIMIZED;

resize:
	client_resize(cc, 0);
}

void
client_vmaximize(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	struct geom		 xine;

	if (cc->flags & CLIENT_FREEZE)
		return;

	if (cc->flags & CLIENT_VMAXIMIZED) {
		cc->geom.y = cc->savegeom.y;
		cc->geom.h = cc->savegeom.h;
		cc->bwidth = Conf.bwidth;
		if (cc->flags & CLIENT_HMAXIMIZED)
			cc->geom.w -= cc->bwidth * 2;
		cc->flags &= ~CLIENT_VMAXIMIZED;
		goto resize;
	}

	cc->savegeom.y = cc->geom.y;
	cc->savegeom.h = cc->geom.h;

	/* if this will make us fully maximized then remove boundary */
	if ((cc->flags & CLIENT_MAXFLAGS) == CLIENT_HMAXIMIZED) {
		cc->geom.w += cc->bwidth * 2;
		cc->bwidth = 0;
	}

	xine = screen_find_xinerama(sc,
	    cc->geom.x + cc->geom.w / 2,
	    cc->geom.y + cc->geom.h / 2);

	cc->geom.y = xine.y;
	cc->geom.h = xine.h - (cc->bwidth * 2);
	cc->flags |= CLIENT_VMAXIMIZED;

resize:
	client_resize(cc, 0);
}

void
client_hmaximize(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	struct geom		 xine;

	if (cc->flags & CLIENT_FREEZE)
		return;

	if (cc->flags & CLIENT_HMAXIMIZED) {
		cc->geom.x = cc->savegeom.x;
		cc->geom.w = cc->savegeom.w;
		cc->bwidth = Conf.bwidth;
		if (cc->flags & CLIENT_VMAXIMIZED)
			cc->geom.h -= cc->bwidth * 2;
		cc->flags &= ~CLIENT_HMAXIMIZED;
		goto resize;
	}

	cc->savegeom.x = cc->geom.x;
	cc->savegeom.w = cc->geom.w;

	/* if this will make us fully maximized then remove boundary */
	if ((cc->flags & CLIENT_MAXFLAGS) == CLIENT_VMAXIMIZED) {
		cc->geom.h += cc->bwidth * 2;
		cc->bwidth = 0;
	}

	xine = screen_find_xinerama(sc,
	    cc->geom.x + cc->geom.w / 2,
	    cc->geom.y + cc->geom.h / 2);

	cc->geom.x = xine.x;
	cc->geom.w = xine.w - (cc->bwidth * 2);
	cc->flags |= CLIENT_HMAXIMIZED;

resize:
	client_resize(cc, 0);
}

void
client_resize(struct client_ctx *cc, int reset)
{
	if (reset) {
		cc->flags &= ~CLIENT_MAXIMIZED;
		cc->bwidth = Conf.bwidth;
	}

	client_draw_border(cc);

	XMoveResizeWindow(X_Dpy, cc->win, cc->geom.x,
	    cc->geom.y, cc->geom.w, cc->geom.h);
	xu_configure(cc);
}

void
client_move(struct client_ctx *cc)
{
	XMoveWindow(X_Dpy, cc->win, cc->geom.x, cc->geom.y);
	xu_configure(cc);
}

void
client_lower(struct client_ctx *cc)
{
	XLowerWindow(X_Dpy, cc->win);
}

void
client_raise(struct client_ctx *cc)
{
	XRaiseWindow(X_Dpy, cc->win);
}

void
client_ptrwarp(struct client_ctx *cc)
{
	int	 x = cc->ptr.x, y = cc->ptr.y;

	if (x == -1 || y == -1) {
		x = cc->geom.w / 2;
		y = cc->geom.h / 2;
	}

	(cc->state == IconicState) ? client_unhide(cc) : client_raise(cc);
	xu_ptr_setpos(cc->win, x, y);
}

void
client_ptrsave(struct client_ctx *cc)
{
	int	 x, y;

	xu_ptr_getpos(cc->win, &x, &y);
	if (client_inbound(cc, x, y)) {
		cc->ptr.x = x;
		cc->ptr.y = y;
	} else {
		cc->ptr.x = -1;
		cc->ptr.y = -1;
	}
}

void
client_hide(struct client_ctx *cc)
{
	/* XXX - add wm_state stuff */
	XUnmapWindow(X_Dpy, cc->win);

	cc->active = 0;
	cc->flags |= CLIENT_HIDDEN;
	xu_setstate(cc, IconicState);

	if (cc == client_current())
		client_none(cc->sc);
}

void
client_unhide(struct client_ctx *cc)
{
	XMapRaised(X_Dpy, cc->win);

	cc->flags &= ~CLIENT_HIDDEN;
	xu_setstate(cc, NormalState);
	client_draw_border(cc);
}

void
client_draw_border(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	unsigned long		 pixel;

	if (cc->active)
		switch (cc->flags & CLIENT_HIGHLIGHT) {
		case CLIENT_GROUP:
			pixel = sc->color[CWM_COLOR_BORDER_GROUP];
			break;
		case CLIENT_UNGROUP:
			pixel = sc->color[CWM_COLOR_BORDER_UNGROUP];
			break;
		default:
			pixel = sc->color[CWM_COLOR_BORDER_ACTIVE];
			break;
		}
	else
		pixel = sc->color[CWM_COLOR_BORDER_INACTIVE];

	XSetWindowBorderWidth(X_Dpy, cc->win, cc->bwidth);
	XSetWindowBorder(X_Dpy, cc->win, pixel);
}

static void
client_update(struct client_ctx *cc)
{
	Atom	*p;
	int	 i;
	long	 n;

	if ((n = xu_getprop(cc->win, cwmh[WM_PROTOCOLS].atom,
		 XA_ATOM, 20L, (u_char **)&p)) <= 0)
		return;

	for (i = 0; i < n; i++)
		if (p[i] == cwmh[WM_DELETE_WINDOW].atom)
			cc->xproto |= CLIENT_PROTO_DELETE;
		else if (p[i] == cwmh[WM_TAKE_FOCUS].atom)
			cc->xproto |= CLIENT_PROTO_TAKEFOCUS;

	XFree(p);
}

void
client_send_delete(struct client_ctx *cc)
{
	if (cc->xproto & CLIENT_PROTO_DELETE)
		xu_sendmsg(cc->win,
		    cwmh[WM_PROTOCOLS].atom, cwmh[WM_DELETE_WINDOW].atom);
	else
		XKillClient(X_Dpy, cc->win);
}

void
client_setname(struct client_ctx *cc)
{
	struct winname	*wn;
	char		*newname;

	if (!xu_getstrprop(cc->win, ewmh[_NET_WM_NAME].atom, &newname))
		if (!xu_getstrprop(cc->win, XA_WM_NAME, &newname))
			newname = xstrdup("");

	TAILQ_FOREACH(wn, &cc->nameq, entry)
		if (strcmp(wn->name, newname) == 0) {
			/* Move to the last since we got a hit. */
			TAILQ_REMOVE(&cc->nameq, wn, entry);
			TAILQ_INSERT_TAIL(&cc->nameq, wn, entry);
			goto match;
		}

	wn = xmalloc(sizeof(*wn));
	wn->name = newname;
	TAILQ_INSERT_TAIL(&cc->nameq, wn, entry);
	cc->nameqlen++;

match:
	cc->name = wn->name;

	/* Now, do some garbage collection. */
	if (cc->nameqlen > CLIENT_MAXNAMEQLEN) {
		wn = TAILQ_FIRST(&cc->nameq);
		assert(wn != NULL);
		TAILQ_REMOVE(&cc->nameq, wn, entry);
		free(wn->name);
		free(wn);
		cc->nameqlen--;
	}
}

void
client_cycle(struct screen_ctx *sc, int flags)
{
	struct client_ctx	*oldcc, *newcc;
	int			 again = 1;

	oldcc = client_current();

	/* If no windows then you cant cycle */
	if (TAILQ_EMPTY(&sc->mruq))
		return;

	if (oldcc == NULL)
		oldcc = (flags & CWM_RCYCLE ?
		    TAILQ_LAST(&sc->mruq, cycle_entry_q) :
		    TAILQ_FIRST(&sc->mruq));

	newcc = oldcc;
	while (again) {
		again = 0;

		newcc = (flags & CWM_RCYCLE ? client_mruprev(newcc) :
		    client_mrunext(newcc));

		/* Only cycle visible and non-ignored windows. */
		if ((newcc->flags & (CLIENT_HIDDEN|CLIENT_IGNORE))
			|| ((flags & CWM_INGROUP) && (newcc->group != oldcc->group)))
			again = 1;

		/* Is oldcc the only non-hidden window? */
		if (newcc == oldcc) {
			if (again)
				return;	/* No windows visible. */

			break;
		}
	}

	/* reset when cycling mod is released. XXX I hate this hack */
	sc->cycling = 1;
	client_ptrsave(oldcc);
	client_ptrwarp(newcc);
}

void
client_cycle_leave(struct screen_ctx *sc, struct client_ctx *cc)
{
	sc->cycling = 0;

	client_mtf(NULL);
	if (cc) {
		group_sticky_toggle_exit(cc);
		XUngrabKeyboard(X_Dpy, CurrentTime);
	}
}

static struct client_ctx *
client_mrunext(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	struct client_ctx	*ccc;

	return ((ccc = TAILQ_NEXT(cc, mru_entry)) != NULL ?
	    ccc : TAILQ_FIRST(&sc->mruq));
}

static struct client_ctx *
client_mruprev(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	struct client_ctx	*ccc;

	return ((ccc = TAILQ_PREV(cc, cycle_entry_q, mru_entry)) != NULL ?
	    ccc : TAILQ_LAST(&sc->mruq, cycle_entry_q));
}

static void
client_placecalc(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	int			 xslack, yslack;

	if (cc->size->flags & (USPosition|PPosition)) {
		/*
		 * Ignore XINERAMA screens, just make sure it's somewhere
		 * in the virtual desktop. else it stops people putting xterms
		 * at startup in the screen the mouse doesn't start in *sigh*.
		 * XRandR bits mean that {x,y}max shouldn't be outside what's
		 * currently there.
		 */
		xslack = sc->view.w - cc->geom.w - cc->bwidth * 2;
		yslack = sc->view.h - cc->geom.h - cc->bwidth * 2;
		if (cc->size->x > 0)
			cc->geom.x = MIN(cc->size->x, xslack);
		if (cc->size->y > 0)
			cc->geom.y = MIN(cc->size->y, yslack);
	} else {
		struct geom		 xine;
		int			 xmouse, ymouse;

		xu_ptr_getpos(sc->rootwin, &xmouse, &ymouse);
		xine = screen_find_xinerama(sc, xmouse, ymouse);
		xine.w += xine.x;
		xine.h += xine.y;
		xmouse = MAX(xmouse, xine.x) - cc->geom.w / 2;
		ymouse = MAX(ymouse, xine.y) - cc->geom.h / 2;

		xmouse = MAX(xmouse, xine.x);
		ymouse = MAX(ymouse, xine.y);

		xslack = xine.w - cc->geom.w - cc->bwidth * 2;
		yslack = xine.h - cc->geom.h - cc->bwidth * 2;

		if (xslack >= xine.x) {
			cc->geom.x = MAX(MIN(xmouse, xslack), xine.x);
		} else {
			cc->geom.x = xine.x;
			cc->geom.w = xine.w;
		}
		if (yslack >= xine.y) {
			cc->geom.y = MAX(MIN(ymouse, yslack), xine.y);
		} else {
			cc->geom.y = xine.y;
			cc->geom.h = xine.h;
		}
	}
}

static void
client_mtf(struct client_ctx *cc)
{
	struct screen_ctx	*sc;

	if (cc == NULL)
		cc = client_current();
	if (cc == NULL)
		return;

	sc = cc->sc;
	TAILQ_REMOVE(&sc->mruq, cc, mru_entry);
	TAILQ_INSERT_HEAD(&sc->mruq, cc, mru_entry);
}

void
client_getsizehints(struct client_ctx *cc)
{
	long		 tmp;

	if (!XGetWMNormalHints(X_Dpy, cc->win, cc->size, &tmp))
		cc->size->flags = PSize;

	if (cc->size->flags & PBaseSize) {
		cc->hint.basew = cc->size->base_width;
		cc->hint.baseh = cc->size->base_height;
	} else if (cc->size->flags & PMinSize) {
		cc->hint.basew = cc->size->min_width;
		cc->hint.baseh = cc->size->min_height;
	}
	if (cc->size->flags & PMinSize) {
		cc->hint.minw = cc->size->min_width;
		cc->hint.minh = cc->size->min_height;
	} else if (cc->size->flags & PBaseSize) {
		cc->hint.minw = cc->size->base_width;
		cc->hint.minh = cc->size->base_height;
	}
	if (cc->size->flags & PMaxSize) {
		cc->hint.maxw = cc->size->max_width;
		cc->hint.maxh = cc->size->max_height;
	}
	if (cc->size->flags & PResizeInc) {
		cc->hint.incw = cc->size->width_inc;
		cc->hint.inch = cc->size->height_inc;
	}
	cc->hint.incw = MAX(1, cc->hint.incw);
	cc->hint.inch = MAX(1, cc->hint.inch);

	if (cc->size->flags & PAspect) {
		if (cc->size->min_aspect.x > 0)
			cc->hint.mina = (float)cc->size->min_aspect.y /
			    cc->size->min_aspect.x;
		if (cc->size->max_aspect.y > 0)
			cc->hint.maxa = (float)cc->size->max_aspect.x /
			    cc->size->max_aspect.y;
	}
}

void
client_applysizehints(struct client_ctx *cc)
{
	Bool		 baseismin;

	baseismin = (cc->hint.basew == cc->hint.minw) &&
	    (cc->hint.baseh == cc->hint.minh);

	/* temporarily remove base dimensions, ICCCM 4.1.2.3 */
	if (!baseismin) {
		cc->geom.w -= cc->hint.basew;
		cc->geom.h -= cc->hint.baseh;
	}

	/* adjust for aspect limits */
	if (cc->hint.mina > 0 && cc->hint.maxa > 0) {
		if (cc->hint.maxa <
		    (float)cc->geom.w / cc->geom.h)
			cc->geom.w = cc->geom.h * cc->hint.maxa;
		else if (cc->hint.mina <
		    (float)cc->geom.h / cc->geom.w)
			cc->geom.h = cc->geom.w * cc->hint.mina;
	}

	/* remove base dimensions for increment */
	if (baseismin) {
		cc->geom.w -= cc->hint.basew;
		cc->geom.h -= cc->hint.baseh;
	}

	/* adjust for increment value */
	cc->geom.w -= cc->geom.w % cc->hint.incw;
	cc->geom.h -= cc->geom.h % cc->hint.inch;

	/* restore base dimensions */
	cc->geom.w += cc->hint.basew;
	cc->geom.h += cc->hint.baseh;

	/* adjust for min width/height */
	cc->geom.w = MAX(cc->geom.w, cc->hint.minw);
	cc->geom.h = MAX(cc->geom.h, cc->hint.minh);

	/* adjust for max width/height */
	if (cc->hint.maxw)
		cc->geom.w = MIN(cc->geom.w, cc->hint.maxw);
	if (cc->hint.maxh)
		cc->geom.h = MIN(cc->geom.h, cc->hint.maxh);
}

static void
client_getmwmhints(struct client_ctx *cc)
{
	struct mwm_hints	*mwmh;

	if (xu_getprop(cc->win,
	    cwmh[_MOTIF_WM_HINTS].atom, cwmh[_MOTIF_WM_HINTS].atom,
	    PROP_MWM_HINTS_ELEMENTS, (u_char **)&mwmh) == MWM_NUMHINTS)
		if (mwmh->flags & MWM_HINTS_DECORATIONS &&
		    !(mwmh->decorations & MWM_DECOR_ALL) &&
		    !(mwmh->decorations & MWM_DECOR_BORDER))
			cc->bwidth = 0;
}

void
client_transient(struct client_ctx *cc)
{
	struct client_ctx	*tc;
	Window			 trans;

	if (XGetTransientForHint(X_Dpy, cc->win, &trans)) {
		if ((tc = client_find(trans)) && tc->group) {
			group_movetogroup(cc, tc->group->shortcut);
			if (tc->flags & CLIENT_IGNORE)
				cc->flags |= CLIENT_IGNORE;
		}
	}
}

static int
client_inbound(struct client_ctx *cc, int x, int y)
{
	return (x < cc->geom.w && x >= 0 &&
	    y < cc->geom.h && y >= 0);
}

int
client_snapcalc(int n0, int n1, int e0, int e1, int snapdist)
{
	int	 s0, s1;

	s0 = s1 = 0;

	if (abs(e0 - n0) <= snapdist)
		s0 = e0 - n0;

	if (abs(e1 - n1) <= snapdist)
		s1 = e1 - n1;

	/* possible to snap in both directions */
	if (s0 != 0 && s1 != 0)
		if (abs(s0) < abs(s1))
			return (s0);
		else
			return (s1);
	else if (s0 != 0)
		return (s0);
	else if (s1 != 0)
		return (s1);
	else
		return (0);
}

void
client_htile(struct client_ctx *cc)
{
	struct client_ctx	*ci;
	struct group_ctx 	*gc = cc->group;
	struct screen_ctx 	*sc = cc->sc;
	struct geom 		 xine;
	int 			 i, n, mh, x, h, w;

	if (!gc)
		return;
	i = n = 0;

	TAILQ_FOREACH(ci, &gc->clients, group_entry) {
		if (ci->flags & CLIENT_HIDDEN ||
		    ci->flags & CLIENT_IGNORE || (ci == cc))
			continue;
		n++;
	}
	if (n == 0)
		return;

	xine = screen_find_xinerama(sc,
	    cc->geom.x + cc->geom.w / 2,
	    cc->geom.y + cc->geom.h / 2);

	if (cc->flags & CLIENT_VMAXIMIZED ||
	    cc->geom.h + (cc->bwidth * 2) >= xine.h)
		return;

	cc->flags &= ~CLIENT_HMAXIMIZED;
	cc->geom.x = xine.x;
	cc->geom.y = xine.y;
	cc->geom.w = xine.w - (cc->bwidth * 2);
	client_resize(cc, 1);
	client_ptrwarp(cc);

	mh = cc->geom.h + (cc->bwidth * 2);
	x = xine.x;
	w = xine.w / n;
	h = xine.h - mh;
	TAILQ_FOREACH(ci, &gc->clients, group_entry) {
		if (ci->flags & CLIENT_HIDDEN ||
		    ci->flags & CLIENT_IGNORE || (ci == cc))
			continue;
		ci->bwidth = Conf.bwidth;
		ci->geom.y = xine.y + mh;
		ci->geom.x = x;
		ci->geom.h = h - (ci->bwidth * 2);
		ci->geom.w = w - (ci->bwidth * 2);
		if (i + 1 == n)
			ci->geom.w = xine.x + xine.w -
			    ci->geom.x - (ci->bwidth * 2);
		x += w;
		client_resize(ci, 1);
		i++;
	}
}

void
client_vtile(struct client_ctx *cc)
{
	struct client_ctx	*ci;
	struct group_ctx 	*gc = cc->group;
	struct screen_ctx 	*sc = cc->sc;
	struct geom 		 xine;
	int 			 i, n, mw, y, h, w;

	if (!gc)
		return;
	i = n = 0;

	TAILQ_FOREACH(ci, &gc->clients, group_entry) {
		if (ci->flags & CLIENT_HIDDEN ||
		    ci->flags & CLIENT_IGNORE || (ci == cc))
			continue;
		n++;
	}
	if (n == 0)
		return;

	xine = screen_find_xinerama(sc,
	    cc->geom.x + cc->geom.w / 2,
	    cc->geom.y + cc->geom.h / 2);

	if (cc->flags & CLIENT_HMAXIMIZED ||
	    cc->geom.w + (cc->bwidth * 2) >= xine.w)
		return;

	cc->flags &= ~CLIENT_VMAXIMIZED;
	cc->geom.x = xine.x;
	cc->geom.y = xine.y;
	cc->geom.h = xine.h - (cc->bwidth * 2);
	client_resize(cc, 1);
	client_ptrwarp(cc);

	mw = cc->geom.w + (cc->bwidth * 2);
	y = xine.y;
	h = xine.h / n;
	w = xine.w - mw;
	TAILQ_FOREACH(ci, &gc->clients, group_entry) {
		if (ci->flags & CLIENT_HIDDEN ||
		    ci->flags & CLIENT_IGNORE || (ci == cc))
			continue;
		ci->bwidth = Conf.bwidth;
		ci->geom.y = y;
		ci->geom.x = xine.x + mw;
		ci->geom.h = h - (ci->bwidth * 2);
		ci->geom.w = w - (ci->bwidth * 2);
		if (i + 1 == n)
			ci->geom.h = xine.y + xine.h -
			    ci->geom.y - (ci->bwidth * 2);
		y += h;
		client_resize(ci, 1);
		i++;
	}
}

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
 * $Id$
 */

#include "headers.h"
#include "calmwm.h"

static int		 _client_inbound(struct client_ctx *, int, int);

static char		 emptystring[] = "";
struct client_ctx	*_curcc = NULL;

void
client_setup(void)
{
	TAILQ_INIT(&Clientq);
}

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
	XWindowAttributes	 wattr;
	XWMHints		*wmhints;
	long			 tmp;
	int			 state;

	if (win == None)
		return (NULL);

	XCALLOC(cc, struct client_ctx);

	XGrabServer(X_Dpy);

	cc->state = mapped ? NormalState : IconicState;
	cc->sc = sc;
	cc->win = win;
	cc->size = XAllocSizeHints();
	XGetWMNormalHints(X_Dpy, cc->win, cc->size, &tmp);
	if (cc->size->width_inc == 0)
		cc->size->width_inc = 1;
	if (cc->size->height_inc == 0)
		cc->size->height_inc = 1;

	TAILQ_INIT(&cc->nameq);
	client_setname(cc);

	/*
	 * conf_client() needs at least cc->win and cc->name
	 */
	conf_client(cc);

	XGetWindowAttributes(X_Dpy, cc->win, &wattr);

	if (cc->size->flags & PBaseSize) {
		cc->geom.min_dx = cc->size->base_width;
		cc->geom.min_dy = cc->size->base_height;
	} else if (cc->size->flags & PMinSize) {
		cc->geom.min_dx = cc->size->min_width;
		cc->geom.min_dy = cc->size->min_height;
	}

	/* Saved pointer position */
	cc->ptr.x = -1;
	cc->ptr.y = -1;

	cc->geom.x = wattr.x;
	cc->geom.y = wattr.y;
	cc->geom.width = wattr.width;
	cc->geom.height = wattr.height;
	cc->cmap = wattr.colormap;

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

	/* Notify client of its configuration. */
	xev_reconfig(cc);

	if (state == IconicState)
		client_hide(cc);
	else
		client_unhide(cc);

	xu_setstate(cc, cc->state);

	XSync(X_Dpy, False);
	XUngrabServer(X_Dpy);

	TAILQ_INSERT_TAIL(&sc->mruq, cc, mru_entry);
	TAILQ_INSERT_TAIL(&Clientq, cc, entry);

	client_gethints(cc);
	client_update(cc);

	if (mapped)
		group_autogroup(cc);

	return (cc);
}

int
client_delete(struct client_ctx *cc)
{
	struct screen_ctx	*sc = CCTOSC(cc);
	struct winname		*wn;

	group_client_delete(cc);

	XGrabServer(X_Dpy);
	xu_setstate(cc, WithdrawnState);
	XRemoveFromSaveSet(X_Dpy, cc->win);

	XSync(X_Dpy, False);
	XUngrabServer(X_Dpy);

	TAILQ_REMOVE(&sc->mruq, cc, mru_entry);
	TAILQ_REMOVE(&Clientq, cc, entry);

	if (_curcc == cc)
		_curcc = NULL;

	XFree(cc->size);

	while ((wn = TAILQ_FIRST(&cc->nameq)) != NULL) {
		TAILQ_REMOVE(&cc->nameq, wn, entry);
		if (wn->name != emptystring)
			XFree(wn->name);
		xfree(wn);
	}

	client_freehints(cc);

	xfree(cc);

	return (0);
}

void
client_leave(struct client_ctx *cc)
{
	struct screen_ctx	*sc;

	if (cc == NULL)
		cc = _curcc;
	if (cc == NULL)
		return;

	sc = CCTOSC(cc);
	xu_btn_ungrab(sc->rootwin, AnyModifier, Button1);
}

void
client_setactive(struct client_ctx *cc, int fg)
{
	struct screen_ctx	*sc;

	if (cc == NULL)
		cc = _curcc;
	if (cc == NULL)
		return;

	sc = CCTOSC(cc);

	if (fg) {
		XInstallColormap(X_Dpy, cc->cmap);
		XSetInputFocus(X_Dpy, cc->win,
		    RevertToPointerRoot, CurrentTime);
		conf_grab_mouse(cc);
		/*
		 * If we're in the middle of alt-tabbing, don't change
		 * the order please.
		 */
		if (!sc->altpersist)
			client_mtf(cc);
	} else
		client_leave(cc);

	if (fg && _curcc != cc) {
		client_setactive(NULL, 0);
		_curcc = cc;
	}

	cc->active = fg;
	client_draw_border(cc);
}

struct client_ctx *
client_current(void)
{
	return (_curcc);
}

void
client_maximize(struct client_ctx *cc)
{
	struct screen_ctx	*sc = CCTOSC(cc);
	int			 xmax = sc->xmax, ymax = sc->ymax;
	int			 x_org = 0, y_org = 0;

	if (cc->flags & CLIENT_MAXIMIZED) {
		cc->geom = cc->savegeom;
	} else {
		if (!(cc->flags & CLIENT_VMAXIMIZED))
			cc->savegeom = cc->geom;
		if (HasXinerama) {
			XineramaScreenInfo *xine;
			/*
			 * pick screen that the middle of the window is on.
			 * that's probably more fair than if just the origin of
			 * a window is poking over a boundary
			 */
			xine = screen_find_xinerama(CCTOSC(cc),
			    cc->geom.x + cc->geom.width / 2,
			    cc->geom.y + cc->geom.height / 2);
			if (xine == NULL)
				goto calc;
			x_org = xine->x_org;
			y_org = xine->y_org;
			xmax = xine->width;
			ymax = xine->height;
		}
calc:
		cc->geom.x = x_org + Conf.gap_left;
		cc->geom.y = y_org + Conf.gap_top;
		cc->geom.height = ymax - (Conf.gap_top + Conf.gap_bottom);
		cc->geom.width = xmax - (Conf.gap_left + Conf.gap_right);
		cc->flags |= CLIENT_DOMAXIMIZE;
	}

	client_resize(cc);
}

void
client_vertmaximize(struct client_ctx *cc)
{
	struct screen_ctx	*sc = CCTOSC(cc);
	int			 y_org = 0, ymax = sc->ymax;

	if (cc->flags & CLIENT_VMAXIMIZED) {
		cc->geom = cc->savegeom;
	} else {
		if (!(cc->flags & CLIENT_MAXIMIZED))
			cc->savegeom = cc->geom;
		if (HasXinerama) {
			XineramaScreenInfo *xine;
			xine = screen_find_xinerama(CCTOSC(cc),
			    cc->geom.x + cc->geom.width / 2,
			    cc->geom.y + cc->geom.height / 2);
			if (xine == NULL)
				goto calc;
			y_org = xine->y_org;
			ymax = xine->height;
		}
calc:
		cc->geom.y = y_org + cc->bwidth + Conf.gap_top;
		cc->geom.height = ymax - (cc->bwidth * 2) - (Conf.gap_top +
		    Conf.gap_bottom);
		cc->flags |= CLIENT_DOVMAXIMIZE;
	}

	client_resize(cc);
}

void
client_resize(struct client_ctx *cc)
{
	if (cc->flags & (CLIENT_MAXIMIZED | CLIENT_VMAXIMIZED))
		cc->flags &= ~(CLIENT_MAXIMIZED | CLIENT_VMAXIMIZED);

	if (cc->flags & CLIENT_DOMAXIMIZE) {
		cc->flags &= ~CLIENT_DOMAXIMIZE;
		cc->flags |= CLIENT_MAXIMIZED;
	} else if (cc->flags & CLIENT_DOVMAXIMIZE) {
		cc->flags &= ~CLIENT_DOVMAXIMIZE;
		cc->flags |= CLIENT_VMAXIMIZED;
	}

	XMoveResizeWindow(X_Dpy, cc->win, cc->geom.x - cc->bwidth,
	    cc->geom.y - cc->bwidth, cc->geom.width, cc->geom.height);
	xev_reconfig(cc);
}

void
client_move(struct client_ctx *cc)
{
	XMoveWindow(X_Dpy, cc->win,
	    cc->geom.x - cc->bwidth, cc->geom.y - cc->bwidth);
	xev_reconfig(cc);
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
		x = cc->geom.width / 2;
		y = cc->geom.height / 2;
	}

	if (cc->state == IconicState)
		client_unhide(cc);
	else
		client_raise(cc);

	xu_ptr_setpos(cc->win, x, y);
}

void
client_ptrsave(struct client_ctx *cc)
{
	int	 x, y;

	xu_ptr_getpos(cc->win, &x, &y);
	if (_client_inbound(cc, x, y)) {
		cc->ptr.x = x;
		cc->ptr.y = y;
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

	if (cc == _curcc)
		_curcc = NULL;
}

void
client_unhide(struct client_ctx *cc)
{
	XMapRaised(X_Dpy, cc->win);

	cc->highlight = 0;
	cc->flags &= ~CLIENT_HIDDEN;
	xu_setstate(cc, NormalState);
}

void
client_draw_border(struct client_ctx *cc)
{
	struct screen_ctx	*sc = CCTOSC(cc);
	u_long			 pixl;

	if (cc->active)
		switch (cc->highlight) {
		case CLIENT_HIGHLIGHT_BLUE:
			pixl = sc->bluepixl;
			break;
		case CLIENT_HIGHLIGHT_RED:
			pixl = sc->redpixl;
			break;
		default:
			pixl = sc->whitepixl;
			break;
		}
	else
		pixl = sc->graypixl;
	XSetWindowBorderWidth(X_Dpy, cc->win, cc->bwidth);
	XSetWindowBorder(X_Dpy, cc->win, pixl);
}

void
client_update(struct client_ctx *cc)
{
	Atom	*p, wm_delete, wm_protocols, wm_take_focus;
	int	 i;
	long	 n;

	/* XXX cache these. */
	wm_delete = XInternAtom(X_Dpy, "WM_DELETE_WINDOW", False);
	wm_protocols = XInternAtom(X_Dpy, "WM_PROTOCOLS", False);
	wm_take_focus = XInternAtom(X_Dpy, "WM_TAKE_FOCUS", False);

	if ((n = xu_getprop(cc, wm_protocols,
		 XA_ATOM, 20L, (u_char **)&p)) <= 0)
		return;

	for (i = 0; i < n; i++)
		if (p[i] == wm_delete)
			cc->xproto |= CLIENT_PROTO_DELETE;
		else if (p[i] == wm_take_focus)
			cc->xproto |= CLIENT_PROTO_TAKEFOCUS;

	XFree(p);
}

void
client_send_delete(struct client_ctx *cc)
{
	Atom	 wm_delete, wm_protocols;

	/* XXX - cache */
	wm_delete = XInternAtom(X_Dpy, "WM_DELETE_WINDOW", False);
	wm_protocols = XInternAtom(X_Dpy, "WM_PROTOCOLS", False);

	if (cc->xproto & CLIENT_PROTO_DELETE)
		xu_sendmsg(cc, wm_protocols, wm_delete);
	else
		XKillClient(X_Dpy, cc->win);
}

void
client_setname(struct client_ctx *cc)
{
	struct winname	*wn;
	char		*newname;

	XFetchName(X_Dpy, cc->win, &newname);
	if (newname == NULL)
		newname = emptystring;

	TAILQ_FOREACH(wn, &cc->nameq, entry)
		if (strcmp(wn->name, newname) == 0) {
			/* Move to the last since we got a hit. */
			TAILQ_REMOVE(&cc->nameq, wn, entry);
			TAILQ_INSERT_TAIL(&cc->nameq, wn, entry);
			goto match;
		}

	XMALLOC(wn, struct winname);
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
		if (wn->name != emptystring)
			XFree(wn->name);
		xfree(wn);
		cc->nameqlen--;
	}

	return;
}

struct client_ctx *
client_cycle(int reverse)
{
	struct client_ctx	*oldcc, *newcc;
	struct screen_ctx	*sc;
	int			 again = 1;

	oldcc = client_current();
	sc = screen_current();

	/* If no windows then you cant cycle */
	if (TAILQ_EMPTY(&sc->mruq))
		return (NULL);

	if (oldcc == NULL)
		oldcc = (reverse ? TAILQ_LAST(&sc->mruq, cycle_entry_q) :
		    TAILQ_FIRST(&sc->mruq));

	newcc = oldcc;
	while (again) {
		again = 0;

		newcc = (reverse ? client_mruprev(newcc) :
		    client_mrunext(newcc));

		/* Only cycle visible and non-ignored windows. */
		if (newcc->flags & (CLIENT_HIDDEN|CLIENT_IGNORE))
			again = 1;

		/* Is oldcc the only non-hidden window? */
		if (newcc == oldcc) {
			if (again)
				return (NULL);	/* No windows visible. */

			break;
		}
	}

	/* reset when alt is released. XXX I hate this hack */
	sc->altpersist = 1;
	client_ptrsave(oldcc);
	client_ptrwarp(newcc);

	return (newcc);
}

struct client_ctx *
client_mrunext(struct client_ctx *cc)
{
	struct screen_ctx	*sc = CCTOSC(cc);
	struct client_ctx	*ccc;

	return ((ccc = TAILQ_NEXT(cc, mru_entry)) != NULL ?
	    ccc : TAILQ_FIRST(&sc->mruq));
}

struct client_ctx *
client_mruprev(struct client_ctx *cc)
{
	struct screen_ctx	*sc = CCTOSC(cc);
	struct client_ctx	*ccc;

	return ((ccc = TAILQ_PREV(cc, cycle_entry_q, mru_entry)) != NULL ?
	    ccc : TAILQ_LAST(&sc->mruq, cycle_entry_q));
}

void
client_placecalc(struct client_ctx *cc)
{
	struct screen_ctx	*sc = CCTOSC(cc);
	int			 yslack, xslack;

	if (cc->size->flags & USPosition) {
		/*
		 * Ignore XINERAMA screens, just make sure it's somewhere
		 * in the virtual desktop. else it stops people putting xterms
		 * at startup in the screen the mouse doesn't start in *sigh*.
		 * XRandR bits mean that {x,y}max shouldn't be outside what's
		 * currently there.
		 */
		yslack = sc->ymax - cc->geom.height - cc->bwidth;
		xslack = sc->xmax - cc->geom.width - cc->bwidth;
		if (cc->size->x >= 0)
			cc->geom.x = MAX(MIN(cc->size->x, xslack), cc->bwidth);
		else
			cc->geom.x = cc->bwidth;
		if (cc->size->y >= 0)
			cc->geom.y = MAX(MIN(cc->size->y, yslack), cc->bwidth);
		else 
			cc->geom.y = cc->bwidth;
	} else {
		XineramaScreenInfo	*info;
		int			 xmouse, ymouse, xorig, yorig;
		int			 xmax, ymax;

		xu_ptr_getpos(sc->rootwin, &xmouse, &ymouse);
		if (HasXinerama) {
			info = screen_find_xinerama(sc, xmouse, ymouse);
			if (info == NULL)
				goto noxine;
			xorig = info->x_org;
			yorig = info->y_org;
			xmax = xorig + info->width;
			ymax = yorig + info->height;
		} else {
noxine:
			xorig = yorig = 0;
			xmax = sc->xmax;
			ymax = sc->ymax;
		}
		xmouse = MAX(xmouse, xorig + cc->bwidth) - cc->geom.width / 2;
		ymouse = MAX(ymouse, yorig + cc->bwidth) - cc->geom.height / 2;

		xmouse = MAX(xmouse, xorig + (int)cc->bwidth);
		ymouse = MAX(ymouse, yorig + (int)cc->bwidth);

		xslack = xmax - cc->geom.width - cc->bwidth;
		yslack = ymax - cc->geom.height - cc->bwidth;
		if (xslack >= xorig) {
			cc->geom.x = MAX(MIN(xmouse, xslack),
			    xorig + Conf.gap_left + cc->bwidth);
			if (cc->geom.x > (xslack - Conf.gap_right))
				cc->geom.x -= Conf.gap_right;
		} else {
			cc->geom.x = xorig + cc->bwidth + Conf.gap_left;
			cc->geom.width = xmax - Conf.gap_left;
		}
		if (yslack >= yorig) {
			cc->geom.y = MAX(MIN(ymouse, yslack),
			    yorig + Conf.gap_top + cc->bwidth);
			if (cc->geom.y > (yslack - Conf.gap_bottom))
				cc->geom.y -= Conf.gap_bottom;
		} else {
			cc->geom.y = yorig + cc->bwidth + Conf.gap_top;
			cc->geom.height = ymax - Conf.gap_top;
		}
	}
}

void
client_mtf(struct client_ctx *cc)
{
	struct screen_ctx	*sc;

	if (cc == NULL)
		cc = _curcc;
	if (cc == NULL)
		return;

	sc = CCTOSC(cc);

	/* Move to front. */
	TAILQ_REMOVE(&sc->mruq, cc, mru_entry);
	TAILQ_INSERT_HEAD(&sc->mruq, cc, mru_entry);
}

void
client_gethints(struct client_ctx *cc)
{
	XClassHint		 xch;
	int			 argc;
	char			**argv;
	Atom			 mha;
	struct mwm_hints	*mwmh;

	if (XGetClassHint(X_Dpy, cc->win, &xch)) {
		if (xch.res_name != NULL)
			cc->app_name = xch.res_name;
		if (xch.res_class != NULL)
			cc->app_class = xch.res_class;
	}

	mha = XInternAtom(X_Dpy, "_MOTIF_WM_HINTS", False);
	if (xu_getprop(cc, mha, mha, PROP_MWM_HINTS_ELEMENTS,
	    (u_char **)&mwmh) == MWM_NUMHINTS)
		if (mwmh->flags & MWM_HINTS_DECORATIONS &&
		    !(mwmh->decorations & MWM_DECOR_ALL) &&
		    !(mwmh->decorations & MWM_DECOR_BORDER))
			cc->bwidth = 0;
	if (XGetCommand(X_Dpy, cc->win, &argv, &argc)) {
#define MAX_ARGLEN 512
#define ARG_SEP_ " "
		int	 i, o, len = MAX_ARGLEN;
		char	*buf;

		buf = xmalloc(len);
		buf[0] = '\0';

		for (o = 0, i = 0; o < len && i < argc; i++) {
			if (argv[i] == NULL)
				break;
			strlcat(buf, argv[i], len);
			o += strlen(buf);
			strlcat(buf, ARG_SEP_, len);
			o += strlen(ARG_SEP_);
		}

		if (strlen(buf) > 0)
			cc->app_cliarg = buf;

		XFreeStringList(argv);
	}
}

void
client_freehints(struct client_ctx *cc)
{
	if (cc->app_name != NULL)
		XFree(cc->app_name);
	if (cc->app_class != NULL)
		XFree(cc->app_class);
	if (cc->app_cliarg != NULL)
		xfree(cc->app_cliarg);
}

static int
_client_inbound(struct client_ctx *cc, int x, int y)
{
	return (x < cc->geom.width && x >= 0 &&
	    y < cc->geom.height && y >= 0);
}

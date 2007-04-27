/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2004 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id$
 */

#include "headers.h"
#include "calmwm.h"

static struct client_ctx *client__cycle(struct client_ctx *cc,
    struct client_ctx *(*iter)(struct client_ctx *));
int _inwindowbounds(struct client_ctx *, int, int);

static char emptystring[] = "";

struct client_ctx *_curcc = NULL;

void
client_setup(void)
{
	TAILQ_INIT(&G_clientq);
}

struct client_ctx *
client_find(Window win)
{
	struct client_ctx *cc;

	TAILQ_FOREACH(cc, &G_clientq, entry)
		if (cc->pwin == win || cc->win == win)
			return (cc);

	return (NULL);
}

struct client_ctx *
client_new(Window win, struct screen_ctx *sc, int mapped)
{
	struct client_ctx *cc;
	long tmp;
	XSetWindowAttributes pxattr;
 	XWindowAttributes wattr;
	int x, y, height, width, state;
	XWMHints *wmhints;

	if (win == None)
		return (NULL);

	XCALLOC(cc, struct client_ctx);

	XGrabServer(G_dpy);

	cc->state = mapped ? NormalState : IconicState;
	cc->sc = sc;
	cc->win = win;
	cc->size= XAllocSizeHints();
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

	XGetWMNormalHints(G_dpy, cc->win, cc->size, &tmp);
	XGetWindowAttributes(G_dpy, cc->win, &wattr);

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

	client_gravitate(cc, 1);

	cc->geom.x = wattr.x;
	cc->geom.y = wattr.y;
	cc->geom.width = wattr.width;
	cc->geom.height = wattr.height;
	cc->geom.height = wattr.height;
	cc->cmap = wattr.colormap;

	if (wattr.map_state != IsViewable) {
		client_placecalc(cc);
		if ((wmhints = XGetWMHints(G_dpy, cc->win)) != NULL) {
			if (wmhints->flags & StateHint)
				xu_setstate(cc, wmhints->initial_state);

			XFree(wmhints);
		}
	}

	if (xu_getstate(cc, &state) < 0)
		state = NormalState;

	XSelectInput(G_dpy, cc->win,
	    ColormapChangeMask|EnterWindowMask|PropertyChangeMask|KeyReleaseMask);

	x = cc->geom.x - cc->bwidth;
	y = cc->geom.y - cc->bwidth;

	width = cc->geom.width;
	height = cc->geom.height;
	if (cc->bwidth > 1) {
		width += (cc->bwidth)*2;
		height += (cc->bwidth)*2;
	}

	pxattr.override_redirect = True;
	pxattr.background_pixel = sc->bgcolor.pixel;
	pxattr.event_mask =
	    ChildMask|ButtonPressMask|ButtonReleaseMask|
	    ExposureMask|EnterWindowMask;
/* 	pxattr.border_pixel = sc->blackpix; */
/* 	pxattr.background_pixel = sc->whitepix; */
	

/* 	cc->pwin = XCreateSimpleWindow(G_dpy, sc->rootwin, */
/* 	    x, y, width, height, 1, sc->blackpix, sc->whitepix); */

	cc->pwin = XCreateWindow(G_dpy, sc->rootwin, x, y,
	    width, height, 0,	/* XXX */
	    DefaultDepth(G_dpy, sc->which), CopyFromParent,
	    DefaultVisual(G_dpy, sc->which),
	    CWOverrideRedirect | CWBackPixel | CWEventMask, &pxattr);

	if (G_doshape) {
		XRectangle *r;
		int n, tmp;

		XShapeSelectInput(G_dpy, cc->win, ShapeNotifyMask);

		r = XShapeGetRectangles(G_dpy, cc->win, ShapeBounding, &n, &tmp);
		if (n > 1)
			XShapeCombineShape(G_dpy, cc->pwin, ShapeBounding,
			    0,	0, /* XXX border */
			    cc->win, ShapeBounding, ShapeSet);
		XFree(r);
	}

	cc->active = 0;
	client_draw_border(cc);

	XAddToSaveSet(G_dpy, cc->win);
	XSetWindowBorderWidth(G_dpy, cc->win, 0);
	XReparentWindow(G_dpy, cc->win, cc->pwin, cc->bwidth, cc->bwidth);

	/* Notify client of its configuration. */
	xev_reconfig(cc);

	XMapRaised(G_dpy, cc->pwin);
	XMapWindow(G_dpy, cc->win);
	xu_setstate(cc, cc->state);

	XSync(G_dpy, False);
	XUngrabServer(G_dpy);

	TAILQ_INSERT_TAIL(&sc->mruq, cc, mru_entry);
	TAILQ_INSERT_TAIL(&G_clientq, cc, entry);

	client_gethints(cc);
	client_update(cc);
	
	if (mapped) {
		if (G_conf.flags & CONF_STICKY_GROUPS)
			group_sticky(cc);
		else
			group_autogroup(cc);
	}

	return (cc);
}

int
client_delete(struct client_ctx *cc, int sendevent, int ignorewindow)
{
	struct screen_ctx *sc = CCTOSC(cc);
	struct winname *wn;

	if (cc->state == IconicState && !sendevent)
		return (1);

	group_client_delete(cc);
	XGrabServer(G_dpy);

	xu_setstate(cc, WithdrawnState);
	XRemoveFromSaveSet(G_dpy, cc->win);

	if (!ignorewindow) {
		client_gravitate(cc, 0);
		XSetWindowBorderWidth(G_dpy, cc->win, 1);	/* XXX */
		XReparentWindow(G_dpy, cc->win,
		    sc->rootwin, cc->geom.x, cc->geom.y);
	}
	if (cc->pwin)
		XDestroyWindow(G_dpy, cc->pwin);

	XSync(G_dpy, False);
	XUngrabServer(G_dpy);

	TAILQ_REMOVE(&sc->mruq, cc, mru_entry);
	TAILQ_REMOVE(&G_clientq, cc, entry);

	if (_curcc == cc)
		_curcc = NULL;

	if (sc->cycle_client == cc)
		sc->cycle_client = NULL;

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
	struct screen_ctx *sc;

	if (cc == NULL)
		cc = _curcc;
	if (cc == NULL)
		return;
	sc = CCTOSC(cc);

	xu_btn_ungrab(sc->rootwin, AnyModifier, Button1);
}

void
client_nocurrent(void)
{
	if (_curcc != NULL)
		client_setactive(_curcc, 0);

	_curcc = NULL;
}

void
client_setactive(struct client_ctx *cc, int fg)
{
	struct screen_ctx* sc;

	if (cc == NULL)
		cc = _curcc;
	if (cc == NULL)
		return;

	sc = CCTOSC(cc);

	if (fg) {
		XInstallColormap(G_dpy, cc->cmap);
		XSetInputFocus(G_dpy, cc->win,
		    RevertToPointerRoot, CurrentTime);
		xu_btn_grab(cc->pwin, Mod1Mask, AnyButton);
		xu_btn_grab(cc->pwin, ControlMask|Mod1Mask, Button1);
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
client_gravitate(struct client_ctx *cc, int yes)
{
        int dx = 0, dy = 0, mult = yes ? 1 : -1;
        int gravity = (cc->size->flags & PWinGravity) ?
	    cc->size->win_gravity : NorthWestGravity;

        switch (gravity) {
	case NorthWestGravity:
	case SouthWestGravity:
	case NorthEastGravity:
	case StaticGravity:
		dx = cc->bwidth;
	case NorthGravity:
		dy = cc->bwidth;
		break;
        }

        cc->geom.x += mult*dx;
        cc->geom.y += mult*dy;
}

void
client_maximize(struct client_ctx *cc)
{
	if (cc->flags & CLIENT_MAXIMIZED) {
		cc->flags &= ~CLIENT_MAXIMIZED;
		cc->geom = cc->savegeom;
	} else {
		XWindowAttributes rootwin_geom;
		struct screen_ctx *sc = CCTOSC(cc);

		XGetWindowAttributes(G_dpy, sc->rootwin, &rootwin_geom);
		cc->savegeom = cc->geom;
		cc->geom.x = 0;
		cc->geom.y = 0;
		cc->geom.height = rootwin_geom.height;
		cc->geom.width = rootwin_geom.width;
		cc->flags |= CLIENT_MAXIMIZED;
	}

	client_resize(cc);
}

void
client_push_geometry(struct client_ctx *cc)
{
       cc->savegeom = cc->geom;
}

void
client_restore_geometry(struct client_ctx *cc)
{
       cc->geom = cc->savegeom;
       client_resize(cc);
}

void
client_resize(struct client_ctx *cc)
{
	XMoveResizeWindow(G_dpy, cc->pwin, cc->geom.x - cc->bwidth,
	    cc->geom.y - cc->bwidth, cc->geom.width + cc->bwidth*2,
	    cc->geom.height + cc->bwidth*2);
	XMoveResizeWindow(G_dpy, cc->win, cc->bwidth, cc->bwidth,
	    cc->geom.width, cc->geom.height);
	xev_reconfig(cc);
	client_draw_border(cc);
}

void
client_move(struct client_ctx *cc)
{
	XMoveWindow(G_dpy, cc->pwin,
	    cc->geom.x - cc->bwidth, cc->geom.y - cc->bwidth);
	xev_reconfig(cc);
}

void
client_lower(struct client_ctx *cc)
{
	XLowerWindow(G_dpy, cc->pwin);
}

void
client_raise(struct client_ctx *cc)
{
	XRaiseWindow(G_dpy, cc->pwin);
	client_draw_border(cc);
}

void
client_warp(struct client_ctx *cc)
{
	client_raise(cc);
	xu_ptr_setpos(cc->pwin, 0, 0);
}

void
client_ptrwarp(struct client_ctx *cc)
{
	int x = cc->ptr.x, y = cc->ptr.y;

	if (x == -1 || y == -1) {
		x = cc->geom.width / 2;
		y = cc->geom.height / 2;
	}

	client_raise(cc);
	xu_ptr_setpos(cc->pwin, x, y);
}

void
client_ptrsave(struct client_ctx *cc)
{
	int x, y;

	xu_ptr_getpos(cc->pwin, &x, &y);
        if (_inwindowbounds(cc, x, y)) {
		cc->ptr.x = x;
		cc->ptr.y = y;
        }
}

void
client_hide(struct client_ctx *cc)
{
	/* XXX - add wm_state stuff */
	XUnmapWindow(G_dpy, cc->pwin);
	XUnmapWindow(G_dpy, cc->win);

	cc->active = 0;
	cc->flags |= CLIENT_HIDDEN;
	xu_setstate(cc, IconicState);

	if (cc == _curcc)
		_curcc = NULL;
}

void
client_unhide(struct client_ctx *cc)
{
	XMapWindow(G_dpy, cc->win);
	XMapRaised(G_dpy, cc->pwin);

	cc->flags &= ~CLIENT_HIDDEN;
	xu_setstate(cc, NormalState);
}

void
client_draw_border(struct client_ctx *cc)
{
	struct screen_ctx *sc = CCTOSC(cc);

	if (cc->active) {
		XSetWindowBackground(G_dpy, cc->pwin, client_bg_pixel(cc));
		XClearWindow(G_dpy, cc->pwin);

		if (!cc->highlight && cc->bwidth > 1)
			XDrawRectangle(G_dpy, cc->pwin, sc->gc, 1, 1,
			    cc->geom.width + cc->bwidth,
			    cc->geom.height + cc->bwidth);
	} else {
		XSetWindowBackgroundPixmap(G_dpy, cc->pwin,
		    client_bg_pixmap(cc));
		if (cc->bwidth > 1)
			XSetWindowBackgroundPixmap(G_dpy,
			    cc->pwin, client_bg_pixmap(cc));

		XClearWindow(G_dpy, cc->pwin);
	}
}

u_long
client_bg_pixel(struct client_ctx *cc)
{
	struct screen_ctx *sc = CCTOSC(cc);
	u_long pixl;

	switch (cc->highlight) {
	case CLIENT_HIGHLIGHT_BLUE:
		pixl = sc->bluepixl;
		break;
	case CLIENT_HIGHLIGHT_RED:
		pixl = sc->redpixl;
		break;
	default:
		pixl = sc->blackpixl;
		break;
	}

	return (pixl);
}

Pixmap
client_bg_pixmap(struct client_ctx *cc)
{
	struct screen_ctx *sc = CCTOSC(cc);
	Pixmap pix;

	switch (cc->highlight) {
	case CLIENT_HIGHLIGHT_BLUE:
		pix = sc->blue;
		break;
	case CLIENT_HIGHLIGHT_RED:
		pix = sc->red;
		break;
	default:
		pix = sc->gray;
		break;
	}

	return (pix);
}

void
client_update(struct client_ctx *cc)
{
	Atom *p, wm_delete, wm_protocols, wm_take_focus;
	int i;
	long n;

	/* XXX cache these. */
	wm_delete = XInternAtom(G_dpy, "WM_DELETE_WINDOW", False);
	wm_protocols = XInternAtom(G_dpy, "WM_PROTOCOLS", False);
	wm_take_focus = XInternAtom(G_dpy, "WM_TAKE_FOCUS", False);

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
	Atom wm_delete, wm_protocols;

	/* XXX - cache */
	wm_delete = XInternAtom(G_dpy, "WM_DELETE_WINDOW", False);
	wm_protocols = XInternAtom(G_dpy, "WM_PROTOCOLS", False);

	if (cc->xproto & CLIENT_PROTO_DELETE)
		xu_sendmsg(cc, wm_protocols, wm_delete);
	else
		XKillClient(G_dpy, cc->win);
}

void
client_setname(struct client_ctx *cc)
{
	char *newname;
	struct winname *wn;

	XFetchName(G_dpy, cc->win, &newname);
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

/*
 * TODO: seems to have some issues still on the first invocation
 * (globally the first)
 */

struct client_ctx *
client_cyclenext(struct client_ctx *cc, int reverse)
{
	struct screen_ctx *sc = CCTOSC(cc);
	struct client_ctx *(*iter)(struct client_ctx *) =
	    reverse ? &client_mruprev : &client_mrunext;

	/* TODO: maybe this should just be a CIRCLEQ. */

	/* if altheld; then reset the iterator to the beginning */
	if (!sc->altpersist || sc->cycle_client == NULL)
		sc->cycle_client = TAILQ_FIRST(&sc->mruq);

	if (sc->cycle_client == NULL)
		return (NULL);

	/*
	 * INVARIANT: as long as sc->cycle_client != NULL here, we
	 * won't exit with sc->cycle_client == NULL
	 */

	if ((sc->cycle_client = client__cycle(cc, iter)) == NULL)
		sc->cycle_client = cc;

	/* Do the actual warp. */
	client_ptrsave(cc);
	client_ptrwarp(sc->cycle_client);
	sc->altpersist = 1;   /* This is reset when alt is let go... */

	/* Draw window. */
	client_cycleinfo(sc->cycle_client);

	return (sc->cycle_client);
}

/*
 * XXX - have to have proper exposure handling here.  we will probably
 * have to do this by registering with the event loop a function to
 * redraw, then match that on windows.
 */

void
client_cycleinfo(struct client_ctx *cc)
{
	int w, h, nlines, i, n, oneh, curn = -1, x, y, diff;
	struct client_ctx *ccc, *list[3];
	struct screen_ctx *sc = CCTOSC(cc);
	struct fontdesc *font = DefaultFont;

	memset(list, 0, sizeof(list));

	nlines = 0;
	TAILQ_FOREACH(ccc, &sc->mruq, mru_entry)
		nlines++;
	nlines = MIN(nlines, 3);

	oneh = font_ascent(font) + font_descent(font) + 1;
	h = nlines*oneh;

	list[1] = cc;

	if (nlines > 1)
		list[2] = client__cycle(cc, &client_mrunext);
	if (nlines > 2)
		list[0] = client__cycle(cc, &client_mruprev);

	w = 0;
	for (i = 0; i < sizeof(list)/sizeof(list[0]); i++) {
		if ((ccc = list[i]) == NULL)
			continue;		
		w = MAX(w, font_width(font, ccc->name, strlen(ccc->name)));
	}

	w += 4;

	/* try to fit. */

	if ((x = cc->ptr.x) < 0 || (y = cc->ptr.y) < 0) {
		x = cc->geom.width / 2;
		y = cc->geom.height / 2;
	}

	if ((diff = cc->geom.width - (x + w)) < 0)
		x += diff;

	if ((diff = cc->geom.height - (y + h)) < 0)
		y += diff;

	XReparentWindow(G_dpy, sc->infowin, cc->win, 0, 0);
	XMoveResizeWindow(G_dpy, sc->infowin, x, y, w, h);
	XMapRaised(G_dpy, sc->infowin);
	XClearWindow(G_dpy, sc->infowin);

	for (i = 0, n = 0; i < sizeof(list)/sizeof(list[0]); i++) {
		if ((ccc = list[i]) == NULL)
			continue;
		font_draw(font, ccc->name, strlen(ccc->name), sc->infowin,
		    2, n*oneh + font_ascent(font) + 1);
		if (i == 1)
			curn = n;
		n++;
	}

	assert(curn != -1);

	/* Highlight the current entry. */
	XFillRectangle(G_dpy, sc->infowin, sc->hlgc, 0, curn*oneh, w, oneh);
}

struct client_ctx *
client_mrunext(struct client_ctx *cc)
{
	struct screen_ctx *sc = CCTOSC(cc);
	struct client_ctx *ccc;

	return ((ccc = TAILQ_NEXT(cc, mru_entry)) != NULL ?
	    ccc : TAILQ_FIRST(&sc->mruq));
}

struct client_ctx *
client_mruprev(struct client_ctx *cc)
{
	struct screen_ctx *sc = CCTOSC(cc);
	struct client_ctx *ccc;

	return ((ccc = TAILQ_PREV(cc, cycle_entry_q, mru_entry)) != NULL ?
	    ccc : TAILQ_LAST(&sc->mruq, cycle_entry_q));
}

static struct client_ctx *
client__cycle(struct client_ctx *cc,
    struct client_ctx *(*iter)(struct client_ctx *))
{
	struct client_ctx *save = cc;

	do {
		if (!((cc = (*iter)(cc))->flags & CLIENT_HIDDEN))
			break;
	} while (cc != save);

	return cc != save ? cc : NULL;
}

void
client_altrelease()
{
	struct client_ctx *cc = _curcc;
	struct screen_ctx *sc;

	if (cc == NULL)
		return;
	sc = CCTOSC(cc);

	XUnmapWindow(G_dpy, sc->infowin);
	XReparentWindow(G_dpy, sc->infowin, sc->rootwin, 0, 0);
}

void
client_placecalc(struct client_ctx *cc)
{
	struct screen_ctx *sc = CCTOSC(cc);
	int yslack, xslack;
	int x, y, height, width, ymax, xmax, mousex, mousey;

	y = cc->geom.y;
	x = cc->geom.x;

	height = cc->geom.height;
	width = cc->geom.width;

	ymax = DisplayHeight(G_dpy, sc->which) - cc->bwidth;
	xmax = DisplayWidth(G_dpy, sc->which) - cc->bwidth;

	yslack = ymax - cc->geom.height;
	xslack = xmax - cc->geom.width;

	xu_ptr_getpos(sc->rootwin, &mousex, &mousey);

	mousex = MAX(mousex, cc->bwidth) - cc->geom.width/2;
	mousey = MAX(mousey, cc->bwidth) - cc->geom.height/2;

	mousex = MAX(mousex, (int)cc->bwidth);
	mousey = MAX(mousey, (int)cc->bwidth);

	if (cc->size->flags & USPosition) {
		x = cc->size->x;
		if (x <= 0 || x >= xmax)
			x = cc->bwidth;
		y = cc->size->y;
		if (y <= 0 || y >= ymax)
			y = cc->bwidth;
	} else {
		if (yslack < 0) {
			y = cc->bwidth;
			height = ymax;
		} else {
			if (y == 0 || y > yslack)
				y = MIN(mousey, yslack);
			height = cc->geom.height;
		}

		if (xslack < 0) {
			x = cc->bwidth;
			width = xmax;
		} else {
			if (x == 0 || x > xslack)
				x = MIN(mousex, xslack);
			width = cc->geom.width;
		}
	}

	cc->geom.y = y;
	cc->geom.x = x;

	cc->geom.height = height;
	cc->geom.width = width;
}

void
client_vertmaximize(struct client_ctx *cc)
{
	if (cc->flags & CLIENT_MAXIMIZED) {
		cc->flags &= ~CLIENT_MAXIMIZED;
		cc->geom = cc->savegeom;
	} else {
		struct screen_ctx *sc = CCTOSC(cc);
		int display_height = DisplayHeight(G_dpy, sc->which) -
		    cc->bwidth*2;
        
		cc->savegeom = cc->geom;
		cc->geom.y = cc->bwidth;
		if (cc->geom.min_dx == 0)
			cc->geom.height = display_height; 
		else
			cc->geom.height = display_height -
			    (display_height % cc->geom.min_dx);
		cc->flags |= CLIENT_MAXIMIZED;
	}

	client_resize(cc);
}

void
client_map(struct client_ctx *cc)
{
	/* mtf? */
	client_ptrwarp(cc);
}

void
client_mtf(struct client_ctx *cc)
{
	struct screen_ctx *sc;

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
	XClassHint xch;
	int argc;
	char **argv;
	Atom mha;
	struct mwm_hints *mwmh;

	if (XGetClassHint(G_dpy, cc->win, &xch)) {
		if (xch.res_name != NULL)
			cc->app_name = xch.res_name;
		if (xch.res_class != NULL)
			cc->app_class = xch.res_class;
	}

	mha = XInternAtom(G_dpy, "_MOTIF_WM_HINTS", False);
	if (xu_getprop(cc, mha, mha, PROP_MWM_HINTS_ELEMENTS,
		(u_char **)&mwmh) == MWM_NUMHINTS)
		if (mwmh->flags & MWM_HINTS_DECORATIONS &&
		    !(mwmh->decorations & MWM_DECOR_ALL) &&
		    !(mwmh->decorations & MWM_DECOR_BORDER))
			cc->bwidth = 0;
	if (XGetCommand(G_dpy, cc->win, &argv, &argc)) {
#define MAX_ARGLEN 512
#define ARG_SEP_ " "
		int len = MAX_ARGLEN;
		int i, o;
		char *buf;

		buf = xmalloc(len);
		buf[0] = '\0';

		for (o = 0, i = 0; o < len && i < argc; i++) {
			if (argv[i] == NULL)
				break;
			strlcat(buf, argv[i], len);
			o += strlen(buf);
			strlcat(buf,  ARG_SEP_, len);
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

int
_inwindowbounds(struct client_ctx *cc, int x, int y)
{
	return (x < cc->geom.width && x >= 0 &&
	    y < cc->geom.height && y >= 0);
}

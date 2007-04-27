/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2004 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id$
 */

/*
 * NOTE:
 *   It is the responsibility of the caller to deal with memory
 *   management of the xevent's.
 */

#include "headers.h"
#include "calmwm.h"

void _sendxmsg(Window, Atom, long);

/*
 * NOTE: in reality, many of these should move to client.c now that
 * we've got this nice event layer.
 */

void
xev_handle_maprequest(struct xevent *xev, XEvent *ee)
{
	XMapRequestEvent *e = &ee->xmaprequest;
	struct client_ctx *cc = NULL, *old_cc = client_current();
	XWindowAttributes xattr;
	struct screen_ctx *sc;
#ifdef notyet
	int state;
#endif

	if (old_cc != NULL)
		client_ptrsave(old_cc);

	if ((cc = client_find(e->window)) == NULL) { 
		XGetWindowAttributes(G_dpy, e->window, &xattr);
		cc = client_new(e->window, screen_fromroot(xattr.root), 1);
		sc = CCTOSC(cc);
	} else {
		cc->beepbeep = 1;
	}

#ifdef notyet			/* XXX - possibly, we shouldn't map if
				 * the window is withdrawn. */
	if (xu_getstate(cc, &state) == 0 && state == WithdrawnState)
		warnx("WITHDRAWNSTATE for %s", cc->name);
#endif

	client_ptrwarp(cc);
	xev_register(xev);
}

void
xev_handle_unmapnotify(struct xevent *xev, XEvent *ee)
{
	XUnmapEvent *e = &ee->xunmap;
	struct client_ctx *cc;
	struct screen_ctx *sc;
	int wascurrent;

	if ((cc = client_find(e->window)) != NULL) {
		sc = CCTOSC(cc);
		wascurrent = cc == client_current();
		client_delete(cc, e->send_event, 0);

#ifdef notyet
		/* XXX disable the ptrwarp until we handle it
		 * better. */
		if (!client_delete(cc, e->send_event, 0) && wascurrent) 
			;/* 			client_ptrwarp(new_cc); */
#endif
	}

	xev_register(xev);
}

void
xev_handle_destroynotify(struct xevent *xev, XEvent *ee)
{
	XDestroyWindowEvent *e = &ee->xdestroywindow;
	struct client_ctx *cc;

	if ((cc = client_find(e->window)) != NULL)
		client_delete(cc, 1, 1);

	xev_register(xev);
}

void
xev_handle_configurerequest(struct xevent *xev, XEvent *ee)
{
	XConfigureRequestEvent *e = &ee->xconfigurerequest;
	struct client_ctx *cc;
	struct screen_ctx *sc;
	XWindowChanges wc;

	if ((cc = client_find(e->window)) != NULL) {
		sc = CCTOSC(cc);

		client_gravitate(cc, 0);
		if (e->value_mask & CWWidth)
			cc->geom.width = e->width;
		if (e->value_mask & CWHeight)
			cc->geom.height = e->height;
		if (e->value_mask & CWX)
			cc->geom.x = e->x;
		if (e->value_mask & CWY)
			cc->geom.y = e->y;

                if (cc->geom.x == 0 &&
		    cc->geom.width >= DisplayWidth(G_dpy, sc->which))
			cc->geom.x -= cc->bwidth;

                if (cc->geom.y == 0 &&
		    cc->geom.height >= DisplayHeight(G_dpy, sc->which))
                        cc->geom.y -= cc->bwidth;

		client_gravitate(cc, 1);

		wc.x = cc->geom.x - cc->bwidth;
		wc.y = cc->geom.y - cc->bwidth;
		wc.width = cc->geom.width + cc->bwidth*2;
		wc.height = cc->geom.height + cc->bwidth*2;
		wc.border_width = 0;

		/* We need to move the parent window, too. */
		XConfigureWindow(G_dpy, cc->pwin, e->value_mask, &wc);
		xev_reconfig(cc);
	}

	wc.x = cc != NULL ? 0 : e->x;
	wc.y = cc != NULL ? 0 : e->y;
	wc.width = e->width;
	wc.height = e->height;
	wc.stack_mode = Above;
	wc.border_width = 0;
	e->value_mask &= ~CWStackMode;
	e->value_mask |= CWBorderWidth;

	XConfigureWindow(G_dpy, e->window, e->value_mask, &wc);

	xev_register(xev);
}

void
xev_handle_propertynotify(struct xevent *xev, XEvent *ee)
{
	XPropertyEvent *e = &ee->xproperty;
	struct client_ctx *cc;
	long tmp;

	if ((cc = client_find(e->window)) != NULL) {
		switch(e->atom) { 
		case XA_WM_NORMAL_HINTS:
			XGetWMNormalHints(G_dpy, cc->win, cc->size, &tmp);
			break;
		case XA_WM_NAME:
			client_setname(cc);
			break;
		default:
			/* do nothing */
			break;
		}
	}

	xev_register(xev);
}

void
xev_reconfig(struct client_ctx *cc)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.event = cc->win;
	ce.window = cc->win;
	ce.x = cc->geom.x;
	ce.y = cc->geom.y;
	ce.width = cc->geom.width;
	ce.height = cc->geom.height;
	ce.border_width = 0;
	ce.above = None;
	ce.override_redirect = 0;

	XSendEvent(G_dpy, cc->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
xev_handle_enternotify(struct xevent *xev, XEvent *ee)
{
	XCrossingEvent *e = &ee->xcrossing;
	struct client_ctx *cc;

	if ((cc = client_find(e->window)) == NULL) {
		/*
		 * XXX - later.  messes up unclutter.  but may be
		 * needed when we introduce menu windows and such into
		 * the main event loop.
		 */
#ifdef notyet
		if (e->window != e->root)
			client_nocurrent();
#endif
	} else
		client_setactive(cc, 1);

	xev_register(xev);
}

void
xev_handle_leavenotify(struct xevent *xev, XEvent *ee)
{
	client_leave(NULL);

	xev_register(xev);
}

/* We can split this into two event handlers. */
void
xev_handle_buttonpress(struct xevent *xev, XEvent *ee)
{
	XButtonEvent *e = &ee->xbutton;
	struct client_ctx *cc, *old_cc = client_current();
	struct screen_ctx *sc = screen_fromroot(e->root);
	char *wname;
	int altcontrol = e->state == (ControlMask|Mod1Mask);

	cc = client_find(e->window);

	if (sc->rootwin == e->window && !altcontrol) {
		struct menu_q menuq;
		struct menu *mi;

		/* XXXSIGH!!!! */
		if (e->button == Button2) {
			group_menu(e);
			goto out;
		}

		TAILQ_INIT(&menuq);

		switch (e->button) {
		case Button1:
			TAILQ_FOREACH(cc, &G_clientq, entry) {
				if (cc->flags & CLIENT_HIDDEN) {
					if (cc->label != NULL)
						wname = cc->label;
					else
				  		wname = cc->name;

					if (wname == NULL)
						continue;

					XCALLOC(mi, struct menu);
					strlcpy(mi->text,
					    wname, sizeof(mi->text));
					mi->ctx = cc;
					TAILQ_INSERT_TAIL(&menuq, mi, entry);
				}
			}
			break;
		case Button3: {
			struct cmd *cmd;
			if (conf_cmd_changed(G_conf.menu_path)) {
				conf_cmd_clear(&G_conf);
				conf_cmd_populate(&G_conf, G_conf.menu_path);
			}
			TAILQ_FOREACH(cmd, &G_conf.cmdq, entry) {
				XCALLOC(mi, struct menu);
				strlcpy(mi->text, cmd->label, sizeof(mi->text));
				mi->ctx = cmd;
				TAILQ_INSERT_TAIL(&menuq, mi, entry);
			}
			break;
		}
		default:
			break;
		}

		if (TAILQ_EMPTY(&menuq))
			goto out;

		mi = (struct menu *)grab_menu(e, &menuq);
		if (mi == NULL)
			goto cleanup;

		switch (e->button) {
		case Button1:
			cc = (struct client_ctx *)mi->ctx;
			client_unhide(cc);

			if (old_cc != NULL)
				client_ptrsave(old_cc);
			client_ptrwarp(cc);
			break;
		case Button3:
			u_spawn(((struct cmd *)mi->ctx)->image);
			break;
		default:
			break;
		}

	cleanup:
		while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
			TAILQ_REMOVE(&menuq, mi, entry);
			xfree(mi);
		}

		goto out;
	}

	if (cc == NULL || e->state == 0)
		goto out;

	sc = CCTOSC(cc);

	switch (e->button) {
	case Button1:
		if (altcontrol && !G_groupmode)
			group_sticky_toggle_enter(cc);
		else {
			grab_drag(cc);
			client_move(cc);
		}
		break;
	case Button2:
		/* XXXSIGH!!! */
		if (G_groupmode)
			group_click(cc);
		else {
			grab_sweep(cc);
			client_resize(cc);
		}
		break;
	case Button3:
		client_ptrsave(cc);
		client_lower(cc);
		break;
	}
 out:
	xev_register(xev);
}

void
xev_handle_buttonrelease(struct xevent *xev, XEvent *ee)
{
	struct client_ctx *cc = client_current();

	if (cc != NULL && !G_groupmode)
		group_sticky_toggle_exit(cc);

	xev_register(xev);

}

void
xev_handle_keypress(struct xevent *xev, XEvent *ee)
{
	XKeyEvent *e = &ee->xkey;
	struct client_ctx *cc = NULL; /* Make gcc happy. */
	struct keybinding *kb;
	KeySym keysym, skeysym;
	int modshift;

	keysym = XKeycodeToKeysym(G_dpy, e->keycode, 0);
	skeysym = XKeycodeToKeysym(G_dpy, e->keycode, 1);
	
        TAILQ_FOREACH(kb, &G_conf.keybindingq, entry) {
		if (keysym != kb->keysym && skeysym == kb->keysym)
			modshift = ShiftMask;
		else
			modshift = 0;

		if ((kb->modmask | modshift) != e->state)
			continue;

		if ((kb->keycode != 0 && kb->keysym == NoSymbol &&
			kb->keycode == e->keycode) || kb->keysym ==
			(modshift == 0 ? keysym : skeysym))
			break;
        }

	if (kb == NULL && e->window == screen_current()->groupwin)
		group_display_keypress(e->keycode);

	if (kb == NULL)
		goto out;

	if ((kb->flags & (KBFLAG_NEEDCLIENT|KBFLAG_FINDCLIENT)) && 
	    (cc = client_find(e->window)) == NULL &&
	    (cc = client_current()) == NULL)
		if (kb->flags & KBFLAG_NEEDCLIENT)
			goto out;

	(*kb->callback)(cc, kb->argument);

out:
	xev_register(xev);
}

/*
 * This is only used for the alt supression detection.
 */
void
xev_handle_keyrelease(struct xevent *xev, XEvent *ee)
{
	XKeyEvent *e = &ee->xkey;
	struct screen_ctx *sc = screen_fromroot(e->root);
	int keysym;

	keysym = XKeycodeToKeysym(G_dpy, e->keycode, 0);
	if (keysym != XK_Alt_L && keysym != XK_Alt_R)
		goto out;

	sc->altpersist = 0;
	
	/*
	 * XXX - better interface... xevents should not know about
	 * how/when to mtf.
	 */
	client_mtf(NULL);
	client_altrelease();

 out:
	xev_register(xev);
}

void
xev_handle_clientmessage(struct xevent *xev, XEvent *ee)
{
	XClientMessageEvent *e = &ee->xclient;
	struct client_ctx *cc = client_find(e->window);
	Atom xa_wm_change_state = XInternAtom(G_dpy, "WM_CHANGE_STATE", False);

	if (cc == NULL)
		goto out;

	if (e->message_type == xa_wm_change_state && e->format == 32 &&
	    e->data.l[0] == IconicState)
		client_hide(cc);
out:
	xev_register(xev);
}

/*
 * X Event handling
 */

static struct xevent_q _xevq, _xevq_putaway;
static short _xev_q_lock = 0;

void
xev_init(void)
{
	TAILQ_INIT(&_xevq);
	TAILQ_INIT(&_xevq_putaway);
}

struct xevent *
xev_new(Window *win, Window *root,
    int type, void (*cb)(struct xevent *, XEvent *), void *arg)
{
	struct xevent *xev;

	XMALLOC(xev, struct xevent);
	xev->xev_win = win;
	xev->xev_root = root;
	xev->xev_type = type;
	xev->xev_cb = cb;
	xev->xev_arg = arg;

	return (xev);
}

void
xev_register(struct xevent *xev)
{
	struct xevent_q *xq;

	xq = _xev_q_lock ? &_xevq_putaway : &_xevq;
	TAILQ_INSERT_TAIL(xq, xev, entry);
}

void
_xev_reincorporate(void)
{
	struct xevent *xev;

	while ((xev = TAILQ_FIRST(&_xevq_putaway)) != NULL) {
		TAILQ_REMOVE(&_xevq_putaway, xev, entry);
		TAILQ_INSERT_TAIL(&_xevq, xev, entry);
	}
}

void
xev_handle_expose(struct xevent *xev, XEvent *ee)
{
	XExposeEvent *e = &ee->xexpose;
	struct screen_ctx *sc = screen_current();
	struct client_ctx *cc;

	if ((cc = client_find(e->window)) != NULL)
		client_draw_border(cc);

	if (sc->groupwin == e->window)
		group_display_draw(sc);

	xev_register(xev);
}

#define ASSIGN(xtype) do {			\
	root = e. xtype .root;			\
	win = e. xtype .window;			\
} while (0)

#define ASSIGN1(xtype) do {			\
	win = e. xtype .window;			\
} while (0)

void
xev_loop(void)
{
	Window win, root;
	int type;
	XEvent e;
	struct xevent *xev, *nextxev;

	for (;;) {
#ifdef DIAGNOSTIC
		if (TAILQ_EMPTY(&_xevq))
			errx(1, "X event queue empty");
#endif		

		XNextEvent(G_dpy, &e);
		type = e.type;

		win = root = 0;

		switch (type) {
		case MapRequest:
			ASSIGN1(xmaprequest);
			break;
		case UnmapNotify:
			ASSIGN1(xunmap);
			break;
		case ConfigureRequest:
			ASSIGN1(xconfigurerequest);
			break;
		case PropertyNotify:
			ASSIGN1(xproperty);
			break;
		case EnterNotify:
		case LeaveNotify:
			ASSIGN(xcrossing);
			break;
		case ButtonPress:
			ASSIGN(xbutton);
			break;
		case ButtonRelease:
			ASSIGN(xbutton);
			break;
		case KeyPress:
		case KeyRelease:
			ASSIGN(xkey);
			break;
		case DestroyNotify:
			ASSIGN1(xdestroywindow);
			break;
		case ClientMessage:
			ASSIGN1(xclient);
			break;
		default:	/* XXX - still need shape event support. */
			break;
		}

		/*
		 * Now, search for matches, and call each of them.
		 */
		_xev_q_lock = 1;
		for (xev = TAILQ_FIRST(&_xevq); xev != NULL; xev = nextxev) {
			nextxev = TAILQ_NEXT(xev, entry);

			if ((type != xev->xev_type && xev->xev_type != 0) ||
			    (xev->xev_win != NULL && win != *xev->xev_win) ||
			    (xev->xev_root != NULL && root != *xev->xev_root))
				continue;

			TAILQ_REMOVE(&_xevq, xev, entry);

			(*xev->xev_cb)(xev, &e);
		}
		_xev_q_lock = 0;
		_xev_reincorporate();
	}
}

#undef ASSIGN
#undef ASSIGN1

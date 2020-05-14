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

/*
 * NOTE:
 *   It is the responsibility of the caller to deal with memory
 *   management of the xevent's.
 */

#include <sys/types.h>
#include "queue.h"

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

static void	 xev_handle_maprequest(XEvent *);
static void	 xev_handle_unmapnotify(XEvent *);
static void	 xev_handle_destroynotify(XEvent *);
static void	 xev_handle_configurerequest(XEvent *);
static void	 xev_handle_propertynotify(XEvent *);
static void	 xev_handle_enternotify(XEvent *);
static void	 xev_handle_buttonpress(XEvent *);
static void	 xev_handle_buttonrelease(XEvent *);
static void	 xev_handle_keypress(XEvent *);
static void	 xev_handle_keyrelease(XEvent *);
static void	 xev_handle_clientmessage(XEvent *);
static void	 xev_handle_randr(XEvent *);
static void	 xev_handle_mappingnotify(XEvent *);
static void	 xev_handle_expose(XEvent *);

void		(*xev_handlers[LASTEvent])(XEvent *) = {
			[MapRequest] = xev_handle_maprequest,
			[UnmapNotify] = xev_handle_unmapnotify,
			[DestroyNotify] = xev_handle_destroynotify,
			[ConfigureRequest] = xev_handle_configurerequest,
			[PropertyNotify] = xev_handle_propertynotify,
			[EnterNotify] = xev_handle_enternotify,
			[ButtonPress] = xev_handle_buttonpress,
			[ButtonRelease] = xev_handle_buttonrelease,
			[KeyPress] = xev_handle_keypress,
			[KeyRelease] = xev_handle_keyrelease,
			[ClientMessage] = xev_handle_clientmessage,
			[MappingNotify] = xev_handle_mappingnotify,
			[Expose] = xev_handle_expose,
};

static KeySym modkeys[] = { XK_Alt_L, XK_Alt_R, XK_Super_L, XK_Super_R,
			    XK_Control_L, XK_Control_R, XK_ISO_Level3_Shift };

static void
xev_handle_maprequest(XEvent *ee)
{
	XMapRequestEvent	*e = &ee->xmaprequest;
	struct screen_ctx	*sc;
	struct client_ctx	*cc, *old_cc;

	LOG_DEBUG3("parent: 0x%lx window: 0x%lx", e->parent, e->window);

	if ((sc = screen_find(e->parent)) == NULL)
		return;

	if ((old_cc = client_current(sc)) != NULL)
		client_ptr_save(old_cc);

	if ((cc = client_find(e->window)) == NULL)
		cc = client_init(e->window, NULL);

	if ((cc != NULL) && (!(cc->flags & CLIENT_IGNORE)))
		client_ptr_warp(cc);
}

static void
xev_handle_unmapnotify(XEvent *ee)
{
	XUnmapEvent		*e = &ee->xunmap;
	struct client_ctx	*cc;

	LOG_DEBUG3("window: 0x%lx", e->window);

	if ((cc = client_find(e->window)) != NULL) {
		if (e->send_event) {
			xu_set_wm_state(cc->win, WithdrawnState);
		} else {
			if (!(cc->flags & CLIENT_HIDDEN))
				client_remove(cc);
		}
	}
}

static void
xev_handle_destroynotify(XEvent *ee)
{
	XDestroyWindowEvent	*e = &ee->xdestroywindow;
	struct client_ctx	*cc;

	LOG_DEBUG3("window: 0x%lx", e->window);

	if ((cc = client_find(e->window)) != NULL)
		client_remove(cc);
}

static void
xev_handle_configurerequest(XEvent *ee)
{
	XConfigureRequestEvent	*e = &ee->xconfigurerequest;
	struct client_ctx	*cc;
	struct screen_ctx	*sc;
	XWindowChanges		 wc;

	LOG_DEBUG3("window: 0x%lx", e->window);

	if ((cc = client_find(e->window)) != NULL) {
		sc = cc->sc;

		if (e->value_mask & CWWidth)
			cc->geom.w = e->width;
		if (e->value_mask & CWHeight)
			cc->geom.h = e->height;
		if (e->value_mask & CWX)
			cc->geom.x = e->x;
		if (e->value_mask & CWY)
			cc->geom.y = e->y;
		if (e->value_mask & CWBorderWidth)
			cc->bwidth = e->border_width;
		if (e->value_mask & CWSibling)
			wc.sibling = e->above;
		if (e->value_mask & CWStackMode)
			wc.stack_mode = e->detail;

		if (cc->geom.x == 0 && cc->geom.w >= sc->view.w)
			cc->geom.x -= cc->bwidth;

		if (cc->geom.y == 0 && cc->geom.h >= sc->view.h)
			cc->geom.y -= cc->bwidth;

		wc.x = cc->geom.x;
		wc.y = cc->geom.y;
		wc.width = cc->geom.w;
		wc.height = cc->geom.h;
		wc.border_width = cc->bwidth;

		XConfigureWindow(X_Dpy, cc->win, e->value_mask, &wc);
		client_config(cc);
	} else {
		/* let it do what it wants, it'll be ours when we map it. */
		wc.x = e->x;
		wc.y = e->y;
		wc.width = e->width;
		wc.height = e->height;
		wc.border_width = e->border_width;
		wc.stack_mode = Above;
		e->value_mask &= ~CWStackMode;

		XConfigureWindow(X_Dpy, e->window, e->value_mask, &wc);
	}
}

static void
xev_handle_propertynotify(XEvent *ee)
{
	XPropertyEvent		*e = &ee->xproperty;
	struct screen_ctx	*sc;
	struct client_ctx	*cc;

	LOG_DEBUG3("window: 0x%lx", e->window);

	if ((cc = client_find(e->window)) != NULL) {
		switch (e->atom) {
		case XA_WM_NORMAL_HINTS:
			client_get_sizehints(cc);
			break;
		case XA_WM_NAME:
			client_set_name(cc);
			break;
		case XA_WM_HINTS:
			client_wm_hints(cc);
			client_draw_border(cc);
			break;
		case XA_WM_TRANSIENT_FOR:
			client_transient(cc);
			client_draw_border(cc);
			if (cc->gc)
				group_movetogroup(cc, cc->gc->num);
			break;
		default:
			if (e->atom == ewmh[_NET_WM_NAME])
				client_set_name(cc);
			break;
		}
	} else {
		if (e->atom == ewmh[_NET_DESKTOP_NAMES])  {
			if ((sc = screen_find(e->window)) != NULL)
				xu_ewmh_net_desktop_names(sc);
		}
	}
}

static void
xev_handle_enternotify(XEvent *ee)
{
	XCrossingEvent		*e = &ee->xcrossing;
	struct client_ctx	*cc;

	LOG_DEBUG3("window: 0x%lx", e->window);

	Last_Event_Time = e->time;

	if ((cc = client_find(e->window)) != NULL)
		client_set_active(cc);
}

static void
xev_handle_buttonpress(XEvent *ee)
{
	XButtonEvent		*e = &ee->xbutton;
	struct client_ctx	*cc;
	struct screen_ctx	*sc;
	struct bind_ctx		*mb;

	LOG_DEBUG3("root: 0x%lx window: 0x%lx subwindow: 0x%lx",
	    e->root, e->window, e->subwindow);

	if ((sc = screen_find(e->root)) == NULL)
		return;

	e->state &= ~IGNOREMODMASK;

	TAILQ_FOREACH(mb, &Conf.mousebindq, entry) {
		if (e->button == mb->press.button && e->state == mb->modmask)
			break;
	}
	if (mb == NULL)
		return;
	mb->cargs->xev = CWM_XEV_BTN;
	switch (mb->context) {
	case CWM_CONTEXT_CC:
		if (((cc = client_find(e->window)) == NULL) &&
		    ((cc = client_current(sc)) == NULL))
			return;
		(*mb->callback)(cc, mb->cargs);
		break;
	case CWM_CONTEXT_SC:
		(*mb->callback)(sc, mb->cargs);
		break;
	case CWM_CONTEXT_NONE:
		(*mb->callback)(NULL, mb->cargs);
		break;
	}
}

static void
xev_handle_buttonrelease(XEvent *ee)
{
	XButtonEvent		*e = &ee->xbutton;
	struct client_ctx	*cc;

	LOG_DEBUG3("root: 0x%lx window: 0x%lx subwindow: 0x%lx",
	    e->root, e->window, e->subwindow);

	if ((cc = client_find(e->window)) != NULL) {
		if (cc->flags & (CLIENT_ACTIVE | CLIENT_HIGHLIGHT)) {
			cc->flags &= ~CLIENT_HIGHLIGHT;
			client_draw_border(cc);
		}
	}
}

static void
xev_handle_keypress(XEvent *ee)
{
	XKeyEvent		*e = &ee->xkey;
	struct client_ctx	*cc;
	struct screen_ctx	*sc;
	struct bind_ctx		*kb;
	KeySym			 keysym, skeysym;
	unsigned int		 modshift;

	LOG_DEBUG3("root: 0x%lx window: 0x%lx subwindow: 0x%lx",
	    e->root, e->window, e->subwindow);

	if ((sc = screen_find(e->root)) == NULL)
		return;

	keysym = XkbKeycodeToKeysym(X_Dpy, e->keycode, 0, 0);
	skeysym = XkbKeycodeToKeysym(X_Dpy, e->keycode, 0, 1);

	e->state &= ~IGNOREMODMASK;

	TAILQ_FOREACH(kb, &Conf.keybindq, entry) {
		if (keysym != kb->press.keysym && skeysym == kb->press.keysym)
			modshift = ShiftMask;
		else
			modshift = 0;

		if ((kb->modmask | modshift) != e->state)
			continue;

		if (kb->press.keysym == ((modshift == 0) ? keysym : skeysym))
			break;
	}
	if (kb == NULL)
		return;
	kb->cargs->xev = CWM_XEV_KEY;
	switch (kb->context) {
	case CWM_CONTEXT_CC:
		if (((cc = client_find(e->subwindow)) == NULL) &&
		    ((cc = client_current(sc)) == NULL))
			return;
		(*kb->callback)(cc, kb->cargs);
		break;
	case CWM_CONTEXT_SC:
		(*kb->callback)(sc, kb->cargs);
		break;
	case CWM_CONTEXT_NONE:
		(*kb->callback)(NULL, kb->cargs);
		break;
	}
}

/*
 * This is only used for the modifier suppression detection.
 */
static void
xev_handle_keyrelease(XEvent *ee)
{
	XKeyEvent		*e = &ee->xkey;
	struct screen_ctx	*sc;
	struct client_ctx	*cc;
	KeySym			 keysym;
	unsigned int		 i;

	LOG_DEBUG3("root: 0x%lx window: 0x%lx subwindow: 0x%lx",
	    e->root, e->window, e->subwindow);

	if ((sc = screen_find(e->root)) == NULL)
		return;

	keysym = XkbKeycodeToKeysym(X_Dpy, e->keycode, 0, 0);
	for (i = 0; i < nitems(modkeys); i++) {
		if (keysym == modkeys[i]) {
			if ((cc = client_current(sc)) != NULL) {
				if (sc->cycling) {
					sc->cycling = 0;
					client_mtf(cc);
				}
				if (cc->flags & CLIENT_HIGHLIGHT) {
					cc->flags &= ~CLIENT_HIGHLIGHT;
					client_draw_border(cc);
				}
			}
			XUngrabKeyboard(X_Dpy, CurrentTime);
			break;
		}
	}
}

static void
xev_handle_clientmessage(XEvent *ee)
{
	XClientMessageEvent	*e = &ee->xclient;
	struct client_ctx	*cc, *old_cc;
	struct screen_ctx       *sc;

	LOG_DEBUG3("window: 0x%lx", e->window);

	if (e->message_type == cwmh[WM_CHANGE_STATE]) {
		if ((cc = client_find(e->window)) != NULL) {
	    		if (e->data.l[0] == IconicState)
				client_hide(cc);
		}
	} else if (e->message_type == ewmh[_NET_CLOSE_WINDOW]) {
		if ((cc = client_find(e->window)) != NULL) {
			client_close(cc);
		}
	} else if (e->message_type == ewmh[_NET_ACTIVE_WINDOW]) {
		if ((cc = client_find(e->window)) != NULL) {
			if ((old_cc = client_current(NULL)) != NULL)
				client_ptr_save(old_cc);
			client_show(cc);
			client_ptr_warp(cc);
		}
	} else if (e->message_type == ewmh[_NET_WM_DESKTOP]) {
		if ((cc = client_find(e->window)) != NULL) {
			/*
			 * The EWMH spec states that if the cardinal returned
			 * is 0xFFFFFFFF (-1) then the window should appear
			 * on all desktops, in our case, group 0.
			 */
			if (e->data.l[0] == (unsigned long)-1)
				group_movetogroup(cc, 0);
			else
				if (e->data.l[0] >= 0 &&
				    e->data.l[0] < Conf.ngroups)
					group_movetogroup(cc, e->data.l[0]);
		}
	} else if (e->message_type == ewmh[_NET_WM_STATE]) {
		if ((cc = client_find(e->window)) != NULL) {
			xu_ewmh_handle_net_wm_state_msg(cc,
			    e->data.l[0], e->data.l[1], e->data.l[2]);
		}
	} else if (e->message_type == ewmh[_NET_CURRENT_DESKTOP]) {
		if ((sc = screen_find(e->window)) != NULL) {
			if (e->data.l[0] >= 0 &&
			    e->data.l[0] < Conf.ngroups)
				group_only(sc, e->data.l[0]);
		}
	}
}

static void
xev_handle_randr(XEvent *ee)
{
	XRRScreenChangeNotifyEvent	*e = (XRRScreenChangeNotifyEvent *)ee;
	struct screen_ctx		*sc;

	LOG_DEBUG3("size: %d/%d", e->width, e->height);

	if ((sc = screen_find(e->root)) == NULL)
		return;

	XRRUpdateConfiguration(ee);
	screen_update_geometry(sc);
	screen_assert_clients_within(sc);
}

/*
 * Called when the keymap has changed.
 * Ungrab all keys, reload keymap and then regrab
 */
static void
xev_handle_mappingnotify(XEvent *ee)
{
	XMappingEvent		*e = &ee->xmapping;
	struct screen_ctx	*sc;

	LOG_DEBUG3("window: 0x%lx", e->window);

	XRefreshKeyboardMapping(e);
	if (e->request == MappingKeyboard) {
		TAILQ_FOREACH(sc, &Screenq, entry)
			conf_grab_kbd(sc->rootwin);
	}
}

static void
xev_handle_expose(XEvent *ee)
{
	XExposeEvent		*e = &ee->xexpose;
	struct client_ctx	*cc;

	LOG_DEBUG3("window: 0x%lx", e->window);

	if ((cc = client_find(e->window)) != NULL && e->count == 0)
		client_draw_border(cc);
}

void
xev_process(void)
{
	XEvent		 e;

	while (XPending(X_Dpy)) {
		XNextEvent(X_Dpy, &e);
		if ((e.type - Conf.xrandr_event_base) == RRScreenChangeNotify)
			xev_handle_randr(&e);
		else if ((e.type < LASTEvent) && (xev_handlers[e.type] != NULL))
			(*xev_handlers[e.type])(&e);
	}
}

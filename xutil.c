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

void
xu_ptr_get(Window win, int *x, int *y)
{
	Window		 w0, w1;
	int		 tmp0, tmp1;
	unsigned int	 tmp2;

	XQueryPointer(X_Dpy, win, &w0, &w1, &tmp0, &tmp1, x, y, &tmp2);
}

void
xu_ptr_set(Window win, int x, int y)
{
	XWarpPointer(X_Dpy, None, win, 0, 0, 0, 0, x, y);
}

int
xu_get_prop(Window win, Atom atm, Atom type, long len, unsigned char **p)
{
	Atom		 realtype;
	unsigned long	 n, extra;
	int		 format;

	if (XGetWindowProperty(X_Dpy, win, atm, 0L, len, False, type,
	    &realtype, &format, &n, &extra, p) != Success || *p == NULL)
		return -1;

	if (n == 0)
		XFree(*p);

	return n;
}

int
xu_get_strprop(Window win, Atom atm, char **text) {
	XTextProperty	 prop;
	char		**list;
	int		 nitems = 0;

	*text = NULL;

	XGetTextProperty(X_Dpy, win, &prop, atm);
	if (!prop.nitems) {
		XFree(prop.value);
		return 0;
	}

	if (Xutf8TextPropertyToTextList(X_Dpy, &prop, &list,
	    &nitems) == Success && nitems > 0 && *list) {
		if (nitems > 1) {
			XTextProperty    prop2;
			if (Xutf8TextListToTextProperty(X_Dpy, list, nitems,
			    XUTF8StringStyle, &prop2) == Success) {
				*text = xstrdup((const char *)prop2.value);
				XFree(prop2.value);
			}
		} else {
			*text = xstrdup(*list);
		}
		XFreeStringList(list);
	}
	XFree(prop.value);

	return nitems;
}

void
xu_send_clientmsg(Window win, Atom proto, Time ts)
{
	XClientMessageEvent	 cm;

	(void)memset(&cm, 0, sizeof(cm));
	cm.type = ClientMessage;
	cm.window = win;
	cm.message_type = cwmh[WM_PROTOCOLS];
	cm.format = 32;
	cm.data.l[0] = proto;
	cm.data.l[1] = ts;

	XSendEvent(X_Dpy, win, False, NoEventMask, (XEvent *)&cm);
}

void
xu_get_wm_state(Window win, long *state)
{
	long	*p;

	*state = -1;
	if (xu_get_prop(win, cwmh[WM_STATE], cwmh[WM_STATE], 2L,
	    (unsigned char **)&p) > 0) {
		*state = *p;
		XFree(p);
	}
}

void
xu_set_wm_state(Window win, long state)
{
	long	 data[] = { state, None };

	XChangeProperty(X_Dpy, win, cwmh[WM_STATE], cwmh[WM_STATE], 32,
	    PropModeReplace, (unsigned char *)data, 2);
}
void
xu_xorcolor(XftColor a, XftColor b, XftColor *r)
{
	r->pixel = a.pixel ^ b.pixel;
	r->color.red = a.color.red ^ b.color.red;
	r->color.green = a.color.green ^ b.color.green;
	r->color.blue = a.color.blue ^ b.color.blue;
	r->color.alpha = 0xffff;
}

void
xu_atom_init(void)
{
	char *cwmhints[] = {
		"WM_STATE",
		"WM_DELETE_WINDOW",
		"WM_TAKE_FOCUS",
		"WM_PROTOCOLS",
		"_MOTIF_WM_HINTS",
		"UTF8_STRING",
		"WM_CHANGE_STATE",
	};
	char *ewmhints[] = {
		"_NET_SUPPORTED",
		"_NET_SUPPORTING_WM_CHECK",
		"_NET_ACTIVE_WINDOW",
		"_NET_CLIENT_LIST",
		"_NET_CLIENT_LIST_STACKING",
		"_NET_NUMBER_OF_DESKTOPS",
		"_NET_CURRENT_DESKTOP",
		"_NET_DESKTOP_VIEWPORT",
		"_NET_DESKTOP_GEOMETRY",
		"_NET_VIRTUAL_ROOTS",
		"_NET_SHOWING_DESKTOP",
		"_NET_DESKTOP_NAMES",
		"_NET_WORKAREA",
		"_NET_WM_NAME",
		"_NET_WM_DESKTOP",
		"_NET_CLOSE_WINDOW",
		"_NET_WM_STATE",
		"_NET_WM_STATE_STICKY",
		"_NET_WM_STATE_MAXIMIZED_VERT",
		"_NET_WM_STATE_MAXIMIZED_HORZ",
		"_NET_WM_STATE_HIDDEN",
		"_NET_WM_STATE_FULLSCREEN",
		"_NET_WM_STATE_DEMANDS_ATTENTION",
		"_NET_WM_STATE_SKIP_PAGER",
		"_NET_WM_STATE_SKIP_TASKBAR",
		"_CWM_WM_STATE_FREEZE",
	};

	XInternAtoms(X_Dpy, cwmhints, nitems(cwmhints), False, cwmh);
	XInternAtoms(X_Dpy, ewmhints, nitems(ewmhints), False, ewmh);
}

/* Root Window Properties */
void
xu_ewmh_net_supported(struct screen_ctx *sc)
{
	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_SUPPORTED],
	    XA_ATOM, 32, PropModeReplace, (unsigned char *)ewmh, EWMH_NITEMS);
}

void
xu_ewmh_net_supported_wm_check(struct screen_ctx *sc)
{
	Window	 w;

	w = XCreateSimpleWindow(X_Dpy, sc->rootwin, -1, -1, 1, 1, 0, 0, 0);
	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_SUPPORTING_WM_CHECK],
	    XA_WINDOW, 32, PropModeReplace, (unsigned char *)&w, 1);
	XChangeProperty(X_Dpy, w, ewmh[_NET_SUPPORTING_WM_CHECK],
	    XA_WINDOW, 32, PropModeReplace, (unsigned char *)&w, 1);
	XChangeProperty(X_Dpy, w, ewmh[_NET_WM_NAME],
	    cwmh[UTF8_STRING], 8, PropModeReplace,
	    (unsigned char *)Conf.wmname, strlen(Conf.wmname));
}

void
xu_ewmh_net_desktop_geometry(struct screen_ctx *sc)
{
	long	 geom[2] = { sc->view.w, sc->view.h };

	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_DESKTOP_GEOMETRY],
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)geom , 2);
}

void
xu_ewmh_net_desktop_viewport(struct screen_ctx *sc)
{
	long	 viewports[2] = {0, 0};

	/* We don't support large desktops, so this is (0, 0). */
	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_DESKTOP_VIEWPORT],
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)viewports, 2);
}

void
xu_ewmh_net_workarea(struct screen_ctx *sc)
{
	unsigned long	*workarea;
	int		 i, ngroups = Conf.ngroups;

	workarea = xreallocarray(NULL, ngroups * 4, sizeof(unsigned long));
	for (i = 0; i < ngroups; i++) {
		workarea[4 * i + 0] = sc->work.x;
		workarea[4 * i + 1] = sc->work.y;
		workarea[4 * i + 2] = sc->work.w;
		workarea[4 * i + 3] = sc->work.h;
	}
	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_WORKAREA],
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)workarea,
	    ngroups * 4);
	free(workarea);
}

void
xu_ewmh_net_client_list(struct screen_ctx *sc)
{
	struct client_ctx	*cc;
	Window			*winlist;
	int			 i = 0, j = 0;

	TAILQ_FOREACH(cc, &sc->clientq, entry)
		i++;
	if (i == 0)
		return;

	winlist = xreallocarray(NULL, i, sizeof(*winlist));
	TAILQ_FOREACH(cc, &sc->clientq, entry)
		winlist[j++] = cc->win;
	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_CLIENT_LIST],
	    XA_WINDOW, 32, PropModeReplace, (unsigned char *)winlist, i);
	free(winlist);
}

void
xu_ewmh_net_client_list_stacking(struct screen_ctx *sc)
{
	struct client_ctx	*cc;
	Window			*winlist;
	int			 i = 0, j;

	TAILQ_FOREACH(cc, &sc->clientq, entry)
		i++;
	if (i == 0)
		return;

	j = i;
	winlist = xreallocarray(NULL, i, sizeof(*winlist));
	TAILQ_FOREACH(cc, &sc->clientq, entry)
		winlist[--j] = cc->win;
	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_CLIENT_LIST_STACKING],
	    XA_WINDOW, 32, PropModeReplace, (unsigned char *)winlist, i);
	free(winlist);
}

void
xu_ewmh_net_active_window(struct screen_ctx *sc, Window w)
{
	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_ACTIVE_WINDOW],
	    XA_WINDOW, 32, PropModeReplace, (unsigned char *)&w, 1);
}

void
xu_ewmh_net_number_of_desktops(struct screen_ctx *sc)
{
	long	 ndesks = Conf.ngroups;

	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_NUMBER_OF_DESKTOPS],
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&ndesks, 1);
}

void
xu_ewmh_net_showing_desktop(struct screen_ctx *sc)
{
	long	 zero = 0;

	/* We don't support `showing desktop' mode, so this is zero.
	 * Note that when we hide all groups, or when all groups are
	 * hidden we could technically set this later on.
	 */
	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_SHOWING_DESKTOP],
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&zero, 1);
}

void
xu_ewmh_net_virtual_roots(struct screen_ctx *sc)
{
	/* We don't support virtual roots, so delete if set by previous wm. */
	XDeleteProperty(X_Dpy, sc->rootwin, ewmh[_NET_VIRTUAL_ROOTS]);
}

void
xu_ewmh_net_current_desktop(struct screen_ctx *sc)
{
	long	 num = sc->group_active->num;

	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_CURRENT_DESKTOP],
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&num, 1);
}

void
xu_ewmh_net_desktop_names(struct screen_ctx *sc)
{
	struct group_ctx	*gc;
	char			*p, *q;
	unsigned char		*prop_ret;
	int			 i = 0, j = 0, nstrings = 0, n = 0;
	size_t			 len = 0, tlen, slen;

	/* Let group names be overwritten if _NET_DESKTOP_NAMES is set. */

	if ((j = xu_get_prop(sc->rootwin, ewmh[_NET_DESKTOP_NAMES],
	    cwmh[UTF8_STRING], 0xffffff, (unsigned char **)&prop_ret)) > 0) {
		prop_ret[j - 1] = '\0'; /* paranoia */
		while (i < j) {
			if (prop_ret[i++] == '\0')
				nstrings++;
		}
	}

	p = (char *)prop_ret;
	while (n < nstrings) {
		TAILQ_FOREACH(gc, &sc->groupq, entry) {
			if (gc->num == n) {
				free(gc->name);
				gc->name = xstrdup(p);
				p += strlen(p) + 1;
				break;
			}
		}
		n++;
	}
	if (prop_ret != NULL)
		XFree(prop_ret);

	TAILQ_FOREACH(gc, &sc->groupq, entry)
		len += strlen(gc->name) + 1;
	q = p = xreallocarray(NULL, len, sizeof(*p));

	tlen = len;
	TAILQ_FOREACH(gc, &sc->groupq, entry) {
		slen = strlen(gc->name) + 1;
		(void)strlcpy(q, gc->name, tlen);
		tlen -= slen;
		q += slen;
	}

	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_DESKTOP_NAMES],
	    cwmh[UTF8_STRING], 8, PropModeReplace, (unsigned char *)p, len);
	free(p);
}

/* Application Window Properties */
int
xu_ewmh_get_net_wm_desktop(struct client_ctx *cc, long *n)
{
	long		*p;

	if (xu_get_prop(cc->win, ewmh[_NET_WM_DESKTOP], XA_CARDINAL, 1L,
	    (unsigned char **)&p) <= 0)
		return 0;
	*n = *p;
	XFree(p);
	return 1;
}

void
xu_ewmh_set_net_wm_desktop(struct client_ctx *cc)
{
	long	 num = 0xffffffff;

	if (cc->gc)
		num = cc->gc->num;

	XChangeProperty(X_Dpy, cc->win, ewmh[_NET_WM_DESKTOP],
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&num, 1);
}

Atom *
xu_ewmh_get_net_wm_state(struct client_ctx *cc, int *n)
{
	Atom	*state, *p = NULL;

	if ((*n = xu_get_prop(cc->win, ewmh[_NET_WM_STATE], XA_ATOM, 64L,
	    (unsigned char **)&p)) <= 0)
		return NULL;

	state = xreallocarray(NULL, *n, sizeof(Atom));
	(void)memcpy(state, p, *n * sizeof(Atom));
	XFree((char *)p);

	return state;
}

void
xu_ewmh_handle_net_wm_state_msg(struct client_ctx *cc, int action,
    Atom first, Atom second)
{
	unsigned int i;
	struct handlers {
		Atom atom;
		int flag;
		void (*toggle)(struct client_ctx *);
	} handlers[] = {
		{ _NET_WM_STATE_STICKY,
			CLIENT_STICKY,
			client_toggle_sticky },
		{ _NET_WM_STATE_MAXIMIZED_VERT,
			CLIENT_VMAXIMIZED,
			client_toggle_vmaximize },
		{ _NET_WM_STATE_MAXIMIZED_HORZ,
			CLIENT_HMAXIMIZED,
			client_toggle_hmaximize },
		{ _NET_WM_STATE_HIDDEN,
			CLIENT_HIDDEN,
			client_toggle_hidden },
		{ _NET_WM_STATE_FULLSCREEN,
			CLIENT_FULLSCREEN,
			client_toggle_fullscreen },
		{ _NET_WM_STATE_DEMANDS_ATTENTION,
			CLIENT_URGENCY,
			client_urgency },
		{ _NET_WM_STATE_SKIP_PAGER,
			CLIENT_SKIP_PAGER,
			client_toggle_skip_pager},
		{ _NET_WM_STATE_SKIP_TASKBAR,
			CLIENT_SKIP_TASKBAR,
			client_toggle_skip_taskbar},
		{ _CWM_WM_STATE_FREEZE,
			CLIENT_FREEZE,
			client_toggle_freeze },
	};

	for (i = 0; i < nitems(handlers); i++) {
		if (first != ewmh[handlers[i].atom] &&
		    second != ewmh[handlers[i].atom])
			continue;
		switch (action) {
		case _NET_WM_STATE_ADD:
			if (!(cc->flags & handlers[i].flag))
				handlers[i].toggle(cc);
			break;
		case _NET_WM_STATE_REMOVE:
			if (cc->flags & handlers[i].flag)
				handlers[i].toggle(cc);
			break;
		case _NET_WM_STATE_TOGGLE:
			handlers[i].toggle(cc);
		}
	}
}

void
xu_ewmh_restore_net_wm_state(struct client_ctx *cc)
{
	Atom	*atoms;
	int	 i, n;

	atoms = xu_ewmh_get_net_wm_state(cc, &n);
	for (i = 0; i < n; i++) {
		if (atoms[i] == ewmh[_NET_WM_STATE_STICKY])
			client_toggle_sticky(cc);
		if (atoms[i] == ewmh[_NET_WM_STATE_MAXIMIZED_VERT])
			client_toggle_vmaximize(cc);
		if (atoms[i] == ewmh[_NET_WM_STATE_MAXIMIZED_HORZ])
			client_toggle_hmaximize(cc);
		if (atoms[i] == ewmh[_NET_WM_STATE_HIDDEN])
			client_toggle_hidden(cc);
		if (atoms[i] == ewmh[_NET_WM_STATE_FULLSCREEN])
			client_toggle_fullscreen(cc);
		if (atoms[i] == ewmh[_NET_WM_STATE_DEMANDS_ATTENTION])
			client_urgency(cc);
		if (atoms[i] == ewmh[_NET_WM_STATE_SKIP_PAGER])
			client_toggle_skip_pager(cc);
		if (atoms[i] == ewmh[_NET_WM_STATE_SKIP_TASKBAR])
			client_toggle_skip_taskbar(cc);
		if (atoms[i] == ewmh[_CWM_WM_STATE_FREEZE])
			client_toggle_freeze(cc);
	}
	free(atoms);
}

void
xu_ewmh_set_net_wm_state(struct client_ctx *cc)
{
	Atom	*atoms, *oatoms;
	int	 n, i, j;

	oatoms = xu_ewmh_get_net_wm_state(cc, &n);
	atoms = xreallocarray(NULL, (n + _NET_WM_STATES_NITEMS), sizeof(Atom));
	for (i = j = 0; i < n; i++) {
		if (oatoms[i] != ewmh[_NET_WM_STATE_STICKY] &&
		    oatoms[i] != ewmh[_NET_WM_STATE_MAXIMIZED_VERT] &&
		    oatoms[i] != ewmh[_NET_WM_STATE_MAXIMIZED_HORZ] &&
		    oatoms[i] != ewmh[_NET_WM_STATE_HIDDEN] &&
		    oatoms[i] != ewmh[_NET_WM_STATE_FULLSCREEN] &&
		    oatoms[i] != ewmh[_NET_WM_STATE_DEMANDS_ATTENTION] &&
		    oatoms[i] != ewmh[_NET_WM_STATE_SKIP_PAGER] &&
		    oatoms[i] != ewmh[_NET_WM_STATE_SKIP_TASKBAR] &&
		    oatoms[i] != ewmh[_CWM_WM_STATE_FREEZE])
			atoms[j++] = oatoms[i];
	}
	free(oatoms);
	if (cc->flags & CLIENT_STICKY)
		atoms[j++] = ewmh[_NET_WM_STATE_STICKY];
	if (cc->flags & CLIENT_HIDDEN)
		atoms[j++] = ewmh[_NET_WM_STATE_HIDDEN];
	if (cc->flags & CLIENT_FULLSCREEN)
		atoms[j++] = ewmh[_NET_WM_STATE_FULLSCREEN];
	else {
		if (cc->flags & CLIENT_VMAXIMIZED)
			atoms[j++] = ewmh[_NET_WM_STATE_MAXIMIZED_VERT];
		if (cc->flags & CLIENT_HMAXIMIZED)
			atoms[j++] = ewmh[_NET_WM_STATE_MAXIMIZED_HORZ];
	}
	if (cc->flags & CLIENT_URGENCY)
		atoms[j++] = ewmh[_NET_WM_STATE_DEMANDS_ATTENTION];
	if (cc->flags & CLIENT_SKIP_PAGER)
		atoms[j++] = ewmh[_NET_WM_STATE_SKIP_PAGER];
	if (cc->flags & CLIENT_SKIP_TASKBAR)
		atoms[j++] = ewmh[_NET_WM_STATE_SKIP_TASKBAR];
	if (cc->flags & CLIENT_FREEZE)
		atoms[j++] = ewmh[_CWM_WM_STATE_FREEZE];
	if (j > 0)
		XChangeProperty(X_Dpy, cc->win, ewmh[_NET_WM_STATE],
		    XA_ATOM, 32, PropModeReplace, (unsigned char *)atoms, j);
	else
		XDeleteProperty(X_Dpy, cc->win, ewmh[_NET_WM_STATE]);
	free(atoms);
}

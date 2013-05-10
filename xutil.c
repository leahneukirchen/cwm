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

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

static unsigned int ign_mods[] = { 0, LockMask, Mod2Mask, Mod2Mask | LockMask };

int
xu_ptr_grab(Window win, int mask, Cursor curs)
{
	return (XGrabPointer(X_Dpy, win, False, mask,
	    GrabModeAsync, GrabModeAsync,
	    None, curs, CurrentTime) == GrabSuccess ? 0 : -1);
}

int
xu_ptr_regrab(int mask, Cursor curs)
{
	return (XChangeActivePointerGrab(X_Dpy, mask,
	    curs, CurrentTime) == GrabSuccess ? 0 : -1);
}

void
xu_ptr_ungrab(void)
{
	XUngrabPointer(X_Dpy, CurrentTime);
}

void
xu_btn_grab(Window win, int mask, u_int btn)
{
	u_int	i;

	for (i = 0; i < nitems(ign_mods); i++)
		XGrabButton(X_Dpy, btn, (mask | ign_mods[i]), win,
		    False, BUTTONMASK, GrabModeAsync,
		    GrabModeSync, None, None);
}

void
xu_btn_ungrab(Window win, int mask, u_int btn)
{
	u_int	i;

	for (i = 0; i < nitems(ign_mods); i++)
		XUngrabButton(X_Dpy, btn, (mask | ign_mods[i]), win);
}

void
xu_ptr_getpos(Window win, int *x, int *y)
{
	Window	 w0, w1;
	int	 tmp0, tmp1;
	u_int	 tmp2;

	XQueryPointer(X_Dpy, win, &w0, &w1, &tmp0, &tmp1, x, y, &tmp2);
}

void
xu_ptr_setpos(Window win, int x, int y)
{
	XWarpPointer(X_Dpy, None, win, 0, 0, 0, 0, x, y);
}

void
xu_key_grab(Window win, int mask, KeySym keysym)
{
	KeyCode	 code;
	u_int	 i;

	code = XKeysymToKeycode(X_Dpy, keysym);
	if ((XkbKeycodeToKeysym(X_Dpy, code, 0, 0) != keysym) &&
	    (XkbKeycodeToKeysym(X_Dpy, code, 0, 1) == keysym))
		mask |= ShiftMask;

	for (i = 0; i < nitems(ign_mods); i++)
		XGrabKey(X_Dpy, code, (mask | ign_mods[i]), win,
		    True, GrabModeAsync, GrabModeAsync);
}

void
xu_key_ungrab(Window win, int mask, KeySym keysym)
{
	KeyCode	 code;
	u_int	 i;

	code = XKeysymToKeycode(X_Dpy, keysym);
	if ((XkbKeycodeToKeysym(X_Dpy, code, 0, 0) != keysym) &&
	    (XkbKeycodeToKeysym(X_Dpy, code, 0, 1) == keysym))
		mask |= ShiftMask;

	for (i = 0; i < nitems(ign_mods); i++)
		XUngrabKey(X_Dpy, code, (mask | ign_mods[i]), win);
}

void
xu_configure(struct client_ctx *cc)
{
	XConfigureEvent	 ce;

	ce.type = ConfigureNotify;
	ce.event = cc->win;
	ce.window = cc->win;
	ce.x = cc->geom.x;
	ce.y = cc->geom.y;
	ce.width = cc->geom.w;
	ce.height = cc->geom.h;
	ce.border_width = cc->bwidth;
	ce.above = None;
	ce.override_redirect = 0;

	XSendEvent(X_Dpy, cc->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
xu_sendmsg(Window win, Atom atm, long val)
{
	XEvent	 e;

	(void)memset(&e, 0, sizeof(e));
	e.xclient.type = ClientMessage;
	e.xclient.window = win;
	e.xclient.message_type = atm;
	e.xclient.format = 32;
	e.xclient.data.l[0] = val;
	e.xclient.data.l[1] = CurrentTime;

	XSendEvent(X_Dpy, win, False, 0, &e);
}

int
xu_getprop(Window win, Atom atm, Atom type, long len, u_char **p)
{
	Atom	 realtype;
	u_long	 n, extra;
	int	 format;

	if (XGetWindowProperty(X_Dpy, win, atm, 0L, len, False, type,
	    &realtype, &format, &n, &extra, p) != Success || *p == NULL)
		return (-1);

	if (n == 0)
		XFree(*p);

	return (n);
}

int
xu_getstrprop(Window win, Atom atm, char **text) {
	XTextProperty	 prop;
	char		**list;
	int		 nitems = 0;

	*text = NULL;

	XGetTextProperty(X_Dpy, win, &prop, atm);
	if (!prop.nitems)
		return (0);

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

	return (nitems);
}

int
xu_get_wm_state(Window win, int *state)
{
	long	*p = NULL;

	if (xu_getprop(win, cwmh[WM_STATE].atom, cwmh[WM_STATE].atom, 2L,
	    (u_char **)&p) <= 0)
		return (-1);

	*state = (int)*p;
	XFree((char *)p);

	return (0);
}

void
xu_set_wm_state(Window win, int state)
{
	long	 dat[2];

	dat[0] = state;
	dat[1] = None;

	XChangeProperty(X_Dpy, win,
	    cwmh[WM_STATE].atom, cwmh[WM_STATE].atom, 32,
	    PropModeReplace, (unsigned char *)dat, 2);
}

struct atom_ctx cwmh[CWMH_NITEMS] = {
	{"WM_STATE",			None},
	{"WM_DELETE_WINDOW",		None},
	{"WM_TAKE_FOCUS",		None},
	{"WM_PROTOCOLS",		None},
	{"_MOTIF_WM_HINTS",		None},
	{"UTF8_STRING",			None},
};
struct atom_ctx ewmh[EWMH_NITEMS] = {
	{"_NET_SUPPORTED",		None},
	{"_NET_SUPPORTING_WM_CHECK",	None},
	{"_NET_ACTIVE_WINDOW",		None},
	{"_NET_CLIENT_LIST",		None},
	{"_NET_NUMBER_OF_DESKTOPS",	None},
	{"_NET_CURRENT_DESKTOP",	None},
	{"_NET_DESKTOP_VIEWPORT",	None},
	{"_NET_DESKTOP_GEOMETRY",	None},
	{"_NET_VIRTUAL_ROOTS",		None},
	{"_NET_SHOWING_DESKTOP",	None},
	{"_NET_DESKTOP_NAMES",		None},
	{"_NET_WORKAREA",		None},
	{"_NET_WM_NAME",		None},
	{"_NET_WM_DESKTOP",		None},
};

void
xu_getatoms(void)
{
	u_int	 i;

	for (i = 0; i < nitems(cwmh); i++)
		cwmh[i].atom = XInternAtom(X_Dpy, cwmh[i].name, False);
	for (i = 0; i < nitems(ewmh); i++)
		ewmh[i].atom = XInternAtom(X_Dpy, ewmh[i].name, False);
}

/* Root Window Properties */
void
xu_ewmh_net_supported(struct screen_ctx *sc)
{
	Atom	 atom[EWMH_NITEMS];
	u_int	 i;

	for (i = 0; i < nitems(ewmh); i++)
		atom[i] = ewmh[i].atom;

	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_SUPPORTED].atom,
	    XA_ATOM, 32, PropModeReplace, (unsigned char *)atom, EWMH_NITEMS);
}

void
xu_ewmh_net_supported_wm_check(struct screen_ctx *sc)
{
	Window	 w;

	w = XCreateSimpleWindow(X_Dpy, sc->rootwin, -1, -1, 1, 1, 0, 0, 0);
	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_SUPPORTING_WM_CHECK].atom,
	    XA_WINDOW, 32, PropModeReplace, (unsigned char *)&w, 1);
	XChangeProperty(X_Dpy, w, ewmh[_NET_SUPPORTING_WM_CHECK].atom,
	    XA_WINDOW, 32, PropModeReplace, (unsigned char *)&w, 1);
	XChangeProperty(X_Dpy, w, ewmh[_NET_WM_NAME].atom,
	    cwmh[UTF8_STRING].atom, 8, PropModeReplace, (unsigned char *)WMNAME,
	    strlen(WMNAME));
}

void
xu_ewmh_net_desktop_geometry(struct screen_ctx *sc)
{
	long	 geom[2] = { sc->view.w, sc->view.h };

	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_DESKTOP_GEOMETRY].atom,
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)geom , 2);
}

void
xu_ewmh_net_workarea(struct screen_ctx *sc)
{
	long	 workareas[CALMWM_NGROUPS][4];
	int	 i;

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		workareas[i][0] = sc->work.x;
		workareas[i][1] = sc->work.y;
		workareas[i][2] = sc->work.w;
		workareas[i][3] = sc->work.h;
	}

	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_WORKAREA].atom,
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)workareas,
	    CALMWM_NGROUPS * 4);
}

void
xu_ewmh_net_client_list(struct screen_ctx *sc)
{
	struct client_ctx	*cc;
	Window			*winlist;
	int			 i = 0, j = 0;

	TAILQ_FOREACH(cc, &Clientq, entry)
		i++;
	if (i == 0)
		return;

	winlist = xmalloc(i * sizeof(*winlist));
	TAILQ_FOREACH(cc, &Clientq, entry)
		winlist[j++] = cc->win;
	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_CLIENT_LIST].atom,
	    XA_WINDOW, 32, PropModeReplace, (unsigned char *)winlist, i);
	free(winlist);
}

void
xu_ewmh_net_active_window(struct screen_ctx *sc, Window w)
{
	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_ACTIVE_WINDOW].atom,
	    XA_WINDOW, 32, PropModeReplace, (unsigned char *)&w, 1);
}

void
xu_ewmh_net_wm_desktop_viewport(struct screen_ctx *sc)
{
	long	 viewports[2] = {0, 0};

	/* We don't support large desktops, so this is (0, 0). */
	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_DESKTOP_VIEWPORT].atom,
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)viewports, 2);
}

void
xu_ewmh_net_wm_number_of_desktops(struct screen_ctx *sc)
{
	long	 ndesks = CALMWM_NGROUPS;

	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_NUMBER_OF_DESKTOPS].atom,
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
	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_SHOWING_DESKTOP].atom,
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&zero, 1);
}

void
xu_ewmh_net_virtual_roots(struct screen_ctx *sc)
{
	/* We don't support virtual roots, so delete if set by previous wm. */
	XDeleteProperty(X_Dpy, sc->rootwin, ewmh[_NET_VIRTUAL_ROOTS].atom);
}

void
xu_ewmh_net_current_desktop(struct screen_ctx *sc, long idx)
{
	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_CURRENT_DESKTOP].atom,
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&idx, 1);
}

void
xu_ewmh_net_desktop_names(struct screen_ctx *sc, char *data, int n)
{
	XChangeProperty(X_Dpy, sc->rootwin, ewmh[_NET_DESKTOP_NAMES].atom,
	    cwmh[UTF8_STRING].atom, 8, PropModeReplace, (unsigned char *)data, n);
}

/* Application Window Properties */
void
xu_ewmh_net_wm_desktop(struct client_ctx *cc)
{
	struct group_ctx	*gc = cc->group;
	long			 no = 0xffffffff;

	if (gc)
		no = gc->shortcut;

	XChangeProperty(X_Dpy, cc->win, ewmh[_NET_WM_DESKTOP].atom,
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&no, 1);
}

unsigned long
xu_getcolor(struct screen_ctx *sc, char *name)
{
	XColor	 color, tmp;

	if (!XAllocNamedColor(X_Dpy, sc->colormap, name, &color, &tmp)) {
		warnx("XAllocNamedColor error: '%s'", name);
		return (0);
	}

	return (color.pixel);
}

void
xu_xorcolor(XRenderColor a, XRenderColor b, XRenderColor *r)
{
	r->red = a.red ^ b.red;
	r->green = a.green ^ b.green;
	r->blue = a.blue ^ b.blue;
	r->alpha = 0xffff;
}

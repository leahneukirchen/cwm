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

int
xu_ptr_grab(Window win, int mask, Cursor curs)
{
	return (XGrabPointer(G_dpy, win, False, mask,
		    GrabModeAsync, GrabModeAsync,
		    None, curs, CurrentTime) == GrabSuccess ? 0 : -1);
}

int
xu_ptr_regrab(int mask, Cursor curs)
{
	return (XChangeActivePointerGrab(G_dpy, mask,
		curs, CurrentTime) == GrabSuccess ? 0 : -1);
}

void
xu_ptr_ungrab(void)
{
	XUngrabPointer(G_dpy, CurrentTime);
}

int
xu_btn_grab(Window win, int mask, u_int btn)
{
        return (XGrabButton(G_dpy, btn, mask, win,
		    False, ButtonMask, GrabModeAsync,
		    GrabModeSync, None, None) == GrabSuccess ? 0 : -1);
}

void
xu_btn_ungrab(Window win, int mask, u_int btn)
{
	XUngrabButton(G_dpy, btn, mask, win);
}

void
xu_ptr_getpos(Window rootwin, int *x, int *y)
{
	int tmp0, tmp1;
	u_int tmp2;
	Window w0, w1;

        XQueryPointer(G_dpy, rootwin, &w0, &w1, &tmp0, &tmp1, x, y, &tmp2);
}

void
xu_ptr_setpos(Window win, int x, int y)
{
	XWarpPointer(G_dpy, None, win, 0, 0, 0, 0, x, y);
}

void
xu_key_grab(Window win, int mask, int keysym)
{
	KeyCode code;

	code = XKeysymToKeycode(G_dpy, keysym);
	if ((XKeycodeToKeysym(G_dpy, code, 0) != keysym) &&
	    (XKeycodeToKeysym(G_dpy, code, 1) == keysym))
		mask |= ShiftMask;

        XGrabKey(G_dpy, XKeysymToKeycode(G_dpy, keysym), mask, win, True,
	    GrabModeAsync, GrabModeAsync);
#if 0
        XGrabKey(G_dpy, XKeysymToKeycode(G_dpy, keysym), LockMask|mask,
	    win, True, GrabModeAsync, GrabModeAsync);
#endif
}

void
xu_key_grab_keycode(Window win, int mask, int keycode)
{
        XGrabKey(G_dpy, keycode, mask, win, True, GrabModeAsync, GrabModeAsync);
}

void
xu_sendmsg(struct client_ctx *cc, Atom atm, long val)
{
	XEvent e;

	memset(&e, 0, sizeof(e));
	e.xclient.type = ClientMessage;
	e.xclient.window = cc->win;
	e.xclient.message_type = atm;
	e.xclient.format = 32;
	e.xclient.data.l[0] = val;
	e.xclient.data.l[1] = CurrentTime;

	XSendEvent(G_dpy, cc->win, False, 0, &e);
}

int
xu_getprop(struct client_ctx *cc, Atom atm, Atom type, long len, u_char **p)
{
	Atom realtype;
	u_long n, extra;
	int format;

	if (XGetWindowProperty(G_dpy, cc->win, atm, 0L, len, False, type,
		&realtype, &format, &n, &extra, p) != Success || *p == NULL)
		return (-1);

	if (n == 0)
		XFree(*p);

	return (n);
}

int
xu_getstate(struct client_ctx *cc, int *state)
{
	Atom wm_state = XInternAtom(G_dpy, "WM_STATE", False);
	long *p = NULL;

	if (xu_getprop(cc, wm_state, wm_state, 2L, (u_char **)&p) <= 0)
		return (-1);

	*state = (int)*p;
	XFree((char *)p);

	return (0);
}

char *
xu_getstrprop(struct client_ctx *cc, Atom atm)
{
	u_char *cp;

	if (xu_getprop(cc, atm, XA_STRING, 100L, &cp) <= 0)
		return (NULL);

	return ((char *)cp);
}

void
xu_setstate(struct client_ctx *cc, int state)
{
	long dat[2];
	Atom wm_state;

	/* XXX cache */
	wm_state = XInternAtom(G_dpy, "WM_STATE", False);

	dat[0] = (long)state;
	dat[1] = (long)None;

	cc->state = state;
	XChangeProperty(G_dpy, cc->win, wm_state, wm_state, 32,
	    PropModeReplace, (unsigned char *)dat, 2);
}

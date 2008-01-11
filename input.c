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

int
input_keycodetrans(KeyCode kc, u_int state,
    enum ctltype *ctl, char *chr, int normalize)
{
	int ks;

	*ctl = CTL_NONE;
	*chr = '\0';

	if (state & ShiftMask)
		ks = XKeycodeToKeysym(X_Dpy, kc, 1);
	else
		ks = XKeycodeToKeysym(X_Dpy, kc, 0);

	/* Look for control characters. */
	switch (ks) {
	case XK_BackSpace:
		*ctl = CTL_ERASEONE;
		break;
	case XK_Return:
		*ctl = CTL_RETURN;
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

	if (*ctl == CTL_NONE && (state & ControlMask)) {
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
		case XK_a:
		case XK_A:
			*ctl = CTL_ALL;
			break;
		}
	}

	if (*ctl != CTL_NONE)
		return (0);

	/*
	 * For regular characters, only (part of, actually) Latin 1
	 * for now.
	 */
	if (ks < 0x20 || ks > 0x07e)
		return (-1);

	*chr = (char)ks;
	if (normalize)
		*chr = tolower(*chr);

	return (0);
}

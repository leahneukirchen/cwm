/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2005 Marius Eriksen <marius@monkey.org>
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
#include "queue.h"

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

int
font_ascent(struct screen_ctx *sc)
{
	return (sc->xftfont->ascent);
}

int
font_descent(struct screen_ctx *sc)
{
	return (sc->xftfont->descent);
}

u_int
font_height(struct screen_ctx *sc)
{
	return (sc->xftfont->height + 1);
}

void
font_init(struct screen_ctx *sc, const char *name, const char **color)
{
	int		 i;
	XRenderColor	 c;

	sc->xftdraw = XftDrawCreate(X_Dpy, sc->rootwin,
	    sc->visual, sc->colormap);
	if (sc->xftdraw == NULL)
		errx(1, "XftDrawCreate");

	sc->xftfont = XftFontOpenName(X_Dpy, sc->which, name);
	if (sc->xftfont == NULL)
		errx(1, "XftFontOpenName");

	for (i = 0; i < CWM_COLOR_MENU_MAX; i++) {
		if (*color[i] == '\0')
			break;
		if (!XftColorAllocName(X_Dpy, sc->visual, sc->colormap,
			color[i], &sc->xftcolor[i]))
			errx(1, "XftColorAllocName");
	}
	if (i == CWM_COLOR_MENU_MAX)
		return;

	xu_xorcolor(sc->xftcolor[CWM_COLOR_MENU_BG].color,
		    sc->xftcolor[CWM_COLOR_MENU_FG].color, &c);
	xu_xorcolor(sc->xftcolor[CWM_COLOR_MENU_FONT].color, c, &c);
	if (!XftColorAllocValue(X_Dpy, sc->visual, sc->colormap,
		&c, &sc->xftcolor[CWM_COLOR_MENU_FONT_SEL]))
		errx(1, "XftColorAllocValue");
}

int
font_width(struct screen_ctx *sc, const char *text, int len)
{
	XGlyphInfo	 extents;

	XftTextExtentsUtf8(X_Dpy, sc->xftfont, (const FcChar8*)text,
	    len, &extents);

	return (extents.xOff);
}

void
font_draw(struct screen_ctx *sc, const char *text, int len,
    Drawable d, int active, int x, int y)
{
	int	 color;

	color = active ? CWM_COLOR_MENU_FONT_SEL : CWM_COLOR_MENU_FONT;
	XftDrawChange(sc->xftdraw, d);
	XftDrawStringUtf8(sc->xftdraw, &sc->xftcolor[color], sc->xftfont, x, y,
	    (const FcChar8*)text, len);
}

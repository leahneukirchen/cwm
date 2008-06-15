/*
 * font.c - cwm font abstraction
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
 */

#include "headers.h"
#include "calmwm.h"

void
font_init(struct screen_ctx *sc)
{
	XColor xcolor, tmp;

	sc->xftdraw = XftDrawCreate(X_Dpy, sc->rootwin,
	    DefaultVisual(X_Dpy, sc->which), DefaultColormap(X_Dpy, sc->which));
	if (sc->xftdraw == NULL)
		errx(1, "XftDrawCreate");

	if (!XAllocNamedColor(X_Dpy, DefaultColormap(X_Dpy, sc->which),
	    "black", &xcolor, &tmp))
		errx(1, "XAllocNamedColor");

	sc->xftcolor.color.red = xcolor.red;
	sc->xftcolor.color.green = xcolor.green;
	sc->xftcolor.color.blue = xcolor.blue;
	sc->xftcolor.color.alpha = 0x00ff00;
	sc->xftcolor.pixel = xcolor.pixel;
}

int
font_width(const char *text, int len)
{
	XGlyphInfo extents;
	XftTextExtents8(X_Dpy, Conf.DefaultFont, (const XftChar8*)text,
	    len, &extents);

	return (extents.xOff);
}

void
font_draw(struct screen_ctx *sc, const char *text, int len,
    Drawable d, int x, int y)
{
	XftDrawChange(sc->xftdraw, d);
	/* Really needs to be UTF8'd. */
	XftDrawString8(sc->xftdraw, &sc->xftcolor, Conf.DefaultFont, x, y,
	    (const FcChar8*)text, len);
}

XftFont *
font_make(struct screen_ctx *sc, const char *name)
{
	XftFont *fn = NULL;
	FcPattern *pat, *patx;
	XftResult res;

	if ((pat = FcNameParse(name)) == NULL)
		return (NULL);

	if ((patx = XftFontMatch(X_Dpy, sc->which, pat, &res)) != NULL)
		fn = XftFontOpenPattern(X_Dpy, patx);

	FcPatternDestroy(pat);

	return (fn);
}

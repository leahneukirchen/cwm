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

#include "hash.h"
#include "headers.h"
#include "calmwm.h"

static XftFont *_make_font(struct screen_ctx *sc, struct fontdesc *fdp);

HASH_GENERATE(fonthash, fontdesc, node, fontdesc_cmp);

int
fontdesc_cmp(struct fontdesc *a, struct fontdesc *b)
{
	return strcmp(a->name, b->name);
}

/*
 * Fowler/Noll/Vo hash
 *    http://www.isthe.com/chongo/tech/comp/fnv/
 */

#define FNV_P_32 ((unsigned int)0x01000193) /* 16777619 */
#define FNV_1_32 ((unsigned int)0x811c9dc5) /* 2166136261 */

unsigned int
fontdesc_hash(struct fontdesc *fdp)
{
        const unsigned char *p, *end, *start;
        unsigned int hash = FNV_1_32;

	start = fdp->name;
	end = (const unsigned char *)fdp->name + strlen(fdp->name);

        for (p = start; p < end; p++) {
                hash *= FNV_P_32;
                hash ^= (unsigned int)*p;
        }

        return (hash);
}

void
font_init(struct screen_ctx *sc)
{
	XColor xcolor, tmp;

	HASH_INIT(&sc->fonthash, fontdesc_hash);
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

struct fontdesc *
font_getx(struct screen_ctx *sc, const char *name)
{
	struct fontdesc *fdp;

	if ((fdp = font_get(sc, name)) == NULL)
		errx(1, "font_get()");

	return (fdp);
}

struct fontdesc *
font_get(struct screen_ctx *sc, const char *name)
{
	struct fontdesc fd, *fdp;
	XftFont *fn;

	fd.name = name;

	if ((fdp = HASH_FIND(fonthash, &sc->fonthash, &fd)) == NULL
	    && (fn = _make_font(sc, &fd)) != NULL) {
		fdp = xmalloc(sizeof(*fdp));
		fdp->name = xstrdup(fd.name);
		fdp->fn = fn;
		fdp->sc = sc;
		HASH_INSERT(fonthash, &sc->fonthash, fdp);
	}

	return (fdp);
}

int
font_width(struct fontdesc *fdp, const char *text, int len)
{
    XGlyphInfo extents;
    XftTextExtents8(X_Dpy, fdp->fn, (const XftChar8*)text, len, &extents);

    return (extents.xOff);
}

void
font_draw(struct fontdesc *fdp, const char *text, int len,
    Drawable d, int x, int y)
{
	XftDrawChange(fdp->sc->xftdraw, d);
	/* Really needs to be UTF8'd. */
	XftDrawString8(fdp->sc->xftdraw, &fdp->sc->xftcolor, fdp->fn, x, y,
	    (const FcChar8*)text, len);
}

int
font_ascent(struct fontdesc *fdp)
{
	return fdp->fn->ascent;
}

int
font_descent(struct fontdesc *fdp)
{
	return fdp->fn->descent;
}

static XftFont *
_make_font(struct screen_ctx *sc, struct fontdesc *fdp)
{
	XftFont *fn = NULL;
	FcPattern *pat, *patx;
	XftResult res;

	if ((pat = FcNameParse(fdp->name)) == NULL)
		return (NULL);

	if ((patx = XftFontMatch(X_Dpy, sc->which, pat, &res)) != NULL)
		fn = XftFontOpenPattern(X_Dpy, patx);

	FcPatternDestroy(pat);

	return (fn);
}


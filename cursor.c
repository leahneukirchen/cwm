/*
 * cursor.c
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

/* Pretty much straight out of 9wm... */

struct cursor_data {
	int width;
	int hot[2];
	u_char mask[64];
	u_char fore[64];
};

static struct cursor_data Bigarrow = {
	16,
	{0, 0},
	{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0x3F, 
	  0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x1F, 0xFF, 0x3F, 
	  0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0x3F, 
	  0xCF, 0x1F, 0x8F, 0x0F, 0x07, 0x07, 0x03, 0x02, 
	},
	{ 0x00, 0x00, 0xFE, 0x7F, 0xFE, 0x3F, 0xFE, 0x0F, 
	  0xFE, 0x07, 0xFE, 0x07, 0xFE, 0x0F, 0xFE, 0x1F, 
	  0xFE, 0x3F, 0xFE, 0x7F, 0xFE, 0x3F, 0xCE, 0x1F, 
	  0x86, 0x0F, 0x06, 0x07, 0x02, 0x02, 0x00, 0x00, 
	},
};

static Cursor
_mkcursor(struct cursor_data *c, struct screen_ctx *sc)
{
	Pixmap f, m;

	f = XCreatePixmapFromBitmapData(G_dpy, sc->rootwin, (char *)c->fore,
	    c->width, c->width, 1, 0, 1);
	m = XCreatePixmapFromBitmapData(G_dpy, sc->rootwin, (char *)c->mask,
	    c->width, c->width, 1, 0, 1);

	return (XCreatePixmapCursor(G_dpy, f, m,
		&sc->blackcolor, &sc->whitecolor, c->hot[0], c->hot[1]));
}

Cursor
cursor_bigarrow(struct screen_ctx *sc)
{
	return _mkcursor(&Bigarrow, sc);
}


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

static int	_sweepcalc(struct client_ctx *, int, int, int, int);
static int	menu_calc_entry(int, int, int, int, int);

#define ADJUST_HEIGHT(cc, dy)	((cc->geom.height - cc->geom.min_dy)/ dy)
#define ADJUST_WIDTH(cc, dx)	((cc->geom.width - cc->geom.min_dx)/ dx)

void
grab_sweep_draw(struct client_ctx *cc, int dx, int dy)
{
	struct screen_ctx *sc = CCTOSC(cc);
	int x0 = cc->geom.x, y0 = cc->geom.y;
	char asize[10];	/* fits "nnnnxnnnn\0" */
	int wide, height, wide_size, wide_name;
	struct fontdesc *font = DefaultFont;

	snprintf(asize, sizeof(asize), "%dx%d",
	    ADJUST_WIDTH(cc, dx), ADJUST_HEIGHT(cc, dy));
	wide_size = font_width(font, asize, strlen(asize)) + 4;
	wide_name = font_width(font, cc->name, strlen(cc->name)) + 4;
	wide = MAX(wide_size, wide_name);
	height = font_ascent(font) + font_descent(font) + 1;

	XMoveResizeWindow(X_Dpy, sc->menuwin, x0, y0, wide, height * 2);
	XMapWindow(X_Dpy, sc->menuwin);
	XReparentWindow(X_Dpy, sc->menuwin, cc->win, 0, 0);
	XClearWindow(X_Dpy, sc->menuwin);
	font_draw(font, cc->name, strlen(cc->name), sc->menuwin,
	    2, font_ascent(font) + 1);
	font_draw(font, asize, strlen(asize), sc->menuwin,
	    wide/2 - wide_size/2, height + font_ascent(font) + 1);
}

void
grab_sweep(struct client_ctx *cc)
{
	XEvent ev;
	struct screen_ctx *sc = CCTOSC(cc);
	int x0 = cc->geom.x, y0 = cc->geom.y;
	int dx, dy;

	dx = MAX(1, cc->size->width_inc);
	dy = MAX(1, cc->size->height_inc);

	client_raise(cc);
	client_ptrsave(cc);

	if (xu_ptr_grab(sc->rootwin, MouseMask, Cursor_resize) < 0)
		return;

	xu_ptr_setpos(cc->win, cc->geom.width, cc->geom.height);
	grab_sweep_draw(cc, dx, dy);

	for (;;) {
		/* Look for changes in ptr position. */
		XMaskEvent(X_Dpy, MouseMask|ExposureMask, &ev);

		switch (ev.type) {
		case Expose:
			client_draw_border(cc);
			break;
		case MotionNotify:
			if (_sweepcalc(cc, x0, y0, ev.xmotion.x, ev.xmotion.y))
				/* Recompute window output */
				grab_sweep_draw(cc, dx, dy);

			XMoveResizeWindow(X_Dpy, cc->pwin,
			    cc->geom.x - cc->bwidth,
			    cc->geom.y - cc->bwidth,
			    cc->geom.width + cc->bwidth*2,
			    cc->geom.height + cc->bwidth*2);
			XMoveResizeWindow(X_Dpy, cc->win,
			    cc->bwidth, cc->bwidth,
			    cc->geom.width, cc->geom.height);

			break;
		case ButtonRelease:
			XUnmapWindow(X_Dpy, sc->menuwin);
			XReparentWindow(X_Dpy, sc->menuwin, sc->rootwin, 0, 0);
			xu_ptr_ungrab();
			client_ptrwarp(cc);
			return;
		}
	}
	/* NOTREACHED */
}

void
grab_drag(struct client_ctx *cc)
{
	int x0 = cc->geom.x, y0 = cc->geom.y, xm, ym;
	struct screen_ctx *sc = CCTOSC(cc);
	XEvent ev;

	client_raise(cc);

	if (xu_ptr_grab(sc->rootwin, MouseMask, Cursor_move) < 0)
		return;

	xu_ptr_getpos(sc->rootwin, &xm, &ym);

	for (;;) {
		XMaskEvent(X_Dpy, MouseMask|ExposureMask, &ev);

		switch (ev.type) {
		case Expose:
			client_draw_border(cc);
			break;
		case MotionNotify:
			cc->geom.x = x0 + (ev.xmotion.x - xm);
			cc->geom.y = y0 + (ev.xmotion.y - ym);

			XMoveWindow(X_Dpy, cc->pwin,
			    cc->geom.x - cc->bwidth, cc->geom.y - cc->bwidth);

			break;
		case ButtonRelease:
			xu_ptr_ungrab();
			return;
		}
	}
	/* NOTREACHED */
}

#define MenuMask	(ButtonMask|ButtonMotionMask|ExposureMask)
#define MenuGrabMask	(ButtonMask|ButtonMotionMask|StructureNotifyMask)
#define AllButtonMask	(Button1Mask|Button2Mask|Button3Mask|Button4Mask|Button5Mask)

void *
grab_menu(XButtonEvent *e, struct menu_q *menuq)
{
	struct screen_ctx *sc;
	struct menu *mi;
	XEvent event;
	struct fontdesc *font = DefaultFont;
	int x, y, width, height, tothigh, i, no, entry, prev;
	int fx, fy;

	no = i = width = 0;

	if ((sc = screen_fromroot(e->root)) == NULL || e->window == sc->menuwin)
		return (NULL);

	TAILQ_FOREACH(mi, menuq, entry) {
		i = font_width(font, mi->text, strlen(mi->text)) + 4;
		if (i > width)
			width = i;
		no++;
	}

	if (!sc->maxinitialised) {
		sc->xmax = DisplayWidth(X_Dpy, sc->which);
		sc->ymax = DisplayHeight(X_Dpy, sc->which);
	}

	height = font_ascent(font) + font_descent(font) + 1;
	tothigh = height * no;

	x = e->x - width/2;
	y = e->y - height/2;

	/* does it fit on the screen? */
	if (x < 0)
		x = 0;
	else if (x+width >= sc->xmax)
		x = sc->xmax - width;

	if (y < 0)
		y = 0;
	else if (y+tothigh >= sc->ymax)
		y = sc->ymax - tothigh;

	xu_ptr_setpos(e->root, x + width/2, y + height/2);

	XMoveResizeWindow(X_Dpy, sc->menuwin, x, y, width, tothigh);
	XSelectInput(X_Dpy, sc->menuwin, MenuMask);
	XMapRaised(X_Dpy, sc->menuwin);

	if (xu_ptr_grab(sc->menuwin, MenuGrabMask, Cursor_select) < 0) {
		XUnmapWindow(X_Dpy, sc->menuwin);
		return (NULL);
	}

	entry = prev = -1;

	for (;;) {
		XMaskEvent(X_Dpy, MenuMask, &event);
		switch (event.type) {
		case Expose:
			XClearWindow(X_Dpy, sc->menuwin);
			i = 0;
			TAILQ_FOREACH(mi, menuq, entry) {
				fx = (width - font_width(font, mi->text,
				    strlen(mi->text)))/2;
				fy = height*i + font_ascent(font) + 1;
				font_draw(font, mi->text, strlen(mi->text),
				    sc->menuwin, fx, fy);
				i++;
			}
			/* FALLTHROUGH */
		case MotionNotify:
			prev = entry;
			entry = menu_calc_entry(event.xbutton.x,
			    event.xbutton.y, width, height, no);
			if (prev != -1)
				XFillRectangle(X_Dpy, sc->menuwin, sc->hlgc,
				    0, height*prev, width, height);
			if (entry != -1) {
				xu_ptr_regrab(MenuGrabMask, Cursor_select);
				XFillRectangle(X_Dpy, sc->menuwin, sc->hlgc,
				    0, height*entry, width, height);
			} else
				xu_ptr_regrab(MenuGrabMask, Cursor_default);
			break;
		case ButtonRelease:
			if (event.xbutton.button != e->button)
				break;
			entry = menu_calc_entry(event.xbutton.x,
			    event.xbutton.y, width, height, no);
			xu_ptr_ungrab();
			XUnmapWindow(X_Dpy, sc->menuwin);

			i = 0;
			TAILQ_FOREACH(mi, menuq, entry)
				if (entry == i++)
					break;
			return (mi);
		default:
			break;
		}
	}
}

void
grab_menuinit(struct screen_ctx *sc)
{
	sc->menuwin = XCreateSimpleWindow(X_Dpy, sc->rootwin, 0, 0,
	    1, 1, 1, sc->blackpixl, sc->whitepixl);
}

#define LABEL_MAXLEN 256
#define LabelMask (KeyPressMask|ExposureMask)

void
grab_label(struct client_ctx *cc)
{
	struct screen_ctx *sc = screen_current();
	int x, y, dx, dy, fontheight, focusrevert;
	XEvent e;
	char labelstr[LABEL_MAXLEN];
	char dispstr[LABEL_MAXLEN + sizeof("label>") - 1];
	Window focuswin;
	char chr;
	enum ctltype ctl;
	size_t len;
	struct fontdesc *font = DefaultFont;

	if (cc->label != NULL)
		strlcpy(labelstr, cc->label, sizeof(labelstr));
	else
		labelstr[0] = '\0';

	xu_ptr_getpos(sc->rootwin, &x, &y);

	dy = fontheight = font_ascent(font) + font_descent(font) + 1;
	dx = font_width(font, "label>", 6);

	XMoveResizeWindow(X_Dpy, sc->searchwin, x, y, dx, dy);
	XSelectInput(X_Dpy, sc->searchwin, LabelMask);
	XMapRaised(X_Dpy, sc->searchwin);

	XGetInputFocus(X_Dpy, &focuswin, &focusrevert);
	XSetInputFocus(X_Dpy, sc->searchwin,
	    RevertToPointerRoot, CurrentTime);

	for (;;) {
		XMaskEvent(X_Dpy, LabelMask, &e);

		switch (e.type) {
		case KeyPress:
			if (input_keycodetrans(e.xkey.keycode, e.xkey.state,
				&ctl, &chr, 0) < 0)
				continue;

			switch (ctl) {
			case CTL_ERASEONE:
				if ((len = strlen(labelstr)) > 0)
					labelstr[len - 1] = '\0';
				break;
			case CTL_RETURN:
				/* Done */
				if (strlen(labelstr) == 0)
					goto out;

				if (cc->label != NULL)
					xfree(cc->label);

				cc->label = xstrdup(labelstr);
				/* FALLTHROUGH */
			case CTL_ABORT:
				goto out;
			default:
				break;
			}

			if (chr != '\0') {
				char str[2];

				str[0] = chr;
				str[1] = '\0';
				strlcat(labelstr, str, sizeof(labelstr));
			}

		case Expose:
			snprintf(dispstr, sizeof(dispstr), "label>%s",
			    labelstr);
			dx = font_width(font, dispstr, strlen(dispstr));
			dy = fontheight;

			XClearWindow(X_Dpy, sc->searchwin);
			XResizeWindow(X_Dpy, sc->searchwin, dx, dy);

			font_draw(font, dispstr, strlen(dispstr),
			    sc->searchwin, 0, font_ascent(font) + 1);
			break;
		}
	}

 out:
	XSetInputFocus(X_Dpy, focuswin,
	    focusrevert, CurrentTime);
	XUnmapWindow(X_Dpy, sc->searchwin);
}

static int
_sweepcalc(struct client_ctx *cc, int x0, int y0, int motionx, int motiony)
{
	int width, height;

	width = cc->geom.width;
	height = cc->geom.height;

	cc->geom.width = abs(x0 - motionx);
	cc->geom.height = abs(y0 - motiony);

	if (cc->size->flags & PResizeInc) {
		cc->geom.width -=
		    (cc->geom.width - cc->geom.min_dx) % cc->size->width_inc;
		cc->geom.height -=
		    (cc->geom.height - cc->geom.min_dy) % cc->size->height_inc;
	}

	if (cc->size->flags & PMinSize) {
		cc->geom.width = MAX(cc->geom.width, cc->size->min_width);
		cc->geom.height = MAX(cc->geom.height, cc->size->min_height);
	}

	if (cc->size->flags & PMaxSize) {
		cc->geom.width = MIN(cc->geom.width, cc->size->max_width);
		cc->geom.height = MIN(cc->geom.height, cc->size->max_height);
	}

	cc->geom.x = x0 <= motionx ? x0 : x0 - cc->geom.width;
	cc->geom.y = y0 <= motiony ? y0 : y0 - cc->geom.height;

	return (width != cc->geom.width || height != cc->geom.height);
}

static int
menu_calc_entry(int x, int y, int width, int height, int noentries)
{
	int entry = y/height;

	/* in bounds? */
	if (x < 0 || x > width || y < 0 || y > height*noentries ||
	    entry < 0 || entry >= noentries)
		entry = -1;

	return (entry);
}

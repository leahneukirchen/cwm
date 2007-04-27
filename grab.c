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

int _sweepcalc(struct client_ctx *, int, int, int, int);
int  _nobuttons(XButtonEvent *);

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

	XMoveResizeWindow(G_dpy, sc->menuwin, x0, y0, wide, height * 2);
	XMapWindow(G_dpy, sc->menuwin);
	XReparentWindow(G_dpy, sc->menuwin, cc->win, 0, 0);
	XClearWindow(G_dpy, sc->menuwin);
	font_draw(font, cc->name, strlen(cc->name), sc->menuwin,
	    2, font_ascent(font) + 1);
	font_draw(font, asize, strlen(asize), sc->menuwin,
	    wide/2 - wide_size/2, height + font_ascent(font) + 1);
}

int
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

	if (xu_ptr_grab(sc->rootwin, MouseMask, G_cursor_resize) < 0)
		return (-1);

	xu_ptr_setpos(cc->win, cc->geom.width, cc->geom.height);
	grab_sweep_draw(cc, dx, dy);

	for (;;) {
		/* Look for changes in ptr position. */
		XMaskEvent(G_dpy, MouseMask, &ev);

		switch (ev.type) {
		case MotionNotify:
			if (_sweepcalc(cc, x0, y0, ev.xmotion.x, ev.xmotion.y))
 				/* Recompute window output */
				grab_sweep_draw(cc, dx, dy);

			XMoveResizeWindow(G_dpy, cc->pwin,
			    cc->geom.x - cc->bwidth,
			    cc->geom.y - cc->bwidth,
			    cc->geom.width + cc->bwidth*2,
			    cc->geom.height + cc->bwidth*2);
			XMoveResizeWindow(G_dpy, cc->win,
			    cc->bwidth, cc->bwidth,
			    cc->geom.width, cc->geom.height);

			break;
		case ButtonRelease:
			XUnmapWindow(G_dpy, sc->menuwin);
			XReparentWindow(G_dpy, sc->menuwin, sc->rootwin, 0, 0);
			xu_ptr_ungrab();
			client_ptrwarp(cc);
			return (0);
		}
	}
	/* NOTREACHED */
}

int
grab_drag(struct client_ctx *cc)
{
	int x0 = cc->geom.x, y0 = cc->geom.y, xm, ym;
	struct screen_ctx *sc = CCTOSC(cc);
	XEvent ev;

	client_raise(cc);

	if (xu_ptr_grab(sc->rootwin, MouseMask, G_cursor_move) < 0)
		return (-1);

	xu_ptr_getpos(sc->rootwin, &xm, &ym);

	for (;;) {
		XMaskEvent(G_dpy, MouseMask, &ev);

		switch (ev.type) {
		case MotionNotify:
			cc->geom.x = x0 + (ev.xmotion.x - xm);
			cc->geom.y = y0 + (ev.xmotion.y - ym);

			XMoveWindow(G_dpy, cc->pwin,
			    cc->geom.x - cc->bwidth, cc->geom.y - cc->bwidth);

			break;
		case ButtonRelease:
			xu_ptr_ungrab();
			return (0);
		}
	}
	/* NOTREACHED */
}

/*
 * Adapted from 9wm.
 */

/* XXX - this REALLY needs to be cleaned up. */

#define MenuMask       (ButtonMask|ButtonMotionMask|ExposureMask)
#define MenuGrabMask   (ButtonMask|ButtonMotionMask|StructureNotifyMask)
#define AllButtonMask   (Button1Mask|Button2Mask|Button3Mask|Button4Mask|Button5Mask)

#ifdef notyet
struct client_ctx *
grab_menu_getcc(struct menu_q *menuq, int off)
{
	int where = 0;
	struct menu *mi;

	TAILQ_FOREACH(mi, menuq, entry)
	    if (off == where++)
		    return mi->ctx;
	return (NULL);
}
#endif

void *
grab_menu(XButtonEvent *e, struct menu_q *menuq)
{
	struct screen_ctx *sc;
	struct menu *mi;
	XEvent ev;
	int i, n, cur = 0, old, wide, high, status, drawn, warp;
	int x, y, dx, dy, xmax, ymax;
	int tx, ty;
	struct fontdesc *font = DefaultFont;

	if ((sc = screen_fromroot(e->root)) == NULL || e->window == sc->menuwin)
		return (NULL);

	dx = 0;
	i = 0;
	TAILQ_FOREACH(mi, menuq, entry) {
		wide = font_width(font, mi->text, strlen(mi->text)) + 4;
		if (wide > dx)
			dx = wide;
		if (mi->lasthit)
			cur = i;
		i++;
	}

	n = i;

	wide = dx;
	high = font_ascent(font) + font_descent(font) + 1;
	dy = n*high;
	x = e->x - wide/2;
	y = e->y - cur*high - high/2;
	warp = 0;
	/* XXX - cache these in sc. */
	xmax = DisplayWidth(G_dpy, sc->which);
	ymax = DisplayHeight(G_dpy, sc->which);
	if (x < 0) {
		e->x -= x;
		x = 0;
		warp++;
	}
	if (x+wide >= xmax) {
		e->x -= x+wide-xmax;
		x = xmax-wide;
		warp++;
	}
	if (y < 0) {
		e->y -= y;
		y = 0;
		warp++;
	}
	if (y+dy >= ymax) {
		e->y -= y+dy-ymax;
		y = ymax-dy;
		warp++;
	}
	if (warp)
		xu_ptr_setpos(e->root, e->x, e->y);

	XMoveResizeWindow(G_dpy, sc->menuwin, x, y, dx, dy);
	XSelectInput(G_dpy, sc->menuwin, MenuMask);
	XMapRaised(G_dpy, sc->menuwin);
	status = xu_ptr_grab(sc->menuwin, MenuGrabMask, G_cursor_select);
	if (status < 0) {
		XUnmapWindow(G_dpy, sc->menuwin);
		return (NULL);
	}
	drawn = 0;

#ifdef notyet
	if (e->button == Button1) {
		struct client_ctx *cc;
		cc = grab_menu_getcc(menuq, cur);
		if (cc != NULL) {
			client_unhide(cc);
			XRaiseWindow(G_dpy, sc->menuwin);
		}
	}
#endif

	for (;;) {
		XMaskEvent(G_dpy, MenuMask, &ev);
		switch (ev.type) {
		default:
			warnx("menuhit: unknown ev.type %d\n", ev.type);
			break;
		case ButtonPress:
			break;
		case ButtonRelease:
			if (ev.xbutton.button != e->button)
				break;
			x = ev.xbutton.x;
			y = ev.xbutton.y;
			i = y/high;
			if (cur >= 0 && y >= cur*high-3 && y < (cur+1)*high+3)
				i = cur;
			if (x < 0 || x > wide || y < -3)
				i = -1;
			else if (i < 0 || i >= n)
				i = -1;
/* 			else */
/* 				m->lasthit = i; */
			if (!_nobuttons(&ev.xbutton))
				i = -1;

			/* XXX */
			/* 			ungrab(&ev.xbutton); */
			xu_ptr_ungrab();
			XUnmapWindow(G_dpy, sc->menuwin);
			n = 0;
			TAILQ_FOREACH(mi, menuq, entry)
				if (i == n++)
					break;

			return (mi);
		case MotionNotify:
			if (!drawn)
				break;
			x = ev.xbutton.x;
			y = ev.xbutton.y;
			old = cur;
			cur = y/high;
			if (old >= 0 && y >= old*high-3 && y < (old+1)*high+3)
				cur = old;
			if (x < 0 || x > wide || y < -3)
				cur = -1;
			else if (cur < 0 || cur >= n)
				cur = -1;
			if (cur == old)
				break;
			if (old >= 0 && old < n) {
#ifdef notyet
				if (e->button == Button1) {
					struct client_ctx *cc;
					cc = grab_menu_getcc(menuq, old);
					if (cc != NULL)
						client_hide(cc);
				}
#endif
				XFillRectangle(G_dpy, sc->menuwin,
				    sc->hlgc, 0, old*high, wide, high);
			}
			if (cur >= 0 && cur < n) {
#ifdef notyet
				if (e->button == Button1) {
					struct client_ctx *cc;
					cc = grab_menu_getcc(menuq, cur);
					if (cc != NULL) {
						client_unhide(cc);
						XRaiseWindow(G_dpy,
						    sc->menuwin);
					}
				}
#endif
				xu_ptr_regrab(MenuGrabMask, G_cursor_select);
				XFillRectangle(G_dpy, sc->menuwin,
				    sc->hlgc, 0, cur*high, wide, high);
			} else
				xu_ptr_regrab(MenuGrabMask, G_cursor_default);
			break;
		case Expose:
			XClearWindow(G_dpy, sc->menuwin);
			i = 0;
			TAILQ_FOREACH(mi, menuq, entry) {
				tx = (wide - font_width(font, mi->text,
					strlen(mi->text)))/2;
				ty = i*high + font_ascent(font) + 1;
				font_draw(font, mi->text, strlen(mi->text),
				    sc->menuwin, tx, ty);
				i++;
			}
			if (cur >= 0 && cur < n)
				XFillRectangle(G_dpy, sc->menuwin,
				    sc->hlgc, 0, cur*high, wide, high);
			drawn = 1;
		}
	}
}

void
grab_menuinit(struct screen_ctx *sc)
{
	sc->menuwin = XCreateSimpleWindow(G_dpy, sc->rootwin, 0, 0,
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

	XMoveResizeWindow(G_dpy, sc->searchwin, x, y, dx, dy);
	XSelectInput(G_dpy, sc->searchwin, LabelMask);
	XMapRaised(G_dpy, sc->searchwin);

	XGetInputFocus(G_dpy, &focuswin, &focusrevert);
	XSetInputFocus(G_dpy, sc->searchwin,
	    RevertToPointerRoot, CurrentTime);

	for (;;) {
		XMaskEvent(G_dpy, LabelMask, &e);

		switch (e.type) {
		case KeyPress:
			if (input_keycodetrans(e.xkey.keycode, e.xkey.state,
				&ctl, &chr, 1) < 0)
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
			snprintf(dispstr, sizeof(dispstr), "label>%s", labelstr);
			dx = font_width(font, dispstr, strlen(dispstr));
			dy = fontheight;

			XClearWindow(G_dpy, sc->searchwin);
			XResizeWindow(G_dpy, sc->searchwin, dx, dy);

			font_draw(font, dispstr, strlen(dispstr),
			    sc->searchwin, 0, font_ascent(font) + 1);
			break;
		}
	}

 out:
	XSetInputFocus(G_dpy, focuswin,
	    focusrevert, CurrentTime);
	XUnmapWindow(G_dpy, sc->searchwin);
}

int
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

/* XXX */
int
_nobuttons(XButtonEvent *e)	/* Einstuerzende */
{
	int state;

	state = (e->state & AllButtonMask);
	return (e->type == ButtonRelease) && (state & (state - 1)) == 0;
}

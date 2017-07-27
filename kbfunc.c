/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2004 Martin Murray <mmurray@monkey.org>
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

/* For FreeBSD. */
#define _WITH_GETLINE

#include <sys/types.h>
#include "queue.h"

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

#define HASH_MARKER	"|1|"

extern sig_atomic_t	 cwm_status;

static void kbfunc_amount(int, int, int *, int *);
static void kbfunc_client_move_kb(void *, struct cargs *);
static void kbfunc_client_move_mb(void *, struct cargs *);
static void kbfunc_client_resize_kb(void *, struct cargs *);
static void kbfunc_client_resize_mb(void *, struct cargs *);

void
kbfunc_cwm_status(void *ctx, struct cargs *cargs)
{
	cwm_status = cargs->flag;
}

static void
kbfunc_amount(int flags, int amt, int *mx, int *my)
{
#define CWM_FACTOR 10

	if (flags & CWM_BIGAMOUNT)
		amt *= CWM_FACTOR;

	switch (flags & DIRECTIONMASK) {
	case CWM_UP:
		*my -= amt;
		break;
	case CWM_DOWN:
		*my += amt;
		break;
	case CWM_RIGHT:
		*mx += amt;
		break;
	case CWM_LEFT:
		*mx -= amt;
		break;
	}
}

void
kbfunc_ptrmove(void *ctx, struct cargs *cargs)
{
	struct screen_ctx	*sc = ctx;
	int			 x, y;
	int			 mx = 0, my = 0;

	kbfunc_amount(cargs->flag, Conf.mamount, &mx, &my);

	xu_ptr_getpos(sc->rootwin, &x, &y);
	xu_ptr_setpos(sc->rootwin, x + mx, y + my);
}

void
kbfunc_client_move(void *ctx, struct cargs *cargs)
{
	if (cargs->xev == CWM_XEV_BTN)
		kbfunc_client_move_mb(ctx, cargs);
	else
		kbfunc_client_move_kb(ctx, cargs);
}

void
kbfunc_client_resize(void *ctx, struct cargs *cargs)
{
	if (cargs->xev == CWM_XEV_BTN)
		kbfunc_client_resize_mb(ctx, cargs);
	else
		kbfunc_client_resize_kb(ctx, cargs);
}

static void
kbfunc_client_move_kb(void *ctx, struct cargs *cargs)
{
	struct client_ctx	*cc = ctx;
	struct screen_ctx	*sc = cc->sc;
	struct geom		 area;
	int			 mx = 0, my = 0;

	if (cc->flags & CLIENT_FREEZE)
		return;

	kbfunc_amount(cargs->flag, Conf.mamount, &mx, &my);

	cc->geom.x += mx;
	if (cc->geom.x < -(cc->geom.w + cc->bwidth - 1))
		cc->geom.x = -(cc->geom.w + cc->bwidth - 1);
	if (cc->geom.x > (sc->view.w - cc->bwidth - 1))
		cc->geom.x = sc->view.w - cc->bwidth - 1;
	cc->geom.y += my;
	if (cc->geom.y < -(cc->geom.h + cc->bwidth - 1))
		cc->geom.y = -(cc->geom.h + cc->bwidth - 1);
	if (cc->geom.y > (sc->view.h - cc->bwidth - 1))
		cc->geom.y = sc->view.h - cc->bwidth - 1;

	area = screen_area(sc,
	    cc->geom.x + cc->geom.w / 2,
	    cc->geom.y + cc->geom.h / 2, CWM_GAP);
	cc->geom.x += client_snapcalc(cc->geom.x,
	    cc->geom.x + cc->geom.w + (cc->bwidth * 2),
	    area.x, area.x + area.w, sc->snapdist);
	cc->geom.y += client_snapcalc(cc->geom.y,
	    cc->geom.y + cc->geom.h + (cc->bwidth * 2),
	    area.y, area.y + area.h, sc->snapdist);

	client_move(cc);
	client_ptr_inbound(cc, 1);
}

static void
kbfunc_client_move_mb(void *ctx, struct cargs *cargs)
{
	struct client_ctx	*cc = ctx;
	XEvent			 ev;
	Time			 ltime = 0;
	struct screen_ctx	*sc = cc->sc;
	struct geom		 area;
	int			 move = 1;

	client_raise(cc);

	if (cc->flags & CLIENT_FREEZE)
		return;

	client_ptr_inbound(cc, 1);

	if (XGrabPointer(X_Dpy, cc->win, False, MOUSEMASK,
	    GrabModeAsync, GrabModeAsync, None, Conf.cursor[CF_MOVE],
	    CurrentTime) != GrabSuccess)
		return;

	menu_windraw(sc, cc->win, "%4d, %-4d", cc->geom.x, cc->geom.y);

	while (move) {
		XWindowEvent(X_Dpy, cc->win, MOUSEMASK, &ev);
		switch (ev.type) {
		case MotionNotify:
			/* not more than 60 times / second */
			if ((ev.xmotion.time - ltime) <= (1000 / 60))
				continue;
			ltime = ev.xmotion.time;

			cc->geom.x = ev.xmotion.x_root - cc->ptr.x - cc->bwidth;
			cc->geom.y = ev.xmotion.y_root - cc->ptr.y - cc->bwidth;

			area = screen_area(sc,
			    cc->geom.x + cc->geom.w / 2,
			    cc->geom.y + cc->geom.h / 2, CWM_GAP);
			cc->geom.x += client_snapcalc(cc->geom.x,
			    cc->geom.x + cc->geom.w + (cc->bwidth * 2),
			    area.x, area.x + area.w, sc->snapdist);
			cc->geom.y += client_snapcalc(cc->geom.y,
			    cc->geom.y + cc->geom.h + (cc->bwidth * 2),
			    area.y, area.y + area.h, sc->snapdist);
			client_move(cc);
			menu_windraw(sc, cc->win,
			    "%4d, %-4d", cc->geom.x, cc->geom.y);
			break;
		case ButtonRelease:
			move = 0;
			break;
		}
	}
	if (ltime)
		client_move(cc);
	XUnmapWindow(X_Dpy, sc->menu.win);
	XReparentWindow(X_Dpy, sc->menu.win, sc->rootwin, 0, 0);
	XUngrabPointer(X_Dpy, CurrentTime);
}

static void
kbfunc_client_resize_kb(void *ctx, struct cargs *cargs)
{
	struct client_ctx	*cc = ctx;
	int			 mx = 0, my = 0;
	int			 amt = 1;

	if (cc->flags & CLIENT_FREEZE)
		return;

	if (!(cc->hint.flags & PResizeInc))
		amt = Conf.mamount;

	kbfunc_amount(cargs->flag, amt, &mx, &my);

	if ((cc->geom.w += mx * cc->hint.incw) < cc->hint.minw)
		cc->geom.w = cc->hint.minw;
	if ((cc->geom.h += my * cc->hint.inch) < cc->hint.minh)
		cc->geom.h = cc->hint.minh;
	if (cc->geom.x + cc->geom.w + cc->bwidth - 1 < 0)
		cc->geom.x = -(cc->geom.w + cc->bwidth - 1);
	if (cc->geom.y + cc->geom.h + cc->bwidth - 1 < 0)
		cc->geom.y = -(cc->geom.h + cc->bwidth - 1);

	client_resize(cc, 1);
	client_ptr_inbound(cc, 1);
}

static void
kbfunc_client_resize_mb(void *ctx, struct cargs *cargs)
{
	struct client_ctx	*cc = ctx;
	XEvent			 ev;
	Time			 ltime = 0;
	struct screen_ctx	*sc = cc->sc;
	int			 resize = 1;

	if (cc->flags & CLIENT_FREEZE)
		return;

	client_raise(cc);
	client_ptrsave(cc);

	xu_ptr_setpos(cc->win, cc->geom.w, cc->geom.h);

	if (XGrabPointer(X_Dpy, cc->win, False, MOUSEMASK,
	    GrabModeAsync, GrabModeAsync, None, Conf.cursor[CF_RESIZE],
	    CurrentTime) != GrabSuccess)
		return;

	menu_windraw(sc, cc->win, "%4d x %-4d", cc->dim.w, cc->dim.h);
	while (resize) {
		XWindowEvent(X_Dpy, cc->win, MOUSEMASK, &ev);
		switch (ev.type) {
		case MotionNotify:
			/* not more than 60 times / second */
			if ((ev.xmotion.time - ltime) <= (1000 / 60))
				continue;
			ltime = ev.xmotion.time;

			cc->geom.w = ev.xmotion.x;
			cc->geom.h = ev.xmotion.y;
			client_applysizehints(cc);
			client_resize(cc, 1);
			menu_windraw(sc, cc->win,
			    "%4d x %-4d", cc->dim.w, cc->dim.h);
			break;
		case ButtonRelease:
			resize = 0;
			break;
		}
	}
	if (ltime)
		client_resize(cc, 1);
	XUnmapWindow(X_Dpy, sc->menu.win);
	XReparentWindow(X_Dpy, sc->menu.win, sc->rootwin, 0, 0);
	XUngrabPointer(X_Dpy, CurrentTime);

	/* Make sure the pointer stays within the window. */
	client_ptr_inbound(cc, 0);
}

void
kbfunc_client_delete(void *ctx, struct cargs *cargs)
{
	client_send_delete(ctx);
}

void
kbfunc_client_lower(void *ctx, struct cargs *cargs)
{
	client_ptrsave(ctx);
	client_lower(ctx);
}

void
kbfunc_client_raise(void *ctx, struct cargs *cargs)
{
	client_raise(ctx);
}

void
kbfunc_client_hide(void *ctx, struct cargs *cargs)
{
	client_hide(ctx);
}

void
kbfunc_client_toggle_freeze(void *ctx, struct cargs *cargs)
{
	client_toggle_freeze(ctx);
}

void
kbfunc_client_toggle_sticky(void *ctx, struct cargs *cargs)
{
	client_toggle_sticky(ctx);
}

void
kbfunc_client_toggle_fullscreen(void *ctx, struct cargs *cargs)
{
	client_toggle_fullscreen(ctx);
}

void
kbfunc_client_toggle_maximize(void *ctx, struct cargs *cargs)
{
	client_toggle_maximize(ctx);
}

void
kbfunc_client_toggle_hmaximize(void *ctx, struct cargs *cargs)
{
	client_toggle_hmaximize(ctx);
}

void
kbfunc_client_toggle_vmaximize(void *ctx, struct cargs *cargs)
{
	client_toggle_vmaximize(ctx);
}

void
kbfunc_client_htile(void *ctx, struct cargs *cargs)
{
	client_htile(ctx);
}

void
kbfunc_client_vtile(void *ctx, struct cargs *cargs)
{
	client_vtile(ctx);
}

void
kbfunc_client_cycle(void *ctx, struct cargs *cargs)
{
	client_cycle(ctx, cargs->flag);
}

void
kbfunc_client_toggle_group(void *ctx, struct cargs *cargs)
{
	struct client_ctx	*cc = ctx;

	/* For X apps that steal events. */
	if (cargs->xev == CWM_XEV_KEY)
		XGrabKeyboard(X_Dpy, cc->win, True,
		    GrabModeAsync, GrabModeAsync, CurrentTime);

	group_toggle_membership_enter(cc);
}

void
kbfunc_client_movetogroup(void *ctx, struct cargs *cargs)
{
	group_movetogroup(ctx, cargs->flag);
}

void
kbfunc_group_toggle(void *ctx, struct cargs *cargs)
{
	group_hidetoggle(ctx, cargs->flag);
}

void
kbfunc_group_only(void *ctx, struct cargs *cargs)
{
	group_only(ctx, cargs->flag);
}

void
kbfunc_group_cycle(void *ctx, struct cargs *cargs)
{
	group_cycle(ctx, cargs->flag);
}

void
kbfunc_group_alltoggle(void *ctx, struct cargs *cargs)
{
	group_alltoggle(ctx);
}

void
kbfunc_menu_client(void *ctx, struct cargs *cargs)
{
	struct screen_ctx	*sc = ctx;
	struct client_ctx	*cc, *old_cc;
	struct menu		*mi;
	struct menu_q		 menuq;
	int			 m = (cargs->xev == CWM_XEV_BTN);
	int			 all = (cargs->flag & CWM_MENU_WINDOW_ALL);

	old_cc = client_current();

	TAILQ_INIT(&menuq);
	TAILQ_FOREACH(cc, &sc->clientq, entry) {
		if (!all) {
			if (cc->flags & CLIENT_HIDDEN)
				menuq_add(&menuq, cc, NULL);
		} else
			menuq_add(&menuq, cc, NULL);
	}

	if ((mi = menu_filter(sc, &menuq,
	    (m) ? NULL : "window", NULL,
	    ((m) ? CWM_MENU_LIST : 0),
	    search_match_client, search_print_client)) != NULL) {
		cc = (struct client_ctx *)mi->ctx;
		if (cc->flags & CLIENT_HIDDEN)
			client_unhide(cc);
		else
			client_raise(cc);
		if (old_cc)
			client_ptrsave(old_cc);
		client_ptrwarp(cc);
	}

	menuq_clear(&menuq);
}

void
kbfunc_menu_cmd(void *ctx, struct cargs *cargs)
{
	struct screen_ctx	*sc = ctx;
	struct cmd_ctx		*cmd;
	struct menu		*mi;
	struct menu_q		 menuq;
	int			 m = (cargs->xev == CWM_XEV_BTN);

	TAILQ_INIT(&menuq);
	TAILQ_FOREACH(cmd, &Conf.cmdq, entry) {
		if ((strcmp(cmd->name, "lock") == 0) ||
		    (strcmp(cmd->name, "term") == 0))
			continue;
		/* search_match_text() needs mi->text */
		menuq_add(&menuq, cmd, "%s", cmd->name);
	}

	if ((mi = menu_filter(sc, &menuq,
	    (m) ? NULL : "application", NULL,
	    ((m) ? CWM_MENU_LIST : 0),
	    search_match_text, search_print_cmd)) != NULL) {
		cmd = (struct cmd_ctx *)mi->ctx;
		u_spawn(cmd->path);
	}

	menuq_clear(&menuq);
}

void
kbfunc_menu_group(void *ctx, struct cargs *cargs)
{
	struct screen_ctx	*sc = ctx;
	struct group_ctx	*gc;
	struct menu		*mi;
	struct menu_q		 menuq;
	int			 m = (cargs->xev == CWM_XEV_BTN);

	TAILQ_INIT(&menuq);
	TAILQ_FOREACH(gc, &sc->groupq, entry) {
		if (group_holds_only_sticky(gc))
			continue;
		menuq_add(&menuq, gc, "%d %s", gc->num, gc->name);
	}

	if ((mi = menu_filter(sc, &menuq,
	    (m) ? NULL : "group", NULL, (CWM_MENU_LIST),
	    search_match_text, search_print_group)) != NULL) {
		gc = (struct group_ctx *)mi->ctx;
		(group_holds_only_hidden(gc)) ?
		    group_show(gc) : group_hide(gc);
	}

	menuq_clear(&menuq);
}

void
kbfunc_menu_exec(void *ctx, struct cargs *cargs)
{
#define NPATHS 256
	struct screen_ctx	*sc = ctx;
	char			**ap, *paths[NPATHS], *path, *pathcpy;
	char			 tpath[PATH_MAX];
	struct stat		 sb;
	const char		*label;
	DIR			*dirp;
	struct dirent		*dp;
	struct menu		*mi;
	struct menu_q		 menuq;
	int			 l, i, cmd = cargs->flag;

	switch (cmd) {
	case CWM_MENU_EXEC_EXEC:
		label = "exec";
		break;
	case CWM_MENU_EXEC_WM:
		label = "wm";
		break;
	default:
		errx(1, "%s: invalid cmd %d", __func__, cmd);
		/* NOTREACHED */
	}

	TAILQ_INIT(&menuq);

	if ((path = getenv("PATH")) == NULL)
		path = _PATH_DEFPATH;
	pathcpy = path = xstrdup(path);

	for (ap = paths; ap < &paths[NPATHS - 1] &&
	    (*ap = strsep(&pathcpy, ":")) != NULL;) {
		if (**ap != '\0')
			ap++;
	}
	*ap = NULL;
	for (i = 0; i < NPATHS && paths[i] != NULL; i++) {
		if ((dirp = opendir(paths[i])) == NULL)
			continue;

		while ((dp = readdir(dirp)) != NULL) {
			(void)memset(tpath, '\0', sizeof(tpath));
			l = snprintf(tpath, sizeof(tpath), "%s/%s", paths[i],
			    dp->d_name);
			if (l == -1 || l >= sizeof(tpath))
				continue;
			/* Skip everything but regular files and symlinks. */
			if (dp->d_type != DT_REG && dp->d_type != DT_LNK) {
				/* lstat(2) in case d_type isn't supported. */
				if (lstat(tpath, &sb) == -1)
					continue;
				if (!S_ISREG(sb.st_mode) && 
				    !S_ISLNK(sb.st_mode))
					continue;
			}
			if (access(tpath, X_OK) == 0)
				menuq_add(&menuq, NULL, "%s", dp->d_name);
		}
		(void)closedir(dirp);
	}
	free(path);

	if ((mi = menu_filter(sc, &menuq, label, NULL,
	    (CWM_MENU_DUMMY | CWM_MENU_FILE),
	    search_match_exec, search_print_text)) != NULL) {
		if (mi->text[0] == '\0')
			goto out;
		switch (cmd) {
		case CWM_MENU_EXEC_EXEC:
			u_spawn(mi->text);
			break;
		case CWM_MENU_EXEC_WM:
			cwm_status = CWM_EXEC_WM;
			free(Conf.wm_argv);
			Conf.wm_argv = xstrdup(mi->text);
			break;
		default:
			errx(1, "%s: egad, cmd changed value!", __func__);
			/* NOTREACHED */
		}
	}
out:
	if (mi != NULL && mi->dummy)
		free(mi);
	menuq_clear(&menuq);
}

void
kbfunc_menu_ssh(void *ctx, struct cargs *cargs)
{
	struct screen_ctx	*sc = ctx;
	struct cmd_ctx		*cmd;
	struct menu		*mi;
	struct menu_q		 menuq;
	FILE			*fp;
	char			*buf, *lbuf, *p;
	char			 hostbuf[_POSIX_HOST_NAME_MAX+1];
	char			 path[PATH_MAX];
	int			 l;
	size_t			 len;
	ssize_t			 slen;

	TAILQ_FOREACH(cmd, &Conf.cmdq, entry) {
		if (strcmp(cmd->name, "term") == 0)
			break;
	}
	TAILQ_INIT(&menuq);

	if ((fp = fopen(Conf.known_hosts, "r")) == NULL) {
		warn("%s: %s", __func__, Conf.known_hosts);
		goto menu;
	}

	lbuf = NULL;
	len = 0;
	while ((slen = getline(&lbuf, &len, fp)) != -1) {
		buf = lbuf;
		if (buf[slen - 1] == '\n')
			buf[slen - 1] = '\0';
		
		/* skip hashed hosts */
		if (strncmp(buf, HASH_MARKER, strlen(HASH_MARKER)) == 0)
			continue;
		for (p = buf; *p != ',' && *p != ' ' && p != buf + slen; p++) {
			/* do nothing */
		}
		/* ignore badness */
		if (p - buf + 1 > sizeof(hostbuf))
			continue;
		(void)strlcpy(hostbuf, buf, p - buf + 1);
		menuq_add(&menuq, NULL, "%s", hostbuf);
	}
	free(lbuf);
	if (ferror(fp))
		err(1, "%s", path);
	(void)fclose(fp);
menu:
	if ((mi = menu_filter(sc, &menuq, "ssh", NULL, (CWM_MENU_DUMMY),
	    search_match_text, search_print_text)) != NULL) {
		if (mi->text[0] == '\0')
			goto out;
		l = snprintf(path, sizeof(path), "%s -T '[ssh] %s' -e ssh %s",
		    cmd->path, mi->text, mi->text);
		if (l == -1 || l >= sizeof(path))
			goto out;
		u_spawn(path);
	}
out:
	if (mi != NULL && mi->dummy)
		free(mi);
	menuq_clear(&menuq);
}

void
kbfunc_client_menu_label(void *ctx, struct cargs *cargs)
{
	struct client_ctx	*cc = ctx;
	struct menu		*mi;
	struct menu_q		 menuq;

	TAILQ_INIT(&menuq);

	/* dummy is set, so this will always return */
	mi = menu_filter(cc->sc, &menuq, "label", cc->label, (CWM_MENU_DUMMY),
	    search_match_text, search_print_text);

	if (!mi->abort) {
		free(cc->label);
		cc->label = xstrdup(mi->text);
	}
	free(mi);
}

void
kbfunc_exec_cmd(void *ctx, struct cargs *cargs)
{
	u_spawn(cargs->cmd);
}

void
kbfunc_exec_term(void *ctx, struct cargs *cargs)
{
	struct cmd_ctx	*cmd;

	TAILQ_FOREACH(cmd, &Conf.cmdq, entry) {
		if (strcmp(cmd->name, "term") == 0)
			u_spawn(cmd->path);
	}
}

void
kbfunc_exec_lock(void *ctx, struct cargs *cargs)
{
	struct cmd_ctx	*cmd;

	TAILQ_FOREACH(cmd, &Conf.cmdq, entry) {
		if (strcmp(cmd->name, "lock") == 0)
			u_spawn(cmd->path);
	}
}

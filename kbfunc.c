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

#include <sys/types.h>
#include <sys/queue.h>

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

static void kbfunc_amount(int, int, unsigned int *, unsigned int *);

void
kbfunc_cwm_status(void *ctx, union arg *arg, enum xev xev)
{
	cwm_status = arg->i;
}

static void
kbfunc_amount(int flags, int amt, unsigned int *mx, unsigned int *my)
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
kbfunc_ptrmove(void *ctx, union arg *arg, enum xev xev)
{
	struct screen_ctx	*sc = ctx;
	int			 x, y;
	unsigned int		 mx = 0, my = 0;

	kbfunc_amount(arg->i, Conf.mamount, &mx, &my);

	xu_ptr_getpos(sc->rootwin, &x, &y);
	xu_ptr_setpos(sc->rootwin, x + mx, y + my);
}

void
kbfunc_client_move(void *ctx, union arg *arg, enum xev xev)
{
	struct client_ctx	*cc = ctx;
	struct screen_ctx	*sc = cc->sc;
	struct geom		 area;
	int			 x, y, px, py;
	unsigned int		 mx = 0, my = 0;

	if (cc->flags & CLIENT_FREEZE)
		return;

	xu_ptr_getpos(cc->win, &px, &py);
	if (px < 0)
		px = 0;
	else if (px > cc->geom.w)
		px = cc->geom.w;
	if (py < 0)
		py = 0;
	else if (py > cc->geom.h)
		py = cc->geom.h;

	xu_ptr_setpos(cc->win, px, py);

	kbfunc_amount(arg->i, Conf.mamount, &mx, &my);

	cc->geom.x += mx;
	if (cc->geom.x + cc->geom.w < 0)
		cc->geom.x = -cc->geom.w;
	if (cc->geom.x > sc->view.w - 1)
		cc->geom.x = sc->view.w - 1;
	cc->geom.y += my;
	if (cc->geom.y + cc->geom.h < 0)
		cc->geom.y = -cc->geom.h;
	if (cc->geom.y > sc->view.h - 1)
		cc->geom.y = sc->view.h - 1;

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

	xu_ptr_getpos(cc->win, &x, &y);
	cc->ptr.x = x + mx;
	cc->ptr.y = y + my;
	client_ptrwarp(cc);
}

void
kbfunc_client_resize(void *ctx, union arg *arg, enum xev xev)
{
	struct client_ctx	*cc = ctx;
	unsigned int		 mx = 0, my = 0;
	int			 amt = 1;

	if (cc->flags & CLIENT_FREEZE)
		return;

	if (!(cc->hint.flags & PResizeInc))
		amt = Conf.mamount;

	kbfunc_amount(arg->i, amt, &mx, &my);

	if ((cc->geom.w += mx * cc->hint.incw) < cc->hint.minw)
		cc->geom.w = cc->hint.minw;
	if ((cc->geom.h += my * cc->hint.inch) < cc->hint.minh)
		cc->geom.h = cc->hint.minh;
	client_resize(cc, 1);

	/* Make sure the pointer stays within the window. */
	xu_ptr_getpos(cc->win, &cc->ptr.x, &cc->ptr.y);
	if (cc->ptr.x > cc->geom.w)
		cc->ptr.x = cc->geom.w - cc->bwidth;
	if (cc->ptr.y > cc->geom.h)
		cc->ptr.y = cc->geom.h - cc->bwidth;
	client_ptrwarp(cc);
}

void
kbfunc_client_delete(void *ctx, union arg *arg, enum xev xev)
{
	client_send_delete(ctx);
}

void
kbfunc_client_lower(void *ctx, union arg *arg, enum xev xev)
{
	client_ptrsave(ctx);
	client_lower(ctx);
}

void
kbfunc_client_raise(void *ctx, union arg *arg, enum xev xev)
{
	client_raise(ctx);
}

void
kbfunc_client_hide(void *ctx, union arg *arg, enum xev xev)
{
	client_hide(ctx);
}

void
kbfunc_client_toggle_freeze(void *ctx, union arg *arg, enum xev xev)
{
	client_toggle_freeze(ctx);
}

void
kbfunc_client_toggle_sticky(void *ctx, union arg *arg, enum xev xev)
{
	client_toggle_sticky(ctx);
}

void
kbfunc_client_toggle_fullscreen(void *ctx, union arg *arg, enum xev xev)
{
	client_toggle_fullscreen(ctx);
}

void
kbfunc_client_toggle_maximize(void *ctx, union arg *arg, enum xev xev)
{
	client_toggle_maximize(ctx);
}

void
kbfunc_client_toggle_hmaximize(void *ctx, union arg *arg, enum xev xev)
{
	client_toggle_hmaximize(ctx);
}

void
kbfunc_client_toggle_vmaximize(void *ctx, union arg *arg, enum xev xev)
{
	client_toggle_vmaximize(ctx);
}

void
kbfunc_client_htile(void *ctx, union arg *arg, enum xev xev)
{
	client_htile(ctx);
}

void
kbfunc_client_vtile(void *ctx, union arg *arg, enum xev xev)
{
	client_vtile(ctx);
}

void
kbfunc_client_cycle(void *ctx, union arg *arg, enum xev xev)
{
	client_cycle(ctx, arg->i);
}

void
kbfunc_client_toggle_group(void *ctx, union arg *arg, enum xev xev)
{
	struct client_ctx	*cc = ctx;

	if (xev == CWM_XEV_KEY) {
		/* For X apps that steal events. */
		XGrabKeyboard(X_Dpy, cc->win, True,
		    GrabModeAsync, GrabModeAsync, CurrentTime);
	}

	group_toggle_membership_enter(cc);
}

void
kbfunc_client_movetogroup(void *ctx, union arg *arg, enum xev xev)
{
	group_movetogroup(ctx, arg->i);
}

void
kbfunc_group_toggle(void *ctx, union arg *arg, enum xev xev)
{
	group_hidetoggle(ctx, arg->i);
}

void
kbfunc_group_only(void *ctx, union arg *arg, enum xev xev)
{
	group_only(ctx, arg->i);
}

void
kbfunc_group_cycle(void *ctx, union arg *arg, enum xev xev)
{
	group_cycle(ctx, arg->i);
}

void
kbfunc_group_alltoggle(void *ctx, union arg *arg, enum xev xev)
{
	group_alltoggle(ctx);
}

void
kbfunc_menu_client(void *ctx, union arg *arg, enum xev xev)
{
	struct screen_ctx	*sc = ctx;
	struct client_ctx	*cc, *old_cc;
	struct menu		*mi;
	struct menu_q		 menuq;
	int			 m = (xev == CWM_XEV_BTN);
	int			 all = (arg->i & CWM_MENU_WINDOW_ALL);

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
kbfunc_menu_cmd(void *ctx, union arg *arg, enum xev xev)
{
	struct screen_ctx	*sc = ctx;
	struct cmd_ctx		*cmd;
	struct menu		*mi;
	struct menu_q		 menuq;
	int			 m = (xev == CWM_XEV_BTN);

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
kbfunc_menu_group(void *ctx, union arg *arg, enum xev xev)
{
	struct screen_ctx	*sc = ctx;
	struct group_ctx	*gc;
	struct menu		*mi;
	struct menu_q		 menuq;
	int			 m = (xev == CWM_XEV_BTN);

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
kbfunc_menu_exec(void *ctx, union arg *arg, enum xev xev)
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
	int			 l, i, cmd = arg->i;

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
	    search_match_exec_path, NULL)) != NULL) {
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
kbfunc_menu_ssh(void *ctx, union arg *arg, enum xev xev)
{
	struct screen_ctx	*sc = ctx;
	struct cmd_ctx		*cmd;
	struct menu		*mi;
	struct menu_q		 menuq;
	FILE			*fp;
	char			*buf, *lbuf, *p;
	char			 hostbuf[HOST_NAME_MAX+1];
	char			 path[PATH_MAX];
	int			 l;
	size_t			 len;

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
	while ((buf = fgetln(fp, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			/* EOF without EOL, copy and add the NUL */
			lbuf = xmalloc(len + 1);
			(void)memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}
		/* skip hashed hosts */
		if (strncmp(buf, HASH_MARKER, strlen(HASH_MARKER)) == 0)
			continue;
		for (p = buf; *p != ',' && *p != ' ' && p != buf + len; p++) {
			/* do nothing */
		}
		/* ignore badness */
		if (p - buf + 1 > sizeof(hostbuf))
			continue;
		(void)strlcpy(hostbuf, buf, p - buf + 1);
		menuq_add(&menuq, NULL, hostbuf);
	}
	free(lbuf);
	(void)fclose(fp);
menu:
	if ((mi = menu_filter(sc, &menuq, "ssh", NULL, (CWM_MENU_DUMMY),
	    search_match_text, NULL)) != NULL) {
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
kbfunc_menu_client_label(void *ctx, union arg *arg, enum xev xev)
{
	struct client_ctx	*cc = ctx;
	struct menu		*mi;
	struct menu_q		 menuq;

	TAILQ_INIT(&menuq);

	/* dummy is set, so this will always return */
	mi = menu_filter(cc->sc, &menuq, "label", cc->label, (CWM_MENU_DUMMY),
	    search_match_text, NULL);

	if (!mi->abort) {
		free(cc->label);
		cc->label = xstrdup(mi->text);
	}
	free(mi);
}

void
kbfunc_exec_cmd(void *ctx, union arg *arg, enum xev xev)
{
	u_spawn(arg->c);
}

void
kbfunc_exec_term(void *ctx, union arg *arg, enum xev xev)
{
	struct cmd_ctx	*cmd;

	TAILQ_FOREACH(cmd, &Conf.cmdq, entry) {
		if (strcmp(cmd->name, "term") == 0)
			u_spawn(cmd->path);
	}
}

void
kbfunc_exec_lock(void *ctx, union arg *arg, enum xev xev)
{
	struct cmd_ctx	*cmd;

	TAILQ_FOREACH(cmd, &Conf.cmdq, entry) {
		if (strcmp(cmd->name, "lock") == 0)
			u_spawn(cmd->path);
	}
}

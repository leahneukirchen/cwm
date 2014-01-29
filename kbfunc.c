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

#include <sys/param.h>
#include <sys/queue.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

#define HASH_MARKER	"|1|"

void
kbfunc_client_lower(struct client_ctx *cc, union arg *arg)
{
	client_ptrsave(cc);
	client_lower(cc);
}

void
kbfunc_client_raise(struct client_ctx *cc, union arg *arg)
{
	client_raise(cc);
}

#define TYPEMASK	(CWM_MOVE | CWM_RESIZE | CWM_PTRMOVE)
#define MOVEMASK	(CWM_UP | CWM_DOWN | CWM_LEFT | CWM_RIGHT)
void
kbfunc_client_moveresize(struct client_ctx *cc, union arg *arg)
{
	struct screen_ctx	*sc = cc->sc;
	struct geom		 xine;
	int			 x, y, flags, amt;
	unsigned int		 mx, my;

	if (cc->flags & CLIENT_FREEZE)
		return;

	mx = my = 0;

	flags = arg->i;
	amt = Conf.mamount;

	if (flags & CWM_BIGMOVE) {
		flags -= CWM_BIGMOVE;
		amt = amt * 10;
	}

	switch (flags & MOVEMASK) {
	case CWM_UP:
		my -= amt;
		break;
	case CWM_DOWN:
		my += amt;
		break;
	case CWM_RIGHT:
		mx += amt;
		break;
	case CWM_LEFT:
		mx -= amt;
		break;
	}
	switch (flags & TYPEMASK) {
	case CWM_MOVE:
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

		xine = screen_find_xinerama(sc,
		    cc->geom.x + cc->geom.w / 2,
		    cc->geom.y + cc->geom.h / 2, CWM_GAP);
		cc->geom.x += client_snapcalc(cc->geom.x,
		    cc->geom.x + cc->geom.w + (cc->bwidth * 2),
		    xine.x, xine.x + xine.w, sc->snapdist);
		cc->geom.y += client_snapcalc(cc->geom.y,
		    cc->geom.y + cc->geom.h + (cc->bwidth * 2),
		    xine.y, xine.y + xine.h, sc->snapdist);

		client_move(cc);
		xu_ptr_getpos(cc->win, &x, &y);
		cc->ptr.x = x + mx;
		cc->ptr.y = y + my;
		client_ptrwarp(cc);
		break;
	case CWM_RESIZE:
		if ((cc->geom.w += mx) < 1)
			cc->geom.w = 1;
		if ((cc->geom.h += my) < 1)
			cc->geom.h = 1;
		client_resize(cc, 1);

		/* Make sure the pointer stays within the window. */
		xu_ptr_getpos(cc->win, &cc->ptr.x, &cc->ptr.y);
		if (cc->ptr.x > cc->geom.w)
			cc->ptr.x = cc->geom.w - cc->bwidth;
		if (cc->ptr.y > cc->geom.h)
			cc->ptr.y = cc->geom.h - cc->bwidth;
		client_ptrwarp(cc);
		break;
	case CWM_PTRMOVE:
		xu_ptr_getpos(sc->rootwin, &x, &y);
		xu_ptr_setpos(sc->rootwin, x + mx, y + my);
		break;
	default:
		warnx("invalid flags passed to kbfunc_client_moveresize");
	}
}

void
kbfunc_client_search(struct client_ctx *cc, union arg *arg)
{
	struct screen_ctx	*sc = cc->sc;
	struct client_ctx	*old_cc;
	struct menu		*mi;
	struct menu_q		 menuq;

	old_cc = client_current();

	TAILQ_INIT(&menuq);
	TAILQ_FOREACH(cc, &Clientq, entry)
		menuq_add(&menuq, cc, "%s", cc->name);

	if ((mi = menu_filter(sc, &menuq, "window", NULL, 0,
	    search_match_client, search_print_client)) != NULL) {
		cc = (struct client_ctx *)mi->ctx;
		if (cc->flags & CLIENT_HIDDEN)
			client_unhide(cc);

		if (old_cc)
			client_ptrsave(old_cc);
		client_ptrwarp(cc);
	}

	menuq_clear(&menuq);
}

void
kbfunc_menu_cmd(struct client_ctx *cc, union arg *arg)
{
	struct screen_ctx	*sc = cc->sc;
	struct cmd		*cmd;
	struct menu		*mi;
	struct menu_q		 menuq;

	TAILQ_INIT(&menuq);
	TAILQ_FOREACH(cmd, &Conf.cmdq, entry)
		menuq_add(&menuq, cmd, "%s", cmd->name);

	if ((mi = menu_filter(sc, &menuq, "application", NULL, 0,
	    search_match_text, NULL)) != NULL)
		u_spawn(((struct cmd *)mi->ctx)->path);

	menuq_clear(&menuq);
}

void
kbfunc_client_cycle(struct client_ctx *cc, union arg *arg)
{
	struct screen_ctx	*sc = cc->sc;

	/* XXX for X apps that ignore events */
	XGrabKeyboard(X_Dpy, sc->rootwin, True,
	    GrabModeAsync, GrabModeAsync, CurrentTime);

	client_cycle(sc, arg->i);
}

void
kbfunc_client_hide(struct client_ctx *cc, union arg *arg)
{
	client_hide(cc);
}

void
kbfunc_cmdexec(struct client_ctx *cc, union arg *arg)
{
	u_spawn(arg->c);
}

void
kbfunc_term(struct client_ctx *cc, union arg *arg)
{
	u_spawn(Conf.termpath);
}

void
kbfunc_lock(struct client_ctx *cc, union arg *arg)
{
	u_spawn(Conf.lockpath);
}

void
kbfunc_exec(struct client_ctx *cc, union arg *arg)
{
#define NPATHS 256
	struct screen_ctx	*sc = cc->sc;
	char			**ap, *paths[NPATHS], *path, *pathcpy;
	char			 tpath[MAXPATHLEN];
	const char		*label;
	DIR			*dirp;
	struct dirent		*dp;
	struct menu		*mi;
	struct menu_q		 menuq;
	int			 l, i, cmd = arg->i;

	switch (cmd) {
	case CWM_EXEC_PROGRAM:
		label = "exec";
		break;
	case CWM_EXEC_WM:
		label = "wm";
		break;
	default:
		errx(1, "kbfunc_exec: invalid cmd %d", cmd);
		/*NOTREACHED*/
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
			/* skip everything but regular files and symlinks */
			if (dp->d_type != DT_REG && dp->d_type != DT_LNK)
				continue;
			(void)memset(tpath, '\0', sizeof(tpath));
			l = snprintf(tpath, sizeof(tpath), "%s/%s", paths[i],
			    dp->d_name);
			/* check for truncation etc */
			if (l == -1 || l >= (int)sizeof(tpath))
				continue;
			if (access(tpath, X_OK) == 0)
				menuq_add(&menuq, NULL, "%s", dp->d_name);
		}
		(void)closedir(dirp);
	}
	free(path);

	if ((mi = menu_filter(sc, &menuq, label, NULL,
	    CWM_MENU_DUMMY | CWM_MENU_FILE,
	    search_match_exec_path, NULL)) != NULL) {
		if (mi->text[0] == '\0')
			goto out;
		switch (cmd) {
		case CWM_EXEC_PROGRAM:
			u_spawn(mi->text);
			break;
		case CWM_EXEC_WM:
			u_exec(mi->text);
			warn("%s", mi->text);
			break;
		default:
			errx(1, "kb_func: egad, cmd changed value!");
			break;
		}
	}
out:
	if (mi != NULL && mi->dummy)
		free(mi);
	menuq_clear(&menuq);
}

void
kbfunc_ssh(struct client_ctx *cc, union arg *arg)
{
	struct screen_ctx	*sc = cc->sc;
	struct menu		*mi;
	struct menu_q		 menuq;
	FILE			*fp;
	char			*buf, *lbuf, *p;
	char			 hostbuf[MAXHOSTNAMELEN];
	char			 cmd[256];
	int			 l;
	size_t			 len;

	if ((fp = fopen(Conf.known_hosts, "r")) == NULL) {
		warn("kbfunc_ssh: %s", Conf.known_hosts);
		return;
	}

	TAILQ_INIT(&menuq);

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

	if ((mi = menu_filter(sc, &menuq, "ssh", NULL, CWM_MENU_DUMMY,
	    search_match_exec, NULL)) != NULL) {
		if (mi->text[0] == '\0')
			goto out;
		l = snprintf(cmd, sizeof(cmd), "%s -T '[ssh] %s' -e ssh %s",
		    Conf.termpath, mi->text, mi->text);
		if (l != -1 && l < sizeof(cmd))
			u_spawn(cmd);
	}
out:
	if (mi != NULL && mi->dummy)
		free(mi);
	menuq_clear(&menuq);
}

void
kbfunc_client_label(struct client_ctx *cc, union arg *arg)
{
	struct menu	*mi;
	struct menu_q	 menuq;

	TAILQ_INIT(&menuq);

	/* dummy is set, so this will always return */
	mi = menu_filter(cc->sc, &menuq, "label", cc->label, CWM_MENU_DUMMY,
	    search_match_text, NULL);

	if (!mi->abort) {
		free(cc->label);
		cc->label = xstrdup(mi->text);
	}
	free(mi);
}

void
kbfunc_client_delete(struct client_ctx *cc, union arg *arg)
{
	client_send_delete(cc);
}

void
kbfunc_client_group(struct client_ctx *cc, union arg *arg)
{
	group_hidetoggle(cc->sc, arg->i);
}

void
kbfunc_client_grouponly(struct client_ctx *cc, union arg *arg)
{
	group_only(cc->sc, arg->i);
}

void
kbfunc_client_cyclegroup(struct client_ctx *cc, union arg *arg)
{
	group_cycle(cc->sc, arg->i);
}

void
kbfunc_client_nogroup(struct client_ctx *cc, union arg *arg)
{
	group_alltoggle(cc->sc);
}

void
kbfunc_client_grouptoggle(struct client_ctx *cc, union arg *arg)
{
	/* XXX for stupid X apps like xpdf and gvim */
	XGrabKeyboard(X_Dpy, cc->win, True,
	    GrabModeAsync, GrabModeAsync, CurrentTime);

	group_sticky_toggle_enter(cc);
}

void
kbfunc_client_movetogroup(struct client_ctx *cc, union arg *arg)
{
	group_movetogroup(cc, arg->i);
}

void
kbfunc_client_fullscreen(struct client_ctx *cc, union arg *arg)
{
	client_fullscreen(cc);
}

void
kbfunc_client_maximize(struct client_ctx *cc, union arg *arg)
{
	client_maximize(cc);
}

void
kbfunc_client_vmaximize(struct client_ctx *cc, union arg *arg)
{
	client_vmaximize(cc);
}

void
kbfunc_client_hmaximize(struct client_ctx *cc, union arg *arg)
{
	client_hmaximize(cc);
}

void
kbfunc_client_freeze(struct client_ctx *cc, union arg *arg)
{
	client_freeze(cc);
}

void
kbfunc_cwm_status(struct client_ctx *cc, union arg *arg)
{
	cwm_status = arg->i;
}

void
kbfunc_tile(struct client_ctx *cc, union arg *arg)
{
	switch (arg->i) {
	case CWM_TILE_HORIZ:
		client_htile(cc);
		break;
	case CWM_TILE_VERT:
		client_vtile(cc);
		break;
	}
}

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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "calmwm.h"

#define KNOWN_HOSTS	".ssh/known_hosts"
#define HASH_MARKER	"|1|"

extern char		**cwm_argv;
extern sig_atomic_t	xev_quit;

void
kbfunc_client_lower(struct client_ctx *cc, union arg *arg)
{
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
kbfunc_moveresize(struct client_ctx *cc, union arg *arg)
{
	struct screen_ctx	*sc = cc->sc;
	int			 x, y, flags, amt;
	u_int			 mx, my;

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
		cc->geom.y += my;
		if (cc->geom.y + cc->geom.h < 0)
			cc->geom.y = -cc->geom.h;
		if (cc->geom.y > sc->view.h - 1)
			cc->geom.y = sc->view.h - 1;

		cc->geom.x += mx;
		if (cc->geom.x + cc->geom.w < 0)
			cc->geom.x = -cc->geom.w;
		if (cc->geom.x > sc->view.w - 1)
			cc->geom.x = sc->view.w - 1;

		cc->geom.x += client_snapcalc(cc->geom.x,
		    cc->geom.w, sc->view.w,
		    cc->bwidth, Conf.snapdist);
		cc->geom.y += client_snapcalc(cc->geom.y,
		    cc->geom.h, sc->view.h,
		    cc->bwidth, Conf.snapdist);

		client_move(cc);
		xu_ptr_getpos(cc->win, &x, &y);
		cc->ptr.y = y + my;
		cc->ptr.x = x + mx;
		client_ptrwarp(cc);
		break;
	case CWM_RESIZE:
		if ((cc->geom.h += my) < 1)
			cc->geom.h = 1;
		if ((cc->geom.w += mx) < 1)
			cc->geom.w = 1;
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

	TAILQ_FOREACH(cc, &Clientq, entry) {
		mi = xcalloc(1, sizeof(*mi));
		(void)strlcpy(mi->text, cc->name, sizeof(mi->text));
		mi->ctx = cc;
		TAILQ_INSERT_TAIL(&menuq, mi, entry);
	}

	if ((mi = menu_filter(sc, &menuq, "window", NULL, 0,
	    search_match_client, search_print_client)) != NULL) {
		cc = (struct client_ctx *)mi->ctx;
		if (cc->flags & CLIENT_HIDDEN)
			client_unhide(cc);

		if (old_cc)
			client_ptrsave(old_cc);
		client_ptrwarp(cc);
	}

	while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
		TAILQ_REMOVE(&menuq, mi, entry);
		free(mi);
	}
}

void
kbfunc_menu_search(struct client_ctx *cc, union arg *arg)
{
	struct screen_ctx	*sc = cc->sc;
	struct cmd		*cmd;
	struct menu		*mi;
	struct menu_q		 menuq;

	TAILQ_INIT(&menuq);

	TAILQ_FOREACH(cmd, &Conf.cmdq, entry) {
		mi = xcalloc(1, sizeof(*mi));
		(void)strlcpy(mi->text, cmd->label, sizeof(mi->text));
		mi->ctx = cmd;
		TAILQ_INSERT_TAIL(&menuq, mi, entry);
	}

	if ((mi = menu_filter(sc, &menuq, "application", NULL, 0,
	    search_match_text, NULL)) != NULL)
		u_spawn(((struct cmd *)mi->ctx)->image);

	while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
		TAILQ_REMOVE(&menuq, mi, entry);
		free(mi);
	}
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
	char			**ap, *paths[NPATHS], *path, *pathcpy, *label;
	char			 tpath[MAXPATHLEN];
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
			err(1, "kbfunc_exec: invalid cmd %d", cmd);
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
			if (access(tpath, X_OK) == 0) {
				mi = xcalloc(1, sizeof(*mi));
				(void)strlcpy(mi->text,
				    dp->d_name, sizeof(mi->text));
				TAILQ_INSERT_TAIL(&menuq, mi, entry);
			}
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
				err(1, "kb_func: egad, cmd changed value!");
				break;
		}
	}
out:
	if (mi != NULL && mi->dummy)
		free(mi);
	while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
		TAILQ_REMOVE(&menuq, mi, entry);
		free(mi);
	}
}

void
kbfunc_ssh(struct client_ctx *cc, union arg *arg)
{
	struct screen_ctx	*sc = cc->sc;
	struct menu		*mi;
	struct menu_q		 menuq;
	FILE			*fp;
	char			*buf, *lbuf, *p, *home;
	char			 hostbuf[MAXHOSTNAMELEN], filename[MAXPATHLEN];
	char			 cmd[256];
	int			 l;
	size_t			 len;

	if ((home = getenv("HOME")) == NULL)
		return;

	l = snprintf(filename, sizeof(filename), "%s/%s", home, KNOWN_HOSTS);
	if (l == -1 || l >= sizeof(filename))
		return;

	if ((fp = fopen(filename, "r")) == NULL)
		return;

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
		mi = xcalloc(1, sizeof(*mi));
		(void)strlcpy(mi->text, hostbuf, sizeof(mi->text));
		TAILQ_INSERT_TAIL(&menuq, mi, entry);
	}
	free(lbuf);
	(void)fclose(fp);

	if ((mi = menu_filter(sc, &menuq, "ssh", NULL, CWM_MENU_DUMMY,
	    search_match_exec, NULL)) != NULL) {
		if (mi->text[0] == '\0')
			goto out;
		l = snprintf(cmd, sizeof(cmd), "%s -e ssh %s", Conf.termpath,
		    mi->text);
		if (l != -1 && l < sizeof(cmd))
			u_spawn(cmd);
	}
out:
	if (mi != NULL && mi->dummy)
		free(mi);
	while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
		TAILQ_REMOVE(&menuq, mi, entry);
		free(mi);
	}
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
	group_hidetoggle(cc->sc, KBTOGROUP(arg->i));
}

void
kbfunc_client_grouponly(struct client_ctx *cc, union arg *arg)
{
	group_only(cc->sc, KBTOGROUP(arg->i));
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
	group_movetogroup(cc, KBTOGROUP(arg->i));
}

void
kbfunc_client_maximize(struct client_ctx *cc, union arg *arg)
{
	client_maximize(cc);
}

void
kbfunc_client_vmaximize(struct client_ctx *cc, union arg *arg)
{
	client_vertmaximize(cc);
}

void
kbfunc_client_hmaximize(struct client_ctx *cc, union arg *arg)
{
	client_horizmaximize(cc);
}

void
kbfunc_client_freeze(struct client_ctx *cc, union arg *arg)
{
	client_freeze(cc);
}

void
kbfunc_quit_wm(struct client_ctx *cc, union arg *arg)
{
	xev_quit = 1;
}

void
kbfunc_restart(struct client_ctx *cc, union arg *arg)
{
	(void)setsid();
	(void)execvp(cwm_argv[0], cwm_argv);
}

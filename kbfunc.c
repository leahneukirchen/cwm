/*
 *  calmwm - the calm window manager
 *
 *  Copyright (c) 2004 Martin Murray <mmurray@monkey.org>
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

#include <paths.h>

#include "headers.h"
#include "calmwm.h"

#define KNOWN_HOSTS	".ssh/known_hosts"
#define HASH_MARKER	"|1|"

extern int		_xev_quit;

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

#define typemask	(CWM_MOVE | CWM_RESIZE | CWM_PTRMOVE)
#define movemask	(CWM_UP | CWM_DOWN | CWM_LEFT | CWM_RIGHT)
void
kbfunc_moveresize(struct client_ctx *cc, union arg *arg)
{
	struct screen_ctx	*sc;
	int			 x, y, flags, amt;
	u_int			 mx, my;

	sc = screen_current();
	mx = my = 0;

	flags = arg->i;
	amt = Conf.mamount;

	if (flags & CWM_BIGMOVE) {
		flags -= CWM_BIGMOVE;
		amt = amt * 10;
	}

	switch (flags & movemask) {
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
	switch (flags & typemask) {
	case CWM_MOVE:
		cc->geom.y += my;
		if (cc->geom.y + cc->geom.height < 0)
			cc->geom.y = -cc->geom.height;
		if (cc->geom.y > cc->sc->ymax - 1)
			cc->geom.y = cc->sc->ymax - 1;

		cc->geom.x += mx;
		if (cc->geom.x + cc->geom.width < 0)
			cc->geom.x = -cc->geom.width;
		if (cc->geom.x > cc->sc->xmax - 1)
			cc->geom.x = cc->sc->xmax - 1;

		client_move(cc);
		xu_ptr_getpos(cc->win, &x, &y);
		cc->ptr.y = y + my;
		cc->ptr.x = x + mx;
		client_ptrwarp(cc);
		break;
	case CWM_RESIZE:
		if ((cc->geom.height += my) < 1)
			cc->geom.height = 1;
		if ((cc->geom.width += mx) < 1)
			cc->geom.width = 1;
		client_resize(cc);

		/* Make sure the pointer stays within the window. */
		xu_ptr_getpos(cc->win, &cc->ptr.x, &cc->ptr.y);
		if (cc->ptr.x > cc->geom.width)
			cc->ptr.x = cc->geom.width - cc->bwidth;
		if (cc->ptr.y > cc->geom.height)
			cc->ptr.y = cc->geom.height - cc->bwidth;
		client_ptrwarp(cc);
		break;
	case CWM_PTRMOVE:
		if (cc) {
			xu_ptr_getpos(cc->win, &x, &y);
			xu_ptr_setpos(cc->win, x + mx, y + my);
		} else {
			xu_ptr_getpos(sc->rootwin, &x, &y);
			xu_ptr_setpos(sc->rootwin, x + mx, y + my);
		}
		break;
	default:
		warnx("invalid flags passed to kbfunc_client_moveresize");
	}
}

void
kbfunc_client_search(struct client_ctx *scratch, union arg *arg)
{
	struct client_ctx	*cc, *old_cc;
	struct menu		*mi;
	struct menu_q		 menuq;

	old_cc = client_current();

	TAILQ_INIT(&menuq);

	TAILQ_FOREACH(cc, &Clientq, entry) {
		XCALLOC(mi, struct menu);
		strlcpy(mi->text, cc->name, sizeof(mi->text));
		mi->ctx = cc;
		TAILQ_INSERT_TAIL(&menuq, mi, entry);
	}

	if ((mi = menu_filter(&menuq, "window", NULL, 0,
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
		xfree(mi);
	}
}

void
kbfunc_menu_search(struct client_ctx *scratch, union arg *arg)
{
	struct cmd	*cmd;
	struct menu	*mi;
	struct menu_q	 menuq;

	TAILQ_INIT(&menuq);

	TAILQ_FOREACH(cmd, &Conf.cmdq, entry) {
		XCALLOC(mi, struct menu);
		strlcpy(mi->text, cmd->label, sizeof(mi->text));
		mi->ctx = cmd;
		TAILQ_INSERT_TAIL(&menuq, mi, entry);
	}

	if ((mi = menu_filter(&menuq, "application", NULL, 0,
	    search_match_text, NULL)) != NULL)
		u_spawn(((struct cmd *)mi->ctx)->image);

	while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
		TAILQ_REMOVE(&menuq, mi, entry);
		xfree(mi);
	}
}

void
kbfunc_client_cycle(struct client_ctx *scratch, union arg *arg)
{
	struct screen_ctx	*sc;

	sc = screen_current();

	/* XXX for X apps that ignore events */
	XGrabKeyboard(X_Dpy, sc->rootwin, True,
	    GrabModeAsync, GrabModeAsync, CurrentTime);

	client_cycle(arg->i);
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
kbfunc_exec(struct client_ctx *scratch, union arg *arg)
{
#define NPATHS 256
	char		**ap, *paths[NPATHS], *path, *pathcpy, *label;
	char		 tpath[MAXPATHLEN];
	int		 l, i, j, ngroups;
	gid_t		 mygroups[NGROUPS_MAX];
	uid_t		 ruid, euid, suid;
	DIR		*dirp;
	struct dirent	*dp;
	struct menu	*mi;
	struct menu_q	 menuq;
	struct stat	 sb;

	int cmd = arg->i;
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

	if (getgroups(0, mygroups) == -1)
		err(1, "getgroups failure");
	if ((ngroups = getresuid(&ruid, &euid, &suid)) == -1)
		err(1, "getresuid failure");

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
			memset(tpath, '\0', sizeof(tpath));
			l = snprintf(tpath, sizeof(tpath), "%s/%s", paths[i],
			    dp->d_name);
			/* check for truncation etc */
			if (l == -1 || l >= (int)sizeof(tpath))
				continue;
			/* just ignore on stat failure */
			if (stat(tpath, &sb) == -1)
				continue;
			/* may we execute this file? */
			if (euid == sb.st_uid) {
					if (sb.st_mode & S_IXUSR)
						goto executable;
					else
						continue;
			}
			for (j = 0; j < ngroups; j++) {
				if (mygroups[j] == sb.st_gid) {
					if (sb.st_mode & S_IXGRP)
						goto executable;
					else
						continue;
				}
			}
			if (sb.st_mode & S_IXOTH)
				goto executable;
			continue;
		executable:
			/* the thing in tpath, we may execute */
			XCALLOC(mi, struct menu);
			strlcpy(mi->text, dp->d_name, sizeof(mi->text));
			TAILQ_INSERT_TAIL(&menuq, mi, entry);
		}
		(void)closedir(dirp);
	}
	xfree(path);

	if ((mi = menu_filter(&menuq, label, NULL, 1,
	    search_match_exec, NULL)) != NULL) {
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

	if (mi != NULL && mi->dummy)
		xfree(mi);
	while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
		TAILQ_REMOVE(&menuq, mi, entry);
		xfree(mi);
	}
}

void
kbfunc_ssh(struct client_ctx *scratch, union arg *arg)
{
	struct menu	*mi;
	struct menu_q	 menuq;
	FILE		*fp;
	char		*buf, *lbuf, *p, *home;
	char		 hostbuf[MAXHOSTNAMELEN], filename[MAXPATHLEN];
	char		 cmd[256];
	int		 l;
	size_t		 len;

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
			memcpy(lbuf, buf, len);
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
		(void) strlcpy(hostbuf, buf, p - buf + 1);
		XCALLOC(mi, struct menu);
		(void) strlcpy(mi->text, hostbuf, sizeof(mi->text));
		TAILQ_INSERT_TAIL(&menuq, mi, entry);
	}
	xfree(lbuf);
	fclose(fp);

	if ((mi = menu_filter(&menuq, "ssh", NULL, 1,
	    search_match_exec, NULL)) != NULL) {
		l = snprintf(cmd, sizeof(cmd), "%s -e ssh %s", Conf.termpath,
		    mi->text);
		if (l != -1 && l < sizeof(cmd))
			u_spawn(cmd);
	}

	if (mi != NULL && mi->dummy)
		xfree(mi);
	while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
		TAILQ_REMOVE(&menuq, mi, entry);
		xfree(mi);
	}
}

void
kbfunc_client_label(struct client_ctx *cc, union arg *arg)
{
	struct menu	*mi;
	struct menu_q	 menuq;
	char		*current;

	TAILQ_INIT(&menuq);
	
	if (cc->label != NULL)
		current = cc->label;
	else
		current = NULL;

	if ((mi = menu_filter(&menuq, "label", current, 1,
	    search_match_text, NULL)) != NULL) {
		if (cc->label != NULL)
			xfree(cc->label);
		cc->label = xstrdup(mi->text);
		xfree(mi);
	}
}

void
kbfunc_client_delete(struct client_ctx *cc, union arg *arg)
{
	client_send_delete(cc);
}

void
kbfunc_client_group(struct client_ctx *cc, union arg *arg)
{
	group_hidetoggle(KBTOGROUP(arg->i));
}

void
kbfunc_client_cyclegroup(struct client_ctx *cc, union arg *arg)
{
	group_cycle(arg->i);
}

void
kbfunc_client_nogroup(struct client_ctx *cc, union arg *arg)
{
	group_alltoggle();
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
kbfunc_quit_wm(struct client_ctx *cc, union arg *arg)
{
	_xev_quit = 1;
}

void
kbfunc_reload(struct client_ctx *cc, union arg *arg)
{
	conf_reload(&Conf);
}

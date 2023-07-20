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
 * $OpenBSD$
 */

#include <sys/types.h>
#include "queue.h"

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

static const char	*conf_bind_mask(const char *, unsigned int *);
static void		 conf_unbind_key(struct conf *, struct bind_ctx *);
static void		 conf_unbind_mouse(struct conf *, struct bind_ctx *);

static const struct {
	int		 num;
	const char	*name;
} group_binds[] = {
	{ 0, "nogroup" },
	{ 1, "one" },
	{ 2, "two" },
	{ 3, "three" },
	{ 4, "four" },
	{ 5, "five" },
	{ 6, "six" },
	{ 7, "seven" },
	{ 8, "eight" },
	{ 9, "nine" },
};
static int cursor_binds[] = {
	XC_left_ptr,		/* CF_NORMAL */
	XC_fleur,		/* CF_MOVE */
	XC_bottom_right_corner,	/* CF_RESIZE */
	XC_question_arrow,	/* CF_QUESTION */
};
static const char *color_binds[] = {
	"#CCCCCC",		/* CWM_COLOR_BORDER_ACTIVE */
	"#666666",		/* CWM_COLOR_BORDER_INACTIVE */
	"#FC8814",		/* CWM_COLOR_BORDER_URGENCY */
	"blue",			/* CWM_COLOR_BORDER_GROUP */
	"red",			/* CWM_COLOR_BORDER_UNGROUP */
	"black",		/* CWM_COLOR_MENU_FG */
	"white",		/* CWM_COLOR_MENU_BG */
	"black",		/* CWM_COLOR_MENU_FONT */
	"",			/* CWM_COLOR_MENU_FONT_SEL */
};
static const struct {
	const char	*tag;
	void		 (*handler)(void *, struct cargs *);
	enum context	 context;
	int		 flag;
} name_to_func[] = {
#define FUNC_CC(t, h, n) \
	#t, kbfunc_ ## h, CWM_CONTEXT_CC, n
#define FUNC_SC(t, h, n) \
	#t, kbfunc_ ## h, CWM_CONTEXT_SC, n

	{ FUNC_CC(window-lower, client_lower, 0) },
	{ FUNC_CC(window-raise, client_raise, 0) },
	{ FUNC_CC(window-hide, client_hide, 0) },
	{ FUNC_CC(window-close, client_close, 0) },
	{ FUNC_CC(window-delete, client_close, 0) },
	{ FUNC_CC(window-htile, client_htile, 0) },
	{ FUNC_CC(window-vtile, client_vtile, 0) },
	{ FUNC_CC(window-stick, client_toggle_sticky, 0) },
	{ FUNC_CC(window-fullscreen, client_toggle_fullscreen, 0) },
	{ FUNC_CC(window-maximize, client_toggle_maximize, 0) },
	{ FUNC_CC(window-vmaximize, client_toggle_vmaximize, 0) },
	{ FUNC_CC(window-hmaximize, client_toggle_hmaximize, 0) },
	{ FUNC_CC(window-freeze, client_toggle_freeze, 0) },
	{ FUNC_CC(window-group, client_toggle_group, 0) },
	{ FUNC_CC(window-movetogroup-1, client_movetogroup, 1) },
	{ FUNC_CC(window-movetogroup-2, client_movetogroup, 2) },
	{ FUNC_CC(window-movetogroup-3, client_movetogroup, 3) },
	{ FUNC_CC(window-movetogroup-4, client_movetogroup, 4) },
	{ FUNC_CC(window-movetogroup-5, client_movetogroup, 5) },
	{ FUNC_CC(window-movetogroup-6, client_movetogroup, 6) },
	{ FUNC_CC(window-movetogroup-7, client_movetogroup, 7) },
	{ FUNC_CC(window-movetogroup-8, client_movetogroup, 8) },
	{ FUNC_CC(window-movetogroup-9, client_movetogroup, 9) },
	{ FUNC_CC(window-snap-up, client_snap, (CWM_UP)) },
	{ FUNC_CC(window-snap-down, client_snap, (CWM_DOWN)) },
	{ FUNC_CC(window-snap-right, client_snap, (CWM_RIGHT)) },
	{ FUNC_CC(window-snap-left, client_snap, (CWM_LEFT)) },
	{ FUNC_CC(window-snap-up-right, client_snap, (CWM_UP_RIGHT)) },
	{ FUNC_CC(window-snap-up-left, client_snap, (CWM_UP_LEFT)) },
	{ FUNC_CC(window-snap-down-right, client_snap, (CWM_DOWN_RIGHT)) },
	{ FUNC_CC(window-snap-down-left, client_snap, (CWM_DOWN_LEFT)) },
	{ FUNC_CC(window-move, client_move, 0) },
	{ FUNC_CC(window-move-up, client_move, (CWM_UP)) },
	{ FUNC_CC(window-move-down, client_move, (CWM_DOWN)) },
	{ FUNC_CC(window-move-right, client_move, (CWM_RIGHT)) },
	{ FUNC_CC(window-move-left, client_move, (CWM_LEFT)) },
	{ FUNC_CC(window-move-up-big, client_move, (CWM_UP_BIG)) },
	{ FUNC_CC(window-move-down-big, client_move, (CWM_DOWN_BIG)) },
	{ FUNC_CC(window-move-right-big, client_move, (CWM_RIGHT_BIG)) },
	{ FUNC_CC(window-move-left-big, client_move, (CWM_LEFT_BIG)) },
	{ FUNC_CC(window-resize, client_resize, 0) },
	{ FUNC_CC(window-resize-up, client_resize, (CWM_UP)) },
	{ FUNC_CC(window-resize-down, client_resize, (CWM_DOWN)) },
	{ FUNC_CC(window-resize-right, client_resize, (CWM_RIGHT)) },
	{ FUNC_CC(window-resize-left, client_resize, (CWM_LEFT)) },
	{ FUNC_CC(window-resize-up-big, client_resize, (CWM_UP_BIG)) },
	{ FUNC_CC(window-resize-down-big, client_resize, (CWM_DOWN_BIG)) },
	{ FUNC_CC(window-resize-right-big, client_resize, (CWM_RIGHT_BIG)) },
	{ FUNC_CC(window-resize-left-big, client_resize, (CWM_LEFT_BIG)) },
	{ FUNC_CC(window-menu-label, client_menu_label, 0) },

	{ FUNC_SC(window-cycle, client_cycle, (CWM_CYCLE_FORWARD)) },
	{ FUNC_SC(window-rcycle, client_cycle, (CWM_CYCLE_REVERSE)) },
	{ FUNC_SC(window-cycle-ingroup, client_cycle,
		(CWM_CYCLE_FORWARD | CWM_CYCLE_INGROUP)) },
	{ FUNC_SC(window-rcycle-ingroup, client_cycle,
		(CWM_CYCLE_REVERSE | CWM_CYCLE_INGROUP)) },
	{ FUNC_SC(window-cycle-inclass, client_cycle,
		(CWM_CYCLE_FORWARD | CWM_CYCLE_INCLASS)) },
	{ FUNC_SC(window-rcycle-inclass, client_cycle,
		(CWM_CYCLE_REVERSE | CWM_CYCLE_INCLASS)) },

	{ FUNC_SC(group-cycle, group_cycle, (CWM_CYCLE_FORWARD)) },
	{ FUNC_SC(group-rcycle, group_cycle, (CWM_CYCLE_REVERSE)) },
	{ FUNC_SC(group-last, group_last, 0) },
	{ FUNC_SC(group-toggle-all, group_toggle_all, 0) },
	{ FUNC_SC(group-toggle-1, group_toggle, 1) },
	{ FUNC_SC(group-toggle-2, group_toggle, 2) },
	{ FUNC_SC(group-toggle-3, group_toggle, 3) },
	{ FUNC_SC(group-toggle-4, group_toggle, 4) },
	{ FUNC_SC(group-toggle-5, group_toggle, 5) },
	{ FUNC_SC(group-toggle-6, group_toggle, 6) },
	{ FUNC_SC(group-toggle-7, group_toggle, 7) },
	{ FUNC_SC(group-toggle-8, group_toggle, 8) },
	{ FUNC_SC(group-toggle-9, group_toggle, 9) },
	{ FUNC_SC(group-only-1, group_only, 1) },
	{ FUNC_SC(group-only-2, group_only, 2) },
	{ FUNC_SC(group-only-3, group_only, 3) },
	{ FUNC_SC(group-only-4, group_only, 4) },
	{ FUNC_SC(group-only-5, group_only, 5) },
	{ FUNC_SC(group-only-6, group_only, 6) },
	{ FUNC_SC(group-only-7, group_only, 7) },
	{ FUNC_SC(group-only-8, group_only, 8) },
	{ FUNC_SC(group-only-9, group_only, 9) },
	{ FUNC_SC(group-close-1, group_close, 1) },
	{ FUNC_SC(group-close-2, group_close, 2) },
	{ FUNC_SC(group-close-3, group_close, 3) },
	{ FUNC_SC(group-close-4, group_close, 4) },
	{ FUNC_SC(group-close-5, group_close, 5) },
	{ FUNC_SC(group-close-6, group_close, 6) },
	{ FUNC_SC(group-close-7, group_close, 7) },
	{ FUNC_SC(group-close-8, group_close, 8) },
	{ FUNC_SC(group-close-9, group_close, 9) },

	{ FUNC_SC(pointer-move-up, ptrmove, (CWM_UP)) },
	{ FUNC_SC(pointer-move-down, ptrmove, (CWM_DOWN)) },
	{ FUNC_SC(pointer-move-left, ptrmove, (CWM_LEFT)) },
	{ FUNC_SC(pointer-move-right, ptrmove, (CWM_RIGHT)) },
	{ FUNC_SC(pointer-move-up-big, ptrmove, (CWM_UP_BIG)) },
	{ FUNC_SC(pointer-move-down-big, ptrmove, (CWM_DOWN_BIG)) },
	{ FUNC_SC(pointer-move-left-big, ptrmove, (CWM_LEFT_BIG)) },
	{ FUNC_SC(pointer-move-right-big, ptrmove, (CWM_RIGHT_BIG)) },

	{ FUNC_SC(menu-cmd, menu_cmd, 0) },
	{ FUNC_SC(menu-group, menu_group, 0) },
	{ FUNC_SC(menu-ssh, menu_ssh, 0) },
	{ FUNC_SC(menu-window, menu_client, CWM_MENU_WINDOW_ALL) },
	{ FUNC_SC(menu-window-hidden, menu_client, CWM_MENU_WINDOW_HIDDEN) },
	{ FUNC_SC(menu-exec, menu_exec, 0) },
	{ FUNC_SC(menu-exec-wm, menu_wm, 0) },

	{ FUNC_SC(terminal, exec_term, 0) },
	{ FUNC_SC(lock, exec_lock, 0) },
	{ FUNC_SC(restart, cwm_status, CWM_EXEC_WM) },
	{ FUNC_SC(quit, cwm_status, CWM_QUIT) },
};
static unsigned int ignore_mods[] = {
	0, LockMask, Mod2Mask, Mod2Mask | LockMask
};
static const struct {
	const char	ch;
	int		mask;
} bind_mods[] = {
	{ 'S',	ShiftMask },
	{ 'C',	ControlMask },
	{ 'M',	Mod1Mask },
	{ '4',	Mod4Mask },
	{ '5',	Mod5Mask },
};
static const struct {
	const char	*key;
	const char	*func;
} key_binds[] = {
	{ "CM-Return",	"terminal" },
	{ "CM-Delete",	"lock" },
	{ "M-question",	"menu-exec" },
	{ "CM-w",	"menu-exec-wm" },
	{ "M-period",	"menu-ssh" },
	{ "M-Return",	"window-hide" },
	{ "M-Down",	"window-lower" },
	{ "M-Up",	"window-raise" },
	{ "M-slash",	"menu-window" },
	{ "C-slash",	"menu-cmd" },
	{ "M-Tab",	"window-cycle" },
	{ "MS-Tab",	"window-rcycle" },
	{ "M-grave",	"window-cycle-inclass" },
	{ "MS-grave",	"window-rcycle-inclass" },
	{ "CM-n",	"window-menu-label" },
	{ "CM-x",	"window-close" },
	{ "CM-a",	"group-toggle-all" },
	{ "CM-0",	"group-toggle-all" },
	{ "CM-1",	"group-toggle-1" },
	{ "CM-2",	"group-toggle-2" },
	{ "CM-3",	"group-toggle-3" },
	{ "CM-4",	"group-toggle-4" },
	{ "CM-5",	"group-toggle-5" },
	{ "CM-6",	"group-toggle-6" },
	{ "CM-7",	"group-toggle-7" },
	{ "CM-8",	"group-toggle-8" },
	{ "CM-9",	"group-toggle-9" },
	{ "M-Right",	"group-cycle" },
	{ "M-Left",	"group-rcycle" },
	{ "CM-g",	"window-group" },
	{ "CM-f",	"window-fullscreen" },
	{ "CM-m",	"window-maximize" },
	{ "CM-s",	"window-stick" },
	{ "CM-equal",	"window-vmaximize" },
	{ "CMS-equal",	"window-hmaximize" },
	{ "CMS-f",	"window-freeze" },
	{ "CMS-r",	"restart" },
	{ "CMS-q",	"quit" },
	{ "M-h",	"window-move-left" },
	{ "M-j",	"window-move-down" },
	{ "M-k",	"window-move-up" },
	{ "M-l",	"window-move-right" },
	{ "MS-h",	"window-move-left-big" },
	{ "MS-j",	"window-move-down-big" },
	{ "MS-k",	"window-move-up-big" },
	{ "MS-l",	"window-move-right-big" },
	{ "CM-h",	"window-resize-left" },
	{ "CM-j",	"window-resize-down" },
	{ "CM-k",	"window-resize-up" },
	{ "CM-l",	"window-resize-right" },
	{ "CMS-h",	"window-resize-left-big" },
	{ "CMS-j",	"window-resize-down-big" },
	{ "CMS-k",	"window-resize-up-big" },
	{ "CMS-l",	"window-resize-right-big" },
},
mouse_binds[] = {
	{ "1",		"menu-window" },
	{ "2",		"menu-group" },
	{ "3",		"menu-cmd" },
	{ "M-1",	"window-move" },
	{ "CM-1",	"window-group" },
	{ "M-2",	"window-resize" },
	{ "M-3",	"window-lower" },
	{ "CMS-3",	"window-hide" },
};

void
conf_init(struct conf *c)
{
	const char	*home;
	struct passwd	*pw;
	unsigned int	i;

	c->stickygroups = 0;
	c->bwidth = 1;
	c->mamount = 1;
	c->htile = 50;
	c->vtile = 50;
	c->snapdist = 0;
	c->ngroups = 0;
	c->nameqlen = 5;

	TAILQ_INIT(&c->ignoreq);
	TAILQ_INIT(&c->autogroupq);
	TAILQ_INIT(&c->keybindq);
	TAILQ_INIT(&c->mousebindq);
	TAILQ_INIT(&c->cmdq);
	TAILQ_INIT(&c->wmq);

	for (i = 0; i < nitems(key_binds); i++)
		conf_bind_key(c, key_binds[i].key, key_binds[i].func);

	for (i = 0; i < nitems(mouse_binds); i++)
		conf_bind_mouse(c, mouse_binds[i].key, mouse_binds[i].func);

	for (i = 0; i < nitems(color_binds); i++)
		c->color[i] = xstrdup(color_binds[i]);

	conf_cmd_add(c, "lock", "xlock");
	conf_cmd_add(c, "term", "xterm");
	conf_wm_add(c, "cwm", "cwm");

	c->font = xstrdup("sans-serif:pixelsize=14:bold");
	c->wmname = xstrdup("CWM");

	home = getenv("HOME");
	if ((home == NULL) || (*home == '\0')) {
		pw = getpwuid(getuid());
		if (pw != NULL && pw->pw_dir != NULL && *pw->pw_dir != '\0')
			home = pw->pw_dir;
		else
			home = "/";
	}
	xasprintf(&c->conf_file, "%s/%s", home, ".cwmrc");
	xasprintf(&c->known_hosts, "%s/%s", home, ".ssh/known_hosts");
}

void
conf_clear(struct conf *c)
{
	struct autogroup	*ag;
	struct bind_ctx		*kb, *mb;
	struct winname		*wn;
	struct cmd_ctx		*cmd, *wm;
	int			 i;

	while ((cmd = TAILQ_FIRST(&c->cmdq)) != NULL) {
		TAILQ_REMOVE(&c->cmdq, cmd, entry);
		free(cmd->name);
		free(cmd->path);
		free(cmd);
	}
	while ((wm = TAILQ_FIRST(&c->wmq)) != NULL) {
		TAILQ_REMOVE(&c->wmq, wm, entry);
		free(wm->name);
		free(wm->path);
		free(wm);
	}
	while ((kb = TAILQ_FIRST(&c->keybindq)) != NULL) {
		TAILQ_REMOVE(&c->keybindq, kb, entry);
		free(kb);
	}
	while ((ag = TAILQ_FIRST(&c->autogroupq)) != NULL) {
		TAILQ_REMOVE(&c->autogroupq, ag, entry);
		free(ag->class);
		free(ag->name);
		free(ag);
	}
	while ((wn = TAILQ_FIRST(&c->ignoreq)) != NULL) {
		TAILQ_REMOVE(&c->ignoreq, wn, entry);
		free(wn->name);
		free(wn);
	}
	while ((mb = TAILQ_FIRST(&c->mousebindq)) != NULL) {
		TAILQ_REMOVE(&c->mousebindq, mb, entry);
		free(mb);
	}
	for (i = 0; i < CWM_COLOR_NITEMS; i++)
		free(c->color[i]);

	free(c->conf_file);
	free(c->known_hosts);
	free(c->font);
	free(c->wmname);
}

void
conf_cmd_add(struct conf *c, const char *name, const char *path)
{
	struct cmd_ctx	*cmd, *cmdtmp = NULL, *cmdnxt;

	cmd = xmalloc(sizeof(*cmd));
	cmd->name = xstrdup(name);
	cmd->path = xstrdup(path);

	TAILQ_FOREACH_SAFE(cmdtmp, &c->cmdq, entry, cmdnxt) {
		if (strcmp(cmdtmp->name, name) == 0) {
			TAILQ_REMOVE(&c->cmdq, cmdtmp, entry);
			free(cmdtmp->name);
			free(cmdtmp->path);
			free(cmdtmp);
		}
	}
	TAILQ_INSERT_TAIL(&c->cmdq, cmd, entry);
}

void
conf_wm_add(struct conf *c, const char *name, const char *path)
{
	struct cmd_ctx	*wm, *wmtmp = NULL, *wmnxt;

	wm = xmalloc(sizeof(*wm));
	wm->name = xstrdup(name);
	wm->path = xstrdup(path);

	TAILQ_FOREACH_SAFE(wmtmp, &c->cmdq, entry, wmnxt) {
		if (strcmp(wmtmp->name, name) == 0) {
			TAILQ_REMOVE(&c->wmq, wmtmp, entry);
			free(wmtmp->name);
			free(wmtmp->path);
			free(wmtmp);
		}
	}
	TAILQ_INSERT_TAIL(&c->wmq, wm, entry);
}

void
conf_autogroup(struct conf *c, int num, const char *name, const char *class)
{
	struct autogroup	*ag;
	char			*p;

	ag = xmalloc(sizeof(*ag));
	if ((p = strchr(class, ',')) == NULL) {
		if (name == NULL)
			ag->name = NULL;
		else
			ag->name = xstrdup(name);

		ag->class = xstrdup(class);
	} else {
		*(p++) = '\0';
		if (name == NULL)
			ag->name = xstrdup(class);
		else
			ag->name = xstrdup(name);

		ag->class = xstrdup(p);
	}
	ag->num = num;
	TAILQ_INSERT_TAIL(&c->autogroupq, ag, entry);
}

void
conf_ignore(struct conf *c, const char *name)
{
	struct winname	*wn;

	wn = xmalloc(sizeof(*wn));
	wn->name = xstrdup(name);
	TAILQ_INSERT_TAIL(&c->ignoreq, wn, entry);
}

void
conf_cursor(struct conf *c)
{
	unsigned int	 i;

	for (i = 0; i < nitems(cursor_binds); i++)
		c->cursor[i] = XCreateFontCursor(X_Dpy, cursor_binds[i]);
}

void
conf_client(struct client_ctx *cc)
{
	struct winname	*wn;

	TAILQ_FOREACH(wn, &Conf.ignoreq, entry) {
		if (strncasecmp(wn->name, cc->name, strlen(wn->name)) == 0) {
			cc->flags |= CLIENT_IGNORE;
			break;
		}
	}
}

void
conf_screen(struct screen_ctx *sc)
{
	unsigned int	 i;
	XftColor	 xc;

	sc->gap = Conf.gap;
	sc->snapdist = Conf.snapdist;

	sc->xftfont = XftFontOpenXlfd(X_Dpy, sc->which, Conf.font);
	if (sc->xftfont == NULL) {
		sc->xftfont = XftFontOpenName(X_Dpy, sc->which, Conf.font);
		if (sc->xftfont == NULL)
			errx(1, "%s: XftFontOpenName: %s", __func__, Conf.font);
	}

	for (i = 0; i < nitems(color_binds); i++) {
		if (i == CWM_COLOR_MENU_FONT_SEL && *Conf.color[i] == '\0') {
			xu_xorcolor(sc->xftcolor[CWM_COLOR_MENU_BG],
			    sc->xftcolor[CWM_COLOR_MENU_FG], &xc);
			xu_xorcolor(sc->xftcolor[CWM_COLOR_MENU_FONT], xc, &xc);
			if (!XftColorAllocValue(X_Dpy, sc->visual, sc->colormap,
			    &xc.color, &sc->xftcolor[CWM_COLOR_MENU_FONT_SEL]))
				warnx("XftColorAllocValue: %s", Conf.color[i]);
			break;
		}
		if (!XftColorAllocName(X_Dpy, sc->visual, sc->colormap,
		    Conf.color[i], &sc->xftcolor[i])) {
			warnx("XftColorAllocName: %s", Conf.color[i]);
			XftColorAllocName(X_Dpy, sc->visual, sc->colormap,
			    color_binds[i], &sc->xftcolor[i]);
		}
	}

	conf_grab_kbd(sc->rootwin);
}

void
conf_group(struct screen_ctx *sc)
{
	unsigned int	 i;

	for (i = 0; i < nitems(group_binds); i++) {
		group_init(sc, group_binds[i].num, group_binds[i].name);
		Conf.ngroups++;
	}
}

static const char *
conf_bind_mask(const char *name, unsigned int *mask)
{
	char		*dash;
	const char	*ch;
	unsigned int 	 i;

	*mask = 0;
	if ((dash = strchr(name, '-')) == NULL)
		return name;
	for (i = 0; i < nitems(bind_mods); i++) {
		if ((ch = strchr(name, bind_mods[i].ch)) != NULL && ch < dash)
			*mask |= bind_mods[i].mask;
	}
	/* Skip past modifiers. */
	return (dash + 1);
}

int
conf_bind_key(struct conf *c, const char *bind, const char *cmd)
{
	struct bind_ctx	*kb;
	struct cargs	*cargs;
	const char	*key;
	unsigned int	 i;

	if ((strcmp(bind, "all") == 0) && (cmd == NULL)) {
		conf_unbind_key(c, NULL);
		return 1;
	}
	kb = xmalloc(sizeof(*kb));
	key = conf_bind_mask(bind, &kb->modmask);
	kb->press.keysym = XStringToKeysym(key);
	if (kb->press.keysym == NoSymbol) {
		warnx("unknown symbol: %s", key);
		free(kb);
		return 0;
	}
	conf_unbind_key(c, kb);
	if (cmd == NULL) {
		free(kb);
		return 1;
	}
	cargs = xcalloc(1, sizeof(*cargs));
	for (i = 0; i < nitems(name_to_func); i++) {
		if (strcmp(name_to_func[i].tag, cmd) != 0)
			continue;
		kb->callback = name_to_func[i].handler;
		kb->context = name_to_func[i].context;
		cargs->flag = name_to_func[i].flag;
		goto out;
	}
	kb->callback = kbfunc_exec_cmd;
	kb->context = CWM_CONTEXT_NONE;
	cargs->flag = 0;
	cargs->cmd = xstrdup(cmd);
out:
	kb->cargs = cargs;
	TAILQ_INSERT_TAIL(&c->keybindq, kb, entry);
	return 1;
}

static void
conf_unbind_key(struct conf *c, struct bind_ctx *unbind)
{
	struct bind_ctx	*key = NULL, *keynxt;

	TAILQ_FOREACH_SAFE(key, &c->keybindq, entry, keynxt) {
		if ((unbind == NULL) ||
		    ((key->modmask == unbind->modmask) &&
		     (key->press.keysym == unbind->press.keysym))) {
			TAILQ_REMOVE(&c->keybindq, key, entry);
			free(key->cargs->cmd);
			free(key->cargs);
			free(key);
		}
	}
}

int
conf_bind_mouse(struct conf *c, const char *bind, const char *cmd)
{
	struct bind_ctx	*mb;
	struct cargs	*cargs;
	const char	*button, *errstr;
	unsigned int	 i;

	if ((strcmp(bind, "all") == 0) && (cmd == NULL)) {
		conf_unbind_mouse(c, NULL);
		return 1;
	}
	mb = xmalloc(sizeof(*mb));
	button = conf_bind_mask(bind, &mb->modmask);
	mb->press.button = strtonum(button, Button1, Button5, &errstr);
	if (errstr) {
		warnx("button number is %s: %s", errstr, button);
		free(mb);
		return 0;
	}
	conf_unbind_mouse(c, mb);
	if (cmd == NULL) {
		free(mb);
		return 1;
	}
	cargs = xcalloc(1, sizeof(*cargs));
	for (i = 0; i < nitems(name_to_func); i++) {
		if (strcmp(name_to_func[i].tag, cmd) != 0)
			continue;
		mb->callback = name_to_func[i].handler;
		mb->context = name_to_func[i].context;
		cargs->flag = name_to_func[i].flag;
		goto out;
	}
	mb->callback = kbfunc_exec_cmd;
	mb->context = CWM_CONTEXT_NONE;
	cargs->flag = 0;
	cargs->cmd = xstrdup(cmd);
out:
	mb->cargs = cargs;
	TAILQ_INSERT_TAIL(&c->mousebindq, mb, entry);
	return 1;
}

static void
conf_unbind_mouse(struct conf *c, struct bind_ctx *unbind)
{
	struct bind_ctx		*mb = NULL, *mbnxt;

	TAILQ_FOREACH_SAFE(mb, &c->mousebindq, entry, mbnxt) {
		if ((unbind == NULL) ||
		    ((mb->modmask == unbind->modmask) &&
		     (mb->press.button == unbind->press.button))) {
			TAILQ_REMOVE(&c->mousebindq, mb, entry);
			free(mb->cargs->cmd);
			free(mb->cargs);
			free(mb);
		}
	}
}

void
conf_grab_kbd(Window win)
{
	struct bind_ctx	*kb;
	KeyCode		 kc;
	unsigned int	 i;

	XUngrabKey(X_Dpy, AnyKey, AnyModifier, win);

	TAILQ_FOREACH(kb, &Conf.keybindq, entry) {
		kc = XKeysymToKeycode(X_Dpy, kb->press.keysym);
		if (kc == 0)
			continue;
		if ((XkbKeycodeToKeysym(X_Dpy, kc, 0, 0) != kb->press.keysym) &&
		    (XkbKeycodeToKeysym(X_Dpy, kc, 0, 1) == kb->press.keysym))
			kb->modmask |= ShiftMask;

		for (i = 0; i < nitems(ignore_mods); i++)
			XGrabKey(X_Dpy, kc, (kb->modmask | ignore_mods[i]), win,
			    True, GrabModeAsync, GrabModeAsync);
	}
}

void
conf_grab_mouse(Window win)
{
	struct bind_ctx	*mb;
	unsigned int	 i;

	XUngrabButton(X_Dpy, AnyButton, AnyModifier, win);

	TAILQ_FOREACH(mb, &Conf.mousebindq, entry) {
		if (mb->context != CWM_CONTEXT_CC)
			continue;
		for (i = 0; i < nitems(ignore_mods); i++) {
			XGrabButton(X_Dpy, mb->press.button,
			    (mb->modmask | ignore_mods[i]), win, False,
			    BUTTONMASK, GrabModeAsync, GrabModeSync,
			    None, None);
		}
	}
}

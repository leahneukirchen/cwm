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
#include <sys/queue.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

static const char	*conf_bind_getmask(const char *, unsigned int *);
static void		 conf_cmd_remove(struct conf *, const char *);
static void		 conf_unbind_key(struct conf *, struct bind_ctx *);
static void		 conf_unbind_key_all(struct conf *);
static void		 conf_unbind_mouse(struct conf *, struct bind_ctx *);
static void		 conf_unbind_mouse_all(struct conf *);

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
	void		 (*handler)(void *, union arg *, enum xev);
	int		 context;
	union arg	 argument;
} name_to_func[] = {
	{ "window-menu-label", kbfunc_menu_client_label, CWM_CONTEXT_CC, {0} },
	{ "window-lower", kbfunc_client_lower, CWM_CONTEXT_CC, {0} },
	{ "window-raise", kbfunc_client_raise, CWM_CONTEXT_CC, {0} },
	{ "window-search", kbfunc_menu_client, CWM_CONTEXT_SC, {0} },
	{ "window-hide", kbfunc_client_hide, CWM_CONTEXT_CC, {0} },
	{ "window-delete", kbfunc_client_delete, CWM_CONTEXT_CC, {0} },
	{ "window-htile", kbfunc_client_htile, CWM_CONTEXT_CC, {0} },
	{ "window-vtile", kbfunc_client_vtile, CWM_CONTEXT_CC, {0} },
	{ "window-stick", kbfunc_client_toggle_sticky, CWM_CONTEXT_CC, {0} },
	{ "window-fullscreen", kbfunc_client_toggle_fullscreen, CWM_CONTEXT_CC, {0} },
	{ "window-maximize", kbfunc_client_toggle_maximize, CWM_CONTEXT_CC, {0} },
	{ "window-vmaximize", kbfunc_client_toggle_vmaximize, CWM_CONTEXT_CC, {0} },
	{ "window-hmaximize", kbfunc_client_toggle_hmaximize, CWM_CONTEXT_CC, {0} },
	{ "window-freeze", kbfunc_client_toggle_freeze, CWM_CONTEXT_CC, {0} },
	{ "window-cycle", kbfunc_client_cycle, CWM_CONTEXT_SC,
	    {.i = (CWM_CYCLE_FORWARD)} },
	{ "window-rcycle", kbfunc_client_cycle, CWM_CONTEXT_SC,
	    {.i = (CWM_CYCLE_REVERSE)} },
	{ "window-cycle-ingroup", kbfunc_client_cycle, CWM_CONTEXT_SC,
	    {.i = (CWM_CYCLE_FORWARD | CWM_CYCLE_INGROUP)} },
	{ "window-rcycle-ingroup", kbfunc_client_cycle, CWM_CONTEXT_SC,
	    {.i = (CWM_CYCLE_REVERSE | CWM_CYCLE_INGROUP)} },
	{ "window-group", kbfunc_client_toggle_group, CWM_CONTEXT_CC, {0} },
	{ "window-movetogroup-1", kbfunc_client_movetogroup, CWM_CONTEXT_CC, {.i = 1} },
	{ "window-movetogroup-2", kbfunc_client_movetogroup, CWM_CONTEXT_CC, {.i = 2} },
	{ "window-movetogroup-3", kbfunc_client_movetogroup, CWM_CONTEXT_CC, {.i = 3} },
	{ "window-movetogroup-4", kbfunc_client_movetogroup, CWM_CONTEXT_CC, {.i = 4} },
	{ "window-movetogroup-5", kbfunc_client_movetogroup, CWM_CONTEXT_CC, {.i = 5} },
	{ "window-movetogroup-6", kbfunc_client_movetogroup, CWM_CONTEXT_CC, {.i = 6} },
	{ "window-movetogroup-7", kbfunc_client_movetogroup, CWM_CONTEXT_CC, {.i = 7} },
	{ "window-movetogroup-8", kbfunc_client_movetogroup, CWM_CONTEXT_CC, {.i = 8} },
	{ "window-movetogroup-9", kbfunc_client_movetogroup, CWM_CONTEXT_CC, {.i = 9} },

	{ "window-move", mousefunc_client_move, CWM_CONTEXT_CC, {0} },
	{ "window-move-up", kbfunc_client_move, CWM_CONTEXT_CC,
	    {.i = (CWM_UP)} },
	{ "window-move-down", kbfunc_client_move, CWM_CONTEXT_CC,
	    {.i = (CWM_DOWN)} },
	{ "window-move-right", kbfunc_client_move, CWM_CONTEXT_CC,
	    {.i = (CWM_RIGHT)} },
	{ "window-move-left", kbfunc_client_move, CWM_CONTEXT_CC,
	    {.i = (CWM_LEFT)} },
	{ "window-move-up-big", kbfunc_client_move, CWM_CONTEXT_CC,
	    {.i = (CWM_UP | CWM_BIGAMOUNT)} },
	{ "window-move-down-big", kbfunc_client_move, CWM_CONTEXT_CC,
	    {.i = (CWM_DOWN | CWM_BIGAMOUNT)} },
	{ "window-move-right-big", kbfunc_client_move, CWM_CONTEXT_CC,
	    {.i = (CWM_RIGHT | CWM_BIGAMOUNT)} },
	{ "window-move-left-big", kbfunc_client_move, CWM_CONTEXT_CC,
	    {.i = (CWM_LEFT | CWM_BIGAMOUNT)} },
	{ "window-resize", mousefunc_client_resize, CWM_CONTEXT_CC, {0} },
	{ "window-resize-up", kbfunc_client_resize, CWM_CONTEXT_CC,
	    {.i = (CWM_UP)} },
	{ "window-resize-down", kbfunc_client_resize, CWM_CONTEXT_CC,
	    {.i = (CWM_DOWN)} },
	{ "window-resize-right", kbfunc_client_resize, CWM_CONTEXT_CC,
	    {.i = (CWM_RIGHT)} },
	{ "window-resize-left", kbfunc_client_resize, CWM_CONTEXT_CC,
	    {.i = (CWM_LEFT)} },
	{ "window-resize-up-big", kbfunc_client_resize, CWM_CONTEXT_CC,
	    {.i = (CWM_UP | CWM_BIGAMOUNT)} },
	{ "window-resize-down-big", kbfunc_client_resize, CWM_CONTEXT_CC,
	    {.i = (CWM_DOWN | CWM_BIGAMOUNT)} },
	{ "window-resize-right-big", kbfunc_client_resize, CWM_CONTEXT_CC,
	    {.i = (CWM_RIGHT | CWM_BIGAMOUNT)} },
	{ "window-resize-left-big", kbfunc_client_resize, CWM_CONTEXT_CC,
	    {.i = (CWM_LEFT | CWM_BIGAMOUNT)} },

	{ "group-cycle", kbfunc_group_cycle, CWM_CONTEXT_SC,
	    {.i = (CWM_CYCLE_FORWARD)} },
	{ "group-rcycle", kbfunc_group_cycle, CWM_CONTEXT_SC,
	    {.i = (CWM_CYCLE_REVERSE)} },
	{ "group-toggle-all", kbfunc_group_alltoggle, CWM_CONTEXT_SC, {0} },
	{ "group-toggle-1", kbfunc_group_toggle, CWM_CONTEXT_SC, {.i = 1} },
	{ "group-toggle-2", kbfunc_group_toggle, CWM_CONTEXT_SC, {.i = 2} },
	{ "group-toggle-3", kbfunc_group_toggle, CWM_CONTEXT_SC, {.i = 3} },
	{ "group-toggle-4", kbfunc_group_toggle, CWM_CONTEXT_SC, {.i = 4} },
	{ "group-toggle-5", kbfunc_group_toggle, CWM_CONTEXT_SC, {.i = 5} },
	{ "group-toggle-6", kbfunc_group_toggle, CWM_CONTEXT_SC, {.i = 6} },
	{ "group-toggle-7", kbfunc_group_toggle, CWM_CONTEXT_SC, {.i = 7} },
	{ "group-toggle-8", kbfunc_group_toggle, CWM_CONTEXT_SC, {.i = 8} },
	{ "group-toggle-9", kbfunc_group_toggle, CWM_CONTEXT_SC, {.i = 9} },
	{ "group-only-1", kbfunc_group_only, CWM_CONTEXT_SC, {.i = 1} },
	{ "group-only-2", kbfunc_group_only, CWM_CONTEXT_SC, {.i = 2} },
	{ "group-only-3", kbfunc_group_only, CWM_CONTEXT_SC, {.i = 3} },
	{ "group-only-4", kbfunc_group_only, CWM_CONTEXT_SC, {.i = 4} },
	{ "group-only-5", kbfunc_group_only, CWM_CONTEXT_SC, {.i = 5} },
	{ "group-only-6", kbfunc_group_only, CWM_CONTEXT_SC, {.i = 6} },
	{ "group-only-7", kbfunc_group_only, CWM_CONTEXT_SC, {.i = 7} },
	{ "group-only-8", kbfunc_group_only, CWM_CONTEXT_SC, {.i = 8} },
	{ "group-only-9", kbfunc_group_only, CWM_CONTEXT_SC, {.i = 9} },

	{ "pointer-move-up", kbfunc_ptrmove, CWM_CONTEXT_SC,
	    {.i = (CWM_UP)} },
	{ "pointer-move-down", kbfunc_ptrmove, CWM_CONTEXT_SC,
	    {.i = (CWM_DOWN)} },
	{ "pointer-move-left", kbfunc_ptrmove, CWM_CONTEXT_SC,
	    {.i = (CWM_LEFT)} },
	{ "pointer-move-right", kbfunc_ptrmove, CWM_CONTEXT_SC,
	    {.i = (CWM_RIGHT)} },
	{ "pointer-move-up-big", kbfunc_ptrmove, CWM_CONTEXT_SC,
	    {.i = (CWM_UP | CWM_BIGAMOUNT)} },
	{ "pointer-move-down-big", kbfunc_ptrmove, CWM_CONTEXT_SC,
	    {.i = (CWM_DOWN | CWM_BIGAMOUNT)} },
	{ "pointer-move-left-big", kbfunc_ptrmove, CWM_CONTEXT_SC,
	    {.i = (CWM_LEFT | CWM_BIGAMOUNT)} },
	{ "pointer-move-right-big", kbfunc_ptrmove, CWM_CONTEXT_SC,
	    {.i = (CWM_RIGHT | CWM_BIGAMOUNT)} },

	{ "menu-cmd", kbfunc_menu_cmd, CWM_CONTEXT_SC, {0} },
	{ "menu-group", kbfunc_menu_group, CWM_CONTEXT_SC, {0} },
	{ "menu-ssh", kbfunc_menu_ssh, CWM_CONTEXT_SC, {0} },
	{ "menu-window", kbfunc_menu_client, CWM_CONTEXT_SC,
	    {.i = CWM_MENU_WINDOW_ALL} },
	{ "menu-window-hidden", kbfunc_menu_client, CWM_CONTEXT_SC,
	    {.i = CWM_MENU_WINDOW_HIDDEN} },
	{ "menu-exec", kbfunc_menu_exec, CWM_CONTEXT_SC,
	    {.i = CWM_MENU_EXEC_EXEC} },
	{ "menu-exec-wm", kbfunc_menu_exec, CWM_CONTEXT_SC,
	    {.i = CWM_MENU_EXEC_WM} },

	{ "terminal", kbfunc_exec_term, CWM_CONTEXT_SC, {0} },
	{ "lock", kbfunc_exec_lock, CWM_CONTEXT_SC, {0} },
	{ "restart", kbfunc_cwm_status, CWM_CONTEXT_SC, {.i = CWM_EXEC_WM} },
	{ "quit", kbfunc_cwm_status, CWM_CONTEXT_SC, {.i = CWM_QUIT} },

};
static unsigned int ignore_mods[] = {
	0, LockMask, Mod2Mask, Mod2Mask | LockMask
};
static const struct {
	const char	ch;
	int		mask;
} bind_mods[] = {
	{ 'C',	ControlMask },
	{ 'M',	Mod1Mask },
	{ '4',	Mod4Mask },
	{ 'S',	ShiftMask },
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
	{ "CM-n",	"window-menu-label" },
	{ "CM-x",	"window-delete" },
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
	unsigned int	i;

	c->stickygroups = 0;
	c->bwidth = 1;
	c->mamount = 1;
	c->snapdist = 0;
	c->ngroups = 10;
	c->nameqlen = 5;

	TAILQ_INIT(&c->ignoreq);
	TAILQ_INIT(&c->cmdq);
	TAILQ_INIT(&c->keybindq);
	TAILQ_INIT(&c->autogroupq);
	TAILQ_INIT(&c->mousebindq);

	for (i = 0; i < nitems(key_binds); i++)
		conf_bind_key(c, key_binds[i].key, key_binds[i].func);

	for (i = 0; i < nitems(mouse_binds); i++)
		conf_bind_mouse(c, mouse_binds[i].key, mouse_binds[i].func);

	for (i = 0; i < nitems(color_binds); i++)
		c->color[i] = xstrdup(color_binds[i]);

	conf_cmd_add(c, "lock", "xlock");
	conf_cmd_add(c, "term", "xterm");

	(void)snprintf(c->known_hosts, sizeof(c->known_hosts), "%s/%s",
	    homedir, ".ssh/known_hosts");

	c->font = xstrdup("sans-serif:pixelsize=14:bold");
	c->wmname = xstrdup("CWM");
}

void
conf_clear(struct conf *c)
{
	struct autogroup	*ag;
	struct bind_ctx		*kb, *mb;
	struct winname		*wn;
	struct cmd_ctx		*cmd;
	int			 i;

	while ((cmd = TAILQ_FIRST(&c->cmdq)) != NULL) {
		TAILQ_REMOVE(&c->cmdq, cmd, entry);
		free(cmd->name);
		free(cmd);
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

	free(c->font);
	free(c->wmname);
}

int
conf_cmd_add(struct conf *c, const char *name, const char *path)
{
	struct cmd_ctx	*cmd;

	cmd = xmalloc(sizeof(*cmd));
	cmd->name = xstrdup(name);
	if (strlcpy(cmd->path, path, sizeof(cmd->path)) >= sizeof(cmd->path)) {
		free(cmd->name);
		free(cmd);
		return(0);
	}
	conf_cmd_remove(c, name);

	TAILQ_INSERT_TAIL(&c->cmdq, cmd, entry);
	return(1);
}

static void
conf_cmd_remove(struct conf *c, const char *name)
{
	struct cmd_ctx	*cmd = NULL, *cmdnxt;

	TAILQ_FOREACH_SAFE(cmd, &c->cmdq, entry, cmdnxt) {
		if (strcmp(cmd->name, name) == 0) {
			TAILQ_REMOVE(&c->cmdq, cmd, entry);
			free(cmd->name);
			free(cmd);
		}
	}
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
	int		 ignore = 0;

	TAILQ_FOREACH(wn, &Conf.ignoreq, entry) {
		if (strncasecmp(wn->name, cc->name, strlen(wn->name)) == 0) {
			ignore = 1;
			break;
		}
	}
	cc->bwidth = (ignore) ? 0 : Conf.bwidth;
	cc->flags |= (ignore) ? CLIENT_IGNORE : 0;
}

void
conf_screen(struct screen_ctx *sc)
{
	unsigned int	 i;
	XftColor	 xc;
	Colormap	 colormap = DefaultColormap(X_Dpy, sc->which);
	Visual		*visual = DefaultVisual(X_Dpy, sc->which);

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
			if (!XftColorAllocValue(X_Dpy, visual, colormap,
			    &xc.color, &sc->xftcolor[CWM_COLOR_MENU_FONT_SEL]))
				warnx("XftColorAllocValue: %s", Conf.color[i]);
			break;
		}
		if (XftColorAllocName(X_Dpy, visual, colormap,
		    Conf.color[i], &xc)) {
			sc->xftcolor[i] = xc;
			XftColorFree(X_Dpy, visual, colormap, &xc);
		} else {
			warnx("XftColorAllocName: %s", Conf.color[i]);
			XftColorAllocName(X_Dpy, visual, colormap,
			    color_binds[i], &sc->xftcolor[i]);
		}
	}

	sc->menu.win = XCreateSimpleWindow(X_Dpy, sc->rootwin, 0, 0, 1, 1,
	    Conf.bwidth,
	    sc->xftcolor[CWM_COLOR_MENU_FG].pixel,
	    sc->xftcolor[CWM_COLOR_MENU_BG].pixel);

	sc->menu.xftdraw = XftDrawCreate(X_Dpy, sc->menu.win, visual, colormap);
	if (sc->menu.xftdraw == NULL)
		errx(1, "%s: XftDrawCreate", __func__);

	conf_grab_kbd(sc->rootwin);
}

static const char *
conf_bind_getmask(const char *name, unsigned int *mask)
{
	char		*dash;
	const char	*ch;
	unsigned int 	 i;

	*mask = 0;
	if ((dash = strchr(name, '-')) == NULL)
		return(name);
	for (i = 0; i < nitems(bind_mods); i++) {
		if ((ch = strchr(name, bind_mods[i].ch)) != NULL && ch < dash)
			*mask |= bind_mods[i].mask;
	}
	/* Skip past modifiers. */
	return(dash + 1);
}

int
conf_bind_key(struct conf *c, const char *bind, const char *cmd)
{
	struct bind_ctx	*kb;
	const char	*key;
	unsigned int	 i;

	if ((strcmp(bind, "all") == 0) && (cmd == NULL)) {
		conf_unbind_key_all(c);
		goto out;
	}
	kb = xmalloc(sizeof(*kb));
	key = conf_bind_getmask(bind, &kb->modmask);
	kb->press.keysym = XStringToKeysym(key);
	if (kb->press.keysym == NoSymbol) {
		warnx("unknown symbol: %s", key);
		free(kb);
		return(0);
	}
	conf_unbind_key(c, kb);
	if (cmd == NULL) {
		free(kb);
		goto out;
	}
	for (i = 0; i < nitems(name_to_func); i++) {
		if (strcmp(name_to_func[i].tag, cmd) != 0)
			continue;
		kb->callback = name_to_func[i].handler;
		kb->context = name_to_func[i].context;
		kb->argument = name_to_func[i].argument;
		TAILQ_INSERT_TAIL(&c->keybindq, kb, entry);
		goto out;
	}
	kb->callback = kbfunc_exec_cmd;
	kb->context = CWM_CONTEXT_NONE;
	kb->argument.c = xstrdup(cmd);
	TAILQ_INSERT_TAIL(&c->keybindq, kb, entry);
out:
	return(1);
}

static void
conf_unbind_key(struct conf *c, struct bind_ctx *unbind)
{
	struct bind_ctx	*key = NULL, *keynxt;

	TAILQ_FOREACH_SAFE(key, &c->keybindq, entry, keynxt) {
		if (key->modmask != unbind->modmask)
			continue;
		if (key->press.keysym == unbind->press.keysym) {
			TAILQ_REMOVE(&c->keybindq, key, entry);
			if (key->context == CWM_CONTEXT_NONE)
				free(key->argument.c);
			free(key);
		}
	}
}

static void
conf_unbind_key_all(struct conf *c)
{
	struct bind_ctx	*key = NULL, *keynxt;

	TAILQ_FOREACH_SAFE(key, &c->keybindq, entry, keynxt) {
		TAILQ_REMOVE(&c->keybindq, key, entry);
		if (key->context == CWM_CONTEXT_NONE)
			free(key->argument.c);
		free(key);
	}
}

int
conf_bind_mouse(struct conf *c, const char *bind, const char *cmd)
{
	struct bind_ctx	*mb;
	const char	*button, *errstr;
	unsigned int	 i;

	if ((strcmp(bind, "all") == 0) && (cmd == NULL)) {
		conf_unbind_mouse_all(c);
		goto out;
	}
	mb = xmalloc(sizeof(*mb));
	button = conf_bind_getmask(bind, &mb->modmask);
	mb->press.button = strtonum(button, Button1, Button5, &errstr);
	if (errstr) {
		warnx("button number is %s: %s", errstr, button);
		free(mb);
		return(0);
	}
	conf_unbind_mouse(c, mb);
	if (cmd == NULL) {
		free(mb);
		goto out;
	}
	for (i = 0; i < nitems(name_to_func); i++) {
		if (strcmp(name_to_func[i].tag, cmd) != 0)
			continue;
		mb->callback = name_to_func[i].handler;
		mb->context = name_to_func[i].context;
		mb->argument = name_to_func[i].argument;
		TAILQ_INSERT_TAIL(&c->mousebindq, mb, entry);
		goto out;
	}
	mb->callback = kbfunc_exec_cmd;
	mb->context = CWM_CONTEXT_NONE;
	mb->argument.c = xstrdup(cmd);
	TAILQ_INSERT_TAIL(&c->mousebindq, mb, entry);
out:
	return(1);
}

static void
conf_unbind_mouse(struct conf *c, struct bind_ctx *unbind)
{
	struct bind_ctx		*mb = NULL, *mbnxt;

	TAILQ_FOREACH_SAFE(mb, &c->mousebindq, entry, mbnxt) {
		if (mb->modmask != unbind->modmask)
			continue;
		if (mb->press.button == unbind->press.button) {
			TAILQ_REMOVE(&c->mousebindq, mb, entry);
			free(mb);
		}
	}
}

static void
conf_unbind_mouse_all(struct conf *c)
{
	struct bind_ctx		*mb = NULL, *mbnxt;

	TAILQ_FOREACH_SAFE(mb, &c->mousebindq, entry, mbnxt) {
		TAILQ_REMOVE(&c->mousebindq, mb, entry);
		if (mb->context == CWM_CONTEXT_NONE)
			free(mb->argument.c);
		free(mb);
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

static char *cwmhints[] = {
	"WM_STATE",
	"WM_DELETE_WINDOW",
	"WM_TAKE_FOCUS",
	"WM_PROTOCOLS",
	"_MOTIF_WM_HINTS",
	"UTF8_STRING",
	"WM_CHANGE_STATE",
};
static char *ewmhints[] = {
	"_NET_SUPPORTED",
	"_NET_SUPPORTING_WM_CHECK",
	"_NET_ACTIVE_WINDOW",
	"_NET_CLIENT_LIST",
	"_NET_CLIENT_LIST_STACKING",
	"_NET_NUMBER_OF_DESKTOPS",
	"_NET_CURRENT_DESKTOP",
	"_NET_DESKTOP_VIEWPORT",
	"_NET_DESKTOP_GEOMETRY",
	"_NET_VIRTUAL_ROOTS",
	"_NET_SHOWING_DESKTOP",
	"_NET_DESKTOP_NAMES",
	"_NET_WORKAREA",
	"_NET_WM_NAME",
	"_NET_WM_DESKTOP",
	"_NET_CLOSE_WINDOW",
	"_NET_WM_STATE",
	"_NET_WM_STATE_STICKY",
	"_NET_WM_STATE_MAXIMIZED_VERT",
	"_NET_WM_STATE_MAXIMIZED_HORZ",
	"_NET_WM_STATE_HIDDEN",
	"_NET_WM_STATE_FULLSCREEN",
	"_NET_WM_STATE_DEMANDS_ATTENTION",
	"_CWM_WM_STATE_FREEZE",
};

void
conf_atoms(void)
{
	XInternAtoms(X_Dpy, cwmhints, nitems(cwmhints), False, cwmh);
	XInternAtoms(X_Dpy, ewmhints, nitems(ewmhints), False, ewmh);
}

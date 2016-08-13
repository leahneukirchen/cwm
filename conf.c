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
static void		 conf_unbind_kbd(struct conf *, struct binding *);
static void		 conf_unbind_mouse(struct conf *, struct binding *);

int
conf_cmd_add(struct conf *c, const char *name, const char *path)
{
	struct cmd	*cmd;

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
	struct cmd	*cmd = NULL, *cmdnxt;

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
	struct autogroupwin	*aw;
	char			*p;

	aw = xmalloc(sizeof(*aw));

	if ((p = strchr(class, ',')) == NULL) {
		if (name == NULL)
			aw->name = NULL;
		else
			aw->name = xstrdup(name);

		aw->class = xstrdup(class);
	} else {
		*(p++) = '\0';

		if (name == NULL)
			aw->name = xstrdup(class);
		else
			aw->name = xstrdup(name);

		aw->class = xstrdup(p);
	}
	aw->num = num;

	TAILQ_INSERT_TAIL(&c->autogroupq, aw, entry);
}

void
conf_ignore(struct conf *c, const char *name)
{
	struct winname	*wn;

	wn = xmalloc(sizeof(*wn));
	wn->name = xstrdup(name);
	TAILQ_INSERT_TAIL(&c->ignoreq, wn, entry);
}

static const char *color_binds[] = {
	"#CCCCCC",	/* CWM_COLOR_BORDER_ACTIVE */
	"#666666",	/* CWM_COLOR_BORDER_INACTIVE */
	"#FC8814",	/* CWM_COLOR_BORDER_URGENCY */
	"blue",		/* CWM_COLOR_BORDER_GROUP */
	"red",		/* CWM_COLOR_BORDER_UNGROUP */
	"black",	/* CWM_COLOR_MENU_FG */
	"white",	/* CWM_COLOR_MENU_BG */
	"black",	/* CWM_COLOR_MENU_FONT */
	"",		/* CWM_COLOR_MENU_FONT_SEL */
};

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
			errx(1, "XftFontOpenName: %s", Conf.font);
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

	sc->menuwin = XCreateSimpleWindow(X_Dpy, sc->rootwin, 0, 0, 1, 1,
	    Conf.bwidth,
	    sc->xftcolor[CWM_COLOR_MENU_FG].pixel,
	    sc->xftcolor[CWM_COLOR_MENU_BG].pixel);

	sc->xftdraw = XftDrawCreate(X_Dpy, sc->menuwin, visual, colormap);
	if (sc->xftdraw == NULL)
		errx(1, "XftDrawCreate");

	conf_grab_kbd(sc->rootwin);
}

static const struct {
	const char	*key;
	const char	*func;
} kbd_binds[] = {
	{ "CM-Return",	"terminal" },
	{ "CM-Delete",	"lock" },
	{ "M-question",	"exec" },
	{ "CM-w",	"exec_wm" },
	{ "M-period",	"ssh" },
	{ "M-Return",	"hide" },
	{ "M-Down",	"lower" },
	{ "M-Up",	"raise" },
	{ "M-slash",	"search" },
	{ "C-slash",	"menusearch" },
	{ "M-Tab",	"cycle" },
	{ "MS-Tab",	"rcycle" },
	{ "CM-n",	"label" },
	{ "CM-x",	"delete" },
	{ "CM-0",	"nogroup" },
	{ "CM-1",	"group1" },
	{ "CM-2",	"group2" },
	{ "CM-3",	"group3" },
	{ "CM-4",	"group4" },
	{ "CM-5",	"group5" },
	{ "CM-6",	"group6" },
	{ "CM-7",	"group7" },
	{ "CM-8",	"group8" },
	{ "CM-9",	"group9" },
	{ "M-Right",	"cyclegroup" },
	{ "M-Left",	"rcyclegroup" },
	{ "CM-g",	"grouptoggle" },
	{ "CM-f",	"fullscreen" },
	{ "CM-m",	"maximize" },
	{ "CM-s",	"sticky" },
	{ "CM-equal",	"vmaximize" },
	{ "CMS-equal",	"hmaximize" },
	{ "CMS-f",	"freeze" },
	{ "CMS-r",	"restart" },
	{ "CMS-q",	"quit" },
	{ "M-h",	"moveleft" },
	{ "M-j",	"movedown" },
	{ "M-k",	"moveup" },
	{ "M-l",	"moveright" },
	{ "MS-h",	"bigmoveleft" },
	{ "MS-j",	"bigmovedown" },
	{ "MS-k",	"bigmoveup" },
	{ "MS-l",	"bigmoveright" },
	{ "CM-h",	"resizeleft" },
	{ "CM-j",	"resizedown" },
	{ "CM-k",	"resizeup" },
	{ "CM-l",	"resizeright" },
	{ "CMS-h",	"bigresizeleft" },
	{ "CMS-j",	"bigresizedown" },
	{ "CMS-k",	"bigresizeup" },
	{ "CMS-l",	"bigresizeright" },
	{ "C-Left",	"ptrmoveleft" },
	{ "C-Down",	"ptrmovedown" },
	{ "C-Up",	"ptrmoveup" },
	{ "C-Right",	"ptrmoveright" },
	{ "CS-Left",	"bigptrmoveleft" },
	{ "CS-Down",	"bigptrmovedown" },
	{ "CS-Up",	"bigptrmoveup" },
	{ "CS-Right",	"bigptrmoveright" },
},
mouse_binds[] = {
	{ "1",		"menu_unhide" },
	{ "2",		"menu_group" },
	{ "3",		"menu_cmd" },
	{ "M-1",	"window_move" },
	{ "CM-1",	"window_grouptoggle" },
	{ "M-2",	"window_resize" },
	{ "M-3",	"window_lower" },
	{ "CMS-3",	"window_hide" },
};

void
conf_init(struct conf *c)
{
	unsigned int	i;

	c->bwidth = CONF_BWIDTH;
	c->mamount = CONF_MAMOUNT;
	c->snapdist = CONF_SNAPDIST;

	TAILQ_INIT(&c->ignoreq);
	TAILQ_INIT(&c->cmdq);
	TAILQ_INIT(&c->keybindingq);
	TAILQ_INIT(&c->autogroupq);
	TAILQ_INIT(&c->mousebindingq);

	for (i = 0; i < nitems(kbd_binds); i++)
		conf_bind_kbd(c, kbd_binds[i].key, kbd_binds[i].func);

	for (i = 0; i < nitems(mouse_binds); i++)
		conf_bind_mouse(c, mouse_binds[i].key, mouse_binds[i].func);

	for (i = 0; i < nitems(color_binds); i++)
		c->color[i] = xstrdup(color_binds[i]);

	conf_cmd_add(c, "lock", "xlock");
	conf_cmd_add(c, "term", "xterm");

	(void)snprintf(c->known_hosts, sizeof(c->known_hosts), "%s/%s",
	    homedir, ".ssh/known_hosts");

	c->font = xstrdup(CONF_FONT);
}

void
conf_clear(struct conf *c)
{
	struct autogroupwin	*aw;
	struct binding		*kb, *mb;
	struct winname		*wn;
	struct cmd		*cmd;
	int			 i;

	while ((cmd = TAILQ_FIRST(&c->cmdq)) != NULL) {
		TAILQ_REMOVE(&c->cmdq, cmd, entry);
		free(cmd->name);
		free(cmd);
	}

	while ((kb = TAILQ_FIRST(&c->keybindingq)) != NULL) {
		TAILQ_REMOVE(&c->keybindingq, kb, entry);
		free(kb);
	}

	while ((aw = TAILQ_FIRST(&c->autogroupq)) != NULL) {
		TAILQ_REMOVE(&c->autogroupq, aw, entry);
		free(aw->class);
		free(aw->name);
		free(aw);
	}

	while ((wn = TAILQ_FIRST(&c->ignoreq)) != NULL) {
		TAILQ_REMOVE(&c->ignoreq, wn, entry);
		free(wn->name);
		free(wn);
	}

	while ((mb = TAILQ_FIRST(&c->mousebindingq)) != NULL) {
		TAILQ_REMOVE(&c->mousebindingq, mb, entry);
		free(mb);
	}

	for (i = 0; i < CWM_COLOR_NITEMS; i++)
		free(c->color[i]);

	free(c->font);
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

static const struct {
	const char	*tag;
	void		 (*handler)(struct client_ctx *, union arg *);
	int		 context;
	union arg	 argument;
} name_to_func[] = {
	{ "lower", kbfunc_client_lower, CWM_CONTEXT_CLIENT, {0} },
	{ "raise", kbfunc_client_raise, CWM_CONTEXT_CLIENT, {0} },
	{ "search", kbfunc_menu_client, CWM_CONTEXT_SCREEN, {0} },
	{ "menusearch", kbfunc_menu_cmd, CWM_CONTEXT_SCREEN, {0} },
	{ "groupsearch", kbfunc_menu_group, CWM_CONTEXT_SCREEN, {0} },
	{ "hide", kbfunc_client_hide, CWM_CONTEXT_CLIENT, {0} },
	{ "cycle", kbfunc_client_cycle, CWM_CONTEXT_SCREEN,
	    {.i = CWM_CLIENT_CYCLE} },
	{ "rcycle", kbfunc_client_cycle, CWM_CONTEXT_SCREEN,
	    {.i = CWM_CLIENT_RCYCLE} },
	{ "label", kbfunc_client_label, CWM_CONTEXT_CLIENT, {0} },
	{ "delete", kbfunc_client_delete, CWM_CONTEXT_CLIENT, {0} },
	{ "group1", kbfunc_group_toggle, CWM_CONTEXT_SCREEN, {.i = 1} },
	{ "group2", kbfunc_group_toggle, CWM_CONTEXT_SCREEN, {.i = 2} },
	{ "group3", kbfunc_group_toggle, CWM_CONTEXT_SCREEN, {.i = 3} },
	{ "group4", kbfunc_group_toggle, CWM_CONTEXT_SCREEN, {.i = 4} },
	{ "group5", kbfunc_group_toggle, CWM_CONTEXT_SCREEN, {.i = 5} },
	{ "group6", kbfunc_group_toggle, CWM_CONTEXT_SCREEN, {.i = 6} },
	{ "group7", kbfunc_group_toggle, CWM_CONTEXT_SCREEN, {.i = 7} },
	{ "group8", kbfunc_group_toggle, CWM_CONTEXT_SCREEN, {.i = 8} },
	{ "group9", kbfunc_group_toggle, CWM_CONTEXT_SCREEN, {.i = 9} },
	{ "grouponly1", kbfunc_group_only, CWM_CONTEXT_SCREEN, {.i = 1} },
	{ "grouponly2", kbfunc_group_only, CWM_CONTEXT_SCREEN, {.i = 2} },
	{ "grouponly3", kbfunc_group_only, CWM_CONTEXT_SCREEN, {.i = 3} },
	{ "grouponly4", kbfunc_group_only, CWM_CONTEXT_SCREEN, {.i = 4} },
	{ "grouponly5", kbfunc_group_only, CWM_CONTEXT_SCREEN, {.i = 5} },
	{ "grouponly6", kbfunc_group_only, CWM_CONTEXT_SCREEN, {.i = 6} },
	{ "grouponly7", kbfunc_group_only, CWM_CONTEXT_SCREEN, {.i = 7} },
	{ "grouponly8", kbfunc_group_only, CWM_CONTEXT_SCREEN, {.i = 8} },
	{ "grouponly9", kbfunc_group_only, CWM_CONTEXT_SCREEN, {.i = 9} },
	{ "movetogroup1", kbfunc_client_movetogroup, CWM_CONTEXT_CLIENT,
	    {.i = 1} },
	{ "movetogroup2", kbfunc_client_movetogroup, CWM_CONTEXT_CLIENT,
	    {.i = 2} },
	{ "movetogroup3", kbfunc_client_movetogroup, CWM_CONTEXT_CLIENT,
	    {.i = 3} },
	{ "movetogroup4", kbfunc_client_movetogroup, CWM_CONTEXT_CLIENT,
	    {.i = 4} },
	{ "movetogroup5", kbfunc_client_movetogroup, CWM_CONTEXT_CLIENT,
	    {.i = 5} },
	{ "movetogroup6", kbfunc_client_movetogroup, CWM_CONTEXT_CLIENT,
	    {.i = 6} },
	{ "movetogroup7", kbfunc_client_movetogroup, CWM_CONTEXT_CLIENT,
	    {.i = 7} },
	{ "movetogroup8", kbfunc_client_movetogroup, CWM_CONTEXT_CLIENT,
	    {.i = 8} },
	{ "movetogroup9", kbfunc_client_movetogroup, CWM_CONTEXT_CLIENT,
	    {.i = 9} },
	{ "nogroup", kbfunc_group_alltoggle, CWM_CONTEXT_SCREEN, {0} },
	{ "cyclegroup", kbfunc_group_cycle, CWM_CONTEXT_SCREEN,
	    {.i = CWM_CLIENT_CYCLE} },
	{ "rcyclegroup", kbfunc_group_cycle, CWM_CONTEXT_SCREEN,
	    {.i = CWM_CLIENT_RCYCLE} },
	{ "cycleingroup", kbfunc_client_cycle, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_CLIENT_CYCLE | CWM_CLIENT_CYCLE_INGRP)} },
	{ "rcycleingroup", kbfunc_client_cycle, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_CLIENT_RCYCLE | CWM_CLIENT_CYCLE_INGRP)} },
	{ "grouptoggle", kbfunc_client_grouptoggle, CWM_CONTEXT_CLIENT,
	    {.i = CWM_KBD}},
	{ "sticky", kbfunc_client_toggle_sticky, CWM_CONTEXT_CLIENT, {0} },
	{ "fullscreen", kbfunc_client_toggle_fullscreen, CWM_CONTEXT_CLIENT,
	    {0} },
	{ "maximize", kbfunc_client_toggle_maximize, CWM_CONTEXT_CLIENT, {0} },
	{ "vmaximize", kbfunc_client_toggle_vmaximize, CWM_CONTEXT_CLIENT,
	    {0} },
	{ "hmaximize", kbfunc_client_toggle_hmaximize, CWM_CONTEXT_CLIENT,
	    {0} },
	{ "freeze", kbfunc_client_toggle_freeze, CWM_CONTEXT_CLIENT, {0} },
	{ "restart", kbfunc_cwm_status, CWM_CONTEXT_SCREEN,
	    {.i = CWM_EXEC_WM} },
	{ "quit", kbfunc_cwm_status, CWM_CONTEXT_SCREEN, {.i = CWM_QUIT} },
	{ "exec", kbfunc_menu_exec, CWM_CONTEXT_SCREEN, {.i = CWM_MENU_EXEC} },
	{ "exec_wm", kbfunc_menu_exec, CWM_CONTEXT_SCREEN,
	    {.i = CWM_MENU_EXEC_WM} },
	{ "ssh", kbfunc_menu_ssh, CWM_CONTEXT_SCREEN, {0} },
	{ "terminal", kbfunc_exec_term, CWM_CONTEXT_SCREEN, {0} },
	{ "lock", kbfunc_exec_lock, CWM_CONTEXT_SCREEN, {0} },
	{ "moveup", kbfunc_client_move, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_UP)} },
	{ "movedown", kbfunc_client_move, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_DOWN)} },
	{ "moveright", kbfunc_client_move, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_RIGHT)} },
	{ "moveleft", kbfunc_client_move, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_LEFT)} },
	{ "bigmoveup", kbfunc_client_move, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_UP | CWM_BIGAMOUNT)} },
	{ "bigmovedown", kbfunc_client_move, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_DOWN | CWM_BIGAMOUNT)} },
	{ "bigmoveright", kbfunc_client_move, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_RIGHT | CWM_BIGAMOUNT)} },
	{ "bigmoveleft", kbfunc_client_move, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_LEFT | CWM_BIGAMOUNT)} },
	{ "resizeup", kbfunc_client_resize, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_UP)} },
	{ "resizedown", kbfunc_client_resize, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_DOWN)} },
	{ "resizeright", kbfunc_client_resize, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_RIGHT)} },
	{ "resizeleft", kbfunc_client_resize, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_LEFT)} },
	{ "bigresizeup", kbfunc_client_resize, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_UP | CWM_BIGAMOUNT)} },
	{ "bigresizedown", kbfunc_client_resize, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_DOWN | CWM_BIGAMOUNT)} },
	{ "bigresizeright", kbfunc_client_resize, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_RIGHT | CWM_BIGAMOUNT)} },
	{ "bigresizeleft", kbfunc_client_resize, CWM_CONTEXT_CLIENT,
	    {.i = (CWM_LEFT | CWM_BIGAMOUNT)} },
	{ "ptrmoveup", kbfunc_ptrmove, CWM_CONTEXT_SCREEN,
	    {.i = (CWM_UP)} },
	{ "ptrmovedown", kbfunc_ptrmove, CWM_CONTEXT_SCREEN,
	    {.i = (CWM_DOWN)} },
	{ "ptrmoveleft", kbfunc_ptrmove, CWM_CONTEXT_SCREEN,
	    {.i = (CWM_LEFT)} },
	{ "ptrmoveright", kbfunc_ptrmove, CWM_CONTEXT_SCREEN,
	    {.i = (CWM_RIGHT)} },
	{ "bigptrmoveup", kbfunc_ptrmove, CWM_CONTEXT_SCREEN,
	    {.i = (CWM_UP | CWM_BIGAMOUNT)} },
	{ "bigptrmovedown", kbfunc_ptrmove, CWM_CONTEXT_SCREEN,
	    {.i = (CWM_DOWN | CWM_BIGAMOUNT)} },
	{ "bigptrmoveleft", kbfunc_ptrmove, CWM_CONTEXT_SCREEN,
	    {.i = (CWM_LEFT | CWM_BIGAMOUNT)} },
	{ "bigptrmoveright", kbfunc_ptrmove, CWM_CONTEXT_SCREEN,
	    {.i = (CWM_RIGHT | CWM_BIGAMOUNT)} },
	{ "htile", kbfunc_client_tile, CWM_CONTEXT_CLIENT,
	    {.i = CWM_CLIENT_TILE_HORIZ} },
	{ "vtile", kbfunc_client_tile, CWM_CONTEXT_CLIENT,
	    {.i = CWM_CLIENT_TILE_VERT} },
	{ "window_lower", kbfunc_client_lower, CWM_CONTEXT_CLIENT, {0} },
	{ "window_raise", kbfunc_client_raise, CWM_CONTEXT_CLIENT, {0} },
	{ "window_hide", kbfunc_client_hide, CWM_CONTEXT_CLIENT, {0} },
	{ "window_move", mousefunc_client_move, CWM_CONTEXT_CLIENT, {0} },
	{ "window_resize", mousefunc_client_resize, CWM_CONTEXT_CLIENT, {0} },
	{ "window_grouptoggle", kbfunc_client_grouptoggle, CWM_CONTEXT_CLIENT,
	   {.i = CWM_MOUSE} },
	{ "menu_group", mousefunc_menu_group, CWM_CONTEXT_SCREEN, {0} },
	{ "menu_unhide", mousefunc_menu_client, CWM_CONTEXT_SCREEN, {0} },
	{ "menu_cmd", mousefunc_menu_cmd, CWM_CONTEXT_SCREEN, {0} },
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
conf_bind_kbd(struct conf *c, const char *bind, const char *cmd)
{
	struct binding	*kb;
	const char	*key;
	unsigned int	 i;

	kb = xmalloc(sizeof(*kb));
	key = conf_bind_getmask(bind, &kb->modmask);

	kb->press.keysym = XStringToKeysym(key);
	if (kb->press.keysym == NoSymbol) {
		warnx("unknown symbol: %s", key);
		free(kb);
		return(0);
	}

	/* We now have the correct binding, remove duplicates. */
	conf_unbind_kbd(c, kb);

	if (strcmp("unmap", cmd) == 0) {
		free(kb);
		return(1);
	}

	for (i = 0; i < nitems(name_to_func); i++) {
		if (strcmp(name_to_func[i].tag, cmd) != 0)
			continue;

		kb->callback = name_to_func[i].handler;
		kb->context = name_to_func[i].context;
		kb->argument = name_to_func[i].argument;
		TAILQ_INSERT_TAIL(&c->keybindingq, kb, entry);
		return(1);
	}

	kb->callback = kbfunc_exec;
	kb->context = CWM_CONTEXT_NONE;
	kb->argument.c = xstrdup(cmd);
	TAILQ_INSERT_TAIL(&c->keybindingq, kb, entry);
	return(1);
}

static void
conf_unbind_kbd(struct conf *c, struct binding *unbind)
{
	struct binding	*key = NULL, *keynxt;

	TAILQ_FOREACH_SAFE(key, &c->keybindingq, entry, keynxt) {
		if (key->modmask != unbind->modmask)
			continue;

		if (key->press.keysym == unbind->press.keysym) {
			TAILQ_REMOVE(&c->keybindingq, key, entry);
			if (key->context == CWM_CONTEXT_NONE)
				free(key->argument.c);
			free(key);
		}
	}
}

int
conf_bind_mouse(struct conf *c, const char *bind, const char *cmd)
{
	struct binding	*mb;
	const char	*button, *errstr;
	unsigned int	 i;

	mb = xmalloc(sizeof(*mb));
	button = conf_bind_getmask(bind, &mb->modmask);

	mb->press.button = strtonum(button, Button1, Button5, &errstr);
	if (errstr) {
		warnx("button number is %s: %s", errstr, button);
		free(mb);
		return(0);
	}

	/* We now have the correct binding, remove duplicates. */
	conf_unbind_mouse(c, mb);

	if (strcmp("unmap", cmd) == 0) {
		free(mb);
		return(1);
	}

	for (i = 0; i < nitems(name_to_func); i++) {
		if (strcmp(name_to_func[i].tag, cmd) != 0)
			continue;

		mb->callback = name_to_func[i].handler;
		mb->context = name_to_func[i].context;
		mb->argument = name_to_func[i].argument;
		TAILQ_INSERT_TAIL(&c->mousebindingq, mb, entry);
		return(1);
	}

	return(0);
}

static void
conf_unbind_mouse(struct conf *c, struct binding *unbind)
{
	struct binding		*mb = NULL, *mbnxt;

	TAILQ_FOREACH_SAFE(mb, &c->mousebindingq, entry, mbnxt) {
		if (mb->modmask != unbind->modmask)
			continue;

		if (mb->press.button == unbind->press.button) {
			TAILQ_REMOVE(&c->mousebindingq, mb, entry);
			free(mb);
		}
	}
}

static int cursor_binds[] = {
	XC_X_cursor,		/* CF_DEFAULT */
	XC_fleur,		/* CF_MOVE */
	XC_left_ptr,		/* CF_NORMAL */
	XC_question_arrow,	/* CF_QUESTION */
	XC_bottom_right_corner,	/* CF_RESIZE */
};

void
conf_cursor(struct conf *c)
{
	unsigned int	 i;

	for (i = 0; i < nitems(cursor_binds); i++)
		c->cursor[i] = XCreateFontCursor(X_Dpy, cursor_binds[i]);
}

void
conf_grab_mouse(Window win)
{
	struct binding	*mb;

	xu_btn_ungrab(win);

	TAILQ_FOREACH(mb, &Conf.mousebindingq, entry) {
		if (mb->context == CWM_CONTEXT_CLIENT)
			xu_btn_grab(win, mb->modmask, mb->press.button);
	}
}

void
conf_grab_kbd(Window win)
{
	struct binding	*kb;

	xu_key_ungrab(win);

	TAILQ_FOREACH(kb, &Conf.keybindingq, entry)
		xu_key_grab(win, kb->modmask, kb->press.keysym);
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

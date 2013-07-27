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

#include <sys/param.h>
#include "queue.h"
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

static const char	*conf_bind_getmask(const char *, u_int *);
static void	 	 conf_unbind_kbd(struct conf *, struct keybinding *);
static void	 	 conf_unbind_mouse(struct conf *, struct mousebinding *);

/* Add an command menu entry to the end of the menu */
void
conf_cmd_add(struct conf *c, char *image, char *label)
{
	/* "term" and "lock" have special meanings. */
	if (strcmp(label, "term") == 0)
		(void)strlcpy(c->termpath, image, sizeof(c->termpath));
	else if (strcmp(label, "lock") == 0)
		(void)strlcpy(c->lockpath, image, sizeof(c->lockpath));
	else {
		struct cmd *cmd = xmalloc(sizeof(*cmd));
		(void)strlcpy(cmd->image, image, sizeof(cmd->image));
		(void)strlcpy(cmd->label, label, sizeof(cmd->label));
		TAILQ_INSERT_TAIL(&c->cmdq, cmd, entry);
	}
}

void
conf_autogroup(struct conf *c, int no, char *val)
{
	struct autogroupwin	*aw;
	char			*p;

	aw = xcalloc(1, sizeof(*aw));

	if ((p = strchr(val, ',')) == NULL) {
		aw->name = NULL;
		aw->class = xstrdup(val);
	} else {
		*(p++) = '\0';
		aw->name = xstrdup(val);
		aw->class = xstrdup(p);
	}
	aw->num = no;

	TAILQ_INSERT_TAIL(&c->autogroupq, aw, entry);
}

void
conf_ignore(struct conf *c, char *val)
{
	struct winmatch	*wm;

	wm = xcalloc(1, sizeof(*wm));

	(void)strlcpy(wm->title, val, sizeof(wm->title));

	TAILQ_INSERT_TAIL(&c->ignoreq, wm, entry);
}

static char *color_binds[] = {
	"#CCCCCC",	/* CWM_COLOR_BORDER_ACTIVE */
	"#666666",	/* CWM_COLOR_BORDER_INACTIVE */
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
	u_int		 i;
	XftColor	 xc;

	sc->gap = Conf.gap;

	sc->xftfont = XftFontOpenName(X_Dpy, sc->which, Conf.font);
	if (sc->xftfont == NULL)
		errx(1, "XftFontOpenName");

	for (i = 0; i < nitems(color_binds); i++) {
		if (i == CWM_COLOR_MENU_FONT_SEL && *Conf.color[i] == '\0') {
			xu_xorcolor(sc->xftcolor[CWM_COLOR_MENU_BG],
			    sc->xftcolor[CWM_COLOR_MENU_FG], &xc);
			xu_xorcolor(sc->xftcolor[CWM_COLOR_MENU_FONT], xc, &xc);
			if (!XftColorAllocValue(X_Dpy, sc->visual, sc->colormap,
			    &xc.color, &sc->xftcolor[CWM_COLOR_MENU_FONT_SEL]))
				warnx("XftColorAllocValue: '%s'", Conf.color[i]);
			break;
		}
		if (XftColorAllocName(X_Dpy, sc->visual, sc->colormap,
		    Conf.color[i], &xc)) {
			sc->xftcolor[i] = xc;
			XftColorFree(X_Dpy, sc->visual, sc->colormap, &xc);
		} else {
			warnx("XftColorAllocName: '%s'", Conf.color[i]);
			XftColorAllocName(X_Dpy, sc->visual, sc->colormap,
			    color_binds[i], &sc->xftcolor[i]);
		}
	}

	sc->menuwin = XCreateSimpleWindow(X_Dpy, sc->rootwin, 0, 0, 1, 1,
	    Conf.bwidth,
	    sc->xftcolor[CWM_COLOR_MENU_FG].pixel,
	    sc->xftcolor[CWM_COLOR_MENU_BG].pixel);

	sc->xftdraw = XftDrawCreate(X_Dpy, sc->menuwin,
	    sc->visual, sc->colormap);
	if (sc->xftdraw == NULL)
		errx(1, "XftDrawCreate");

	conf_grab_kbd(sc->rootwin);
}

static struct {
	char	*key;
	char	*func;
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
	{ "CM-f",	"maximize" },
	{ "CM-equal",	"vmaximize" },
	{ "CMS-equal",	"hmaximize" },
	{ "CMS-f",	"freeze" },
	{ "CMS-r",	"restart" },
	{ "CMS-q",	"quit" },
	{ "M-h",	"moveleft" },
	{ "M-j",	"movedown" },
	{ "M-k",	"moveup" },
	{ "M-l",	"moveright" },
	{ "M-H",	"bigmoveleft" },
	{ "M-J",	"bigmovedown" },
	{ "M-K",	"bigmoveup" },
	{ "M-L",	"bigmoveright" },
	{ "CM-h",	"resizeleft" },
	{ "CM-j",	"resizedown" },
	{ "CM-k",	"resizeup" },
	{ "CM-l",	"resizeright" },
	{ "CM-H",	"bigresizeleft" },
	{ "CM-J",	"bigresizedown" },
	{ "CM-K",	"bigresizeup" },
	{ "CM-L",	"bigresizeright" },
	{ "C-Left",	"ptrmoveleft" },
	{ "C-Down",	"ptrmovedown" },
	{ "C-Up",	"ptrmoveup" },
	{ "C-Right",	"ptrmoveright" },
	{ "CS-Left",	"bigptrmoveleft" },
	{ "CS-Down",	"bigptrmovedown" },
	{ "CS-Up",	"bigptrmoveup" },
	{ "CS-Right",	"bigptrmoveright" },
	{ "4-Page_Up",	"grow" },
	{ "4-Page_Down", "shrink" },
	{ "4-Insert",	"snapleft" },
	{ "4-Home",	"snapup" },
	{ "4-Delete",	"snapdown" },
	{ "4-End",	"snapright" },
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
	u_int	i;

	bzero(c, sizeof(*c));

	c->bwidth = CONF_BWIDTH;
	c->mamount = CONF_MAMOUNT;
	c->snapdist = CONF_SNAPDIST;

	TAILQ_INIT(&c->ignoreq);
	TAILQ_INIT(&c->cmdq);
	TAILQ_INIT(&c->keybindingq);
	TAILQ_INIT(&c->autogroupq);
	TAILQ_INIT(&c->autostartq);
	TAILQ_INIT(&c->mousebindingq);

	for (i = 0; i < nitems(kbd_binds); i++)
		conf_bind_kbd(c, kbd_binds[i].key, kbd_binds[i].func);

	for (i = 0; i < nitems(mouse_binds); i++)
		conf_bind_mouse(c, mouse_binds[i].key, mouse_binds[i].func);

	for (i = 0; i < nitems(color_binds); i++)
		c->color[i] = xstrdup(color_binds[i]);

	/* Default term/lock */
	(void)strlcpy(c->termpath, "xterm", sizeof(c->termpath));
	(void)strlcpy(c->lockpath, "xlock", sizeof(c->lockpath));

	(void)snprintf(c->known_hosts, sizeof(c->known_hosts), "%s/%s",
	    homedir, ".ssh/known_hosts");

	c->font = xstrdup(CONF_FONT);
}

void
conf_clear(struct conf *c)
{
	struct autogroupwin	*ag;
	struct autostartcmd	*as;
	struct keybinding	*kb;
	struct winmatch		*wm;
	struct cmd		*cmd;
	struct mousebinding	*mb;
	int			 i;

	while ((cmd = TAILQ_FIRST(&c->cmdq)) != NULL) {
		TAILQ_REMOVE(&c->cmdq, cmd, entry);
		free(cmd);
	}

	while ((kb = TAILQ_FIRST(&c->keybindingq)) != NULL) {
		TAILQ_REMOVE(&c->keybindingq, kb, entry);
		free(kb);
	}

	while ((ag = TAILQ_FIRST(&c->autogroupq)) != NULL) {
		TAILQ_REMOVE(&c->autogroupq, ag, entry);
		free(ag->class);
		free(ag->name);
		free(ag);
	}

	while ((as = TAILQ_FIRST(&c->autostartq)) != NULL) {
		TAILQ_REMOVE(&c->autostartq, as, entry);
		free(as->cmd);
		free(as);
	}

	while ((wm = TAILQ_FIRST(&c->ignoreq)) != NULL) {
		TAILQ_REMOVE(&c->ignoreq, wm, entry);
		free(wm);
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
	struct winmatch	*wm;
	char		*wname = cc->name;
	int		 ignore = 0;

	TAILQ_FOREACH(wm, &Conf.ignoreq, entry) {
		if (strncasecmp(wm->title, wname, strlen(wm->title)) == 0) {
			ignore = 1;
			break;
		}
	}

	cc->bwidth = ignore ? 0 : Conf.bwidth;
	cc->flags |= ignore ? CLIENT_IGNORE : 0;
}

static struct {
	char		*tag;
	void		 (*handler)(struct client_ctx *, union arg *);
	int		 flags;
	union arg	 argument;
} name_to_kbfunc[] = {
	{ "lower", kbfunc_client_lower, KBFLAG_NEEDCLIENT, {0} },
	{ "raise", kbfunc_client_raise, KBFLAG_NEEDCLIENT, {0} },
	{ "search", kbfunc_client_search, 0, {0} },
	{ "menusearch", kbfunc_menu_search, 0, {0} },
	{ "hide", kbfunc_client_hide, KBFLAG_NEEDCLIENT, {0} },
	{ "cycle", kbfunc_client_cycle, 0, {.i = CWM_CYCLE} },
	{ "rcycle", kbfunc_client_cycle, 0, {.i = CWM_RCYCLE} },
	{ "label", kbfunc_client_label, KBFLAG_NEEDCLIENT, {0} },
	{ "delete", kbfunc_client_delete, KBFLAG_NEEDCLIENT, {0} },
	{ "group1", kbfunc_client_group, 0, {.i = 1} },
	{ "group2", kbfunc_client_group, 0, {.i = 2} },
	{ "group3", kbfunc_client_group, 0, {.i = 3} },
	{ "group4", kbfunc_client_group, 0, {.i = 4} },
	{ "group5", kbfunc_client_group, 0, {.i = 5} },
	{ "group6", kbfunc_client_group, 0, {.i = 6} },
	{ "group7", kbfunc_client_group, 0, {.i = 7} },
	{ "group8", kbfunc_client_group, 0, {.i = 8} },
	{ "group9", kbfunc_client_group, 0, {.i = 9} },
	{ "grouponly1", kbfunc_client_grouponly, 0, {.i = 1} },
	{ "grouponly2", kbfunc_client_grouponly, 0, {.i = 2} },
	{ "grouponly3", kbfunc_client_grouponly, 0, {.i = 3} },
	{ "grouponly4", kbfunc_client_grouponly, 0, {.i = 4} },
	{ "grouponly5", kbfunc_client_grouponly, 0, {.i = 5} },
	{ "grouponly6", kbfunc_client_grouponly, 0, {.i = 6} },
	{ "grouponly7", kbfunc_client_grouponly, 0, {.i = 7} },
	{ "grouponly8", kbfunc_client_grouponly, 0, {.i = 8} },
	{ "grouponly9", kbfunc_client_grouponly, 0, {.i = 9} },
	{ "movetogroup1", kbfunc_client_movetogroup, KBFLAG_NEEDCLIENT,
	    {.i = 1} },
	{ "movetogroup2", kbfunc_client_movetogroup, KBFLAG_NEEDCLIENT,
	    {.i = 2} },
	{ "movetogroup3", kbfunc_client_movetogroup, KBFLAG_NEEDCLIENT,
	    {.i = 3} },
	{ "movetogroup4", kbfunc_client_movetogroup, KBFLAG_NEEDCLIENT,
	    {.i = 4} },
	{ "movetogroup5", kbfunc_client_movetogroup, KBFLAG_NEEDCLIENT,
	    {.i = 5} },
	{ "movetogroup6", kbfunc_client_movetogroup, KBFLAG_NEEDCLIENT,
	    {.i = 6} },
	{ "movetogroup7", kbfunc_client_movetogroup, KBFLAG_NEEDCLIENT,
	    {.i = 7} },
	{ "movetogroup8", kbfunc_client_movetogroup, KBFLAG_NEEDCLIENT,
	    {.i = 8} },
	{ "movetogroup9", kbfunc_client_movetogroup, KBFLAG_NEEDCLIENT,
	    {.i = 9} },
	{ "nogroup", kbfunc_client_nogroup, 0, {0} },
	{ "cyclegroup", kbfunc_client_cyclegroup, 0, {.i = CWM_CYCLE} },
	{ "rcyclegroup", kbfunc_client_cyclegroup, 0, {.i = CWM_RCYCLE} },
	{ "cycleingroup", kbfunc_client_cycle, KBFLAG_NEEDCLIENT,
	    {.i = CWM_CYCLE|CWM_INGROUP} },
	{ "rcycleingroup", kbfunc_client_cycle, KBFLAG_NEEDCLIENT,
	    {.i = CWM_RCYCLE|CWM_INGROUP} },
	{ "grouptoggle", kbfunc_client_grouptoggle, KBFLAG_NEEDCLIENT, {0}},
	{ "maximize", kbfunc_client_maximize, KBFLAG_NEEDCLIENT, {0} },
	{ "vmaximize", kbfunc_client_vmaximize, KBFLAG_NEEDCLIENT, {0} },
	{ "hmaximize", kbfunc_client_hmaximize, KBFLAG_NEEDCLIENT, {0} },
	{ "freeze", kbfunc_client_freeze, KBFLAG_NEEDCLIENT, {0} },
	{ "restart", kbfunc_restart, 0, {0} },
	{ "quit", kbfunc_quit_wm, 0, {0} },
	{ "exec", kbfunc_exec, 0, {.i = CWM_EXEC_PROGRAM} },
	{ "exec_wm", kbfunc_exec, 0, {.i = CWM_EXEC_WM} },
	{ "ssh", kbfunc_ssh, 0, {0} },
	{ "terminal", kbfunc_term, 0, {0} },
	{ "lock", kbfunc_lock, 0, {0} },
	{ "moveup", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_UP|CWM_MOVE)} },
	{ "movedown", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_DOWN|CWM_MOVE)} },
	{ "moveright", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_RIGHT|CWM_MOVE)} },
	{ "moveleft", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_LEFT|CWM_MOVE)} },
	{ "bigmoveup", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_UP|CWM_MOVE|CWM_BIGMOVE)} },
	{ "bigmovedown", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_DOWN|CWM_MOVE|CWM_BIGMOVE)} },
	{ "bigmoveright", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_RIGHT|CWM_MOVE|CWM_BIGMOVE)} },
	{ "bigmoveleft", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_LEFT|CWM_MOVE|CWM_BIGMOVE)} },
	{ "resizeup", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_UP|CWM_RESIZE)} },
	{ "resizedown", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_DOWN|CWM_RESIZE)} },
	{ "resizeright", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_RIGHT|CWM_RESIZE)} },
	{ "resizeleft", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_LEFT|CWM_RESIZE)} },
	{ "bigresizeup", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_UP|CWM_RESIZE|CWM_BIGMOVE)} },
	{ "bigresizedown", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_DOWN|CWM_RESIZE|CWM_BIGMOVE)} },
	{ "bigresizeright", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_RIGHT|CWM_RESIZE|CWM_BIGMOVE)} },
	{ "bigresizeleft", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_LEFT|CWM_RESIZE|CWM_BIGMOVE)} },
	{ "ptrmoveup", kbfunc_client_moveresize, 0,
	    {.i = (CWM_UP|CWM_PTRMOVE)} },
	{ "ptrmovedown", kbfunc_client_moveresize, 0,
	    {.i = (CWM_DOWN|CWM_PTRMOVE)} },
	{ "ptrmoveleft", kbfunc_client_moveresize, 0,
	    {.i = (CWM_LEFT|CWM_PTRMOVE)} },
	{ "ptrmoveright", kbfunc_client_moveresize, 0,
	    {.i = (CWM_RIGHT|CWM_PTRMOVE)} },
	{ "bigptrmoveup", kbfunc_client_moveresize, 0,
	    {.i = (CWM_UP|CWM_PTRMOVE|CWM_BIGMOVE)} },
	{ "bigptrmovedown", kbfunc_client_moveresize, 0,
	    {.i = (CWM_DOWN|CWM_PTRMOVE|CWM_BIGMOVE)} },
	{ "bigptrmoveleft", kbfunc_client_moveresize, 0,
	    {.i = (CWM_LEFT|CWM_PTRMOVE|CWM_BIGMOVE)} },
	{ "bigptrmoveright", kbfunc_client_moveresize, 0,
	    {.i = (CWM_RIGHT|CWM_PTRMOVE|CWM_BIGMOVE)} },
	{ "htile", kbfunc_tile, KBFLAG_NEEDCLIENT,
	    {.i = CWM_TILE_HORIZ } },
	{ "vtile", kbfunc_tile, KBFLAG_NEEDCLIENT,
	    {.i = CWM_TILE_VERT } },
	{ "grow", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_GROW|CWM_SNAP)} },
	{ "shrink", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_SHRINK|CWM_SNAP)} },
	{ "snapup", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_UP|CWM_SNAP)} },
	{ "snapdown", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_DOWN|CWM_SNAP)} },
	{ "snapleft", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_LEFT|CWM_SNAP)} },
	{ "snapright", kbfunc_client_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_RIGHT|CWM_SNAP)} },
};

static struct {
	char	ch;
	int	mask;
} bind_mods[] = {
	{ 'C',	ControlMask },
	{ 'M',	Mod1Mask },
	{ '4',	Mod4Mask },
	{ 'S',	ShiftMask },
};

static const char *
conf_bind_getmask(const char *name, u_int *mask)
{
	char		*dash;
	const char	*ch;
	u_int	 	 i;

	*mask = 0;
	if ((dash = strchr(name, '-')) == NULL)
		return (name);
	for (i = 0; i < nitems(bind_mods); i++) {
		if ((ch = strchr(name, bind_mods[i].ch)) != NULL && ch < dash)
			*mask |= bind_mods[i].mask;
	}

	/* Skip past modifiers. */
	return (dash + 1);
}

void
conf_bind_kbd(struct conf *c, char *name, char *binding)
{
	struct keybinding	*current_binding;
	const char		*substring;
	u_int			 i, mask;

	current_binding = xcalloc(1, sizeof(*current_binding));
	substring = conf_bind_getmask(name, &mask);
	current_binding->modmask |= mask;

	if (substring[0] == '[' &&
	    substring[strlen(substring)-1] == ']') {
		sscanf(substring, "[%d]", &current_binding->keycode);
		current_binding->keysym = NoSymbol;
	} else {
		current_binding->keycode = 0;
		current_binding->keysym = XStringToKeysym(substring);
	}

	if (current_binding->keysym == NoSymbol &&
	    current_binding->keycode == 0) {
		free(current_binding);
		return;
	}

	/* We now have the correct binding, remove duplicates. */
	conf_unbind_kbd(c, current_binding);

	if (strcmp("unmap", binding) == 0) {
		free(current_binding);
		return;
	}

	for (i = 0; i < nitems(name_to_kbfunc); i++) {
		if (strcmp(name_to_kbfunc[i].tag, binding) != 0)
			continue;

		current_binding->callback = name_to_kbfunc[i].handler;
		current_binding->flags = name_to_kbfunc[i].flags;
		current_binding->argument = name_to_kbfunc[i].argument;
		current_binding->argtype |= ARG_INT;
		TAILQ_INSERT_TAIL(&c->keybindingq, current_binding, entry);
		return;
	}

	current_binding->callback = kbfunc_cmdexec;
	current_binding->flags = 0;
	current_binding->argument.c = xstrdup(binding);
	current_binding->argtype |= ARG_CHAR;
	TAILQ_INSERT_TAIL(&c->keybindingq, current_binding, entry);
}

static void
conf_unbind_kbd(struct conf *c, struct keybinding *unbind)
{
	struct keybinding	*key = NULL, *keynxt;

	TAILQ_FOREACH_SAFE(key, &c->keybindingq, entry, keynxt) {
		if (key->modmask != unbind->modmask)
			continue;

		if ((key->keycode != 0 && key->keysym == NoSymbol &&
		    key->keycode == unbind->keycode) ||
		    key->keysym == unbind->keysym) {
			TAILQ_REMOVE(&c->keybindingq, key, entry);
			if (key->argtype & ARG_CHAR)
				free(key->argument.c);
			free(key);
		}
	}
}

static struct {
	char *tag;
	void (*handler)(struct client_ctx *, void *);
	int flags;
} name_to_mousefunc[] = {
	{ "window_move", mousefunc_client_move, MOUSEBIND_CTX_WIN },
	{ "window_resize", mousefunc_client_resize, MOUSEBIND_CTX_WIN },
	{ "window_grouptoggle", mousefunc_client_grouptoggle,
	    MOUSEBIND_CTX_WIN },
	{ "window_lower", mousefunc_client_lower, MOUSEBIND_CTX_WIN },
	{ "window_raise", mousefunc_client_raise, MOUSEBIND_CTX_WIN },
	{ "window_hide", mousefunc_client_hide, MOUSEBIND_CTX_WIN },
	{ "cyclegroup", mousefunc_client_cyclegroup, MOUSEBIND_CTX_ROOT },
	{ "rcyclegroup", mousefunc_client_rcyclegroup, MOUSEBIND_CTX_ROOT },
	{ "menu_group", mousefunc_menu_group, MOUSEBIND_CTX_ROOT },
	{ "menu_unhide", mousefunc_menu_unhide, MOUSEBIND_CTX_ROOT },
	{ "menu_cmd", mousefunc_menu_cmd, MOUSEBIND_CTX_ROOT },
};

static unsigned int mouse_btns[] = {
	Button1, Button2, Button3, Button4, Button5
};

int
conf_bind_mouse(struct conf *c, char *name, char *binding)
{
	struct mousebinding	*current_binding;
	const char		*errstr, *substring;
	u_int			 button, i, mask;

	current_binding = xcalloc(1, sizeof(*current_binding));
	substring = conf_bind_getmask(name, &mask);
	current_binding->modmask |= mask;

	button = strtonum(substring, 1, 5, &errstr);
	if (errstr)
		warnx("button number is %s: %s", errstr, substring);

	for (i = 0; i < nitems(mouse_btns); i++) {
		if (button == mouse_btns[i]) {
			current_binding->button = button;
			break;
		}
	}
	if (!current_binding->button || errstr) {
		free(current_binding);
		return (0);
	}

	/* We now have the correct binding, remove duplicates. */
	conf_unbind_mouse(c, current_binding);

	if (strcmp("unmap", binding) == 0) {
		free(current_binding);
		return (1);
	}

	for (i = 0; i < nitems(name_to_mousefunc); i++) {
		if (strcmp(name_to_mousefunc[i].tag, binding) != 0)
			continue;

		current_binding->callback = name_to_mousefunc[i].handler;
		current_binding->flags = name_to_mousefunc[i].flags;
		TAILQ_INSERT_TAIL(&c->mousebindingq, current_binding, entry);
		return (1);
	}

	return (0);
}

static void
conf_unbind_mouse(struct conf *c, struct mousebinding *unbind)
{
	struct mousebinding	*mb = NULL, *mbnxt;

	TAILQ_FOREACH_SAFE(mb, &c->mousebindingq, entry, mbnxt) {
		if (mb->modmask != unbind->modmask)
			continue;

		if (mb->button == unbind->button) {
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
	u_int	 i;

	for (i = 0; i < nitems(cursor_binds); i++)
		c->cursor[i] = XCreateFontCursor(X_Dpy, cursor_binds[i]);
}

void
conf_grab_mouse(Window win)
{
	struct mousebinding	*mb;

	TAILQ_FOREACH(mb, &Conf.mousebindingq, entry) {
		if (mb->flags != MOUSEBIND_CTX_WIN)
			continue;
		xu_btn_grab(win, mb->modmask, mb->button);
	}
}

void
conf_grab_kbd(Window win)
{
	struct keybinding	*kb;

	XUngrabKey(X_Dpy, AnyKey, AnyModifier, win);

	TAILQ_FOREACH(kb, &Conf.keybindingq, entry)
		xu_key_grab(win, kb->modmask, kb->keysym);
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
	"_NET_WM_STATE_MAXIMIZED_VERT",
	"_NET_WM_STATE_MAXIMIZED_HORZ",
};

void
conf_atoms(void)
{
	XInternAtoms(X_Dpy, cwmhints, nitems(cwmhints), False, cwmh);
	XInternAtoms(X_Dpy, ewmhints, nitems(ewmhints), False, ewmh);
}

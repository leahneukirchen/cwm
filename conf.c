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
#include <sys/queue.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "calmwm.h"

static void	 conf_mouseunbind(struct conf *, struct mousebinding *);
static void	 conf_unbind(struct conf *, struct keybinding *);

/* Add an command menu entry to the end of the menu */
void
conf_cmd_add(struct conf *c, char *image, char *label, int flags)
{
	/* "term" and "lock" have special meanings. */

	if (strcmp(label, "term") == 0)
		(void)strlcpy(c->termpath, image, sizeof(c->termpath));
	else if (strcmp(label, "lock") == 0)
		(void)strlcpy(c->lockpath, image, sizeof(c->lockpath));
	else {
		struct cmd *cmd = xmalloc(sizeof(*cmd));
		cmd->flags = flags;
		(void)strlcpy(cmd->image, image, sizeof(cmd->image));
		(void)strlcpy(cmd->label, label, sizeof(cmd->label));
		TAILQ_INSERT_TAIL(&c->cmdq, cmd, entry);
	}
}

void
conf_gap(struct conf *c, struct screen_ctx *sc)
{
	sc->gap = c->gap;
}

void
conf_font(struct conf *c, struct screen_ctx *sc)
{
	font_init(sc, c->color[CWM_COLOR_FONT].name);
	sc->font = font_make(sc, c->font);
}

static struct color color_binds[] = {
	{ "#CCCCCC",	0 }, /* CWM_COLOR_BORDER_ACTIVE */
	{ "#666666",	0 }, /* CWM_COLOR_BORDER_INACTIVE */
	{ "blue",	0 }, /* CWM_COLOR_BORDER_GROUP */
	{ "red",	0 }, /* CWM_COLOR_BORDER_UNGROUP */
	{ "black",	0 }, /* CWM_COLOR_FG_MENU */
	{ "white",	0 }, /* CWM_COLOR_BG_MENU */
	{ "black",	0 }, /* CWM_COLOR_FONT */
};

void
conf_color(struct conf *c, struct screen_ctx *sc)
{
	int	 i;

	for (i = 0; i < CWM_COLOR_MAX; i++) {
		xu_freecolor(sc, sc->color[i].pixel);
		sc->color[i].pixel = xu_getcolor(sc, c->color[i].name);
	}
}

void
conf_reload(struct conf *c)
{
	struct screen_ctx	*sc;
	struct client_ctx	*cc;

	if (parse_config(c->conf_path, c) == -1) {
		warnx("config file %s has errors, not reloading", c->conf_path);
		return;
	}

	TAILQ_FOREACH(sc, &Screenq, entry) {
		conf_gap(c, sc);
		conf_color(c, sc);
		conf_font(c, sc);
		menu_init(sc);
	}
	TAILQ_FOREACH(cc, &Clientq, entry)
		client_draw_border(cc);
}

static struct {
	char	*key;
	char	*func;
} kb_binds[] = {
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
	{ "CMS-r",	"reload" },
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
m_binds[] = {
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
	int	i;

	c->flags = 0;
	c->bwidth = CONF_BWIDTH;
	c->mamount = CONF_MAMOUNT;
	c->snapdist = CONF_SNAPDIST;

	TAILQ_INIT(&c->ignoreq);
	TAILQ_INIT(&c->cmdq);
	TAILQ_INIT(&c->keybindingq);
	TAILQ_INIT(&c->autogroupq);
	TAILQ_INIT(&c->mousebindingq);

	for (i = 0; i < nitems(kb_binds); i++)
		conf_bindname(c, kb_binds[i].key, kb_binds[i].func);

	for (i = 0; i < nitems(m_binds); i++)
		conf_mousebind(c, m_binds[i].key, m_binds[i].func);

	for (i = 0; i < nitems(color_binds); i++)
		c->color[i].name = xstrdup(color_binds[i].name);

	/* Default term/lock */
	(void)strlcpy(c->termpath, "xterm", sizeof(c->termpath));
	(void)strlcpy(c->lockpath, "xlock", sizeof(c->lockpath));

	c->font = xstrdup(CONF_FONT);
}

void
conf_clear(struct conf *c)
{
	struct autogroupwin	*ag;
	struct keybinding	*kb;
	struct winmatch		*wm;
	struct cmd		*cmd;
	struct mousebinding	*mb;
	int			 i;

	while ((cmd = TAILQ_FIRST(&c->cmdq)) != NULL) {
		TAILQ_REMOVE(&c->cmdq, cmd, entry);
		xfree(cmd);
	}

	while ((kb = TAILQ_FIRST(&c->keybindingq)) != NULL) {
		TAILQ_REMOVE(&c->keybindingq, kb, entry);
		xfree(kb);
	}

	while ((ag = TAILQ_FIRST(&c->autogroupq)) != NULL) {
		TAILQ_REMOVE(&c->autogroupq, ag, entry);
		xfree(ag->class);
		if (ag->name)
			xfree(ag->name);
		xfree(ag);
	}

	while ((wm = TAILQ_FIRST(&c->ignoreq)) != NULL) {
		TAILQ_REMOVE(&c->ignoreq, wm, entry);
		xfree(wm);
	}

	while ((mb = TAILQ_FIRST(&c->mousebindingq)) != NULL) {
		TAILQ_REMOVE(&c->mousebindingq, mb, entry);
		xfree(mb);
	}

	for (i = 0; i < CWM_COLOR_MAX; i++)
		xfree(c->color[i].name);

	xfree(c->font);
}

void
conf_setup(struct conf *c, const char *conf_file)
{
	char		*home;
	struct stat	 sb;
	int		 parse = 0;

	conf_init(c);

	if (conf_file == NULL) {
		if ((home = getenv("HOME")) == NULL)
			errx(1, "No HOME directory.");

		(void)snprintf(c->conf_path, sizeof(c->conf_path), "%s/%s",
		    home, CONFFILE);

		if (stat(c->conf_path, &sb) == 0 && (sb.st_mode & S_IFREG))
			parse = 1;
	} else {
		if (stat(conf_file, &sb) == -1 || !(sb.st_mode & S_IFREG))
			errx(1, "%s: %s", conf_file, strerror(errno));
		else {
			(void)strlcpy(c->conf_path, conf_file,
			    sizeof(c->conf_path));
			parse = 1;
		}
	}

	if (parse && (parse_config(c->conf_path, c) == -1))
		warnx("config file %s has errors, not loading", c->conf_path);
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
	{ "reload", kbfunc_reload, 0, {0} },
	{ "quit", kbfunc_quit_wm, 0, {0} },
	{ "exec", kbfunc_exec, 0, {.i = CWM_EXEC_PROGRAM} },
	{ "exec_wm", kbfunc_exec, 0, {.i = CWM_EXEC_WM} },
	{ "ssh", kbfunc_ssh, 0, {0} },
	{ "terminal", kbfunc_term, 0, {0} },
	{ "lock", kbfunc_lock, 0, {0} },
	{ "moveup", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_UP|CWM_MOVE)} },
	{ "movedown", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_DOWN|CWM_MOVE)} },
	{ "moveright", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_RIGHT|CWM_MOVE)} },
	{ "moveleft", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_LEFT|CWM_MOVE)} },
	{ "bigmoveup", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_UP|CWM_MOVE|CWM_BIGMOVE)} },
	{ "bigmovedown", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_DOWN|CWM_MOVE|CWM_BIGMOVE)} },
	{ "bigmoveright", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_RIGHT|CWM_MOVE|CWM_BIGMOVE)} },
	{ "bigmoveleft", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_LEFT|CWM_MOVE|CWM_BIGMOVE)} },
	{ "resizeup", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_UP|CWM_RESIZE)} },
	{ "resizedown", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_DOWN|CWM_RESIZE)} },
	{ "resizeright", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_RIGHT|CWM_RESIZE)} },
	{ "resizeleft", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_LEFT|CWM_RESIZE)} },
	{ "bigresizeup", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_UP|CWM_RESIZE|CWM_BIGMOVE)} },
	{ "bigresizedown", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_DOWN|CWM_RESIZE|CWM_BIGMOVE)} },
	{ "bigresizeright", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_RIGHT|CWM_RESIZE|CWM_BIGMOVE)} },
	{ "bigresizeleft", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_LEFT|CWM_RESIZE|CWM_BIGMOVE)} },
	{ "ptrmoveup", kbfunc_moveresize, 0, {.i = (CWM_UP|CWM_PTRMOVE)} },
	{ "ptrmovedown", kbfunc_moveresize, 0, {.i = (CWM_DOWN|CWM_PTRMOVE)} },
	{ "ptrmoveleft", kbfunc_moveresize, 0, {.i = (CWM_LEFT|CWM_PTRMOVE)} },
	{ "ptrmoveright", kbfunc_moveresize, 0,
	    {.i = (CWM_RIGHT|CWM_PTRMOVE)} },
	{ "bigptrmoveup", kbfunc_moveresize, 0,
	    {.i = (CWM_UP|CWM_PTRMOVE|CWM_BIGMOVE)} },
	{ "bigptrmovedown", kbfunc_moveresize, 0,
	    {.i = (CWM_DOWN|CWM_PTRMOVE|CWM_BIGMOVE)} },
	{ "bigptrmoveleft", kbfunc_moveresize, 0,
	    {.i = (CWM_LEFT|CWM_PTRMOVE|CWM_BIGMOVE)} },
	{ "bigptrmoveright", kbfunc_moveresize, 0,
	    {.i = (CWM_RIGHT|CWM_PTRMOVE|CWM_BIGMOVE)} },
	{ "grow", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_GROW|CWM_SNAP)} },
	{ "shrink", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_SHRINK|CWM_SNAP)} },
	{ "snapup", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_UP|CWM_SNAP)} },
	{ "snapdown", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_DOWN|CWM_SNAP)} },
	{ "snapleft", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_LEFT|CWM_SNAP)} },
	{ "snapright", kbfunc_moveresize, KBFLAG_NEEDCLIENT,
	    {.i = (CWM_RIGHT|CWM_SNAP)} },
};

/*
 * The following two functions are used when grabbing and ungrabbing keys for
 * bindings
 */

/*
 * Grab key combination on all screens and add to the global queue
 */
void
conf_grab(struct conf *c, struct keybinding *kb)
{
	extern struct screen_ctx_q	 Screenq;
	struct screen_ctx		*sc;

	TAILQ_FOREACH(sc, &Screenq, entry)
		xu_key_grab(sc->rootwin, kb->modmask, kb->keysym);
}

/*
 * Ungrab key combination from all screens and remove from global queue
 */
void
conf_ungrab(struct conf *c, struct keybinding *kb)
{
	extern struct screen_ctx_q	 Screenq;
	struct screen_ctx		*sc;

	TAILQ_FOREACH(sc, &Screenq, entry)
		xu_key_ungrab(sc->rootwin, kb->modmask, kb->keysym);
}

static struct {
	char	chr;
	int	mask;
} bind_mods[] = {
	{ 'C',	ControlMask },
	{ 'M',	Mod1Mask },
	{ '4',	Mod4Mask },
	{ 'S',	ShiftMask },
};

void
conf_bindname(struct conf *c, char *name, char *binding)
{
	struct keybinding	*current_binding;
	char			*substring, *tmp;
	int			 iter;

	current_binding = xcalloc(1, sizeof(*current_binding));

	if ((substring = strchr(name, '-')) != NULL) {
		for (iter = 0; iter < nitems(bind_mods); iter++) {
			if ((tmp = strchr(name, bind_mods[iter].chr)) !=
			    NULL && tmp < substring) {
				current_binding->modmask |=
				    bind_mods[iter].mask;
			}
		}

		/* skip past the modifiers */
		substring++;
	} else {
		substring = name;
	}

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
		xfree(current_binding);
		return;
	}

	/* We now have the correct binding, remove duplicates. */
	conf_unbind(c, current_binding);

	if (strcmp("unmap", binding) == 0)
		return;

	for (iter = 0; iter < nitems(name_to_kbfunc); iter++) {
		if (strcmp(name_to_kbfunc[iter].tag, binding) != 0)
			continue;

		current_binding->callback = name_to_kbfunc[iter].handler;
		current_binding->flags = name_to_kbfunc[iter].flags;
		current_binding->argument = name_to_kbfunc[iter].argument;
		conf_grab(c, current_binding);
		TAILQ_INSERT_TAIL(&c->keybindingq, current_binding, entry);
		return;
	}

	current_binding->callback = kbfunc_cmdexec;
	current_binding->argument.c = xstrdup(binding);
	current_binding->flags = 0;
	conf_grab(c, current_binding);
	TAILQ_INSERT_TAIL(&c->keybindingq, current_binding, entry);
}

static void
conf_unbind(struct conf *c, struct keybinding *unbind)
{
	struct keybinding	*key = NULL, *keynxt;

	for (key = TAILQ_FIRST(&c->keybindingq);
	    key != TAILQ_END(&c->keybindingq); key = keynxt) {
		keynxt = TAILQ_NEXT(key, entry);

		if (key->modmask != unbind->modmask)
			continue;

		if ((key->keycode != 0 && key->keysym == NoSymbol &&
		    key->keycode == unbind->keycode) ||
		    key->keysym == unbind->keysym) {
			conf_ungrab(c, key);
			TAILQ_REMOVE(&c->keybindingq, key, entry);
			xfree(key);
		}
	}
}

static struct {
	char *tag;
	void (*handler)(struct client_ctx *, void *);
	int context;
} name_to_mousefunc[] = {
	{ "window_move", mousefunc_window_move, MOUSEBIND_CTX_WIN },
	{ "window_resize", mousefunc_window_resize, MOUSEBIND_CTX_WIN },
	{ "window_grouptoggle", mousefunc_window_grouptoggle,
	    MOUSEBIND_CTX_WIN },
	{ "window_lower", mousefunc_window_lower, MOUSEBIND_CTX_WIN },
	{ "window_raise", mousefunc_window_raise, MOUSEBIND_CTX_WIN },
	{ "window_hide", mousefunc_window_hide, MOUSEBIND_CTX_WIN },
	{ "menu_group", mousefunc_menu_group, MOUSEBIND_CTX_ROOT },
	{ "menu_unhide", mousefunc_menu_unhide, MOUSEBIND_CTX_ROOT },
	{ "menu_cmd", mousefunc_menu_cmd, MOUSEBIND_CTX_ROOT },
};

void
conf_mousebind(struct conf *c, char *name, char *binding)
{
	struct mousebinding	*current_binding;
	char			*substring, *tmp;
	const char		*errstr;
	int			 iter;

	current_binding = xcalloc(1, sizeof(*current_binding));

	if ((substring = strchr(name, '-')) != NULL) {
		for (iter = 0; iter < nitems(bind_mods); iter++) {
			if ((tmp = strchr(name, bind_mods[iter].chr)) !=
			    NULL && tmp < substring) {
				current_binding->modmask |=
				    bind_mods[iter].mask;
			}
		}

		/* skip past the modifiers */
		substring++;
	} else
		substring = name;

	current_binding->button = strtonum(substring, 1, 3, &errstr);
	if (errstr)
		warnx("number of buttons is %s: %s", errstr, substring);

	conf_mouseunbind(c, current_binding);

	if (strcmp("unmap", binding) == 0)
		return;

	for (iter = 0; iter < nitems(name_to_mousefunc); iter++) {
		if (strcmp(name_to_mousefunc[iter].tag, binding) != 0)
			continue;

		current_binding->context = name_to_mousefunc[iter].context;
		current_binding->callback = name_to_mousefunc[iter].handler;
		TAILQ_INSERT_TAIL(&c->mousebindingq, current_binding, entry);
		return;
	}
}

static void
conf_mouseunbind(struct conf *c, struct mousebinding *unbind)
{
	struct mousebinding	*mb = NULL, *mbnxt;

	for (mb = TAILQ_FIRST(&c->mousebindingq);
	    mb != TAILQ_END(&c->mousebindingq); mb = mbnxt) {
		mbnxt = TAILQ_NEXT(mb, entry);

		if (mb->modmask != unbind->modmask)
			continue;

		if (mb->button == unbind->button) {
			TAILQ_REMOVE(&c->mousebindingq, mb, entry);
			xfree(mb);
		}
	}
}

/*
 * Grab the mouse buttons that we need for bindings for this client
 */
void
conf_grab_mouse(struct client_ctx *cc)
{
	struct mousebinding	*mb;
	int			 button;

	TAILQ_FOREACH(mb, &Conf.mousebindingq, entry) {
		if (mb->context != MOUSEBIND_CTX_WIN)
			continue;

		switch(mb->button) {
		case 1:
			button = Button1;
			break;
		case 2:
			button = Button2;
			break;
		case 3:
			button = Button3;
			break;
		default:
			warnx("strange button in mousebinding\n");
			continue;
		}
		xu_btn_grab(cc->win, mb->modmask, button);
	}
}

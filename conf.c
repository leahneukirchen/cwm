/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2004 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id$
 */

#include "headers.h"
#include "calmwm.h"

#ifndef timespeccmp
#define timespeccmp(tsp, usp, cmp)			\
        (((tsp)->tv_sec == (usp)->tv_sec) ?		\
            ((tsp)->tv_nsec cmp (usp)->tv_nsec) :	\
            ((tsp)->tv_sec cmp (usp)->tv_sec))
#endif

#define CONF_MAX_WINTITLE 256
#define CONF_IGNORECASE   0x01


/*
 * Match a window.
 */
struct winmatch {
	TAILQ_ENTRY(winmatch) entry;

	char title[CONF_MAX_WINTITLE];
	int  opts;
};

TAILQ_HEAD(winmatch_q, winmatch);
struct winmatch_q ignoreq;

/* XXX - until we get a real configuration parser. */
#define WINMATCH_ADD(queue, str) do {			\
	struct winmatch *wm;				\
	XCALLOC(wm, struct winmatch);		\
	strlcpy(wm->title, str, sizeof(wm->title));	\
	wm->opts |= CONF_IGNORECASE;			\
	TAILQ_INSERT_TAIL(queue, wm, entry);		\
} while (0)

/* #define SYSTR_PRE "systrace -C -g /usr/local/bin/notification -d /usr/home/marius/policy/X11 " */

/* Initializes the command menu */

void
conf_cmd_init(struct conf *c)
{
	TAILQ_INIT(&c->cmdq);
}

/* Removes all static entries */

void
conf_cmd_clear(struct conf *c)
{
	struct cmd *cmd, *next;

	for (cmd = TAILQ_FIRST(&c->cmdq); cmd != NULL; cmd = next) {
		next = TAILQ_NEXT(cmd, entry);

		/* Do not remove static entries */
		if (cmd->flags & CMD_STATIC)
			continue;

		TAILQ_REMOVE(&c->cmdq, cmd, entry);
		free(cmd);
	}
}

/* Add an command menu entry to the end of the menu */
void
conf_cmd_add(struct conf *c, char *image, char *label, int flags)
{
	/* "term" and "lock" have special meanings. */

	if (strcmp(label, "term") == 0) {
		strlcpy(Conf.termpath, image, sizeof(Conf.termpath));
	} else if (strcmp(label, "lock") == 0) {
		strlcpy(Conf.lockpath, image, sizeof(Conf.lockpath));
	} else {
		struct cmd *cmd;
		XMALLOC(cmd, struct cmd);
		cmd->flags = flags;
		strlcpy(cmd->image, image, sizeof(cmd->image));
		strlcpy(cmd->label, label, sizeof(cmd->label));
		TAILQ_INSERT_TAIL(&c->cmdq, cmd, entry);
	}
}

int
conf_cmd_changed(char *path)
{
#ifdef __OpenBSD__
	static struct timespec old_ts;
#else
	static time_t old_time;
#endif
	struct stat sb;
	int changed;

	/* If the directory does not exist we pretend that nothing changed */
	if (stat(path, &sb) == -1 || !(sb.st_mode & S_IFDIR))
		return (0);

#ifdef __OpenBSD__
	changed = !timespeccmp(&sb.st_mtimespec, &old_ts, ==);
	old_ts = sb.st_mtimespec;
#else
	changed = old_time != sb.st_mtime;
	old_time = sb.st_mtime;
#endif

	return (changed);
}

void
conf_cmd_populate(struct conf *c, char *path)
{
	DIR *dir;
	struct dirent *file;
	char fullname[PATH_MAX];
	int off;

	if (strlen(path) >= sizeof (fullname) - 2)
		errx(1, "directory name too long");

	dir = opendir(path);
	if (dir == NULL)
		err(1, "opendir");

	strlcpy(fullname, path, sizeof (fullname));
	off = strlen(fullname);
	if (fullname[off - 1] != '/') {
		strlcat(fullname, "/", sizeof(fullname));
		off++;
	}

	while ((file = readdir(dir)) != NULL) {
		char *filename = file->d_name;
                if (filename[0] == '.')
			continue;

		strlcpy(fullname + off, filename, sizeof(fullname) - off);

		/* Add a dynamic entry to the command menu */
		conf_cmd_add(c, fullname, filename, 0);
	}

	closedir(dir);

}

void
conf_cmd_refresh(struct conf *c)
{
	if (!conf_cmd_changed(c->menu_path))
		return;

	conf_cmd_clear(c);
	conf_cmd_populate(c, c->menu_path);
}

void
conf_setup(struct conf *c)
{
 	char dir_keydefs[MAXPATHLEN];
 	char dir_settings[MAXPATHLEN];
 	char dir_ignored[MAXPATHLEN];
	char dir_autogroup[MAXPATHLEN];
	char *home = getenv("HOME");

	if (home == NULL)
		errx(1, "No HOME directory.");
	snprintf(c->menu_path, sizeof(c->menu_path), "%s/.calmwm", home);

	conf_cmd_init(c);

        TAILQ_INIT(&c->keybindingq);

	conf_bindname(c, "CM-Return", "terminal");
	conf_bindname(c, "CM-Delete", "lock");
	conf_bindname(c, "M-question", "exec");
	conf_bindname(c, "CM-q", "exec_wm");
	conf_bindname(c, "M-period", "ssh");
	conf_bindname(c, "M-Return", "hide");
	conf_bindname(c, "M-Down", "lower");
	conf_bindname(c, "M-Up", "raise");
	conf_bindname(c, "M-slash", "search");
	conf_bindname(c, "C-slash", "menusearch");
	conf_bindname(c, "M-Tab", "cycle");
	conf_bindname(c, "MS-Tab", "rcycle");
	conf_bindname(c, "CM-n", "label");
	conf_bindname(c, "CM-x", "delete");
	conf_bindname(c, "CM-Escape", "groupselect");
	conf_bindname(c, "CM-0", "nogroup");
	conf_bindname(c, "CM-1", "group1");
	conf_bindname(c, "CM-2", "group2");
	conf_bindname(c, "CM-3", "group3");
	conf_bindname(c, "CM-4", "group4");
	conf_bindname(c, "CM-5", "group5");
	conf_bindname(c, "CM-6", "group6");
	conf_bindname(c, "CM-7", "group7");
	conf_bindname(c, "CM-8", "group8");
	conf_bindname(c, "CM-9", "group9");
	conf_bindname(c, "M-Right", "nextgroup");
	conf_bindname(c, "M-Left", "prevgroup");
	conf_bindname(c, "CM-f", "maximize");
	conf_bindname(c, "CM-equal", "vmaximize");

	conf_bindname(c, "M-h", "moveleft");
	conf_bindname(c, "M-j", "movedown");
	conf_bindname(c, "M-k", "moveup");
	conf_bindname(c, "M-l", "moveright");
	conf_bindname(c, "M-H", "bigmoveleft");
	conf_bindname(c, "M-J", "bigmovedown");
	conf_bindname(c, "M-K", "bigmoveup");
	conf_bindname(c, "M-L", "bigmoveright");

	conf_bindname(c, "CM-h", "resizeleft");
	conf_bindname(c, "CM-j", "resizedown");
	conf_bindname(c, "CM-k", "resizeup");
	conf_bindname(c, "CM-l", "resizeright");
	conf_bindname(c, "CM-H", "bigresizeleft");
	conf_bindname(c, "CM-J", "bigresizedown");
	conf_bindname(c, "CM-K", "bigresizeup");
	conf_bindname(c, "CM-L", "bigresizeright");

	conf_bindname(c, "C-Left", "ptrmoveleft");
	conf_bindname(c, "C-Down", "ptrmovedown");
	conf_bindname(c, "C-Up", "ptrmoveup");
	conf_bindname(c, "C-Right", "ptrmoveright");
	conf_bindname(c, "CS-Left", "bigptrmoveleft");
	conf_bindname(c, "CS-Down", "bigptrmovedown");
	conf_bindname(c, "CS-Up", "bigptrmoveup");
	conf_bindname(c, "CS-Right", "bigptrmoveright");

        snprintf(dir_keydefs, sizeof(dir_keydefs), "%s/.calmwm/.keys", home);
        if (dirent_isdir(dir_keydefs))
                conf_parsekeys(c, dir_keydefs);

 	snprintf(dir_settings, sizeof(dir_settings),
	    "%s/.calmwm/.settings", home);
 	if (dirent_isdir(dir_settings))
 		conf_parsesettings(c, dir_settings);

	TAILQ_INIT(&ignoreq);

	snprintf(dir_ignored, sizeof(dir_ignored), "%s/.calmwm/.ignore", home);
	if (dirent_isdir(dir_ignored))
		conf_parseignores(c, dir_ignored);
	else {
		WINMATCH_ADD(&ignoreq, "XMMS");
		WINMATCH_ADD(&ignoreq, "xwi");
		WINMATCH_ADD(&ignoreq, "xapm");
		WINMATCH_ADD(&ignoreq, "xclock");
	}

	TAILQ_INIT(&c->autogroupq);

	snprintf(dir_autogroup, sizeof(dir_autogroup),
	    "%s/.calmwm/.autogroup", home);
	if (dirent_isdir(dir_autogroup))
		conf_parseautogroups(c, dir_autogroup);

	c->flags = 0;

	/* Default term/lock */
	strlcpy(Conf.termpath, "xterm", sizeof(Conf.termpath));
	strlcpy(Conf.lockpath, "xlock", sizeof(Conf.lockpath));
}

int
conf_get_int(struct client_ctx *cc, enum conftype ctype)
{
	int val = -1, ignore = 0;
	char *wname;
	struct winmatch *wm;

	wname = cc->name;

	/* Can wname be NULL? */

	if (wname != NULL) {
		TAILQ_FOREACH(wm, &ignoreq, entry) {
			int (*cmpfun)(const char *, const char *, size_t) =
			    wm->opts & CONF_IGNORECASE ? strncasecmp : strncmp;
			if ((*cmpfun)(wm->title, wname, strlen(wm->title)) == 0) {
				ignore = 1;
				break;
			}
		}
		
	} else
		ignore = 1;

	switch (ctype) {
	case CONF_BWIDTH:
		/*
		 * XXX this will be a list, specified in the
		 * configuration file.
		 */
		val = ignore ? 0 : 3;
		break;
	case CONF_IGNORE:
		val = ignore;
		break;
	default:
		break;
	}

	return (val);
}

char *
conf_get_str(struct client_ctx *cc, enum conftype ctype)
{
	switch (ctype) {
	case CONF_NOTIFIER:
		return xstrdup("./notifier.py"); /* XXX */
		break;
	default:
		break;
	}
    return NULL;
}

void
conf_client(struct client_ctx *cc)
{
	cc->bwidth = conf_get_int(cc, CONF_BWIDTH);
	cc->flags |= conf_get_int(cc, CONF_IGNORE) ? CLIENT_IGNORE : 0;
}

struct {
	char *tag;
	void (*handler)(struct client_ctx *, void *);
	int flags;
	void *argument;
} name_to_kbfunc[] = {
	{ "lower", kbfunc_client_lower, KBFLAG_NEEDCLIENT, 0 },
	{ "raise", kbfunc_client_raise, KBFLAG_NEEDCLIENT, 0 },
	{ "search", kbfunc_client_search, 0, 0 },
	{ "menusearch", kbfunc_menu_search, 0, 0 },
	{ "hide", kbfunc_client_hide, KBFLAG_NEEDCLIENT, 0 },
	{ "cycle", kbfunc_client_cycle, KBFLAG_NEEDCLIENT, 0 },
	{ "rcycle", kbfunc_client_rcycle, KBFLAG_NEEDCLIENT, 0 },
	{ "label", kbfunc_client_label, KBFLAG_NEEDCLIENT, 0 },
	{ "delete", kbfunc_client_delete, KBFLAG_NEEDCLIENT, 0 },
	{ "ptrmoveup", kbfunc_ptrmove, 0, (void *)CWM_UP },
	{ "ptrmovedown", kbfunc_ptrmove, 0, (void *)CWM_DOWN },
	{ "ptrmoveleft", kbfunc_ptrmove, 0, (void *)CWM_LEFT },
	{ "ptrmoveright", kbfunc_ptrmove, 0, (void *)CWM_RIGHT },
	{ "bigptrmoveup", kbfunc_ptrmove, 0, (void *)(CWM_UP|CWM_BIGMOVE) },
	{ "bigptrmovedown", kbfunc_ptrmove, 0, (void *)(CWM_DOWN|CWM_BIGMOVE) },
	{ "bigptrmoveleft", kbfunc_ptrmove, 0, (void *)(CWM_LEFT|CWM_BIGMOVE) },
	{ "bigptrmoveright", kbfunc_ptrmove, 0, (void *)(CWM_RIGHT|CWM_BIGMOVE) },
	{ "groupselect", kbfunc_client_groupselect, 0, 0 },
	{ "group1", kbfunc_client_group, 0, (void *)1 },
	{ "group2", kbfunc_client_group, 0, (void *)2 },
	{ "group3", kbfunc_client_group, 0, (void *)3 },
	{ "group4", kbfunc_client_group, 0, (void *)4 },
	{ "group5", kbfunc_client_group, 0, (void *)5 },
	{ "group6", kbfunc_client_group, 0, (void *)6 },
	{ "group7", kbfunc_client_group, 0, (void *)7 },
	{ "group8", kbfunc_client_group, 0, (void *)8 },
	{ "group9", kbfunc_client_group, 0, (void *)9 },
	{ "nogroup", kbfunc_client_nogroup, 0, 0 },
	{ "nextgroup", kbfunc_client_nextgroup, 0, 0 },
	{ "prevgroup", kbfunc_client_prevgroup, 0, 0 },
	{ "maximize", kbfunc_client_maximize, KBFLAG_NEEDCLIENT, 0 },
	{ "vmaximize", kbfunc_client_vmaximize, KBFLAG_NEEDCLIENT, 0 },
	{ "exec", kbfunc_exec, 0, CWM_EXEC_PROGRAM },
	{ "exec_wm", kbfunc_exec, 0, CWM_EXEC_WM },
	{ "ssh", kbfunc_ssh, 0, 0 },
	{ "terminal", kbfunc_term, 0, 0 },
	{ "lock", kbfunc_lock, 0, 0 },
	{ "moveup", kbfunc_client_move, KBFLAG_NEEDCLIENT, (void *)CWM_UP },
	{ "movedown", kbfunc_client_move, KBFLAG_NEEDCLIENT, (void *)CWM_DOWN },
	{ "moveright", kbfunc_client_move, KBFLAG_NEEDCLIENT, (void *)CWM_RIGHT },
	{ "moveleft", kbfunc_client_move, KBFLAG_NEEDCLIENT, (void *)CWM_LEFT },
	{ "bigmoveup", kbfunc_client_move, KBFLAG_NEEDCLIENT, (void *)(CWM_UP|CWM_BIGMOVE) },
	{ "bigmovedown", kbfunc_client_move, KBFLAG_NEEDCLIENT, (void *)(CWM_DOWN|CWM_BIGMOVE) },
	{ "bigmoveright", kbfunc_client_move, KBFLAG_NEEDCLIENT, (void *)(CWM_RIGHT|CWM_BIGMOVE) },
	{ "bigmoveleft", kbfunc_client_move, KBFLAG_NEEDCLIENT, (void *)(CWM_LEFT|CWM_BIGMOVE) },
	{ "resizeup", kbfunc_client_resize, KBFLAG_NEEDCLIENT, (void *)(CWM_UP) },
	{ "resizedown", kbfunc_client_resize, KBFLAG_NEEDCLIENT, (void *)CWM_DOWN },
	{ "resizeright", kbfunc_client_resize, KBFLAG_NEEDCLIENT, (void *)CWM_RIGHT },
	{ "resizeleft", kbfunc_client_resize, KBFLAG_NEEDCLIENT, (void *)CWM_LEFT },
	{ "bigresizeup", kbfunc_client_resize, KBFLAG_NEEDCLIENT, (void *)(CWM_UP|CWM_BIGMOVE) },
	{ "bigresizedown", kbfunc_client_resize, KBFLAG_NEEDCLIENT, (void *)(CWM_DOWN|CWM_BIGMOVE) },
	{ "bigresizeright", kbfunc_client_resize, KBFLAG_NEEDCLIENT, (void *)(CWM_RIGHT|CWM_BIGMOVE) },
	{ "bigresizeleft", kbfunc_client_resize, KBFLAG_NEEDCLIENT, (void *)(CWM_LEFT|CWM_BIGMOVE) },
	{ NULL, NULL, 0, 0},
};

void
conf_bindkey(struct conf *c, void (*arg_callback)(struct client_ctx *, void *),
    int arg_keysym, int arg_modmask, int arg_flags, void * arg_arg)
{
	struct keybinding *kb;

	XMALLOC(kb, struct keybinding);

	kb->modmask = arg_modmask;
	kb->keysym = arg_keysym;
	kb->keycode = 0;
	kb->flags = arg_flags;
	kb->callback = arg_callback;
	kb->argument = arg_arg;
	TAILQ_INSERT_TAIL(&c->keybindingq, kb, entry);
}

void
conf_parsekeys(struct conf *c, char *filename)
{
	DIR *dir;
	struct dirent *ent;
	char buffer[MAXPATHLEN];
	char current_file[MAXPATHLEN];

	dir = opendir(filename);
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.')
			continue;

		snprintf(current_file, sizeof(current_file),
		    "%s/%s", filename, ent->d_name);
		if (strchr(ent->d_name, '-') == NULL && ent->d_name[0] != '[')
			continue;
		if (!dirent_islink(current_file))
			continue;


		memset(buffer, 0, MAXPATHLEN);
		if (readlink(current_file, buffer, MAXPATHLEN) < 0)
			continue;

		conf_bindname(c, ent->d_name, buffer);
	}

	closedir(dir);
}

void
conf_bindname(struct conf *c, char *name, char *binding)
{
	int iter;
	struct keybinding *current_binding;
	char *substring;

	XCALLOC(current_binding, struct keybinding);

	if (strchr(name, 'C') != NULL &&
	    strchr(name, 'C') < strchr(name, '-'))
		current_binding->modmask |= ControlMask;

	if (strchr(name, 'M') != NULL &&
	    strchr(name, 'M') < strchr(name, '-')) 
		current_binding->modmask |= Mod1Mask;

	if (strchr(name, '2') != NULL &&
	    strchr(name, '2') < strchr(name, '-')) 
		current_binding->modmask |= Mod2Mask;

	if (strchr(name, '3') != NULL &&
	    strchr(name, '3') < strchr(name, '-')) 
		current_binding->modmask |= Mod3Mask;

	if (strchr(name, '4') != NULL &&
	    strchr(name, '4') < strchr(name, '-')) 
		current_binding->modmask |= Mod4Mask;

	if (strchr(name, 'S') != NULL &&
	    strchr(name, 'S') < strchr(name, '-')) 
		current_binding->modmask |= ShiftMask;

	substring = strchr(name, '-') + 1;

	/* If there is no '-' in name, continue as is */
	if (strchr(name, '-') == NULL)
		substring = name;

	if (substring[0] == '[' &&
	    substring[strlen(substring)-1] == ']') {
		sscanf(substring, "[%d]", &current_binding->keycode);
		current_binding->keysym = NoSymbol;
	} else {
		current_binding->keycode = 0;
		current_binding->keysym = XStringToKeysym(substring);
	}

	if (current_binding->keysym == NoSymbol &&
	    current_binding->keycode == 0 ) {
		xfree(current_binding);
		return;
	}

	/* We now have the correct binding, remove duplicates. */
	conf_unbind(c, current_binding);	

	if (strcmp("unmap",binding) == 0)
		return;

	for (iter = 0; name_to_kbfunc[iter].tag != NULL; iter++) {
		if (strcmp(name_to_kbfunc[iter].tag, binding) != 0)
			continue;

		current_binding->callback = name_to_kbfunc[iter].handler;
		current_binding->flags = name_to_kbfunc[iter].flags;
		current_binding->argument = name_to_kbfunc[iter].argument;
		TAILQ_INSERT_TAIL(&c->keybindingq, current_binding, entry);
		break;
	}

	if (name_to_kbfunc[iter].tag != NULL)
		return;

	current_binding->callback = kbfunc_cmdexec;
	current_binding->argument = strdup(binding);
	current_binding->flags = 0;
	TAILQ_INSERT_TAIL(&c->keybindingq, current_binding, entry);
	return;
}

void conf_unbind(struct conf *c, struct keybinding *unbind)
{
	struct keybinding *key = NULL;

	TAILQ_FOREACH(key, &c->keybindingq, entry) {
		if (key->modmask != unbind->modmask)
			continue;

		if ((key->keycode != 0 && key->keysym == NoSymbol &&
			key->keycode == unbind->keycode) || 
			key->keysym == unbind->keysym)
			TAILQ_REMOVE(&c->keybindingq, key, entry);
	}
}

void
conf_parsesettings(struct conf *c, char *filename)
{
	DIR *dir;
	struct dirent *ent;

	dir = opendir(filename);
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.')
			continue;
		if (strncmp(ent->d_name, "sticky", 7)==0)
			Conf.flags |= CONF_STICKY_GROUPS;
	}
	closedir(dir);
}

void
conf_parseignores(struct conf *c, char *filename)
{
	DIR *dir;
	struct dirent *ent;

	dir = opendir(filename);
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.')
			continue;
		WINMATCH_ADD(&ignoreq, ent->d_name);
	}

	closedir(dir);
}

void
conf_parseautogroups(struct conf *c, char *filename)
{
	DIR *dir;
	struct dirent *ent;
	struct autogroupwin *aw;
	char current_file[MAXPATHLEN], *p;
	char group[CALMWM_MAXNAMELEN];
	int len;

	dir = opendir(filename);
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.')
			continue;

		snprintf(current_file, sizeof(current_file),
		    "%s/%s", filename, ent->d_name);
		if (!dirent_islink(current_file))
			continue;

		if ((len = readlink(current_file,
			    group, sizeof(group) - 1)) < 0)
			continue;
		group[len] = '\0';

		XCALLOC(aw, struct autogroupwin);
		
		if ((p = strchr(ent->d_name, ',')) == NULL) {
			aw->name = NULL;
			aw->class = xstrdup(ent->d_name);
		} else {
			*(p++) = '\0';
			aw->name = xstrdup(ent->d_name);
			aw->class = xstrdup(p);
		}
		aw->group = xstrdup(group);

		TAILQ_INSERT_TAIL(&c->autogroupq, aw, entry);
	}

	closedir(dir);

}

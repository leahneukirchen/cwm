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
#include <fnmatch.h>
#include <glob.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calmwm.h"

#define PATH_ANY 	0x0001
#define PATH_EXEC 	0x0002

static void	search_match_path_type(struct menu_q *, struct menu_q *,
		    char *, int);
static int	strsubmatch(char *, char *, int);

void
search_match_client(struct menu_q *menuq, struct menu_q *resultq, char *search)
{
	struct winname	*wn;
	struct menu	*mi, *tierp[4], *before = NULL;

	TAILQ_INIT(resultq);

	(void)memset(tierp, 0, sizeof(tierp));

	TAILQ_FOREACH(mi, menuq, entry) {
		int tier = -1, t;
		struct client_ctx *cc = (struct client_ctx *)mi->ctx;

		/* Match on label. */
		if (strsubmatch(search, cc->label, 0))
			tier = 0;

		/* Match on window name history, from present to past. */
		if (tier < 0) {
			TAILQ_FOREACH_REVERSE(wn, &cc->nameq, name_q, entry)
				if (strsubmatch(search, wn->name, 0)) {
					tier = 2;
					break;
				}
		}

		/* Match on window resource class. */
		if ((tier < 0) && strsubmatch(search, cc->ch.res_class, 0))
			tier = 3;

		if (tier < 0)
			continue;

		/* Current window is ranked down. */
		if ((tier < nitems(tierp) - 1) && (cc->flags & CLIENT_ACTIVE))
			tier++;

		/* Hidden window is ranked up. */
		if ((tier > 0) && (cc->flags & CLIENT_HIDDEN))
			tier--;

		if (tier >= nitems(tierp))
			errx(1, "%s: invalid tier", __func__);

		/*
		 * If you have a tierp, insert after it, and make it
		 * the new tierp.  If you don't have a tierp, find the
		 * first nonzero tierp above you, insert after it.
		 * Always make your current tierp the newly inserted
		 * entry.
		 */
		for (t = tier; t >= 0 && ((before = tierp[t]) == NULL); t--)
			;

		if (before == NULL)
			TAILQ_INSERT_HEAD(resultq, mi, resultentry);
		else
			TAILQ_INSERT_AFTER(resultq, before, mi, resultentry);

		tierp[tier] = mi;
	}
}

void
search_print_text(struct menu *mi, int listing)
{
	(void)snprintf(mi->print, sizeof(mi->print), "%s", mi->text);
}

void
search_print_cmd(struct menu *mi, int listing)
{
	struct cmd_ctx	*cmd = (struct cmd_ctx *)mi->ctx;

	(void)snprintf(mi->print, sizeof(mi->print), "%s", cmd->name);
}

void
search_print_group(struct menu *mi, int listing)
{
	struct group_ctx	*gc = (struct group_ctx *)mi->ctx;

	(void)snprintf(mi->print, sizeof(mi->print),
	    (group_holds_only_hidden(gc)) ? "%d: [%s]" : "%d: %s",
	    gc->num, gc->name);
}

void
search_print_client(struct menu *mi, int listing)
{
	struct client_ctx	*cc = (struct client_ctx *)mi->ctx;
	char			 flag = ' ';

	if (cc->flags & CLIENT_ACTIVE)
		flag = '!';
	else if (cc->flags & CLIENT_HIDDEN)
		flag = '&';

	(void)snprintf(mi->print, sizeof(mi->print), "(%d) %c[%s] %s",
	    (cc->gc) ? cc->gc->num : 0, flag,
	    (cc->label) ? cc->label : "", cc->name);
}

static void
search_match_path_type(struct menu_q *menuq, struct menu_q *resultq,
    char *search, int flag)
{
	struct menu     *mi;
	char 		 pattern[PATH_MAX];
	glob_t		 g;
	int		 i;

	(void)strlcpy(pattern, search, sizeof(pattern));
	(void)strlcat(pattern, "*", sizeof(pattern));

	if (glob(pattern, GLOB_MARK, NULL, &g) != 0)
		return;
	for (i = 0; i < g.gl_pathc; i++) {
		if ((flag & PATH_EXEC) && access(g.gl_pathv[i], X_OK))
			continue;
		mi = xcalloc(1, sizeof(*mi));
		(void)strlcpy(mi->text, g.gl_pathv[i], sizeof(mi->text));
		TAILQ_INSERT_TAIL(resultq, mi, resultentry);
	}
	globfree(&g);
}

void
search_match_path(struct menu_q *menuq, struct menu_q *resultq, char *search)
{
	TAILQ_INIT(resultq);

	search_match_path_type(menuq, resultq, search, PATH_ANY);
}

void
search_match_text(struct menu_q *menuq, struct menu_q *resultq, char *search)
{
	struct menu	*mi;

	TAILQ_INIT(resultq);

	TAILQ_FOREACH(mi, menuq, entry)
		if (strsubmatch(search, mi->text, 0))
			TAILQ_INSERT_TAIL(resultq, mi, resultentry);
}

void
search_match_exec(struct menu_q *menuq, struct menu_q *resultq, char *search)
{
	struct menu	*mi, *mj;
	int		 r;

	TAILQ_INIT(resultq);

	TAILQ_FOREACH(mi, menuq, entry) {
		if (strsubmatch(search, mi->text, 1) == 0 &&
		    fnmatch(search, mi->text, 0) == FNM_NOMATCH)
			continue;
		TAILQ_FOREACH(mj, resultq, resultentry) {
			r = strcasecmp(mi->text, mj->text);
			if (r < 0)
				TAILQ_INSERT_BEFORE(mj, mi, resultentry);
			if (r <= 0)
				break;
		}
		if (mj == NULL)
			TAILQ_INSERT_TAIL(resultq, mi, resultentry);
	}

	if (TAILQ_EMPTY(resultq))
		search_match_path_type(menuq, resultq, search, PATH_EXEC);
}

static int
strsubmatch(char *sub, char *str, int zeroidx)
{
	size_t		 len, sublen;
	unsigned int	 n, flen;

	if (sub == NULL || str == NULL)
		return(0);

	len = strlen(str);
	sublen = strlen(sub);

	if (sublen > len)
		return(0);

	if (!zeroidx)
		flen = len - sublen;
	else
		flen = 0;

	for (n = 0; n <= flen; n++)
		if (strncasecmp(sub, str + n, sublen) == 0)
			return(1);

	return(0);
}

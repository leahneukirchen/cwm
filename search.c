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

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "calmwm.h"

static int	strsubmatch(char *, char *, int);

/*
 * Match: label, title, class.
 */

void
search_match_client(struct menu_q *menuq, struct menu_q *resultq, char *search)
{
	struct winname	*wn;
	struct menu	*mi, *tierp[4], *before = NULL;

	TAILQ_INIT(resultq);

	(void)memset(tierp, 0, sizeof(tierp));

	/*
	 * In order of rank:
	 *
	 *   1. Look through labels.
	 *   2. Look at title history, from present to past.
	 *   3. Look at window class name.
	 */

	TAILQ_FOREACH(mi, menuq, entry) {
		int tier = -1, t;
		struct client_ctx *cc = mi->ctx;

		/* First, try to match on labels. */
		if (cc->label != NULL && strsubmatch(search, cc->label, 0)) {
			cc->matchname = cc->label;
			tier = 0;
		}

		/* Then, on window names. */
		if (tier < 0) {
			TAILQ_FOREACH_REVERSE(wn, &cc->nameq, winname_q, entry)
				if (strsubmatch(search, wn->name, 0)) {
					cc->matchname = wn->name;
					tier = 2;
					break;
				}
		}

		/* Then if there is a match on the window class name. */
		if (tier < 0 && strsubmatch(search, cc->app_class, 0)) {
			cc->matchname = cc->app_class;
			tier = 3;
		}

		if (tier < 0)
			continue;

		/*
		 * De-rank a client one tier if it's the current
		 * window.  Furthermore, this is denoted by a "!" when
		 * printing the window name in the search menu.
		 */
		if (cc == client_current() && tier < nitems(tierp) - 1)
			tier++;

		/* Clients that are hidden get ranked one up. */
		if (cc->flags & CLIENT_HIDDEN && tier > 0)
			tier--;

		assert(tier < nitems(tierp));

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
search_print_client(struct menu *mi, int list)
{
	struct client_ctx	*cc;
	char			 flag = ' ';

	cc = mi->ctx;

	if (cc == client_current())
		flag = '!';
	else if (cc->flags & CLIENT_HIDDEN)
		flag = '&';

	if (list)
		cc->matchname = cc->name;

	(void)snprintf(mi->print, sizeof(mi->print), "%c%s", flag,
	    cc->matchname);

	if (!list && cc->matchname != cc->name &&
	    strlen(mi->print) < sizeof(mi->print) - 1) {
		const char	*marker = "";
		char		 buf[MENU_MAXENTRY + 1];
		int		 diff;

		diff = sizeof(mi->print) - 1 - strlen(mi->print);

		/* One for the ':' */
		diff -= 1;

		if (strlen(cc->name) > diff) {
			marker = "..";
			diff -= 2;
		} else {
			diff = strlen(cc->name);
		}

		(void)strlcpy(buf, mi->print, sizeof(buf));
		(void)snprintf(mi->print, sizeof(mi->print),
		    "%s:%.*s%s", buf, diff, cc->name, marker);
	}
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

	TAILQ_INIT(resultq);

	TAILQ_FOREACH(mi, menuq, entry) {
		if (strsubmatch(search, mi->text, 1) == 0 &&
		    fnmatch(search, mi->text, 0) == FNM_NOMATCH)
				continue;
		for (mj = TAILQ_FIRST(resultq); mj != NULL;
		     mj = TAILQ_NEXT(mj, resultentry)) {
			if (strcasecmp(mi->text, mj->text) < 0) {
				TAILQ_INSERT_BEFORE(mj, mi, resultentry);
				break;
			}
		}
		if (mj == NULL)
			TAILQ_INSERT_TAIL(resultq, mi, resultentry);
	}
}

static int
strsubmatch(char *sub, char *str, int zeroidx)
{
	size_t	 len, sublen;
	u_int	 n, flen;

	if (sub == NULL || str == NULL)
		return (0);

	len = strlen(str);
	sublen = strlen(sub);

	if (sublen > len)
		return (0);

	if (!zeroidx)
		flen = len - sublen;
	else
		flen = 0;

	for (n = 0; n <= flen; n++)
		if (strncasecmp(sub, str + n, sublen) == 0)
			return (1);

	return (0);
}

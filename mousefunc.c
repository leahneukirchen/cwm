/*
 *  calmwm - the calm window manager
 *
 *  Copyright (c) 2008 rivo nurges <rix@estpak.ee>
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

#include "headers.h"
#include "calmwm.h"

void
mousefunc_window_resize(struct client_ctx *cc, void *arg)
{
	grab_sweep(cc);
	client_resize(cc);
}

void
mousefunc_window_move(struct client_ctx *cc, void *arg)
{
	grab_drag(cc);
	client_move(cc);
}

void
mousefunc_window_grouptoggle(struct client_ctx *cc, void *arg)
{
	group_sticky_toggle_enter(cc);
}

void
mousefunc_window_lower(struct client_ctx *cc, void *arg)
{
	client_ptrsave(cc);
	client_lower(cc);
}

void
mousefunc_window_hide(struct client_ctx *cc, void *arg)
{
	client_hide(cc);
}

void
mousefunc_menu_group(struct client_ctx *cc, void *arg)
{
	group_menu(arg);
}

void
mousefunc_menu_unhide(struct client_ctx *cc, void *arg)
{
	struct menu *mi;
	struct menu_q menuq;
	char *wname;
	struct client_ctx *old_cc = client_current();

	TAILQ_INIT(&menuq);
	TAILQ_FOREACH(cc, &Clientq, entry)
		if (cc->flags & CLIENT_HIDDEN) {
			if (cc->label != NULL)
				wname = cc->label;
			else
				wname = cc->name;

			if (wname == NULL)
				continue;

			XCALLOC(mi, struct menu);
			strlcpy(mi->text, wname, sizeof(mi->text));
			mi->ctx = cc;
			TAILQ_INSERT_TAIL(&menuq, mi, entry);
		}

	if (TAILQ_EMPTY(&menuq))
		return;

	mi = menu_filter(&menuq, NULL, NULL, 0, NULL, NULL);
	if (mi != NULL) {
		cc = (struct client_ctx *)mi->ctx;
		client_unhide(cc);

		if (old_cc != NULL)
			client_ptrsave(old_cc);
		client_ptrwarp(cc);
	} else
		while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
			TAILQ_REMOVE(&menuq, mi, entry);
			xfree(mi);
		}
}

void
mousefunc_menu_cmd(struct client_ctx *cc, void *arg)
{
	struct menu *mi;
	struct menu_q menuq;
	struct cmd *cmd;
	conf_reload(&Conf);

	TAILQ_INIT(&menuq);
	TAILQ_FOREACH(cmd, &Conf.cmdq, entry) {
		XCALLOC(mi, struct menu);
		strlcpy(mi->text, cmd->label, sizeof(mi->text));
		mi->ctx = cmd;
		TAILQ_INSERT_TAIL(&menuq, mi, entry);
	}
	if (TAILQ_EMPTY(&menuq))
		return;

	mi = menu_filter(&menuq, NULL, NULL, 0, NULL, NULL);
	if (mi != NULL)
		u_spawn(((struct cmd *)mi->ctx)->image);
	else
		while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
			TAILQ_REMOVE(&menuq, mi, entry);
			xfree(mi);
		}
}

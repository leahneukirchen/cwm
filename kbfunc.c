/*
 *  calmwm - the calm window manager
 * 
 *  Copyright (c) 2004 Martin Murray <mmurray@monkey.org>
 *  All rights reserved.
 *
 * $Id$
 */

#include "headers.h"
#include "calmwm.h"

void
kbfunc_client_lower(struct client_ctx *cc, void *arg)
{
	client_lower(cc);
}

void
kbfunc_client_raise(struct client_ctx *cc, void *arg)
{
	client_raise(cc);
}

void
kbfunc_client_search(struct client_ctx *scratch, void *arg)
{
	struct menu_q menuq;
	struct client_ctx *cc, *old_cc = client_current(); 
	struct menu *mi;
	
	TAILQ_INIT(&menuq);
	
	TAILQ_FOREACH(cc, &G_clientq, entry) {
		struct menu *mi;
		XCALLOC(mi, struct menu);
		strlcpy(mi->text, cc->name, sizeof(mi->text));
		mi->ctx = cc;
		TAILQ_INSERT_TAIL(&menuq, mi, entry);
	}

	if ((mi = search_start(&menuq,
		    search_match_client, NULL,
		    search_print_client, "window")) != NULL) {
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
kbfunc_menu_search(struct client_ctx *scratch, void *arg)
{
	struct menu_q menuq;
	struct menu *mi;
	struct cmd *cmd;

	TAILQ_INIT(&menuq);

	conf_cmd_refresh(&G_conf);
	TAILQ_FOREACH(cmd, &G_conf.cmdq, entry) {
		XCALLOC(mi, struct menu);
		strlcpy(mi->text, cmd->label, sizeof(mi->text));
		mi->ctx = cmd;
		TAILQ_INSERT_TAIL(&menuq, mi, entry);
	}

	if ((mi = search_start(&menuq,
		    search_match_text, NULL, NULL, "application")) != NULL)
		u_spawn(((struct cmd *)mi->ctx)->image);

	while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
		TAILQ_REMOVE(&menuq, mi, entry);
		xfree(mi);
	}
}

void
kbfunc_client_cycle(struct client_ctx *cc, void *arg)
{
	client_cyclenext(cc, 0);
}

void
kbfunc_client_rcycle(struct client_ctx *cc, void *arg)
{
	client_cyclenext(cc, 1);
}

void
kbfunc_client_hide(struct client_ctx *cc, void *arg)
{
	client_hide(cc);
}

void
kbfunc_cmdexec(struct client_ctx *cc, void *arg)
{
	u_spawn((char *)arg);
}

void
kbfunc_term(struct client_ctx *cc, void *arg)
{
	conf_cmd_refresh(&G_conf);
	u_spawn(G_conf.termpath);
}

void
kbfunc_lock(struct client_ctx *cc, void *arg)
{
	conf_cmd_refresh(&G_conf);
	u_spawn(G_conf.lockpath);
}

void
kbfunc_client_label(struct client_ctx *cc, void *arg)
{
	grab_label(cc);
}

void
kbfunc_client_delete(struct client_ctx *cc, void *arg)
{
	client_send_delete(cc);
}

void
kbfunc_client_groupselect(struct client_ctx *cc, void *arg)
{
	if (G_groupmode)
		group_done();
	else
		group_enter();
}

void
kbfunc_client_group(struct client_ctx *cc, void *arg)
{
	if (G_groupmode)
		group_select(KBTOGROUP((int)arg));
	else
		group_hidetoggle(KBTOGROUP((int)arg));
}

void
kbfunc_client_nextgroup(struct client_ctx *cc, void *arg)
{
	group_slide(1);
}

void
kbfunc_client_prevgroup(struct client_ctx *cc, void *arg)
{
	group_slide(0);
}

void
kbfunc_client_nogroup(struct client_ctx *cc, void *arg)
{
	if (G_groupmode)
		group_deletecurrent();
	else
		group_alltoggle();
}

void
kbfunc_client_maximize(struct client_ctx *cc, void *arg)
{
	client_maximize(cc);
}

void
kbfunc_client_vmaximize(struct client_ctx *cc, void *arg)
{
	client_vertmaximize(cc);
}

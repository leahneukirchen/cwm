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

struct client_ctx *
geographic_west(struct client_ctx *from_cc)
{
/* 	Window *wins, w0, w1; */
/* 	struct screen_ctx *sc = screen_current(); */
/* 	u_int nwins, i; */
/* 	struct client_ctx *cc; */

	screen_updatestackingorder();

	return (NULL);
}

#if 0
int
_visible(struct client_ctx *this_cc)
{
	int stacking = cc->stackingorder;
	struct client_ctx *cc;

	if (cc->flags & CLIENT_HIDDEN)
		return (0);

	TAILQ_FOREACH(cc, &G_clientq, entry) {
		if (cc->flags & CLIENT_HIDDEN)
			continue;

		if (cc->stackingorder > stacking &&
		    cc->geom.x <= this_cc->geom.x &&
		    cc->geom.y <= this_cc->geom.y &&
		    cc->geom.width > (this_cc->geom.width +
			(this_cc->geom.x - cc->geom.x) &&


		    cc->geom.height > (this_cc->geom.height - cc->geom.height))
			return (0);
	}

	return (1);
}
#endif

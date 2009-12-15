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
 * $Id$
 */

#include "calmwm.h"

void *
xmalloc(size_t siz)
{
	void	*p;

	if ((p = malloc(siz)) == NULL)
		err(1, "malloc");

	return (p);
}

void *
xcalloc(size_t no, size_t siz)
{
	void	*p;

	if ((p = calloc(no, siz)) == NULL)
		err(1, "calloc");

	return (p);
}

void
xfree(void *p)
{
	free(p);
}

char *
xstrdup(const char *str)
{
	char	*p;

	if ((p = strdup(str)) == NULL)
		err(1, "strdup");

	return (p);
}

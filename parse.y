/*	$OpenBSD$ */

/*
 * Copyright (c) 2002, 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2001 Daniel Hartmeier.  All rights reserved.
 * Copyright (c) 2001 Theo de Raadt.  All rights reserved.
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
 */

%{

#include <sys/param.h>
#include <sys/queue.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calmwm.h"

#define YYSTYPE_IS_DECLARED

TAILQ_HEAD(files, file)		 files = TAILQ_HEAD_INITIALIZER(files);
static struct file {
	TAILQ_ENTRY(file)	 entry;
	FILE			*stream;
	char			*name;
	int			 lineno;
	int			 errors;
} *file;

struct file		*pushfile(const char *);
int			 popfile(void);
int			 yyparse(void);
int			 yylex(void);
int			 yyerror(const char *, ...);
int			 kw_cmp(const void *, const void *);
int			 lookup(char *);
int			 lgetc(int);
int			 lungetc(int);
int			 findeol(void);

static struct conf	*conf;

typedef struct {
	union {
		int64_t			 number;
		char			*string;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	FONTNAME STICKY GAP MOUSEBIND
%token	AUTOGROUP BIND COMMAND IGNORE
%token	YES NO BORDERWIDTH MOVEAMOUNT
%token	COLOR SNAPDIST
%token	ACTIVEBORDER INACTIVEBORDER
%token	GROUPBORDER UNGROUPBORDER
%token	MENUBG MENUFG FONTCOLOR
%token	ERROR
%token	<v.string>		STRING
%token	<v.number>		NUMBER
%type	<v.number>		yesno
%type	<v.string>		string
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar main '\n'
		| grammar color '\n'
		| grammar error '\n'		{ file->errors++; }
		;

string		: string STRING			{
			if (asprintf(&$$, "%s %s", $1, $2) == -1) {
				free($1);
				free($2);
				yyerror("string: asprintf");
				YYERROR;
			}
			free($1);
			free($2);
		}
		| STRING
		;

yesno		: YES				{ $$ = 1; }
		| NO				{ $$ = 0; }
		;

main		: FONTNAME STRING		{
			free(conf->font);
			conf->font = $2;
		}
		| STICKY yesno {
			if ($2 == 0)
				conf->flags &= ~CONF_STICKY_GROUPS;
			else
				conf->flags |= CONF_STICKY_GROUPS;
		}
		| BORDERWIDTH NUMBER {
			conf->bwidth = $2;
		}
		| MOVEAMOUNT NUMBER {
			conf->mamount = $2;
		}
		| SNAPDIST NUMBER {
			conf->snapdist = $2;
		}
		| COMMAND STRING string		{
			conf_cmd_add(conf, $3, $2, 0);
			free($2);
			free($3);
		}
		| AUTOGROUP NUMBER STRING	{
			if ($2 < 0 || $2 > 9) {
				free($3);
				yyerror("autogroup number out of range: %d", $2);
				YYERROR;
			}

			group_make_autogroup(conf, $3, $2);
			free($3);
		}
		| IGNORE STRING {
			struct winmatch	*wm;

			wm = xcalloc(1, sizeof(*wm));
			(void)strlcpy(wm->title, $2, sizeof(wm->title));
			TAILQ_INSERT_TAIL(&conf->ignoreq, wm, entry);

			free($2);
		}
		| BIND STRING string		{
			conf_bindname(conf, $2, $3);
			free($2);
			free($3);
		}
		| GAP NUMBER NUMBER NUMBER NUMBER {
			conf->gap.top = $2;
			conf->gap.bottom = $3;
			conf->gap.left = $4;
			conf->gap.right = $5;
		}
		| MOUSEBIND STRING string	{
			conf_mousebind(conf, $2, $3);
			free($2);
			free($3);
		}
		;

color		: COLOR colors
		;

colors		: ACTIVEBORDER STRING {
			free(conf->color[CWM_COLOR_BORDER_ACTIVE].name);
			conf->color[CWM_COLOR_BORDER_ACTIVE].name = $2;
		}
		| INACTIVEBORDER STRING {
			free(conf->color[CWM_COLOR_BORDER_INACTIVE].name);
			conf->color[CWM_COLOR_BORDER_INACTIVE].name = $2;
		}
		| GROUPBORDER STRING {
			free(conf->color[CWM_COLOR_BORDER_GROUP].name);
			conf->color[CWM_COLOR_BORDER_GROUP].name = $2;
		}
		| UNGROUPBORDER STRING {
			free(conf->color[CWM_COLOR_BORDER_UNGROUP].name);
			conf->color[CWM_COLOR_BORDER_UNGROUP].name = $2;
		}
		| MENUBG STRING {
			free(conf->color[CWM_COLOR_BG_MENU].name);
			conf->color[CWM_COLOR_BG_MENU].name = $2;
		}
		| MENUFG STRING {
			free(conf->color[CWM_COLOR_FG_MENU].name);
			conf->color[CWM_COLOR_FG_MENU].name = $2;
		}
		| FONTCOLOR STRING {
			free(conf->color[CWM_COLOR_FONT].name);
			conf->color[CWM_COLOR_FONT].name = $2;
		}
		;
%%

struct keywords {
	const char	*k_name;
	int		 k_val;
};

int
yyerror(const char *fmt, ...)
{
	va_list ap;

	file->errors++;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d: ", file->name, yylval.lineno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	return (0);
}

int
kw_cmp(const void *k, const void *e)
{
	return (strcmp(k, ((const struct keywords *)e)->k_name));
}

int
lookup(char *s)
{
	/* this has to be sorted always */
	static const struct keywords keywords[] = {
		{ "activeborder",	ACTIVEBORDER},
		{ "autogroup",		AUTOGROUP},
		{ "bind",		BIND},
		{ "borderwidth",	BORDERWIDTH},
		{ "color",		COLOR},
		{ "command",		COMMAND},
		{ "font",		FONTCOLOR},
		{ "fontname",		FONTNAME},
		{ "gap",		GAP},
		{ "groupborder",	GROUPBORDER},
		{ "ignore",		IGNORE},
		{ "inactiveborder",	INACTIVEBORDER},
		{ "menubg",		MENUBG},
		{ "menufg",		MENUFG},
		{ "mousebind",		MOUSEBIND},
		{ "moveamount",		MOVEAMOUNT},
		{ "no",			NO},
		{ "snapdist",		SNAPDIST},
		{ "sticky",		STICKY},
		{ "ungroupborder",	UNGROUPBORDER},
		{ "yes",		YES}
	};
	const struct keywords	*p;

	p = bsearch(s, keywords, sizeof(keywords)/sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);

	if (p)
		return (p->k_val);
	else
		return (STRING);
}

#define MAXPUSHBACK	128

char	*parsebuf;
int	 parseindex;
char	 pushback_buffer[MAXPUSHBACK];
int	 pushback_index = 0;

int
lgetc(int quotec)
{
	int		c, next;

	if (parsebuf) {
		/* Read character from the parsebuffer instead of input. */
		if (parseindex >= 0) {
			c = parsebuf[parseindex++];
			if (c != '\0')
				return (c);
			parsebuf = NULL;
		} else
			parseindex++;
	}

	if (pushback_index)
		return (pushback_buffer[--pushback_index]);

	if (quotec) {
		if ((c = getc(file->stream)) == EOF) {
			yyerror("reached end of file while parsing quoted string");
			if (popfile() == EOF)
				return (EOF);
			return (quotec);
		}
		return (c);
	}

	while ((c = getc(file->stream)) == '\\') {
		next = getc(file->stream);
		if (next != '\n') {
			c = next;
			break;
		}
		yylval.lineno = file->lineno;
		file->lineno++;
	}

	while (c == EOF) {
		if (popfile() == EOF)
			return (EOF);
		c = getc(file->stream);
	}
	return (c);
}

int
lungetc(int c)
{
	if (c == EOF)
		return (EOF);
	if (parsebuf) {
		parseindex--;
		if (parseindex >= 0)
			return (c);
	}
	if (pushback_index < MAXPUSHBACK-1)
		return (pushback_buffer[pushback_index++] = c);
	else
		return (EOF);
}

int
findeol(void)
{
	int	c;

	parsebuf = NULL;
	pushback_index = 0;

	/* skip to either EOF or the first real EOL */
	while (1) {
		c = lgetc(0);
		if (c == '\n') {
			file->lineno++;
			break;
		}
		if (c == EOF)
			break;
	}
	return (ERROR);
}

int
yylex(void)
{
	char	 buf[8096];
	char	*p;
	int	 quotec, next, c;
	int	 token;

	p = buf;
	while ((c = lgetc(0)) == ' ' || c == '\t')
		; /* nothing */

	yylval.lineno = file->lineno;
	if (c == '#')
		while ((c = lgetc(0)) != '\n' && c != EOF)
			; /* nothing */

	switch (c) {
	case '\'':
	case '"':
		quotec = c;
		while (1) {
			if ((c = lgetc(quotec)) == EOF)
				return (0);
			if (c == '\n') {
				file->lineno++;
				continue;
			} else if (c == '\\') {
				if ((next = lgetc(quotec)) == EOF)
					return (0);
				if (next == quotec || c == ' ' || c == '\t')
					c = next;
				else if (next == '\n') {
					file->lineno++;
					continue;
				} else
					lungetc(next);
			} else if (c == quotec) {
				*p = '\0';
				break;
			}
			if (p + 1 >= buf + sizeof(buf) - 1) {
				yyerror("string too long");
				return (findeol());
			}
			*p++ = (char)c;
		}
		yylval.v.string = xstrdup(buf);
		return (STRING);
	}

#define allowed_to_end_number(x) \
	(isspace(x) || x == ')' || x ==',' || x == '/' || x == '}' || x == '=')

	if (c == '-' || isdigit(c)) {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(0)) != EOF && isdigit(c));
		lungetc(c);
		if (p == buf + 1 && buf[0] == '-')
			goto nodigits;
		if (c == EOF || allowed_to_end_number(c)) {
			const char *errstr = NULL;

			*p = '\0';
			yylval.v.number = strtonum(buf, LLONG_MIN,
			    LLONG_MAX, &errstr);
			if (errstr) {
				yyerror("\"%s\" invalid number: %s",
				    buf, errstr);
				return (findeol());
			}
			return (NUMBER);
		} else {
nodigits:
			while (p > buf + 1)
				lungetc(*--p);
			c = *--p;
			if (c == '-')
				return (c);
		}
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && x != '<' && x != '>' && \
	x != '!' && x != '=' && x != '#' && x != ','))

	if (isalnum(c) || c == ':' || c == '_' || c == '*' || c == '/') {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(0)) != EOF && (allowed_in_string(c)));
		lungetc(c);
		*p = '\0';
		if ((token = lookup(buf)) == STRING)
			yylval.v.string = xstrdup(buf);
		return (token);
	}
	if (c == '\n') {
		yylval.lineno = file->lineno;
		file->lineno++;
	}
	if (c == EOF)
		return (0);
	return (c);
}

struct file *
pushfile(const char *name)
{
	struct file	*nfile;

	nfile = xcalloc(1, sizeof(struct file));
	nfile->name = xstrdup(name);

	if ((nfile->stream = fopen(nfile->name, "r")) == NULL) {
		free(nfile->name);
		free(nfile);
		return (NULL);
	}
	nfile->lineno = 1;
	TAILQ_INSERT_TAIL(&files, nfile, entry);
	return (nfile);
}

int
popfile(void)
{
	struct file	*prev;

	if ((prev = TAILQ_PREV(file, files, entry)) != NULL) {
		prev->errors += file->errors;
		TAILQ_REMOVE(&files, file, entry);
		fclose(file->stream);
		free(file->name);
		free(file);
		file = prev;
		return (0);
	}
	return (EOF);
}

int
parse_config(const char *filename, struct conf *xconf)
{
	int			 errors = 0;

	conf = xcalloc(1, sizeof(*conf));

	if ((file = pushfile(filename)) == NULL) {
		free(conf);
		return (-1);
	}

	(void)strlcpy(conf->conf_path, filename, sizeof(conf->conf_path));

	conf_init(conf);

	yyparse();
	errors = file->errors;
	file->errors = 0;
	popfile();

	if (errors) {
		conf_clear(conf);
	}
	else {
		struct autogroupwin	*ag;
		struct keybinding	*kb;
		struct winmatch		*wm;
		struct cmd		*cmd;
		struct mousebinding	*mb;
		int			 i;

		conf_clear(xconf);

		xconf->flags = conf->flags;
		xconf->bwidth = conf->bwidth;
		xconf->mamount = conf->mamount;
		xconf->snapdist = conf->snapdist;
		xconf->gap = conf->gap;

		while ((cmd = TAILQ_FIRST(&conf->cmdq)) != NULL) {
			TAILQ_REMOVE(&conf->cmdq, cmd, entry);
			TAILQ_INSERT_TAIL(&xconf->cmdq, cmd, entry);
		}

		while ((kb = TAILQ_FIRST(&conf->keybindingq)) != NULL) {
			TAILQ_REMOVE(&conf->keybindingq, kb, entry);
			TAILQ_INSERT_TAIL(&xconf->keybindingq, kb, entry);
		}

		while ((ag = TAILQ_FIRST(&conf->autogroupq)) != NULL) {
			TAILQ_REMOVE(&conf->autogroupq, ag, entry);
			TAILQ_INSERT_TAIL(&xconf->autogroupq, ag, entry);
		}

		while ((wm = TAILQ_FIRST(&conf->ignoreq)) != NULL) {
			TAILQ_REMOVE(&conf->ignoreq, wm, entry);
			TAILQ_INSERT_TAIL(&xconf->ignoreq, wm, entry);
		}

		while ((mb = TAILQ_FIRST(&conf->mousebindingq)) != NULL) {
			TAILQ_REMOVE(&conf->mousebindingq, mb, entry);
			TAILQ_INSERT_TAIL(&xconf->mousebindingq, mb, entry);
		}

		(void)strlcpy(xconf->termpath, conf->termpath,
		    sizeof(xconf->termpath));
		(void)strlcpy(xconf->lockpath, conf->lockpath,
		    sizeof(xconf->lockpath));

		for (i = 0; i < CWM_COLOR_MAX; i++)
			xconf->color[i].name = conf->color[i].name;

		xconf->font = conf->font;
	}

	free(conf);

	return (errors ? -1 : 0);
}

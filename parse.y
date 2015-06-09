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

#include <sys/types.h>
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
} *file, *topfile;
struct file	*pushfile(const char *);
int		 popfile(void);
int		 yyparse(void);
int		 yylex(void);
int		 yyerror(const char *, ...)
    __attribute__((__format__ (printf, 1, 2)))
    __attribute__((__nonnull__ (1)));
int		 kw_cmp(const void *, const void *);
int		 lookup(char *);
int		 lgetc(int);
int		 lungetc(int);
int		 findeol(void);

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
%token	ACTIVEBORDER INACTIVEBORDER URGENCYBORDER
%token	GROUPBORDER UNGROUPBORDER
%token	MENUBG MENUFG
%token	FONTCOLOR FONTSELCOLOR
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
			if ($2 < 0 || $2 > UINT_MAX) {
				yyerror("invalid borderwidth");
				YYERROR;
			}
			conf->bwidth = $2;
		}
		| MOVEAMOUNT NUMBER {
			if ($2 < 0 || $2 > INT_MAX) {
				yyerror("invalid movemount");
				YYERROR;
			}
			conf->mamount = $2;
		}
		| SNAPDIST NUMBER {
			if ($2 < 0 || $2 > INT_MAX) {
				yyerror("invalid snapdist");
				YYERROR;
			}
			conf->snapdist = $2;
		}
		| COMMAND STRING string		{
			if (!conf_cmd_add(conf, $2, $3)) {
				yyerror("command name/path too long");
				free($2);
				free($3);
				YYERROR;
			}
			free($2);
			free($3);
		}
		| AUTOGROUP NUMBER STRING	{
			if ($2 < 0 || $2 > 9) {
				yyerror("invalid autogroup");
				free($3);
				YYERROR;
			}
			conf_autogroup(conf, $2, NULL, $3);
			free($3);
		}
		| AUTOGROUP NUMBER STRING ',' STRING {
			if ($2 < 0 || $2 > 9) {
				yyerror("invalid autogroup");
				free($3);
				free($5);
				YYERROR;
			}
			conf_autogroup(conf, $2, $3, $5);
			free($3);
			free($5);
		}
		| IGNORE STRING {
			conf_ignore(conf, $2);
			free($2);
		}
		| BIND STRING string		{
			if (!conf_bind_kbd(conf, $2, $3)) {
				yyerror("invalid bind: %s %s", $2, $3);
				free($2);
				free($3);
				YYERROR;
			}
			free($2);
			free($3);
		}
		| GAP NUMBER NUMBER NUMBER NUMBER {
			if ($2 < 0 || $2 > INT_MAX ||
			    $3 < 0 || $3 > INT_MAX ||
			    $4 < 0 || $4 > INT_MAX ||
			    $5 < 0 || $5 > INT_MAX) {
				yyerror("invalid gap");
				YYERROR;
			}
			conf->gap.top = $2;
			conf->gap.bottom = $3;
			conf->gap.left = $4;
			conf->gap.right = $5;
		}
		| MOUSEBIND STRING string	{
			if (!conf_bind_mouse(conf, $2, $3)) {
				yyerror("invalid mousebind: %s %s", $2, $3);
				free($2);
				free($3);
				YYERROR;
			}
			free($2);
			free($3);
		}
		;

color		: COLOR colors
		;

colors		: ACTIVEBORDER STRING {
			free(conf->color[CWM_COLOR_BORDER_ACTIVE]);
			conf->color[CWM_COLOR_BORDER_ACTIVE] = $2;
		}
		| INACTIVEBORDER STRING {
			free(conf->color[CWM_COLOR_BORDER_INACTIVE]);
			conf->color[CWM_COLOR_BORDER_INACTIVE] = $2;
		}
		| URGENCYBORDER STRING {
			free(conf->color[CWM_COLOR_BORDER_URGENCY]);
			conf->color[CWM_COLOR_BORDER_URGENCY] = $2;
		}
		| GROUPBORDER STRING {
			free(conf->color[CWM_COLOR_BORDER_GROUP]);
			conf->color[CWM_COLOR_BORDER_GROUP] = $2;
		}
		| UNGROUPBORDER STRING {
			free(conf->color[CWM_COLOR_BORDER_UNGROUP]);
			conf->color[CWM_COLOR_BORDER_UNGROUP] = $2;
		}
		| MENUBG STRING {
			free(conf->color[CWM_COLOR_MENU_BG]);
			conf->color[CWM_COLOR_MENU_BG] = $2;
		}
		| MENUFG STRING {
			free(conf->color[CWM_COLOR_MENU_FG]);
			conf->color[CWM_COLOR_MENU_FG] = $2;
		}
		| FONTCOLOR STRING {
			free(conf->color[CWM_COLOR_MENU_FONT]);
			conf->color[CWM_COLOR_MENU_FONT] = $2;
		}
		| FONTSELCOLOR STRING {
			free(conf->color[CWM_COLOR_MENU_FONT_SEL]);
			conf->color[CWM_COLOR_MENU_FONT_SEL] = $2;
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
	va_list		 ap;

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
		{ "selfont", 		FONTSELCOLOR},
		{ "snapdist",		SNAPDIST},
		{ "sticky",		STICKY},
		{ "ungroupborder",	UNGROUPBORDER},
		{ "urgencyborder",	URGENCYBORDER},
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
			yyerror("reached end of file while parsing "
			    "quoted string");
			if (file == topfile || popfile() == EOF)
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
		if (file == topfile || popfile() == EOF)
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

	/* skip to either EOF or the first real EOL */
	while (1) {
		if (pushback_index)
			c = pushback_buffer[--pushback_index];
		else
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
			} else if (c == '\0') {
				yyerror("syntax error");
				return (findeol());
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

/* Similar to other parse.y copies, but also allows '/' in strings */
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
		warn("%s", nfile->name);
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

	if ((prev = TAILQ_PREV(file, files, entry)) != NULL)
		prev->errors += file->errors;

	TAILQ_REMOVE(&files, file, entry);
	fclose(file->stream);
	free(file->name);
	free(file);
	file = prev;
	return (file ? 0 : EOF);
}

int
parse_config(const char *filename, struct conf *xconf)
{
	int		 errors = 0;

	conf = xconf;

	if ((file = pushfile(filename)) == NULL) {
		return (-1);
	}
	topfile = file;

	yyparse();
	errors = file->errors;
	popfile();

	return (errors ? -1 : 0);
}

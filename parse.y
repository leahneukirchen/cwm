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

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "headers.h"
#include "calmwm.h"

TAILQ_HEAD(files, file)		 files = TAILQ_HEAD_INITIALIZER(files);
static struct file {
	TAILQ_ENTRY(file)	 entry;
	FILE			*stream;
	char			*name;
	int			 lineno;
	int			 errors;
} *file;
struct file	*pushfile(const char *);
int		 popfile(void);
int		 yyparse(void);
int		 yylex(void);
int		 yyerror(const char *, ...);
int		 kw_cmp(const void *, const void *);
int		 lookup(char *);
int		 lgetc(int);
int		 lungetc(int);
int		 findeol(void);

static struct conf	*conf;

extern char		*shortcut_to_name[];

typedef struct {
	union {
		int64_t			 number;
		char			*string;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	FONTNAME STICKY
%token	AUTOGROUP BIND COMMAND IGNORE
%token	YES NO
%token	ERROR
%token	<v.string>		STRING
%token	<v.number>		NUMBER
%type	<v.number>		yesno
%type	<v.string>		string
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar main '\n'
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
			if (conf->DefaultFontName != NULL &&
			    conf->DefaultFontName != DEFAULTFONTNAME)
				free(conf->DefaultFontName);
			if ((conf->DefaultFontName = xstrdup($2)) == NULL) {
				free($2);
				yyerror("string: asprintf");
				YYERROR;
			}
			free($2);
		}
		| STICKY yesno {
			if ($2 == 0)
				conf->flags &= ~CONF_STICKY_GROUPS;
			else
				conf->flags |= CONF_STICKY_GROUPS;
		}
		| COMMAND STRING string		{
			conf_cmd_add(conf, $3, $2, 0);
			free($2);
			free($3);
		}
		| AUTOGROUP NUMBER STRING	{
			struct autogroupwin *aw;
			char *p;

			if ($2 < 1 || $2 > 9) {
				free($3);
				yyerror("autogroup number out of range: %d", $2);
				YYERROR;
			}

			XCALLOC(aw, struct autogroupwin);

			if ((p = strchr($3, ',')) == NULL) {
				aw->name = NULL;
				aw->class = xstrdup($3);
			} else {
				*(p++) = '\0';
				aw->name = xstrdup($3);
				aw->class = xstrdup(p);
			}
			aw->group = xstrdup(shortcut_to_name[$2]);

			TAILQ_INSERT_TAIL(&conf->autogroupq, aw, entry);

			free($3);
		}
		| IGNORE STRING {
			struct winmatch	*wm;

			XCALLOC(wm, struct winmatch);
			strlcpy(wm->title, $2, sizeof(wm->title));
			wm->opts |= CONF_IGNORECASE;
			TAILQ_INSERT_TAIL(&conf->ignoreq, wm, entry);

			free($2);
		}
		| BIND STRING string		{
			conf_bindname(conf, $2, $3);
			free($2);
			free($3);
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
	va_list          ap;
                         
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
		{ "autogroup",		AUTOGROUP},
		{ "bind",		BIND},
		{ "command",		COMMAND},
		{ "fontname",		FONTNAME},
		{ "ignore",		IGNORE},
		{ "no",			NO},
		{ "sticky",		STICKY},
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
				else if (next == '\n')
					continue;
				else
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
		yylval.v.string = strdup(buf);
		if (yylval.v.string == NULL)
			err(1, "yylex: strdup");
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
	x != '!' && x != '=' && x != '/' && x != '#' && \
	x != ','))

	if (isalnum(c) || c == ':' || c == '_' || c == '*') {
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
			if ((yylval.v.string = strdup(buf)) == NULL)
				err(1, "yylex: strdup");
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

	if ((nfile = calloc(1, sizeof(struct file))) == NULL ||
	    (nfile->name = strdup(name)) == NULL) {
		warn("malloc");
		return (NULL);
	}
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

void
conf_clear(struct conf *c)
{
	struct autogroupwin	*ag, *agnext;
	struct keybinding	*kb, *kbnext;
	struct winmatch		*wm, *wmnext;
	struct cmd		*cmd, *cmdnext;

	for (cmd = TAILQ_FIRST(&c->cmdq); cmd != NULL; cmd = cmdnext) {
		cmdnext = TAILQ_NEXT(cmd, entry);

		TAILQ_REMOVE(&c->cmdq, cmd, entry);
		free(cmd);
	}

	for (kb = TAILQ_FIRST(&c->keybindingq); kb != NULL; kb = kbnext) {
		kbnext = TAILQ_NEXT(kb, entry);

		TAILQ_REMOVE(&c->keybindingq, kb, entry);
		free(kb);
	}

	for (ag = TAILQ_FIRST(&c->autogroupq); ag != NULL; ag = agnext) {
		agnext = TAILQ_NEXT(ag, entry);

		TAILQ_REMOVE(&c->autogroupq, ag, entry);
		free(ag->class);
		if (ag->name)
			free(ag->name);
		free(ag->group);
		free(ag);
	}

	for (wm = TAILQ_FIRST(&c->ignoreq); wm != NULL; wm = wmnext) {
		wmnext = TAILQ_NEXT(wm, entry);

		TAILQ_REMOVE(&c->ignoreq, wm, entry);
		free(wm);
	}

	if (c->DefaultFontName != NULL &&
	    c->DefaultFontName != DEFAULTFONTNAME)
		free(c->DefaultFontName);
}


int
parse_config(const char *filename, struct conf *xconf)
{
	int			 errors = 0;

	XCALLOC(conf, struct conf);

	if ((file = pushfile(filename)) == NULL) {
		free(conf);
		return (-1);
	}

	strlcpy(conf->conf_path, filename, sizeof(conf->conf_path));

	conf_init(conf);

	yyparse();
	errors = file->errors;
	file->errors = 0;
	popfile();

	if (errors) {
		conf_clear(conf);
	}
	else {
		struct autogroupwin	*ag, *agnext;
		struct keybinding	*kb, *kbnext;
		struct winmatch		*wm, *wmnext;
		struct cmd		*cmd, *cmdnext;

		conf_clear(xconf);

		xconf->flags = conf->flags;

		for (cmd = TAILQ_FIRST(&conf->cmdq); cmd != NULL; cmd = cmdnext) {
			cmdnext = TAILQ_NEXT(cmd, entry);

			TAILQ_REMOVE(&conf->cmdq, cmd, entry);
			TAILQ_INSERT_TAIL(&xconf->cmdq, cmd, entry);
		}

		for (kb = TAILQ_FIRST(&conf->keybindingq); kb != NULL; kb = kbnext) {
			kbnext = TAILQ_NEXT(kb, entry);

			TAILQ_REMOVE(&conf->keybindingq, kb, entry);
			TAILQ_INSERT_TAIL(&xconf->keybindingq, kb, entry);
		}

		for (ag = TAILQ_FIRST(&conf->autogroupq); ag != NULL; ag = agnext) {
			agnext = TAILQ_NEXT(ag, entry);

			TAILQ_REMOVE(&conf->autogroupq, ag, entry);
			TAILQ_INSERT_TAIL(&xconf->autogroupq, ag, entry);
		}

		for (wm = TAILQ_FIRST(&conf->ignoreq); wm != NULL; wm = wmnext) {
			wmnext = TAILQ_NEXT(wm, entry);

			TAILQ_REMOVE(&conf->ignoreq, wm, entry);
			TAILQ_INSERT_TAIL(&xconf->ignoreq, wm, entry);
		}

		strlcpy(xconf->termpath, conf->termpath, sizeof(xconf->termpath));
		strlcpy(xconf->lockpath, conf->lockpath, sizeof(xconf->lockpath));

		xconf->DefaultFontName = conf->DefaultFontName;
	}

	free(conf);

	return (errors ? -1 : 0);
}

# cwm makefile for BSD make and GNU make
# uses pkg-config, DESTDIR and PREFIX

PROG=		cwm

PREFIX=         /usr/local

SRCS=		calmwm.c screen.c xmalloc.c client.c menu.c \
		search.c util.c xutil.c conf.c xevents.c group.c \
		kbfunc.c mousefunc.c font.c parse.y

OBJS=		calmwm.o screen.o xmalloc.o client.o menu.o \
		search.o util.o xutil.o conf.o xevents.o group.o \
		kbfunc.o mousefunc.o font.o strlcpy.o strlcat.o y.tab.o \
		strtonum.o fgetln.o

CPPFLAGS+=	`pkg-config --cflags xft`

CFLAGS=		-Wall -O2 -g

LDFLAGS+=	`pkg-config --libs xft xinerama xrandr`

MANPREFIX=	${PREFIX}/share/man

all: ${PROG}

clean:
	rm -rf ${OBJS} ${PROG} y.tab.c

y.tab.c: parse.y
	yacc parse.y

${PROG}: ${OBJS} y.tab.o
	${CC} ${OBJS} ${LDFLAGS} -o ${PROG}

.c.o:
	${CC} -c ${CFLAGS} ${CPPFLAGS} $<

install: ${PROG}
	install -d ${DESTDIR}${PREFIX}/bin ${DESTDIR}${MANPREFIX}/man1 ${DESTDIR}${MANPREFIX}/man5
	install -m 755 cwm ${DESTDIR}${PREFIX}/bin
	install -m 644 cwm.1 ${DESTDIR}${MANPREFIX}/man1
	install -m 644 cwmrc.5 ${DESTDIR}${MANPREFIX}/man5

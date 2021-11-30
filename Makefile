# cwm makefile for BSD make and GNU make
# uses pkg-config, DESTDIR and PREFIX

PROG=		cwm

PREFIX?=	/usr/local

SRCS=		calmwm.c screen.c xmalloc.c client.c menu.c \
		search.c util.c xutil.c conf.c xevents.c group.c \
		kbfunc.c parse.y

OBJS=		calmwm.o screen.o xmalloc.o client.o menu.o \
		search.o util.o xutil.o conf.o xevents.o group.o \
		kbfunc.o strlcpy.o strlcat.o y.tab.o \
		strtonum.o reallocarray.o
		
PKG_CONFIG?=	pkg-config

CPPFLAGS+=	`${PKG_CONFIG} --cflags x11 xft xrandr`

CFLAGS?=	-Wall -O2 -g -D_GNU_SOURCE

LDFLAGS+=	`${PKG_CONFIG} --libs x11 xft xrandr`

MANPREFIX?=	${PREFIX}/share/man

YACC=           $(if $(shell command -v yacc >/dev/null),yacc,bison --yacc)

all: ${PROG}

clean:
	rm -f ${OBJS} ${PROG} y.tab.c

y.tab.c: parse.y
	$(YACC) parse.y

${PROG}: ${OBJS} y.tab.o
	${CC} ${OBJS} ${LDFLAGS} -o ${PROG}

.c.o:
	${CC} -c ${CFLAGS} ${CPPFLAGS} $<

install: ${PROG}
	install -d ${DESTDIR}${PREFIX}/bin ${DESTDIR}${MANPREFIX}/man1 ${DESTDIR}${MANPREFIX}/man5
	install -m 755 cwm ${DESTDIR}${PREFIX}/bin
	install -m 644 cwm.1 ${DESTDIR}${MANPREFIX}/man1
	install -m 644 cwmrc.5 ${DESTDIR}${MANPREFIX}/man5

release:
	VERSION=$$(git describe --tags | sed 's/^v//;s/-[^.]*$$//') && \
	git archive --prefix=cwm-$$VERSION/ -o cwm-$$VERSION.tar.gz HEAD

sign:
	VERSION=$$(git describe --tags | sed 's/^v//;s/-[^.]*$$//') && \
	gpg2 --armor --detach-sign cwm-$$VERSION.tar.gz && \
	signify -S -s ~/.signify/cwm.sec -m cwm-$$VERSION.tar.gz && \
	sed -i '1cuntrusted comment: verify with cwm.pub' cwm-$$VERSION.tar.gz.sig

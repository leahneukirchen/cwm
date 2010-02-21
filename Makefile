# $OpenBSD$

#.include <bsd.xconf.mk>

PROG=		cwm

BINDIR=		/usr/bin

SRCS=		calmwm.c screen.c xmalloc.c client.c menu.c \
		search.c util.c xutil.c conf.c input.c xevents.c group.c \
		kbfunc.c mousefunc.c font.c parse.y

OBJS=		calmwm.o screen.o xmalloc.o client.o menu.o \
		search.o util.o xutil.o conf.o input.o xevents.o group.o \
		kbfunc.o mousefunc.o font.o strlcpy.o strlcat.o y.tab.o \
		strtonum.o fgetln.o

X11BASE=	/usr

CPPFLAGS+=	-I${X11BASE}/include -I${X11BASE}/include/freetype2 -I.

CFLAGS+=	-Wall

LDADD+=		-L${X11BASE}/lib -lXft -lXrender -lX11 -lXau -lXdmcp \
		-lfontconfig -lexpat -lfreetype -lz -lXinerama -lXrandr -lXext

MANDIR=		${X11BASE}/man/cat
MAN=		cwm.1 cwmrc.5

CLEANFILES=	cwm.cat1 cwmrc.cat5


all: $(PROG)

clean:
	rm -rf $(OBJS) $(PROG) y.tab.c

y.tab.c: parse.y
	byacc parse.y


$(PROG): $(OBJS) y.tab.o
	$(CC) $(OBJS) ${LDADD} -o ${PROG}

$(OBJS): %.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $<

install: ${PROG}
	install -m 755 cwm /usr/local/bin/
	install -m 644 cwm.1 /usr/local/man/man1
	install -m 644 cwmrc.5 /usr/local/man/man5

#.include <bsd.prog.mk>
#.include <bsd.xorg.mk>

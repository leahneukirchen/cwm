# $OpenBSD$

.include <bsd.own.mk>

X11BASE?=	/usr/X11R6

PROG=		cwm

SRCS=		calmwm.c screen.c xmalloc.c client.c grab.c search.c \
		util.c xutil.c conf.c input.c xevents.c group.c \
		kbfunc.c cursor.c font.c

CPPFLAGS+=	-I${X11BASE}/include -I${X11BASE}/include/freetype2

LDADD+=		-L${X11BASE}/lib -lXft -lXrender -lX11 -lXau -lXdmcp \
		-lfontconfig -lexpat -lfreetype -lz -lX11 -lXau -lXdmcp -lXext

MANDIR=		${X11BASE}/man/cat

CLEANFILES=	cwm.cat1

obj: _xenocara_obj

.include <bsd.prog.mk>
.include <bsd.xorg.mk>

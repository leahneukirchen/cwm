include $(top_srcdir)/Makefile.am.inc

CC = gcc

man_MANS = cwm.1

bin_PROGRAMS = cwm

cwm_DEPENDENCIES = @ERRO@ @LIBOBJS@
cwm_SOURCES = calmwm.c draw.c screen.c xmalloc.c client.c grab.c \
	      search.c util.c  xutil.c conf.c input.c xevents.c \
	      group.c geographic.c kbfunc.c cursor.c font.c
cwm_LDADD = @ERRO@ @LIBOBJS@ $(XFT_LIBS)

AM_CFLAGS = -Wall -Icompat $(XFT_CFLAGS)

EXTRA_DIST = LICENSE README cwm.1 strlcpy.c err.c Makefile.am.inc	\
             compat/sys/queue.h compat/sys/tree.h hash.h calmwm.h	\
             headers.h strlcat.c

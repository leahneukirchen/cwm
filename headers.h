/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2004 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id$
 */

#ifndef _CALMWM_HEADERS_H_
#define _CALMWM_HEADERS_H_

#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>
#include <X11/Xlib.h>  
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/Intrinsic.h>
#include <X11/Xos.h>
#include <X11/Xft/Xft.h>

#ifdef USE_XOSD
#include <xosd.h>
#endif /* USE_XOSD */

#include <err.h>

#endif /* _CALMWM_HEADERS_H_ */

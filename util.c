/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2004 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id$
 */

#include "headers.h"
#include "calmwm.h"

#define MAXARGLEN 20

int
u_spawn(char *argstr)
{
	char *args[MAXARGLEN], **ap;
	char **end = &args[MAXARGLEN - 1];

	switch (fork()) {
	case 0:
		ap = args;
		while (ap < end && (*ap = strsep(&argstr, " \t")) != NULL)
			ap++;

		*ap = NULL;
		setsid();
		execvp(args[0], args);
		err(1, args[0]);
		break;
	case -1:
		warn("fork");
		return (-1);
	default:
		break;
	}

	return (0);
}

int dirent_exists(char *filename) {
       struct stat buffer;

       return stat(filename, &buffer);
}

int dirent_isdir(char *filename) {
       struct stat buffer;
       int return_value;

       return_value = stat(filename, &buffer);

       if(return_value == -1)
               return 0;
       else
               return S_ISDIR(buffer.st_mode);
}

int dirent_islink(char *filename) {
       struct stat buffer;
       int return_value;

       return_value = lstat(filename, &buffer);

       if(return_value == -1)
               return 0;
       else
               return S_ISLNK(buffer.st_mode);
}



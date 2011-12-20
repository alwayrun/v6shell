/*-
 * Copyright (c) 2005
 *	Jeffrey Allen Neitzel <jneitzel (at) sdf1 (dot) org>.
 *	All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JEFFREY ALLEN NEITZEL ``AS IS'', AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL JEFFREY ALLEN NEITZEL BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	@(#)pexec.c	1.2 (jneitzel) 2005/09/30
 */
/*
 *	Derived from:
 *		$NetBSD: execvp.c,v 1.24 2003/08/07 16:42:47 agc Exp $
 *		- /usr/src/lib/libc/gen/execvp.c
 */
/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)exec.c	8.1 (Berkeley) 6/4/93
 */

#ifndef	lint
#include "version.h"
OSH_SOURCEID("pexec.c	1.2 (jneitzel) 2005/09/30");
#endif	/* !lint */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pexec.h"

#ifdef	PATH_MAX
#define	PATHMAX		PATH_MAX
#else
#define	PATHMAX		1024
#endif

extern char **environ;

int
pexec(const char *file, char *const *argv)
{
	const char **esh_argv;
	size_t dlen, flen;
	int cnt, eacces = 0;
	const char *d, *esh, *f, *pp, *up;
	char path[PATHMAX];

	/*
	 * Unset errno, and fail if the value of file is invalid.
	 * Note that errno is set to the appropriate value below.
	 */
	errno = 0;
	if (file == NULL || *file == '\0')
		goto fail;
	flen = strlen(file);

	/*
	 * If the name of the specified file contains one or more
	 * `/' characters, it is used as the path name to execute.
	 */
	for (f = file; *f != '\0'; f++)
		if (*f == '/') {
			pp = file, up = "";
			goto got_path;
		}
	pp = path;

	/*
	 * Get the user's PATH.  Fail if PATH is unset or is
	 * set to the empty string, as no PATH search shall be
	 * performed in such a case.
	 */
	up = getenv("PATH");
	if (up == NULL || *up == '\0')
		goto fail;

	do {
		/* Find the end of this PATH element. */
		for (d = up; *up != ':' && *up != '\0'; up++)
			;	/* nothing */
		/*
		 * Since this is a shell PATH, double, leading, and/or
		 * trailing colons indicate the current directory.
		 */
		if (d == up) {
			d = ".";
			dlen = 1;
		} else
			dlen = up - d;

		/*
		 * Complain if this path name for file would be too long.
		 * Otherwise, use this PATH element to build a possible
		 * path name for file.  Then, attempt to execve(2) it.
		 */
		if (dlen + flen + 1 >= sizeof(path)) {
			write(STDERR_FILENO, "pexec: ", (size_t)7);
			write(STDERR_FILENO, d, dlen);
			write(STDERR_FILENO, ": path too long\n", (size_t)16);
			continue;
		}
		memcpy(path, d, dlen);
		path[dlen] = '/';
		memcpy(path + dlen + 1, file, flen);
		path[dlen + flen + 1] = '\0';

got_path:
		execve(pp, argv, environ);
		switch (errno) {
		case EACCES:
#if defined(__OpenBSD__)
		case EISDIR:	/* Treat it as an EACCES error. */
#endif
			eacces = 1;
			/* FALLTHROUGH */
		case ELOOP:
		case ENAMETOOLONG:
		case ENOENT:
		case ENOTDIR:
			break;
		case ENOEXEC:
			/*
			 * Get the user's EXECSHELL.
			 * Fail if its value is unusable.
			 */
			esh = getenv("EXECSHELL");
			if (esh==NULL||*esh=='\0'||strlen(esh)>=sizeof(path))
				goto fail;
			for (cnt = 0; argv[cnt] != NULL; cnt++)
				;	/* nothing */
			/*
			 * NOTE: Using malloc(3) here is OK given the context
			 *	 in which this function is expected to be used
			 *	 (see: fd2.c, if.c, osh-exec.c).
			 */
			esh_argv = malloc((cnt + 2) * sizeof(char *));
			if (esh_argv == NULL)
				goto fail;
			esh_argv[0] = esh;
			esh_argv[1] = pp;
			memcpy(&esh_argv[2], &argv[1], cnt * sizeof(char *));
			execve(esh, (char *const *)esh_argv, environ);
			errno = ENOEXEC;
			/* FALLTHROUGH */
		default:
			goto fail;
		}
	} while (*up++ == ':');		/* Otherwise, *up was NUL. */
	if (eacces)
		errno = EACCES;

fail:
	if (errno == 0)
		errno = ENOENT;
	return -1;
}

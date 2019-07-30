/* pm_script.c
 * - playlist module for external scripts (á la IceS 2.0)
 * - based on playlist_script.c from IceS 2.0 by
 * Copyright (c) 2001 Michael Smith <msmith@labyrinth.net.au>
 *
 * Copyright (C) 2005 Ville Koskinen <ville.koskinen@iki.fi>
 * Copyright (c) 2009 Steve Blinch <centova.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "definitions.h"
#include <stdio.h>
#include <stdlib.h>

#define STR_BUFFER 1024

static char *playlist_metadata = NULL;
static int playlist_track_timelimit = 0;
static char *cmd = NULL;

extern ices_config_t ices_config;

/* Private function declarations */
static char* playlist_script_get_next(void);
static void playlist_script_shutdown(void);
static char* playlist_script_get_metadata(void);
static int playlist_script_get_timelimit(void);
static FILE *ices_pm_script_popen(char *cmd, char *mode);
static char **brk_string(register char *str, int *store_argc);

/* Global function definitions */

/* Initialize the script playlist handler */
int ices_playlist_script_initialize(playlist_module_t* pm) {
	char *bindir = NULL;

	ices_log_debug("Initializing script playlist handler...");

	if (!pm->module) {
		ices_log_error("No playlist script defined");
		return 0;
	}
	
	cmd = pm->module;
	/* make path relative to ices binary dir */
	if (cmd[0] != '/' && !(cmd[0] == '.' && (cmd[1] == '/' || (cmd[1] == '.' && cmd[2] == '/')))) {
	    bindir = ices_util_get_bindir();
	    ices_log_debug("bindir is %s",bindir);
	    if (bindir) {
			cmd = malloc(strlen(pm->module) + strlen(bindir) + 2);
			if (cmd)
				sprintf(cmd, "%s/%s", bindir, pm->module);
	    		ices_log_debug("cmd is %s",cmd);
		}
	} else {
		cmd = strdup(pm->module);
   		ices_log_debug("cmd is unmodified as %s",cmd);
	}

	if (!cmd) {
		ices_log_error_output("Could not allocate memory for playlist path");
		return 0;
	}

	pm->get_next = playlist_script_get_next;
	pm->get_metadata = playlist_script_get_metadata;
	pm->get_timelimit = playlist_script_get_timelimit;
	pm->get_lineno = NULL;
	pm->shutdown = playlist_script_shutdown;

	return 1;
}

static void strclean(char *str) {
	if (str == NULL) return;
	/* Remove linefeeds etc. */
	int i = 0;
	while (str[i]) {
		if (str[i] == '\r' || str[i] == '\n') {
			str[i] = '\0';
			break;
		}
		i++;
	}

}

static char *playlist_script_get_next(void) {
	char *filename = NULL, *metadata = NULL, *timelimit = NULL, *temp = NULL;
	FILE *pipe;
	
	filename = malloc(STR_BUFFER);
	metadata = malloc(STR_BUFFER);
	timelimit = malloc(STR_BUFFER);

	pipe = ices_pm_script_popen(cmd, "r");

	if (!pipe) {
		ices_log_error_output("Couldn't open pipe to program \"%s\"", cmd);
		return NULL;
	}

	if (fgets(filename, STR_BUFFER, pipe) == NULL) {
		temp = malloc(STR_BUFFER);
		ices_log_error_output("Couldn't read filename from pipe to program \"%s\": %s", cmd, ices_util_strerror(errno, temp, STR_BUFFER));
		free(temp);
		free(filename); filename = NULL;
		free(metadata); metadata = NULL;
		free(timelimit); timelimit = NULL;
		fclose(pipe);
		return NULL;
	}

	if (fgets(metadata, STR_BUFFER, pipe) == NULL) {
		/* This is non-fatal. */
		temp = malloc(STR_BUFFER);
		ices_log_debug("No metadata received from pipe to program \"%s\": %s", cmd, ices_util_strerror(errno, temp, STR_BUFFER));
		free(temp);
		free(metadata); metadata = NULL;
	}

	if (fgets(timelimit, STR_BUFFER, pipe) == NULL) {
		/* This is non-fatal, and in fact can be totally ignored. */
		free(timelimit); timelimit = NULL;
	}

	fclose(pipe);

	if (filename[0] == '\n' || (filename[0] == '\r' && filename[1] == '\n')) {
		ices_log_error_output("Got newlines instead of filename from program \"%s\"", cmd);
		free(filename); filename = NULL;
		if (metadata) free(metadata); metadata = NULL;
		if (timelimit) free(timelimit); timelimit = NULL;
		return NULL;
	}
	
	strclean(filename);
	strclean(metadata);
	strclean(timelimit);
	
	/* require absolute paths, or relative paths starting with ./, to ensure that
	 * we don't end up interpreting garbage output (error messages, etc.) as filenames */
	if (filename[0] != '/' && !(filename[0] == '.' && filename[1] == '/')) {
		ices_log_error_output("Playlist script returned something other than a filename; output was:");
		ices_log_error_output(filename);
		free(filename); filename = NULL;

		if (metadata) {
			if (strlen(metadata)) ices_log_error_output(metadata);
			free(metadata); metadata = NULL;
		}
		if (timelimit) {
			if (strlen(timelimit)) ices_log_error_output(timelimit);
			free(timelimit); timelimit = NULL;
		}
		
		return NULL;
	}
	
	if (playlist_metadata) free(playlist_metadata);
	
	if (metadata)
		playlist_metadata = metadata;
	else
		playlist_metadata = NULL;

	playlist_track_timelimit = 0;
	if (timelimit) {
		playlist_track_timelimit = atoi(timelimit);
		free(timelimit);
	}

	ices_log_debug("Script playlist handler serving: %s [%s] (%i)", ices_util_nullcheck(filename), ices_util_nullcheck(playlist_metadata), playlist_track_timelimit);

	return filename;
}

/* Return the file metadata. */
static char*playlist_script_get_metadata(void) {
	if (playlist_metadata)
		return playlist_metadata;
	return NULL;
}

static int playlist_script_get_timelimit(void) {
	return playlist_track_timelimit;
}

/* Shutdown the script playlist handler */
static void playlist_script_shutdown(void) {
	if (cmd)
		free(cmd);
	if (playlist_metadata)
		free(playlist_metadata);
}

static FILE *ices_pm_script_popen(char *cmd, char *mode) {
	pid_t pid;

	#define	PARENT_READ		readpipe[0]
	#define	CHILD_WRITE		readpipe[1]
	#define CHILD_READ		writepipe[0]
	#define PARENT_WRITE	writepipe[1]	
	
	int readpipe[2], writepipe[2];
	
//	ices_log_debug("executing: %s",cmd);
	
	/* setup our pipes */
	if (pipe(readpipe)) {
		ices_log_debug("pipe(readpipe) failed");
		return NULL;
	}
	if (pipe(writepipe)) {
		ices_log_debug("pipe(writepipe) failed");
		return NULL;
	}
	
	/* fork */
	if ( (pid = fork()) == -1) {
		ices_log_debug("unable to fork");
		return NULL;
		
	} else if (pid == 0) {
		char **execargv;
		int execargc;
		
		execargv = brk_string(cmd,&execargc);
		//ices_log_debug("Our argv[0] is: %s",execargv[0]);

		/* child process */
		/* close(ccpipe[1]); */
		
		/* close parent ends of the pipes */
		close(PARENT_WRITE);
		close(PARENT_READ);
		
		/* duplicate and close our descriptors */
		dup2(CHILD_READ,0);
		dup2(CHILD_WRITE,1);
		dup2(CHILD_WRITE,2);
		
		close(CHILD_READ);
		close(CHILD_WRITE);
		
		/* create a new session */
		//setsid();

		execv(execargv[0],execargv);
		
		ices_log_debug("execv failed");
		return NULL;
	}
	
	/* close the child's ends of te pipes */
	close(CHILD_READ);
	close(CHILD_WRITE);
	
	int rpipe;
	
	if (*mode=='r') {
		close(PARENT_WRITE);
		rpipe = PARENT_READ;
	} else if (*mode=='w') {
		close(PARENT_READ);
		rpipe = PARENT_WRITE;
	} else {
		close(PARENT_WRITE);
		close(PARENT_READ);
		
		return NULL;
	}
	
	return fdopen(rpipe,mode);
}

/*-
 * Credits for brk_string() function:
 *
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 */
/*-
 * brk_string --
 *	Fracture a string into an array of words (as delineated by tabs or
 *	spaces) taking quotation marks into account.  Leading tabs/spaces
 *	are ignored.
 *
 * returns --
 *	Pointer to the array of pointers to the words.  To make life easier,
 *	the first word is always the value of the .MAKE variable.
 */
static char **brk_string(register char *str, int *store_argc) {
	static int argmax, curlen;
	static char **argv, *buf;
	register int argc, ch;
	register char inquote, *p, *start, *t;
	int len;

	/* save off pmake variable */
	if (!argv) {
		argv = (char **)malloc((argmax = 50) * sizeof(char *));
	}

	/* skip leading space chars. */
	for (; *str == ' ' || *str == '\t'; ++str);

	/* allocate room for a copy of the string */
	if ((len = strlen(str) + 1) > curlen)
		buf = malloc(curlen = len);

	/*
	 * copy the string; at the same time, parse backslashes,
	 * quotes and build the argument list.
	 */
	argc = 0;
	inquote = '\0';
	for (p = str, start = t = buf;; ++p) {
		switch(ch = *p) {
		case '"':
		case '\'':
			if (inquote)
				if (inquote == ch)
					inquote = '\0';
				else
					break;
			else
				inquote = ch;
			continue;
		case ' ':
		case '\t':
			if (inquote)
				break;
			if (!start)
				continue;
			/* FALLTHROUGH */
		case '\n':
		case '\0':
			/*
			 * end of a token -- make sure there's enough argv
			 * space and save off a pointer.
			 */
			*t++ = '\0';
			if (argc == argmax) {
				argmax *= 2;		/* ramp up fast */
				argv = (char **)realloc(argv,argmax * sizeof(char *));
			}
			argv[argc++] = start;
			start = (char *)NULL;
			if (ch == '\n' || ch == '\0')
				goto done;
			continue;
		case '\\':
			switch (ch = *++p) {
			case '\0':
			case '\n':
				/* hmmm; fix it up as best we can */
				ch = '\\';
				--p;
				break;
			case 'b':
				ch = '\b';
				break;
			case 'f':
				ch = '\f';
				break;
			case 'n':
				ch = '\n';
				break;
			case 'r':
				ch = '\r';
				break;
			case 't':
				ch = '\t';
				break;
			}
			break;
		}
		if (!start)
			start = t;
		*t++ = ch;
	}
done:	argv[argc] = (char *)NULL;
	*store_argc = argc;
	return(argv);
}
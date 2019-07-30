/* playlist_perl.c
 * - Interpreter functions for perl
 * Copyright (c) 2000 Chad Armstrong, Alexander Haväng
 * Copyright (c) 2001-2 Brendan Cully
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

/* Stupid automake and STUPID perl */
#ifdef PACKAGE
#undef PACKAGE
#endif

#include <EXTERN.h>
#include <perl.h>

extern ices_config_t ices_config;

static PerlInterpreter *my_perl = NULL;

static const char* pl_init_hook;
static const char* pl_shutdown_hook;
static const char* pl_get_next_hook;
static const char* pl_get_metadata_hook;
static const char* pl_get_lineno_hook;

/* This is needed for dynamicly loading perl modules in the perl scripts.
 * E.g "use somemodule;"
 */
extern void boot_DynaLoader();

/* -- local prototypes -- */
static int playlist_perl_get_lineno(void);
static char* playlist_perl_get_next(void);
static char* playlist_perl_get_metadata(void);
static int playlist_perl_reload(void);
static void playlist_perl_shutdown(void);

static int pl_perl_init_perl(void);
static void pl_perl_shutdown_perl(void);
static char* pl_perl_eval(const char* func);
static const char* pl_find_func(const char*);

static void xs_init(void);

int ices_playlist_perl_initialize(playlist_module_t* pm) {
	char *str;
	int ret = -1;

	pm->get_next = playlist_perl_get_next;
	pm->get_metadata = playlist_perl_get_metadata;
	pm->get_lineno = playlist_perl_get_lineno;
	pm->shutdown = playlist_perl_shutdown;
	pm->reload = playlist_perl_reload;

	if (pl_perl_init_perl() < 0)
		return -1;

	if (!pl_init_hook)
		return 1;

	if ((str = pl_perl_eval(pl_init_hook))) {
		ret = atoi(str);
		ices_util_free(str);
	}

	if (ret <= 0)
		ices_log_error("Execution of 'ices_init' failed");

	return ret;
}

static int playlist_perl_get_lineno(void) {
	char *str;
	int ret = 0;

	if (pl_get_lineno_hook && (str = pl_perl_eval(pl_get_lineno_hook))) {
		ret = atoi(str); /* allocated in perl.c */
		ices_util_free(str);
	}

	return ret;
}

static char *playlist_perl_get_next(void) {
	if (pl_get_next_hook)
		return pl_perl_eval(pl_get_next_hook);

	return NULL;
}

static char*playlist_perl_get_metadata(void) {
	if (pl_get_metadata_hook)
		return pl_perl_eval(pl_get_metadata_hook);

	return NULL;
}

static int playlist_perl_reload(void) {
	playlist_perl_shutdown();

	return pl_perl_init_perl();
}

static void playlist_perl_shutdown(void) {
	if (pl_shutdown_hook)
		pl_perl_eval(pl_shutdown_hook);

	pl_perl_shutdown_perl();

	return;
}

static void xs_init(void) {
	char *file = __FILE__;

	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

/* most of the following is almost ripped straight out of 'man perlcall'
 * or 'man perlembed'
 *
 * shutdown() will clean up anything allocated at this point
 */

static int pl_perl_init_perl(void) {
	static char *my_argv[4] = { "", "-I" ICES_MODULEDIR, "-e", NULL };
	static char module_space[255];

	if ((my_perl = perl_alloc()) == NULL) {
		ices_log_debug("perl_alloc() error: (no memory!)");
		return -1;
	}

	snprintf(module_space, sizeof(module_space), "use %s",
		 ices_config.pm.module);
	my_argv[3] = module_space;

	perl_construct(my_perl);

	ices_log_debug("Importing perl module: %s", my_argv[3] + 4);

	if (perl_parse(my_perl, xs_init, 4, my_argv, NULL)) {
		ices_log_debug("perl_parse() error: parse problem");
		return -1;
	}

	if (!(pl_init_hook = pl_find_func("ices_init")))
		pl_init_hook = pl_find_func("ices_perl_initialize");
	if (!(pl_shutdown_hook = pl_find_func("ices_shutdown")))
		pl_shutdown_hook = pl_find_func("ices_perl_shutdown");
	if (!(pl_get_next_hook = pl_find_func("ices_get_next")))
		pl_get_next_hook = pl_find_func("ices_perl_get_next");
	if (!(pl_get_metadata_hook = pl_find_func("ices_get_metadata")))
		pl_get_metadata_hook = pl_find_func("ices_perl_get_metadata");
	if (!(pl_get_lineno_hook = pl_find_func("ices_get_lineno")))
		pl_get_lineno_hook = pl_find_func("ices_perl_get_current_lineno");

	if (!pl_get_next_hook) {
		ices_log_error("The playlist module must define at least the ices_get_next method");
		return -1;
	}

	return 0;
}

/* cleanup time! */
static void pl_perl_shutdown_perl(void) {
	if (my_perl != NULL) {
		perl_destruct(my_perl);
		perl_free(my_perl);
		my_perl = NULL;
	}
}


/*
 *  Here be magic...
 *  man perlcall gave me the following steps, to be
 *  able to handle getting return values from the
 *  embedded perl calls.
 */
static char*pl_perl_eval(const char *functionname) {
	int retcount = 0;
	char *retstr = NULL;

	dSP;                            /* initialize stack pointer      */

	ices_log_debug("Interpreting [%s]", functionname);

	ENTER;                          /* everything created after here */
	SAVETMPS;                       /* ...is a temporary variable.   */
	PUSHMARK(SP);                   /* remember the stack pointer    */
	PUTBACK;                        /* make local stack pointer global */

	/* G_SCALAR: get a scalar return | G_EVAL: Trap errors */
	retcount = perl_call_pv(functionname, G_SCALAR | G_EVAL);

	SPAGAIN;                        /* refresh stack pointer         */

	/* Check for errors in execution */
	if (SvTRUE(ERRSV)) {
		STRLEN n_a;
		ices_log_debug("perl error: %s", SvPV(ERRSV, n_a));
		(void) POPs;
	} else if (retcount) {
		/* we're calling strdup here, free() this later! */
		retstr = ices_util_strdup(POPp); /* pop the return value from stack */
		ices_log_debug("perl [%s] returned %d values, last [%s]", functionname, retcount, retstr);
	} else
		ices_log_debug("Perl call returned nothing");

	PUTBACK;
	FREETMPS;                       /* free that return value        */
	LEAVE;                          /* ...and the XPUSHed "mortal" args.*/

	ices_log_debug("Done interpreting [%s]", functionname);

	return retstr;
}

static const char*pl_find_func(const char* func) {
	if (hv_exists(PL_defstash, func, strlen(func))) {
		ices_log_debug("Found method: %s", func);
		return func;
	}

	return NULL;
}

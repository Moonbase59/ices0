/* signals.c
 * - Functions for signal handling in ices
 * Copyright (c) 2000 Alexander Haväng
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

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif

/* Private function declarations */
static RETSIGTYPE signals_child(const int sig);
static RETSIGTYPE signals_int(const int sig);
static RETSIGTYPE signals_hup(const int sig);
static RETSIGTYPE signals_usr1(const int sig);

/* Global function definitions */

/* Setup signal handlers for some signals that we don't want
 * delivered to icecast, and some that we want to handle in
 * a certain way, like SIGINT to cleanup and exit, and hup
 * to close and reopen logfiles */
void ices_signals_setup(void) {
#ifndef _WIN32
	struct sigaction sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);
	sigaction(SIGIO, &sa, NULL);
	sigaction(SIGALRM, &sa, NULL);

	sa.sa_handler = signals_int;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

#ifdef SA_RESTART
	sa.sa_flags = SA_RESTART;
#endif
	sa.sa_handler = signals_child;
	sigaction(SIGCHLD, &sa, NULL);

	sa.sa_handler = signals_hup;
	sigaction(SIGHUP, &sa, NULL);

	sa.sa_handler = signals_usr1;
	sigaction(SIGUSR1, &sa, NULL);
}

/* Guess we fork()ed, let's take care of the dead process */
static RETSIGTYPE signals_child(const int sig) {
	int stat;
	while (waitpid (WAIT_ANY, &stat, WNOHANG) <= 0);
}

/* SIGINT, ok, let's be nice and just drop dead */
static RETSIGTYPE signals_int(const int sig) {
	ices_log_debug("Caught signal, shutting down...");
	ices_setup_shutdown();
}

/* SIGHUP caught, let's cycle logfiles and try to reload the playlist module */
static RETSIGTYPE signals_hup(const int sig) {
	ices_log_debug("Caught SIGHUP, cycling logfiles and reloading playlist...");
	ices_log_reopen_logfile();
	ices_playlist_reload();
}

/* I'm not sure whether I'll keep this... */
static RETSIGTYPE signals_usr1(const int sig) {
	ices_log_debug("Caught SIGUSR1, skipping to next track...");
	ices_stream_next();
#endif
}


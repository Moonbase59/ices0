/* playlist.c
 * - Functions for playlist handling
 * Copyright (c) 2000 Alexander Haväng
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
#include "playlist.h"

extern ices_config_t ices_config;
static int playlist_init = 0;

/* Global function definitions */

/* Wrapper function for the specific playlist handler's current line number.
 * This might not be available if your playlist is a database or something
 * weird, but it's just for the cue file so it doesn't matter much */
int ices_playlist_get_current_lineno(void) {
	if (ices_config.pm.get_lineno)
		return ices_config.pm.get_lineno();

	return 0;
}

/* Wrapper for the playlist handler's next file function.
 * Remember that if this returns non-NULL then the return
 * value is free()ed by the caller. */
char *ices_playlist_get_next(void) {
	return ices_config.pm.get_next();
}

/* Allows a script to override file metadata if it wants. Returns NULL
 *   to mean 'do it yourself'. Modules need not implement this. */
char*ices_playlist_get_metadata(void) {
	if (ices_config.pm.get_metadata)
		return ices_config.pm.get_metadata();

	return NULL;
}

/* Allows a script to set a maximum time limit for the track. Returns 0
 *   to mean 'no limit'. Modules need not implement this. */
int ices_playlist_get_timelimit(void) {
	if (ices_config.pm.get_timelimit)
		return ices_config.pm.get_timelimit();

	return 0;
}

/* Initialize the toplevel playlist handler */
int ices_playlist_initialize(void) {
	int rc = -1;

	ices_log_debug("Initializing playlist handler...");

	ices_config.pm.reload = NULL;
	ices_config.pm.get_timelimit = NULL;

	switch (ices_config.pm.playlist_type) {
	case ices_playlist_builtin_e:
		rc = ices_playlist_builtin_initialize(&ices_config.pm);
		break;
	case ices_playlist_script_e:
		rc = ices_playlist_script_initialize(&ices_config.pm);
		break;
	case ices_playlist_python_e:
#ifdef HAVE_LIBPYTHON
		rc = ices_playlist_python_initialize(&ices_config.pm);
#else
		ices_log_error("This binary has no support for embedded python");
#endif
		break;
	case ices_playlist_perl_e:
#ifdef HAVE_LIBPERL
		rc = ices_playlist_perl_initialize(&ices_config.pm);
#else
		ices_log_error("This binary has no support for embedded perl");
#endif
		break;
	default:
		ices_log_error("Unknown playlist module!");
		break;
	}

	if (rc < 0) {
		ices_log("Initialization of playlist handler failed. [%s]", ices_log_get_error());
		ices_setup_shutdown();
	}

	playlist_init = 1;
	return rc;
}

/* Reload the playlist module */
int ices_playlist_reload(void) {
	if (ices_config.pm.reload)
		return ices_config.pm.reload();

	return 0;
}

/* Shutdown the playlist handler */
void ices_playlist_shutdown(void) {
	if (playlist_init)
		ices_config.pm.shutdown();
}

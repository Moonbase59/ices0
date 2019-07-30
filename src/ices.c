/* ices.c
 * - Main Program *
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

ices_config_t ices_config;

/* Global function definitions */

/* Ices starts here. But ends in ices_setup_shutdown(),
 * which calls exit(0) and that's the last you see of it. */
int main(int argc, char **argv) {
	/* Make command line arguments available through
	 * ices_util_get_argc() and argv */
	ices_util_set_args(argc, argv);

	/* Setup all options, and initialize all submodules */
	ices_setup_initialize();

	/* Connect to server and keep streaming all the good stuff */
	ices_stream_loop(&ices_config);

	/* Not reached */
	return 0;
}

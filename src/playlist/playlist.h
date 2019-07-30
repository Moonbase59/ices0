/* playlist.h
 * - playlist function declarations for ices
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

#ifndef PLAYLIST_H
#define PLAYLIST_H 1

/* Public function declarations */
int ices_playlist_get_current_lineno(void);
char *ices_playlist_get_next(void);
char* ices_playlist_get_metadata(void);
int ices_playlist_get_timelimit(void);
int ices_playlist_initialize(void);
int ices_playlist_reload(void);
void ices_playlist_shutdown(void);

int ices_playlist_builtin_initialize(playlist_module_t* pm);
int ices_playlist_script_initialize(playlist_module_t* pm);
#ifdef HAVE_LIBPYTHON
int ices_playlist_python_initialize(playlist_module_t* pm);
#endif
#ifdef HAVE_LIBPERL
int ices_playlist_perl_initialize(playlist_module_t* pm);
#endif

#endif

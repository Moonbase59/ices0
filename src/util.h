/* util.h
 * - utility function declarations for ices
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

/* Public function declarations */
char **ices_util_get_argv(void);
int ices_util_get_argc(void);
char *ices_util_get_bindir(void);
char *ices_util_strdup(const char *string);
void ices_util_set_args(int argc, char **argv);
int ices_util_open_for_reading(const char *file);
int ices_util_valid_fd(int fd);
FILE *ices_util_fopen_for_writing(const char *file);
FILE *ices_util_fopen_for_reading(const char *file);
void ices_util_fclose(FILE *fp);
char *ices_util_read_line(FILE *fp);
char *ices_util_get_random_filename(char *namespace, char *type);
int ices_util_remove(const char *filename);
int ices_util_get_random(void);
int ices_util_is_regular_file(int fd);
int ices_util_directory_create(const char *filename);
int ices_util_directory_exists(const char *filename);
const char *ices_util_nullcheck(const char *string);
double ices_util_percent(int this, int of_that);
char *ices_util_file_time(unsigned int bitrate, unsigned int filesize,
			  char *namespace);
const char *ices_util_strerror(int error, char *namespace, int maxsize);
void ices_util_free(void *ptr);
int ices_util_verify_file(const char *filename);

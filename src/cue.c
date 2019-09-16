/* cue.c
 * - Functions for cue file in ices
 * Copyright (c) 2000 Alexander Hav�ng
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
#include "metadata.h"

extern ices_config_t ices_config;

char *ices_cue_filename = NULL;
static int ices_cue_lineno = 0;

/* Syntax of cue file
 * filename
 * size
 * bitrate
 * minutes:seconds total
 * % played
 * current line in playlist
 * artist
 * songname
 */

/* Global function definitions */

/* Update the cue file with a set of variables */
void ices_cue_update(input_stream_t* source) {
	if (ices_config.cuefile) {
		char buf[1024];
		char artist[1024];
		char title[1024];
		FILE *fp = ices_util_fopen_for_writing(ices_cue_get_filename());

		if (!fp) {
			ices_log("Could not open cuefile [%s] for writing, cuefile not updated!", ices_cue_get_filename());
			return;
		}

		artist[0] = '\0';
		title[0] = '\0';
		ices_metadata_get(artist, sizeof(artist), title, sizeof(title));

		fprintf(fp, "%s\n%d\n%d\n%s\n%f\n%d\n%s\n%s\n", source->path,
			(int) source->filesize, source->bitrate,
			ices_util_file_time(source->bitrate, source->filesize, buf),
			ices_util_percent(source->bytes_read, source->filesize),
			ices_cue_lineno, artist, title);

		ices_util_fclose(fp);
	}
}

/* Cleanup the cue module by removing the cue file */
void ices_cue_shutdown(void) {
	const char *filename = ices_cue_get_filename();

	if (filename && filename[0])
		remove(filename);
}

void ices_cue_set_lineno(int lineno) {
	ices_cue_lineno = lineno;
}

/* Mutator for the cue filename */
void ices_cue_set_filename(const char *filename) {
	ices_cue_filename = ices_util_strdup(filename);
}

/* Return the current cue filename, and create it if
 * necessary */
const char *ices_cue_get_filename(void) {
	static char buf[1024];

	if (ices_cue_filename)
		return ices_cue_filename;

	if (!ices_config.base_directory) {
		ices_log_error("Base directory is invalid");
		return NULL;
	}

	snprintf(buf, sizeof(buf), "%s/ices.cue", ices_config.base_directory);

	return buf;
}




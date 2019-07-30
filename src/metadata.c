/* metadata.c
 * Copyright (c) 2001-3 Brendan Cully <brendan@xiph.org>
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

#define INITDELAY 500000

extern ices_config_t ices_config;

static char* Artist = NULL;
static char* Title = NULL;
static char* Filename = NULL;

/* Private function declarations */
static char* metadata_clean_filename(const char* path, char* buf,
				     size_t len);
static void metadata_update(int delay);

/* Global function definitions */

void ices_metadata_get(char* artist, size_t alen, char* title, size_t tlen) {
	if (Artist)
		snprintf(artist, alen, "%s", Artist);
	if (Title)
		snprintf(title, tlen, "%s", Title);
}

void ices_metadata_set(const char* artist, const char* title) {
	ices_util_free(Artist);
	Artist = NULL;
	ices_util_free(Title);
	Title = NULL;

	if (artist && *artist)
		Artist = ices_util_strdup(artist);
	if (title && *title)
		Title = ices_util_strdup(title);
}

void ices_metadata_set_file(const char* filename) {
	char buf[1024];

	ices_util_free(Filename);
	Filename = NULL;

	if (filename && *filename) {
		metadata_clean_filename(filename, buf, sizeof(buf));
		Filename = ices_util_strdup(buf);
	}
}

/* Update metadata on server via fork.
 * Note that the very first metadata update after connection is delayed,
 * because if we try to update our new info to the server and the server has
 * not yet accepted us as a source, the information is lost. */
void ices_metadata_update(int delay) {
	pid_t child;

	if (delay)
		ices_log_debug("Delaying metadata update...");

	if ((child = fork()) == 0) {
		metadata_update(delay ? INITDELAY : 0);
		_exit(0);
	}

	if (child == -1)
		ices_log_debug("Metadata update failed: fork");
}

static void metadata_update(int delay) {
	ices_stream_t* stream;
	shout_metadata_t* metadata;
	char song[1024];
	char* playlist_metadata;
	char* value;
	int rc;

	if (delay)
		usleep(delay);

	if (!(playlist_metadata = ices_playlist_get_metadata())) {
		if (Title) {
			if (Artist)
				snprintf(song, sizeof(song), "%s - %s", Artist, Title);
			else
				snprintf(song, sizeof(song), "%s", Title);
		} else
			snprintf(song, sizeof(song), "%s", Filename);

		value = song;
	} else
		value = playlist_metadata;

	if (!(metadata = shout_metadata_new())) {
		ices_log_error("Error allocating metadata structure");
		ices_util_free(playlist_metadata);
		return;
	}

	if (shout_metadata_add(metadata, "song", value) != SHOUTERR_SUCCESS) {
		ices_log_error_output("Error adding info to metadata structure");
		ices_util_free(playlist_metadata);
		shout_metadata_free(metadata);
		return;
	}

	for (stream = ices_config.streams; stream; stream = stream->next) {
		rc = shout_set_metadata(stream->conn, metadata);

		if (rc != SHOUTERR_SUCCESS)
			ices_log_error_output("Updating metadata on %s failed.", stream->mount);
		else
			ices_log_debug("Updated metadata on %s to: %s", stream->mount, value);
	}

	ices_util_free(playlist_metadata);
	shout_metadata_free(metadata);
}

/* Cleanup a filename so it looks more like a song name */
static char *metadata_clean_filename(const char* path, char *buf, size_t len) {
	char *ptr = NULL;

	if (!path || !buf) {
		ices_log("ERROR: Polluted strings sent to filename cleanup.");
		return NULL;
	}

	/* Find last slash */
	ptr = strrchr(path, '/');

	if (ptr && strlen(ptr) > 0)
		strncpy(buf, ptr + 1, len);
	else
		strncpy(buf, path, len);

	if ((ptr = strrchr(buf, '.')))
		*ptr = '\0';

	ices_log_debug("Filename cleaned up from [%s] to [%s]", path, buf);
	return buf;
}

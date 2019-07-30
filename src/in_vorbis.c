/* in_vorbis.c
 * Plugin to read vorbis files as PCM
 *
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

#include "in_vorbis.h"
#include "metadata.h"

#include <string.h>

#include <vorbis/vorbisfile.h>

#define SAMPLESIZE 2
#ifdef WORDS_BIGENDIAN
# define ICES_OV_BE 1
#else
# define ICES_OV_BE 0
#endif

/* -- data structures -- */
typedef struct {
	OggVorbis_File* vf;
	vorbis_info* info;
	int link;
	int16_t buf[2048];
	size_t samples;
	int offset;
} ices_vorbis_in_t;

/* -- static prototypes -- */
static int ices_vorbis_readpcm(input_stream_t* self, size_t len,
			       int16_t* left, int16_t* right);
static int ices_vorbis_close(input_stream_t* self);
static void in_vorbis_parse(input_stream_t* self);
static void in_vorbis_set_metadata(ices_vorbis_in_t* vorbis_data);

/* try to open a vorbis file for decoding. Returns:
 *   0: success
 *   1: not a vorbis file
 *  -1: error opening
 */
int ices_vorbis_open(input_stream_t* self, char* buf, size_t len) {
	ices_vorbis_in_t* vorbis_data;
	OggVorbis_File* vf;
	FILE* fin;
	char errbuf[128];
	int rc;

	if (!(fin = fdopen(self->fd, "rb"))) {
		ices_util_strerror(errno, errbuf, sizeof(errbuf));
		ices_log_error("Error opening %s: %s", self->path, errbuf);

		return -1;
	}

	if (!(vf = (OggVorbis_File*) malloc(sizeof(OggVorbis_File)))) {
		ices_log_error("Malloc failed in ices_vorbis_open");
		return -1;
	}

	if ((rc = ov_open(fin, vf, buf, len)) != 0) {
		free(vf);
		fclose(fin);

		if (rc == OV_ENOTVORBIS)
			return 1;

		if (rc == OV_EREAD)
			ices_log_error("Read error opening vorbis file");
		else if (rc == OV_EVERSION)
			ices_log_error("Vorbis version mismatch");
		else if (rc == OV_EBADHEADER)
			ices_log_error("Invalid vorbis header");
		else
			ices_log_error("Error in ov_open: %d", rc);

		return -1;
	}

	if (!(vorbis_data = (ices_vorbis_in_t*) malloc(sizeof(ices_vorbis_in_t)))) {
		ices_log_error("Malloc failed in ices_vorbis_open");
		ov_clear(vf);
		free(vf);
		return -1;
	}

	if (!(vorbis_data->info = ov_info(vf, -1))) {
		ices_log_error("Vorbis: error reading vorbis info");
		ices_vorbis_close(self);

		return -1;
	}

	if (vorbis_data->info->channels < 1) {
		ices_log_error("Vorbis: Cannot decode, %d channels of audio data!",
			       vorbis_data->info->channels);
		ices_vorbis_close(self);

		return -1;
	}

	vorbis_data->vf = vf;
	vorbis_data->samples = 0;
	vorbis_data->link = -1;

	self->type = ICES_INPUT_VORBIS;
	self->data = vorbis_data;

	self->read = NULL;
	self->readpcm = ices_vorbis_readpcm;
	self->close = ices_vorbis_close;

	in_vorbis_parse(self);

	return 0;
}

static int ices_vorbis_readpcm(input_stream_t* self, size_t olen, int16_t* left,
			       int16_t* right) {
	ices_vorbis_in_t* vorbis_data = (ices_vorbis_in_t*) self->data;
	int link;
	int len;
	int i;

	/* refill buffer if necessary */
	if (!vorbis_data->samples) {
		vorbis_data->offset = 0;
		do {
			if ((len = ov_read(vorbis_data->vf, (char*) vorbis_data->buf,
					   sizeof(vorbis_data->buf), ICES_OV_BE, SAMPLESIZE, 1, &link)) <= 0) {
				if (len == OV_HOLE)
					ices_log_error("Skipping bad vorbis data");
				else
					return len;
			}
		} while (len <= 0);

		if (vorbis_data->link == -1)
			vorbis_data->link = link;
		else if (vorbis_data->link != link) {
			vorbis_data->link = link;
			ices_log_debug("New Ogg link found in bitstream");
			in_vorbis_parse(self);
			ices_reencode_reset(self);
			ices_metadata_update(0);
		}

		vorbis_data->samples = len / SAMPLESIZE;
		if (vorbis_data->info->channels > 1)
			vorbis_data->samples /= vorbis_data->info->channels;
		self->bytes_read = ov_raw_tell(vorbis_data->vf);
	}

	len = 0;
	while (vorbis_data->samples && olen) {
		if (vorbis_data->info->channels == 1) {
			*left = *right = vorbis_data->buf[vorbis_data->offset++];
			left++;
			right++;
		} else {
			*left++ = vorbis_data->buf[vorbis_data->offset++];
			*right++ = vorbis_data->buf[vorbis_data->offset++];
		}
		for (i = 0; i < vorbis_data->info->channels - 2; i++)
			vorbis_data->offset++;
		vorbis_data->samples--;
		olen -= SAMPLESIZE;
		len++;
	}

	return len;
}

static int ices_vorbis_close(input_stream_t* self) {
	ices_vorbis_in_t* vorbis_data = (ices_vorbis_in_t*) self->data;

	ov_clear(vorbis_data->vf);
	free(vorbis_data->vf);
	free(vorbis_data);

	return 0;
}

static void in_vorbis_parse(input_stream_t* self) {
	ices_vorbis_in_t* vorbis_data = (ices_vorbis_in_t*) self->data;

	vorbis_data->info = ov_info(vorbis_data->vf, vorbis_data->link);
	self->bitrate = vorbis_data->info->bitrate_nominal / 1000;
	if (!self->bitrate)
		self->bitrate = ov_bitrate(vorbis_data->vf, vorbis_data->link) / 1000;
	self->samplerate = (unsigned int) vorbis_data->info->rate;
	self->channels = vorbis_data->info->channels;

	ices_log_debug("Ogg vorbis file found, version %d, %d kbps, %d channels, %ld Hz",
		       vorbis_data->info->version, self->bitrate, vorbis_data->info->channels,
		       self->samplerate);
	in_vorbis_set_metadata(vorbis_data);
}

static void in_vorbis_set_metadata(ices_vorbis_in_t* vorbis_data) {
	vorbis_comment* comment;
	char* key;
	char* artist = NULL;
	char* title = NULL;
	int i;

	if (!(comment = ov_comment(vorbis_data->vf, -1)))
		return;

	for (i = 0; i < comment->comments; i++) {
		key = comment->user_comments[i];
		ices_log_debug("Vorbis comment found: %s", key);
		if (!strncasecmp("artist", key, 6))
			artist = key + 7;
		else if (!strncasecmp("title", key, 5))
			title = key + 6;
		else if (!strncasecmp("replaygain_track_gain", key, 21))
			rg_set_track_gain(atof(key + 22));
	}

	ices_metadata_set(artist, title);
}

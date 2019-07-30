/* reencode.c
 * - Functions for reencoding in ices
 * Copyright (c) 2000 Alexander Haväng
 * Copyright (c) 2001-3 Brendan Cully
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

#ifdef HAVE_LAME_LAME_H
# include <lame/lame.h>
#else
# include <lame.h>
#endif

extern ices_config_t ices_config;

/* Global function definitions */

/* Initialize the reencoding engine in ices, initialize
 * the liblame structures and be happy */
void ices_reencode_initialize(void) {
	ices_stream_t* stream;

	/* are any streams reencoding? */
	for (stream = ices_config.streams; stream; stream = stream->next)
		if (stream->reencode) {
			ices_config.reencode = 1;
			break;
		}

	if (!ices_config.reencode)
		return;

	ices_log_debug("Using LAME version %s", get_lame_version());
}

/* For each song, reset the liblame engine, otherwise it craps out if
 * the bitrate or sample rate changes */
void ices_reencode_reset(input_stream_t* source) {
	ices_stream_t* stream;
	lame_global_flags* lame;
	static int init_decoder = 1;

#ifdef HAVE_LAME_DECODE_EXIT
	if (!init_decoder) {
		if (lame_decode_exit() < 0) {
			ices_log("LAME: error shutting down decoder");
			ices_setup_shutdown();
		}
		init_decoder = 1;
	}
#endif

	if (init_decoder) {
		if (lame_decode_init() < 0) {
			ices_log("LAME: error initialising decoder");
			ices_setup_shutdown();
		}
		init_decoder = 0;
	}

	for (stream = ices_config.streams; stream; stream = stream->next) {
		if (!stream->reencode)
			continue;

		/* only reset encoder if audio format changes */
		lame = (lame_global_flags*) stream->encoder_state;
		if (lame) {
			if (lame_get_in_samplerate(lame) == source->samplerate)
				continue;

			lame_close(lame);
		}

		if (!(stream->encoder_state = lame_init())) {
			ices_log("LAME: error resetting encoder.");
			ices_setup_shutdown();
		}

		lame = (lame_global_flags*) stream->encoder_state;

		lame_set_in_samplerate(lame, source->samplerate);
		/* Lame won't reencode mono to stereo for some reason, so we have to
		 * duplicate left into right by hand. */
		if (source->channels == 1 && stream->out_numchannels == 1)
			lame_set_num_channels(lame, source->channels);

		lame_set_brate(lame, stream->bitrate);
		if (stream->out_numchannels == 1)
			lame_set_mode(lame, MONO);
		if (stream->out_samplerate > 0)
			lame_set_out_samplerate(lame, stream->out_samplerate);
		lame_set_original(lame, 0);

		/* lame_init_params isn't more specific about the problem */
		if (lame_init_params(lame) < 0) {
			ices_log("LAME: error resetting sample rate.");
			lame_close(lame);
			ices_setup_shutdown();
		}

		/* adjust default stream sample rate to LAME default for lazy reencoding tests */
		if (stream->out_samplerate <= 0)
			stream->out_samplerate = lame_get_out_samplerate(lame);
	}
}

/* If initialized, shutdown the reencoding engine */
void ices_reencode_shutdown(void) {
	ices_stream_t* stream;

	for (stream = ices_config.streams; stream; stream = stream->next)
		if (stream->encoder_state) {
			lame_close((lame_global_flags*) stream->encoder_state);
			stream->encoder_state = NULL;
		}
}

/* decode buffer, of length buflen, into left and right. Stream-independent
 * (do this once per chunk, not per stream). Result is number of samples
 * for ices_reencode_reencode_chunk. */
int ices_reencode_decode(unsigned char* buf, size_t blen, size_t olen,
			 int16_t* left, int16_t* right) {
	return lame_decode(buf, blen, left, right);
}

/* reencode buff, of len buflen, put max outlen reencoded bytes in outbuf */
int ices_reencode(ices_stream_t* stream, int nsamples, int16_t* left,
		  int16_t* right, unsigned char *outbuf, int outlen) {
	lame_global_flags* lame = (lame_global_flags*) stream->encoder_state;

	return lame_encode_buffer(lame, left, right, nsamples, outbuf, outlen);
}

/* At EOF of each file, flush the liblame buffers and get some extra candy */
int ices_reencode_flush(ices_stream_t* stream, unsigned char *outbuf,
			int maxlen) {
	lame_global_flags* lame = (lame_global_flags*) stream->encoder_state;
	int rc;

	rc = lame_encode_flush_nogap(lame, outbuf, maxlen);

	return rc;
}

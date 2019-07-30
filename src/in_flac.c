/* in_flac.c
 * Plugin to read FLAC files as PCM
 *
 * Copyright (c) 2004 Brendan Cully <brendan@xiph.org>
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
 * $Id: in_flac.c 7433 2004-07-31 17:49:18Z brendan $
 */

#include "config.h"
#include "in_flac.h"
#include "metadata.h"

#include <FLAC/stream_decoder.h>

/* -- data structures -- */
typedef struct {
        FLAC__StreamDecoder* decoder;
        unsigned char parsed;
        /* read buffer */
        char *buf;
        size_t len;
        /* write buffer */
        int16_t *left;
        int16_t *right;
        size_t olen;
} flac_in_t;

/* -- static prototypes -- */
static int ices_flac_readpcm (input_stream_t* self, size_t len,
                              int16_t* left, int16_t* right);
static int ices_flac_close (input_stream_t* self);

/* -- FLAC callbacks -- */
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
# define FLAC_BUFSIZE_TYPE unsigned
#else
# define FLAC_BUFSIZE_TYPE size_t
#endif /* !FLAC_API_VERSION_CURRENT || FLAC_API_VERSION_CURRENT <= 7 */
static FLAC__StreamDecoderReadStatus
flac_read_cb(const FLAC__StreamDecoder* decoder, FLAC__byte buffer[],
             FLAC_BUFSIZE_TYPE* bytes, void* client_data);
static FLAC__StreamDecoderWriteStatus
flac_write_cb(const FLAC__StreamDecoder* decoder, const FLAC__Frame* frame,
              const FLAC__int32* const buffer[], void* client_data);
static void
flac_metadata_cb(const FLAC__StreamDecoder* decoder,
                 const FLAC__StreamMetadata* metadata, void* client_data);
static void
flac_error_cb(const FLAC__StreamDecoder* decoder,
              FLAC__StreamDecoderErrorStatus status, void* client_data);

static inline int16_t scale16(int sample, int bps);

/* try to open a FLAC file for decoding. Returns:
 *   0: success
 *   1: not a FLAC file
 *  -1: error opening
 */
int
ices_flac_open (input_stream_t* self, char* buf, size_t len)
{
        flac_in_t* flac_data;
        FLAC__StreamDecoder* decoder;

        if (!(decoder = FLAC__stream_decoder_new())) {
                ices_log_error("ices_flac_open: Error allocating FLAC decoder");
                return -1;
        }

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
        FLAC__stream_decoder_set_read_callback(decoder, flac_read_cb);
        FLAC__stream_decoder_set_write_callback(decoder, flac_write_cb);
        FLAC__stream_decoder_set_metadata_callback(decoder, flac_metadata_cb);
        FLAC__stream_decoder_set_error_callback(decoder, flac_error_cb);
        FLAC__stream_decoder_set_client_data(decoder, self);

        switch (FLAC__stream_decoder_init(decoder)) {
        case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
                break;
        case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
                ices_log_error("Could not allocate memory during FLAC decoder init");
                goto errDecoder;
        default:
                ices_log_error("Unexpected error during FLAC decoder init");
                goto errDecoder;
        }
#else
        if (FLAC__stream_decoder_init_stream(decoder, flac_read_cb, NULL, NULL, NULL,
                                             NULL, flac_write_cb, flac_metadata_cb,
                                             flac_error_cb, self)
            != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
                ices_log_error("ices_flac_open: Error initializing FLAC decoder");
                goto errDecoder;
        }
#endif /* !FLAC_API_VERSION_CURRENT || FLAC_API_VERSION_CURRENT <= 7 */

        FLAC__stream_decoder_set_metadata_respond(decoder, FLAC__METADATA_TYPE_VORBIS_COMMENT);

        if (!(flac_data = (flac_in_t*)malloc (sizeof (flac_in_t)))) {
                ices_log_error ("Malloc failed in ices_flac_open");
                goto errDecoder;
        }

        flac_data->decoder = decoder;
        flac_data->parsed = 0;
        flac_data->buf = buf;
        flac_data->len = len;

        self->data = flac_data;

        if (!FLAC__stream_decoder_process_single(decoder)) {
                ices_log_error("Could not find FLAC metadata header");
                free(flac_data);
                FLAC__stream_decoder_delete(decoder);
                return 1;
        }
        if (!flac_data->parsed) {
                ices_log_error("Could not find FLAC metadata header in prebuffer");
                free(flac_data);
                FLAC__stream_decoder_delete(decoder);
                return 1;
        }

        self->type = ICES_INPUT_FLAC;

        self->read = NULL;
        self->readpcm = ices_flac_readpcm;
        self->close = ices_flac_close;

        if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder)) {
                ices_log_error("Error seeking past FLAC metadata");
                goto errData;
        }

        return 0;

errData:
        free(flac_data);
errDecoder:
        FLAC__stream_decoder_delete(decoder);

        return -1;
}

static int
ices_flac_readpcm (input_stream_t* self, size_t olen, int16_t* left,
                   int16_t* right)
{
        flac_in_t* flac_data = (flac_in_t*)self->data;

        flac_data->left = left;
        flac_data->right = right;
        flac_data->olen = olen;
        if (!FLAC__stream_decoder_process_single(flac_data->decoder)) {
                switch (FLAC__stream_decoder_get_state(flac_data->decoder)) {
                case FLAC__STREAM_DECODER_END_OF_STREAM:
                        return 0;
                default:
                        ices_log_error("Error reading FLAC stream");
                }
        }
        if (FLAC__stream_decoder_get_state(flac_data->decoder) == FLAC__STREAM_DECODER_END_OF_STREAM)
                return 0;

        return flac_data->olen;
}

static int
ices_flac_close (input_stream_t* self)
{
        flac_in_t* flac_data = (flac_in_t*) self->data;

        FLAC__stream_decoder_finish(flac_data->decoder);
        FLAC__stream_decoder_delete(flac_data->decoder);
        free (flac_data);

        return close(self->fd);
}

/* -- callbacks -- */
static FLAC__StreamDecoderReadStatus
flac_read_cb(const FLAC__StreamDecoder* decoder, FLAC__byte buffer[],
             FLAC_BUFSIZE_TYPE* bytes, void* client_data)
{
        input_stream_t* self = (input_stream_t*)client_data;
        flac_in_t* flac_data = (flac_in_t*)self->data;
        ssize_t len;
        char errbuf[128];

        if (!flac_data->len) {
                if (!flac_data->parsed) {
                        *bytes = 0;
                        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
                }
                if ((len = read(self->fd, buffer, *bytes)) > 0)
                        return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
                if (!len)
                        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
                ices_log_error("Error reading FLAC stream: %s", ices_util_strerror(errno, errbuf, sizeof(errbuf)));
                return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
        }

        if (flac_data->len < *bytes)
                *bytes = flac_data->len;
        memcpy(buffer, flac_data->buf, *bytes);
        flac_data->len -= *bytes;

        return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderWriteStatus
flac_write_cb(const FLAC__StreamDecoder* decoder, const FLAC__Frame* frame,
              const FLAC__int32* const buffer[], void* client_data)
{
        input_stream_t* self = (input_stream_t*)client_data;
        flac_in_t* flac_data = (flac_in_t*)self->data;
        int i;

        for (i = 0; i < frame->header.blocksize; i++) {
                flac_data->left[i] = scale16(buffer[0][i], frame->header.bits_per_sample);
                if (self->channels > 1) {
                        flac_data->right[i] = scale16(buffer[1][i], frame->header.bits_per_sample);
                }
        }
        if (self->channels == 1)
                memcpy(flac_data->right, flac_data->left, frame->header.blocksize << 1);

        flac_data->olen = frame->header.blocksize;

        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void
flac_metadata_cb(const FLAC__StreamDecoder* decoder,
                 const FLAC__StreamMetadata* metadata, void* client_data)
{
        input_stream_t* self = (input_stream_t*)client_data;
        flac_in_t* flac_data = (flac_in_t*)self->data;
        FLAC__StreamMetadata_VorbisComment_Entry* comment;
        char *artist = NULL;
        char *title = NULL;
        int i;

        switch(metadata->type) {
        case FLAC__METADATA_TYPE_STREAMINFO:
                self->samplerate = metadata->data.stream_info.sample_rate;
                self->channels = metadata->data.stream_info.channels;
                flac_data->parsed = 1;
                ices_log_debug("Found FLAC file, %d Hz, %d channels, %d bits", self->samplerate, self->channels, metadata->data.stream_info.bits_per_sample);
                break;
        case FLAC__METADATA_TYPE_VORBIS_COMMENT:
                for (i = 0; i < metadata->data.vorbis_comment.num_comments; i++) {
                        comment = metadata->data.vorbis_comment.comments + i;

                        // if (comment->length >= 6 && !strncasecmp("artist", comment->entry, 6)) {
                        //  if ((artist = malloc(comment->length - 6)))
                        //    snprintf(artist, comment->length - 6, "%s", comment->entry + 7);
                        // } else if (comment->length >= 5 && !strncasecmp("title", comment->entry, 5))
                        //  if ((title = malloc(comment->length - 5)))
                        //    snprintf(title, comment->length - 5, "%s", comment->entry + 6);

                        // better check, there might be something like "artistsort" and "titlesort"
                        if (comment->length >= 7 && !strncasecmp("artist=", (const char *) comment->entry, 7)) {
                                if ((artist = malloc(comment->length - 6)))
                                        snprintf(artist, comment->length - 6, "%s", comment->entry + 7);
                        } else if (comment->length >= 6 && !strncasecmp("title=", (const char *) comment->entry, 6)) {
                                if ((title = malloc(comment->length - 5)))
                                        snprintf(title, comment->length - 5, "%s", comment->entry + 6);
                        } else if (comment->length >= 21 && !strncasecmp("replaygain_track_gain", (const char *) comment->entry, 21)) {
                                rg_set_track_gain(atof((const char *) comment->entry + 22));
                        }
                }

                ices_metadata_set(artist, title);
                if (artist)
                        free(artist);
                if (title)
                        free(title);
                break;

        default:
                ices_log_debug("Ignoring unknown FLAC metadata type");
        }
}

static void
flac_error_cb(const FLAC__StreamDecoder* decoder,
              FLAC__StreamDecoderErrorStatus status, void* client_data)
{
        switch (status) {
        case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
                ices_log_error("FLAC lost sync");
                break;
        case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
                ices_log_error("Bad header in FLAC stream");
                break;
        case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
                ices_log_error("Bad CRC for FLAC frame");
                break;
        default:
                ices_log_error("Unspecified error decoding FLAC stream");
        }
}

/* -- utility -- */
static inline int16_t scale16(int sample, int bps) {
        if (bps == 16)
                return sample;
        if (bps < 16)
                return sample << (16 - bps);
        return sample >> (bps - 16);
}

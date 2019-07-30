/* id3.c
 * - Functions for id3 tags in ices
 * Copyright (c) 2000 Alexander Hav�ng
 * Copyright (c) 2001-3 Brendan Cully <brendan@icecast.org>
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

/* Local definitions */
typedef struct {
	unsigned char major_version;
	unsigned char minor_version;
	unsigned char flags;
	size_t len;

	char* artist;
	char* title;

  unsigned int pos;
  double gain;
} id3v2_tag;

typedef struct {
	size_t frame_len;
	const char* artist_tag;
	const char* title_tag;
	const char* txxx_tag;
} id3v2_version_info;

static id3v2_version_info vi[5] = {
  {  6, "TP1", "TT2", "TXX" },
  {  6, "TP1", "TT2", "TXX" },
  {  6, "TP1", "TT2", "TXX" },
  { 10, "TPE1", "TIT2", "TXXX" },
  { 10, "TPE1", "TIT2", "TXXX" }
};

#define ID3V2_FLAG_UNSYNC (1 << 7)
#define ID3V2_FLAG_EXTHDR (1 << 6)
#define ID3V2_FLAG_EXPHDR (1 << 5)
#define ID3V2_FLAG_FOOTER (1 << 4)

#define ID3V2_FRAME_LEN(tagp) (vi[(tagp)->major_version].frame_len)
#define ID3V2_ARTIST_TAG(tagp) (vi[(tagp)->major_version].artist_tag)
#define ID3V2_TITLE_TAG(tagp) (vi[(tagp)->major_version].title_tag)
#define ID3V2_TXXX_TAG(tagp) (vi[(tagp)->major_version].txxx_tag)

/* Private function declarations */
static int id3v2_read_exthdr(input_stream_t* source, id3v2_tag* tag);
ssize_t id3v2_read_frame(input_stream_t* source, id3v2_tag* tag);
static int id3v2_skip_data(input_stream_t* source, id3v2_tag* tag, size_t len);
static int id3v2_decode_synchsafe(unsigned char* synchsafe);
static int id3v2_decode_synchsafe3(unsigned char* synchsafe);
static int id3v2_decode_unsafe(unsigned char* in);
static int id3v2_code_to_utf8(char *buffer, const uint32_t code);
static int id3v2_convert_utf16(char *str, int len, char *out);
static int id3v2_convert_iso8859_1(char *str, int len, char *out);

/* Global function definitions */

void ices_id3v1_parse(input_stream_t* source) {
	off_t pos;
	char buffer[1024];
	char title_utf8[30*4+1];
	char artist_utf8[30*4+1];
	char title[31];
	int i, decodedlen;

	if (!source->filesize)
		return;

	buffer[30] = '\0';
	title[30] = '\0';
	pos = lseek(source->fd, 0, SEEK_CUR);

	lseek(source->fd, -128, SEEK_END);

	if ((read(source->fd, buffer, 3) == 3) && !strncmp(buffer, "TAG", 3)) {
		/* Don't stream the tag */
		source->filesize -= 128;

		if (read(source->fd, title, 30) != 30) {
			ices_log("Error reading ID3v1 song title: %s",
				 ices_util_strerror(errno, buffer, sizeof(buffer)));
			goto out;
		}

		for (i = strlen(title) - 1; i >= 0 && title[i] == ' '; i--)
			title[i] = '\0';
		// assume title is in ISO-8859-1, convert to UTF-8
		decodedlen = id3v2_convert_iso8859_1(title, 30, title_utf8);
		title_utf8[decodedlen] = '\0';
		ices_log_debug("ID3v1: Title: %s", title_utf8);

		if (read(source->fd, buffer, 30) != 30) {
			ices_log("Error reading ID3v1 artist: %s",
				 ices_util_strerror(errno, buffer, sizeof(buffer)));
			goto out;
		}

		for (i = strlen(buffer) - 1; i >= 0 && buffer[i] == ' '; i--)
			buffer[i] = '\0';
		// assume artist is in ISO-8859-1, convert to UTF-8
		decodedlen = id3v2_convert_iso8859_1(buffer, 30, artist_utf8);
		artist_utf8[decodedlen] = '\0';
		ices_log_debug("ID3v1: Artist: %s", artist_utf8);

		ices_metadata_set(artist_utf8, title_utf8);
	}

 out:
	lseek(source->fd, pos, SEEK_SET);
}

void ices_id3v2_parse(input_stream_t* source) {
	unsigned char buf[1024];
	id3v2_tag tag;
	size_t remaining;
	ssize_t rv;

	if (source->read(source, buf, 10) != 10) {
		ices_log("Error reading ID3v2");

		return;
	}

	tag.artist = tag.title = NULL;
	tag.pos = 0;
	tag.gain = 0;
	tag.major_version = *(buf + 3);
	tag.minor_version = *(buf + 4);
	tag.flags = *(buf + 5);
	tag.len = id3v2_decode_synchsafe(buf + 6);
	ices_log_debug("ID3v2: version %d.%d. Tag size is %d bytes.",
		       tag.major_version, tag.minor_version, tag.len);
	if (tag.major_version > 4) {
		ices_log_debug("ID3v2: Version greater than maximum supported (4), skipping");
		id3v2_skip_data(source, &tag, tag.len);

		return;
	}

	if ((tag.major_version > 2) &&
	    (tag.flags & ID3V2_FLAG_EXTHDR) && id3v2_read_exthdr(source, &tag) < 0) {
		ices_log("Error reading ID3v2 extended header");

		return;
	}

	remaining = tag.len - tag.pos;
	if ((tag.major_version > 3) && (tag.flags & ID3V2_FLAG_FOOTER))
		remaining -= 10;

	/* while (remaining > ID3V2_FRAME_LEN(&tag) && (tag.artist == NULL || tag.title == NULL)) { */
	while (remaining > ID3V2_FRAME_LEN(&tag)) {
		if ((rv = id3v2_read_frame(source, &tag)) < 0) {
			ices_log("Error reading ID3v2 frames, skipping to end of ID3v2 tag");
			break;
		}
		/* found padding */
		if (rv == 0)
			break;

		remaining -= rv;
	}

	/* allow fallback to ID3v1 */
	if (tag.artist || tag.title)
		ices_metadata_set(tag.artist, tag.title);
	ices_util_free(tag.artist);
	ices_util_free(tag.title);

	remaining = tag.len - tag.pos;
	if (remaining)
		id3v2_skip_data(source, &tag, remaining);
}

static int id3v2_read_exthdr(input_stream_t* source, id3v2_tag* tag) {
	char hdr[6];
	size_t len;

	if (source->read(source, hdr, 6) != 6) {
		ices_log("Error reading ID3v2 extended header");

		return -1;
	}
	tag->pos += 6;

	len = id3v2_decode_synchsafe((unsigned char*)hdr);
	ices_log_debug("ID3v2: %d byte extended header found, skipping.", len);

	if (len > 6)
		return id3v2_skip_data(source, tag, len - 6);
	else
		return 0;
}

double id3v2_read_replay_gain(const char *txxx) {
	double gain = 0;

	if (!strcasecmp(txxx, "replaygain_track_gain")) {
		//ices_log_debug("ID3v2: Converting '%s'", txxx + 22);
		gain = atof(txxx + 22);
		//ices_log_debug("ID3v2: Got '%f'", gain);
	}
	if (gain)
		ices_log_debug("ID3v2: Track gain found: %f", gain);

	return gain;
}

double id3v2_get_rva2_track_gain(const char *buf) {
	double gain = 0;
	if (!strcasecmp(buf, "track") && buf[6] == 1)
		gain = (double)(((signed char)(buf[7]) << 8) | buf[9]) / 512.0;
	if (gain)
		ices_log_debug("ID3v2: RVA2 track gain found: %f", gain);

	return gain;
}

ssize_t id3v2_read_frame(input_stream_t* source, id3v2_tag* tag) {
	char hdr[10];
	size_t len, len2;
	ssize_t rlen;
	char *buf, *obuf;
	int decodedlen, datastart;

	if (source->read(source, hdr, ID3V2_FRAME_LEN(tag)) != ID3V2_FRAME_LEN(tag)) {
		ices_log("Error reading ID3v2 frame");

		return -1;
	}
	tag->pos += ID3V2_FRAME_LEN(tag);

	if (hdr[0] == '\0')
		return 0;

	if (tag->major_version < 3) {
		len = id3v2_decode_synchsafe3((unsigned char*)(hdr + 3));
		hdr[3] = '\0';
	} else if (tag->major_version == 3) {
		len = id3v2_decode_unsafe((unsigned char*)(hdr + 4));
		hdr[4] = '\0';
	} else {
		len = id3v2_decode_synchsafe((unsigned char*)(hdr + 4));
		hdr[4] = '\0';
	}
	if (len > tag->len - tag->pos) {
		ices_log("Error parsing ID3v2 frame header: Frame too large (%d bytes)", len);

		return -1;
	}

	// ices_log_debug("ID3v2: Frame type [%s] found, %d bytes", hdr, len);
	if (!strcmp(hdr, ID3V2_ARTIST_TAG(tag)) || !strcmp(hdr, ID3V2_TITLE_TAG(tag)) || !strcmp(hdr, ID3V2_TXXX_TAG(tag)) || !strcmp(hdr, "RVA2")) {
		if (!(buf = malloc(len + 1))) {
			ices_log("Error allocating memory while reading ID3v2 frame");
			return -1;
		}
		// Need separate output buffer, UTF-8 *could* be 4 times as long as source
		if (!(obuf = malloc(len*4 + 1))) {
			ices_log("Error allocating memory while reading ID3v2 frame");
			free(buf);
			return -1;
		}
		len2 = len;
		while (len2) {
			if ((rlen = source->read(source, buf, len2)) < 0) {
				ices_log("Error reading ID3v2 frame data");
				free(buf);
				free(obuf);
				return -1;
			}
			// ices_log("Read %d bytes", rlen);
			tag->pos += rlen;
			len2 -= rlen;
		}

		/* Convert frame contents to UTF-8 */
		/* Possible frame encodings (stored in 1st byte):
				0x00 - ISO-8859-1; terminated with 0x00
				0x01 - UTF-16 with BOM; terminated with 0x0000
				0x02 - UTF-16BE without BOM; terminated with 0x0000
				0x03 - UTF-8; terminated with 0x00
		*/
		switch(buf[0]) {
			case 0:
				// convert from ISO-8859-1 to UTF-8
				// ices_log_debug("ID3v2: Converting frame from ISO-8859-1");
				decodedlen = id3v2_convert_iso8859_1(buf+1, len, obuf);
				// obuf[decodedlen] = '\0';
				// ices_log_debug("ID3v2: obuf=%s, l=%d", obuf, decodedlen);
				break;
			case 1:
				// convert from UTF-16 with BOM to UTF-8
				// ices_log_debug("ID3v2: Converting frame from UTF-16 w/ BOM");
			case 2:
				// convert from UTF-16BE without BOM to UTF-8
				// ices_log_debug("ID3v2: Converting frame from UTF-16 w/o BOM");
				decodedlen = id3v2_convert_utf16(buf+1, len, obuf);
				break;
			case 3:
				// already UTF-8
				// ices_log_debug("ID3v2: Converting frame from UTF-8");
			default:
				// unknown, nothing to do
				// ices_log_debug("ID3v2: Converting frame from unknown");
				decodedlen = len;
				memcpy(obuf, buf+1, decodedlen);
				break;
		}
		
		/* skip encoding */
		if (!strcmp(hdr, ID3V2_TITLE_TAG(tag))) {
			obuf[decodedlen] = '\0';
			ices_log_debug("ID3v2: Title found: %s", obuf);
			tag->title = ices_util_strdup(obuf);
		} else if (!strcmp (hdr, ID3V2_ARTIST_TAG(tag))) {
			obuf[decodedlen] = '\0';
			ices_log_debug("ID3v2: Artist found: %s", obuf);
			tag->artist = ices_util_strdup(obuf);
		} else if (!strcmp (hdr, "RVA2")) {
			ices_log_debug("ID3v2: RVA2 frame found");
			/* only set from RVA2 if not already set by TXXX (TXXX shall "win") */
			if (tag->gain == 0)
				tag->gain = id3v2_get_rva2_track_gain(obuf);
		} else if (!strcmp (hdr, ID3V2_TXXX_TAG(tag))) {
			obuf[decodedlen] = '\0';
			datastart = strlen(obuf) + 1;
			// ices_log_debug("ID3v2: TXXX frame found: %s=%s", obuf, obuf + datastart);
			ices_log_debug("ID3v2: TXXX frame found: %s", obuf);
			/* always set from TXXX even if already set from RVA2 (TXXX shall "win") */
			tag->gain = id3v2_read_replay_gain(obuf);
		}

		if (tag->gain)
			rg_set_track_gain(tag->gain);

		free(buf);
		free(obuf);
	} else if (id3v2_skip_data(source, tag, len))
		return -1;

	return len + ID3V2_FRAME_LEN(tag);
}

// sblinch aug/07: added basic support for unicode string conversion
// 2019-04-20 moonbase: modified to work with TXXX frames (description+data)
// 2019-05-22 moonbase: complete rework of code handling
//   FLAC and OGG output UTF-8 anyway, so we try to also convert anything
//   in ID3v1 and ID3v2 tags to UTF-8, making the stream metadata universally UTF-8

// convert a Unicode code point (uint32_t) into UTF-8
static int id3v2_code_to_utf8(char *buffer, const uint32_t code) {
	if (code <= 0x7F) {
		buffer[0] = code;
		return 1;
	}
	if (code <= 0x7FF) {
		buffer[0] = 0xC0 | (code >> 6);            /* 110xxxxx */
		buffer[1] = 0x80 | (code & 0x3F);          /* 10xxxxxx */
		return 2;
	}
	if (code <= 0xFFFF) {
		buffer[0] = 0xE0 | (code >> 12);           /* 1110xxxx */
		buffer[1] = 0x80 | ((code >> 6) & 0x3F);   /* 10xxxxxx */
		buffer[2] = 0x80 | (code & 0x3F);          /* 10xxxxxx */
		return 3;
	}
	if (code <= 0x10FFFF) {
		buffer[0] = 0xF0 | (code >> 18);           /* 11110xxx */
		buffer[1] = 0x80 | ((code >> 12) & 0x3F);  /* 10xxxxxx */
		buffer[2] = 0x80 | ((code >> 6) & 0x3F);   /* 10xxxxxx */
		buffer[3] = 0x80 | (code & 0x3F);          /* 10xxxxxx */
		return 4;
	}
	return 0;
}

static int id3v2_convert_utf16(char *str, int len, char *out) {
	int c0, c1, i, ofs, o, l, j;
	uint32_t cp;
	char tmp[4];

	// determine byte order
	if ( str[0] == '\xff' && str[1] == '\xfe' ) {						// little-endian 16-bit unicode
		c0 = 1;
		ofs = 2;
	}            
	else if ( str[0] == '\xfe' && str[1] == '\xff' ) {			// big-endian 16-bit unicode
		c0 = 0;
		ofs = 2;
	}       
	else {																									// no BOM - UTF-16BE w/o BOM
		c0 = 0;
		ofs = 0;
	}                                                   
	
	c1 = 1 - c0;
	o = 0;

	for (i=0; (i*2+ofs) < len-1; i++) {
		// TXXX frames have a description and data, separated by 0x0000,
		// and followed again by a Unicode BOM. We simply assume that the BOM
		// is the same as above (and no change between little and big endian mid-stream)
		// and skip it.
		cp = ((unsigned char)str[i*2 + ofs + c0] << 8) + (unsigned char)str[i*2 + ofs + c1];
		if (cp == 0xfffe || cp == 0xfeff) {
			ofs += 2;
			cp = ((unsigned char)str[i*2 + ofs + c0] << 8) + (unsigned char)str[i*2 + ofs + c1];
		}

		l = id3v2_code_to_utf8(tmp, cp);
		// ices_log_debug("ID3v2: in=%x, out=%s, l=%d", cp, tmp, l);
		for (j=1; j<=l; j++) {
			out[o] = tmp[j-1];
			o++;
		}
	}

	return o;
}

static int id3v2_convert_iso8859_1(char *str, int len, char *out) {
	int i, o, l, j;
	char tmp[4];
	
	o = 0;
	
	for (i=0; i < len-1; i++) {
		l = id3v2_code_to_utf8(tmp, (unsigned char)str[i]);
		// ices_log_debug("ID3v2: in=%x, out=%s, l=%d", (unsigned char)str[i], tmp, l);
		for (j=1; j<=l; j++) {
			out[o] = tmp[j-1];
			o++;
		}
	}
	return o;
}

static int id3v2_skip_data(input_stream_t* source, id3v2_tag* tag, size_t len) {
	char* buf;
	ssize_t rlen;

	if (!(buf = malloc(len))) {
		ices_log("Error allocating memory while skipping ID3v2 data");

		return -1;
	}

	while (len) {
		if ((rlen = source->read(source, buf, len)) < 0) {
			ices_log("Error skipping in ID3v2 tag.");
			free(buf);

			return -1;
		}
		tag->pos += rlen;
		len -= rlen;
	}

	free(buf);

	return 0;
}

static int id3v2_decode_synchsafe(unsigned char* synchsafe) {
	int res;

	res = synchsafe[3];
	res |= synchsafe[2] << 7;
	res |= synchsafe[1] << 14;
	res |= synchsafe[0] << 21;

	return res;
}

static int id3v2_decode_synchsafe3(unsigned char* synchsafe) {
	int res;

	res = synchsafe[2];
	res |= synchsafe[1] << 7;
	res |= synchsafe[0] << 14;

	return res;
}

/* id3v2.3 badly specifies frame length */
static int id3v2_decode_unsafe(unsigned char* in) {
	int res;

	res = in[3];
	res |= in[2] << 8;
	res |= in[1] << 16;
	res |= in[0] << 24;

	return res;
}

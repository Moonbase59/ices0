/* icestypes.h
 * - Datatypes for ices
 * Copyright (c) 2000 Alexander Haväng
 * Copyright (c) 2001-4 Brendan Cully <brendan@xiph.org>
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

#ifndef _ICES_ICESTYPES_H
#define _ICES_ICESTYPES_H

typedef enum {
	icy_protocol_e,
	xaudiocast_protocol_e,
	http_protocol_e
} protocol_t;

typedef enum {
	ices_playlist_builtin_e,
	ices_playlist_script_e,
	ices_playlist_python_e,
	ices_playlist_perl_e
} playlist_type_t;

typedef struct ices_stream_St {
	shout_t* conn;
	time_t connect_delay;
	int errs;
	void* encoder_state;

	char *host;
	int port;
	char *password;
	protocol_t protocol;

	char* mount;
	char* dumpfile;

	char* name;
	char* genre;
	char* description;
	char* url;
	int ispublic;

	int reencode;
	int bitrate;
	int out_samplerate;
	int out_numchannels;

	struct ices_stream_St* next;
} ices_stream_t;

typedef struct {
	playlist_type_t playlist_type;
	int randomize;
	char* playlist_file;
	char* module;

	char* (*get_next)(void);        /* caller frees result */
	char* (*get_metadata)(void);    /* caller frees result */
	int (*get_timelimit)(void);
	int (*get_lineno)(void);
	int (*reload)(void);
	void (*shutdown)(void);
} playlist_module_t;

/* -- input stream types -- */
typedef enum {
	ICES_INPUT_VORBIS,
	ICES_INPUT_MP3,
	ICES_INPUT_MP4,
	ICES_INPUT_FLAC
} input_type_t;

typedef struct _input_stream_t {
	input_type_t type;

	char* path;
	time_t interrupttime;
	int fd;
	size_t filesize;
	size_t bytes_read;
	unsigned int bitrate;
	unsigned int samplerate;
	unsigned int channels;

	void* data;

	ssize_t (*read)(struct _input_stream_t* self, void* buf, size_t len);
	/* len is the size in bytes of left or right. The two buffers must be
	 * the same size. */
	ssize_t (*readpcm)(struct _input_stream_t* self, size_t len, int16_t* left,
			   int16_t* right);
	int (*close)(struct _input_stream_t* self);
} input_stream_t;

typedef struct _ices_plugin {
	const char *name;

	int (*init)(void);
	void (*new_track)(input_stream_t *source);
	int (*process)(int ilen, int16_t *il, int16_t *ir);
	void (*shutdown)(void);
	int (*options)(int optid, void *opt);

	struct _ices_plugin *next;
} ices_plugin_t;

typedef struct {
	int daemon;
	int verbose;
	int reencode;
	char *configfile;
	char *base_directory;
	FILE *logfile;

	ices_stream_t* streams;
	playlist_module_t pm;
	ices_plugin_t *plugins;
} ices_config_t;
#endif

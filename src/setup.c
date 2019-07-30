/* setup.c
 * - Functions for initialization in ices
 * Copyright (c) 2000 Alexander Haväng
 * Copyright (c) 2002-4 Brendan Cully <brendan@xiph.org>
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

/* Local function declarations */
static void ices_setup_parse_options(ices_config_t *ices_config);
static void ices_setup_parse_defaults(ices_config_t *ices_config);
#ifdef HAVE_LIBXML
static void ices_setup_parse_config_file(ices_config_t *ices_config, const char *configfile);
#endif
static void ices_setup_parse_command_line(ices_config_t *ices_config, char **argv, int argc);
static void ices_setup_parse_command_line_for_new_configfile(ices_config_t *ices_config, char **argv, int argc);
static void ices_setup_activate_libshout_changes(const ices_config_t *ices_config);
static void ices_setup_usage(void);
static void ices_setup_version(void);
static void ices_setup_update_pidfile(int icespid);
static void ices_setup_daemonize(void);
static void ices_free_all(ices_config_t *ices_config);

extern ices_config_t ices_config;

/* Global function definitions */

/* Top level initialization function for ices.
 * It will parse options, initialize modules,
 * and if requested, become a daemon. */
void ices_setup_initialize(void) {
	ices_stream_t* stream;
	ices_plugin_t* plugin;

	shout_init();

	/* Setup signal handlers */
	ices_signals_setup();

	/* Parse the options in the config file, and the command line */
	ices_setup_parse_options(&ices_config);

	if (ices_config.daemon)
		ices_setup_daemonize();

	/* Open logfiles */
	ices_log_initialize();

	/* Initialize the libshout structure */
	for (stream = ices_config.streams; stream; stream = stream->next) {
		if (!(stream->conn = shout_new())) {
			ices_log("Could not create shout interface");
			ices_setup_shutdown();
		}
	}

	ices_setup_activate_libshout_changes(&ices_config);

	/* Initialize the playlist handler */
	ices_playlist_initialize();

#ifdef HAVE_LIBLAME
	/* Initialize liblame for reeencoding */
	ices_reencode_initialize();

	while (ices_config.plugins && ices_config.plugins->init() < 0)
		ices_config.plugins = ices_config.plugins->next;

	for (plugin = ices_config.plugins; plugin && plugin->next; plugin = plugin->next)
		if (plugin->next->init() < 0)
			plugin->next = plugin->next->next;
#endif

	ices_log_debug("Startup complete\n");
}

/* Top level ices shutdown function.
 * This is the _only_ way out of here */
void ices_setup_shutdown(void) {
	ices_stream_t* stream;
	ices_plugin_t* plugin;

	/* Tell libshout to disconnect from server */
	for (stream = ices_config.streams; stream; stream = stream->next)
		if (stream->conn)
			shout_close(stream->conn);

#ifdef HAVE_LIBLAME
	for (plugin = ices_config.plugins; plugin; plugin = plugin->next)
		plugin->shutdown();

	/* Order the reencoding engine to shutdown */
	ices_reencode_shutdown();
#endif

	/* Tell the playlist module to shutdown and cleanup */
	ices_playlist_shutdown();

	/* Cleanup the cue file (the cue module has no init yet) */
	ices_cue_shutdown();

	/* Make sure we're not leaving any memory allocated around when
	 * we exit. This makes it easier to find memory leaks, and
	 * some systems actually don't clean up that well */
	ices_free_all(&ices_config);

	/* Let the log and console know we went down ok */
	ices_log("Ices Exiting...");

	/* Close logfiles */
	ices_log_shutdown();

	shout_shutdown();

	/* Down and down we go... */
	exit(1);
}

/* Local function definitions */

/* Top level option parsing function.
 * Sets of options object (ices_config), with:
 * - Hardcoded defaults
 * - Configfile settings
 * - Command line options */
static void ices_setup_parse_options(ices_config_t *ices_config) {
	/* Get default values for the settings */
	ices_setup_parse_defaults(ices_config);

	/* Look for given configfile on the commandline */
	ices_setup_parse_command_line_for_new_configfile(ices_config, ices_util_get_argv(), ices_util_get_argc());

#ifdef HAVE_LIBXML
	/* Parse the configfile */
	ices_setup_parse_config_file(ices_config, ices_config->configfile);
#endif

	/* Parse the commandline */
	ices_setup_parse_command_line(ices_config, ices_util_get_argv(), ices_util_get_argc());
}

/* Function for placing hardcoded defaults in the
 * options object (ices_config) */
static void ices_setup_parse_defaults(ices_config_t *ices_config) {
	ices_config->configfile = ices_util_strdup(ICES_DEFAULT_CONFIGFILE);
	ices_config->daemon = ICES_DEFAULT_DAEMON;
	ices_config->base_directory = ices_util_strdup(ICES_DEFAULT_BASE_DIRECTORY);
	ices_config->verbose = ICES_DEFAULT_VERBOSE;
	ices_config->reencode = 0;

	ices_config->pm.playlist_file =
		ices_util_strdup(ICES_DEFAULT_PLAYLIST_FILE);
	ices_config->pm.module = ices_util_strdup(ICES_DEFAULT_MODULE);
	ices_config->pm.randomize = ICES_DEFAULT_RANDOMIZE_PLAYLIST;
	ices_config->pm.playlist_type = ICES_DEFAULT_PLAYLIST_TYPE;

	ices_config->streams = (ices_stream_t*) malloc(sizeof(ices_stream_t));

	ices_setup_parse_stream_defaults(ices_config->streams);
}

/* Place hardcoded defaults into an ices_stream_t object */
void ices_setup_parse_stream_defaults(ices_stream_t* stream) {
	stream->conn = NULL;
	stream->host = ices_util_strdup(ICES_DEFAULT_HOST);
	stream->port = ICES_DEFAULT_PORT;
	stream->password = ices_util_strdup(ICES_DEFAULT_PASSWORD);
	stream->protocol = ICES_DEFAULT_PROTOCOL;

	stream->mount = ices_util_strdup(ICES_DEFAULT_MOUNT);
	stream->dumpfile = NULL;

	stream->name = ices_util_strdup(ICES_DEFAULT_NAME);
	stream->genre = ices_util_strdup(ICES_DEFAULT_GENRE);
	stream->description = ices_util_strdup(ICES_DEFAULT_DESCRIPTION);
	stream->url = ices_util_strdup(ICES_DEFAULT_URL);
	stream->ispublic = ICES_DEFAULT_ISPUBLIC;

	stream->bitrate = ICES_DEFAULT_BITRATE;
	stream->reencode = ICES_DEFAULT_REENCODE;
	stream->out_numchannels = -1;
	stream->out_samplerate = -1;

	stream->encoder_state = NULL;
	stream->connect_delay = 0;

	stream->next = NULL;
}

/* Frees ices_stream_t data (but not the object itself) */
static void ices_setup_free_stream(ices_stream_t* stream) {
	if (stream->conn)
		shout_free(stream->conn);
	ices_util_free(stream->host);
	ices_util_free(stream->password);

	ices_util_free(stream->mount);
	ices_util_free(stream->dumpfile);

	ices_util_free(stream->name);
	ices_util_free(stream->genre);
	ices_util_free(stream->description);
	ices_util_free(stream->url);
}

/* Function to free() all allocated memory when ices shuts down. */
static void ices_free_all(ices_config_t *ices_config) {
	ices_stream_t *stream, *next;

	ices_util_free(ices_config->configfile);
	ices_util_free(ices_config->base_directory);

	ices_util_free(ices_config->pm.playlist_file);
	ices_util_free(ices_config->pm.module);

	for (stream = ices_config->streams; stream; stream = next) {
		next = stream->next;

		ices_setup_free_stream(stream);
		ices_util_free(stream);
	}
}

#ifdef HAVE_LIBXML
/* Tell the xml module to parse the config file. */
static void ices_setup_parse_config_file(ices_config_t *ices_config, const char *configfile) {
	char namespace[1024];
	const char *realname = NULL;
	int ret;

	if (ices_util_verify_file(configfile))
		realname = configfile;
	else {
		sprintf(namespace, "%s/%s", ICES_ETCDIR, configfile);
		if (ices_util_verify_file(namespace))
			realname = &namespace[0];
	}

	if (realname) {
		ret = ices_xml_parse_config_file(ices_config, realname);

		if (ret == -1)
			/* ret == -1 means we have no libxml support */
			ices_log_debug("%s", ices_log_get_error());
		else if (ret == 0)
			/* A real error */
			ices_log("%s", ices_log_get_error());
	}
}
#endif

/* This function looks through the command line options for a new
 * configfile. */
static void ices_setup_parse_command_line_for_new_configfile(ices_config_t *ices_config, char **argv, int argc) {
	int arg;
	char *s;

	arg = 1;

	while (arg < argc) {
		s = argv[arg];

		if (s[0] == '-') {
			switch (s[1]) {
			case 'c':
				arg++;
				if (ices_config->configfile)
					ices_util_free(ices_config->configfile);
				ices_config->configfile = ices_util_strdup(argv[arg]);
#ifndef HAVE_LIBXML
				fprintf(stderr, "Cannot use config file (no XML support).\n");
				ices_setup_shutdown();
#endif
				break;
			}
		}
		arg++;
	}
}

/* This function parses the command line options */
static void ices_setup_parse_command_line(ices_config_t *ices_config, char **argv,
					  int argc) {
	int arg;
	char *s;
	ices_stream_t* stream = ices_config->streams;
	/* each -m option creates a new stream, subsequent options are applied
	 * to it. */
	int nstreams = 1;

	arg = 1;

	while (arg < argc) {
		s = argv[arg];

		if (s[0] == '-') {
			if ((strchr("BRrsVv", s[1]) == NULL) && arg >= (argc - 1)) {
				fprintf(stderr, "Option %c requires an argument!\n", s[1]);
				ices_setup_usage();
				ices_setup_shutdown();
				return;
			}

			switch (s[1]) {
			case 'B':
				ices_config->daemon = 1;
				break;
			case 'b':
				arg++;
				stream->bitrate = atoi(argv[arg]);
				break;
			case 'C':
				arg++;
				if (atoi(argv[arg]) > 0)
					/* TODO: stack plugins */
					ices_config->plugins = crossfade_plugin(atoi(argv[arg]));
			case 'c':
				arg++;
				break;
			case 'd':
				arg++;
				ices_util_free(stream->description);
				stream->description = ices_util_strdup(argv[arg]);
				break;
			case 'D':
				arg++;
				ices_util_free(ices_config->base_directory);
				ices_config->base_directory = ices_util_strdup(argv[arg]);
				break;
			case 'F':
				arg++;
				ices_util_free(ices_config->pm.playlist_file);
				ices_config->pm.playlist_file = ices_util_strdup(argv[arg]);
				break;
			case 'f':
				arg++;
				ices_util_free(stream->dumpfile);
				stream->dumpfile = ices_util_strdup(argv[arg]);
				break;
			case 'g':
				arg++;
				ices_util_free(stream->genre);
				stream->genre = ices_util_strdup(argv[arg]);
				break;
			case 'h':
				arg++;
				ices_util_free(stream->host);
				stream->host = ices_util_strdup(argv[arg]);
				break;
			case 'H':
				arg++;
				stream->out_samplerate = atoi(argv[arg]);
				break;
			case 'M':
				arg++;
				ices_util_free(ices_config->pm.module);
				ices_config->pm.module = ices_util_strdup(argv[arg]);
				break;
			case 'm':
				arg++;
				if (nstreams > 1) {
					stream->next =
						(ices_stream_t*) malloc(sizeof(ices_stream_t));
					stream = stream->next;
					ices_setup_parse_stream_defaults(stream);
				}
				ices_util_free(stream->mount);
				stream->mount = ices_util_strdup(argv[arg]);
				nstreams++;
				break;
			case 'N':
				arg++;
				stream->out_numchannels = atoi(argv[arg]);
				break;
			case 'n':
				arg++;
				ices_util_free(stream->name);
				stream->name = ices_util_strdup(argv[arg]);
				break;
			case 'P':
				arg++;
				ices_util_free(stream->password);
				stream->password = ices_util_strdup(argv[arg]);
				break;
			case 'p':
				arg++;
				stream->port = atoi(argv[arg]);
				break;
			case 'R':
#ifdef HAVE_LIBLAME
				stream->reencode = 1;
#else
				fprintf(stderr, "This ices wasn't compiled with reencoding support\n");
				ices_setup_shutdown();
#endif
				break;
			case 'r':
				ices_config->pm.randomize = 1;
				break;
			case 'S':
				arg++;
				if (strcmp(argv[arg], "python") == 0)
					ices_config->pm.playlist_type = ices_playlist_python_e;
				else if (strcmp(argv[arg], "perl") == 0)
					ices_config->pm.playlist_type = ices_playlist_perl_e;
				else if (strcmp(argv[arg], "script") == 0)
					ices_config->pm.playlist_type = ices_playlist_script_e;
				else
					ices_config->pm.playlist_type = ices_playlist_builtin_e;
				break;
			case 's':
				stream->ispublic = 0;
				break;
			case 't':
				arg++;
				if (!strcmp(argv[arg], "http"))
					stream->protocol = http_protocol_e;
				else if (!strcmp(argv[arg], "xaudiocast"))
					stream->protocol = xaudiocast_protocol_e;
				else if (!strcmp(argv[arg], "icy"))
					stream->protocol = icy_protocol_e;
				else {
					fprintf(stderr, "Unknown protocol %s. Use 'http', 'xaudiocast' or 'icy'.\n", argv[arg]);
					ices_setup_shutdown();
				}
				break;
			case 'u':
				arg++;
				ices_util_free(stream->url);
				stream->url = ices_util_strdup(argv[arg]);
				break;
			case 'V':
				ices_setup_version();
				exit(0);
			case 'v':
				ices_config->verbose = 1;
				break;
			default:
				ices_setup_usage();
				break;
			}
		}
		arg++;
	}
}

/* This function takes all the new configuration and copies it to the
   libshout object. */
static void ices_setup_activate_libshout_changes(const ices_config_t *ices_config) {
	ices_stream_t* stream;
	shout_t* conn;
	int streamno = 0;
	char useragent[64];
	char bitrate[8];

	snprintf(useragent, sizeof(useragent), "ices/" VERSION " libshout/%s",
		 shout_version(NULL, NULL, NULL));

	for (stream = ices_config->streams; stream; stream = stream->next) {
		conn = stream->conn;

		shout_set_host(conn, stream->host);
		shout_set_port(conn, stream->port);
		shout_set_password(conn, stream->password);
		shout_set_format(conn, SHOUT_FORMAT_MP3);
		if (stream->protocol == icy_protocol_e)
			shout_set_protocol(conn, SHOUT_PROTOCOL_ICY);
		else if (stream->protocol == http_protocol_e)
			shout_set_protocol(conn, SHOUT_PROTOCOL_HTTP);
		else
			shout_set_protocol(conn, SHOUT_PROTOCOL_XAUDIOCAST);
		if (stream->dumpfile)
			shout_set_dumpfile(conn, stream->dumpfile);
		shout_set_name(conn, stream->name);
		shout_set_url(conn, stream->url);
		shout_set_genre(conn, stream->genre);
		shout_set_description(conn, stream->description);

		snprintf(bitrate, sizeof(bitrate), "%d", stream->bitrate);
		shout_set_audio_info(conn, SHOUT_AI_BITRATE, bitrate);

		shout_set_public(conn, stream->ispublic);
		shout_set_mount(conn, stream->mount);
		shout_set_agent(conn, useragent);

		ices_log_debug("Sending following information to libshout:");
		ices_log_debug("Stream: %d", streamno);
		ices_log_debug("Host: %s:%d (protocol: %s)", shout_get_host(conn),
			       shout_get_port(conn),
			       stream->protocol == icy_protocol_e ? "icy" :
			       stream->protocol == http_protocol_e ? "http" : "xaudiocast");
		ices_log_debug("Mount: %s, Password: %s", shout_get_mount(conn), shout_get_password(conn));
		ices_log_debug("Name: %s\tURL: %s", shout_get_name(conn), shout_get_url(conn));
		ices_log_debug("Genre: %s\tDesc: %s", shout_get_genre(conn),
			       shout_get_description(conn));
		ices_log_debug("Bitrate: %s\tPublic: %d", shout_get_audio_info(conn, SHOUT_AI_BITRATE),
			       shout_get_public(conn));
		ices_log_debug("Dump file: %s", ices_util_nullcheck(shout_get_dumpfile(conn)));
		streamno++;
	}
}

/* Display all command line options for ices */
static void ices_setup_usage(void) {
	printf("This is ices " VERSION "\n"
	       "ices <options>\n"
	       "Options:\n"
	       "\t-B (Background (daemon mode))\n"
	       "\t-b <stream bitrate>\n"
	       "\t-C <crossfade seconds>\n");
	printf("\t-c <configfile>\n");
	printf("\t-D <base directory>\n");
	printf("\t-d <stream description>\n");
	printf("\t-f <dumpfile on server>\n");
	printf("\t-F <playlist>\n");
	printf("\t-g <stream genre>\n");
	printf("\t-h <host>\n");
	printf("\t-M <interpreter module>\n");
	printf("\t-m <mountpoint>\n");
	printf("\t-n <stream name>\n");
	printf("\t-p <port>\n");
	printf("\t-P <password>\n");
	printf("\t-R (activate reencoding)\n");
	printf("\t-r (randomize playlist)\n");
	printf("\t-s (private stream)\n");
	printf("\t-S <script|perl|python|builtin>\n");
	printf("\t-t <http|xaudiocast|icy>\n");
	printf("\t-u <stream url>\n");
	printf("\t-V (display version number)\n");
	printf("\t-v (verbose output)\n");
	printf("\t-H <reencoded sample rate>\n");
	printf("\t-N <reencoded number of channels>\n");
}

/* display version information */
static void ices_setup_version(void) {
	printf("ices " VERSION "\nFeatures: "
#ifdef HAVE_LIBLAME
	       "LAME "
#endif
#ifdef HAVE_LIBPERL
	       "Perl "
#endif
#ifdef HAVE_LIBPYTHON
	       "python "
#endif
#ifdef HAVE_LIBXML
	       "libxml "
#endif
#ifdef HAVE_LIBVORBISFILE
	       "Vorbis "
#endif
#ifdef HAVE_LIBFLAC
	       "FLAC "
#endif
#ifdef HAVE_LIBFAAD
	       "MP4"
#endif
	       "\n"
	       "System configuration file: " ICES_ETCDIR "/ices.conf\n"
#if defined (HAVE_LIBPERL) || defined (HAVE_LIBPYTHON)
	       "Playlist module directory: " ICES_MODULEDIR "\n"
#endif
	       );
}

/* Put ices in the background, as a daemon */
static void ices_setup_daemonize(void) {
	int icespid = fork();

	if (icespid == -1) {
		ices_log("ERROR: Cannot fork(), that means no daemon, sorry!");
		return;
	}

	if (icespid != 0) {
		/* Update the pidfile (so external applications know what pid
		   ices is running with. */
		printf("Into the land of the dreaded daemons we go... (pid: %d)\n", icespid);
		ices_setup_shutdown();
	}
#ifdef HAVE_SETSID
	setsid();
#endif

	ices_log_daemonize();
	ices_setup_update_pidfile(getpid());
}

/* Update a file called ices.pid with the given process id */
static void ices_setup_update_pidfile(int icespid) {
	char buf[1024];
	FILE* pidfd;

	if (!ices_config.base_directory) {
		ices_log_error("Base directory is invalid");
		return;
	}

	snprintf(buf, sizeof(buf), "%s/ices.pid", ices_config.base_directory);

	pidfd = ices_util_fopen_for_writing(buf);

	if (pidfd) {
		fprintf(pidfd, "%d", icespid);
		ices_util_fclose(pidfd);
	}
}



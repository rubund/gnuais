#if HAVE_CONFIG_H
#include "config.h"
#endif


#include <stdio.h>
#include <stdlib.h>

#include "ais.h"
#include "input.h"
#include "signalin.h"
#include "protodec.h"

#include "hmalloc.h"
#include "hlog.h"
#include "cfg.h"

#include "config.h"

int done;
long int cntr;

void closedown(int sig)
{
	done = 1;
}

int main(int argc, char *argv[])
{
	int err, i;
	done = 0;
	snd_pcm_t *handle;
	FILE *sound_fd = NULL;
	short *buffer;
	int buffer_l;
	float *buff_f, *buff_fs;
	char *buff_b;
	char lastbit = 0;
	struct demod_state_t demod_state;

	/* command line */
	parse_cmdline(argc, argv);
	
	/* open syslog, write an initial log message and read configuration */
	open_log(logname, 0);
	hlog(LOG_NOTICE, "Starting up...");
	if (read_config()) {
		hlog(LOG_CRIT, "Initial configuration failed.");
		exit(1);
	}
	
	/* fork a daemon */
	if (fork_a_daemon) {
		int i = fork();
		if (i < 0) {
			hlog(LOG_CRIT, "Fork to background failed: %s", strerror(errno));
			fprintf(stderr, "Fork to background failed: %s\n", strerror(errno));
			exit(1);
		} else if (i == 0) {
			/* child */
		} else {
			/* parent, quitting */
			hlog(LOG_DEBUG, "Forked daemon process %d, parent quitting", i);
			exit(0);
		}
	}
	
	/* write pid file, now that we have our final pid... might fail, which is critical */
	if (!writepid(pidfile))
		exit(1);
	
	memset(demod_state.skip_type, 0, sizeof(demod_state.skip_type));
	signal(SIGINT, closedown);

	protodec_initialize(&demod_state);

	if (sound_device) {
		if ((err = snd_pcm_open(&handle, sound_device, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
			hlog(LOG_CRIT, "Error opening sound device (%s)", sound_device);
			return -1;
		}
		
		if (input_initialize(handle, &buffer, &buffer_l) < 0)
			return -1;
	} else if (sound_file) {
		if ((sound_fd = fopen(sound_file, "r")) == NULL) {
			hlog(LOG_CRIT, "Could not open sound file");
			return -1;
		}
		buffer_l = 1024;
		int extra = buffer_l % 5;
		buffer_l -= extra;
		buffer = (short *) hmalloc((buffer_l) * sizeof(short));
	} else {
		hlog(LOG_CRIT, "Neither sound device or sound file configured.");
		return -1;
	}
	
	buff_f = (float *) hmalloc(sizeof(float) * buffer_l);
	buff_fs = (float *) hmalloc(sizeof(float) * buffer_l / 5);
	buff_b = (char *) hmalloc(sizeof(char) * buffer_l / 5);
	
#ifdef HAVE_MYSQL
	if (mysql_db) {
		hlog(LOG_DEBUG, "Saving to MySQL database \"%s\"", mysql_db);
		if (strcmp(mysql_keepsmall, "yes") == 0 || strcmp(mysql_keepsmall, "YES") == 0)
			hlog(LOG_DEBUG, "Updating database rows only.");
		else
			hlog(LOG_DEBUG, "Inserting data to database.");
	}
	if (mysql_oldlimit && demod_state.enable_mysql)
		hlog(LOG_DEBUG, "Deleting data older than %s seconds", mysql_oldlimit);
#endif

	if (serial_port) {
		if (demod_state.my_fd != -1)
			hlog(LOG_DEBUG, "Sending NMEA out to: %s", serial_port);
	}
	hlog(LOG_NOTICE, "Started");

	while (!done) {
		if (sound_fd) {
			cntr += buffer_l;
			if (fread(buffer, 2, buffer_l, sound_fd) == 0)
				done = 1;
		} else {
			input_read(handle, buffer, buffer_l);
		}
		
		signal_filter(buffer, buffer_l, buff_f);
		signal_clockrecovery(buff_f, buffer_l, buff_fs);
		signal_bitslice(buff_fs, buffer_l, buff_b, &lastbit);
		protodec_decode(buff_b, buffer_l, &demod_state);
	}
	
	hlog(LOG_NOTICE, "Closing down...");
	if (sound_fd) {
		fclose(sound_fd);
	} else {
		input_cleanup(handle);
	}
	hfree(buffer);
	hfree(buff_f);
	hfree(buff_fs);
	hfree(buff_b);
	if (demod_state.my_fd != -1)
		close(demod_state.my_fd);
	hlog(LOG_INFO,
		"Received correctly: %d packets, wrong CRC: %d packets, wrong size: %d packets",
		demod_state.receivedframes, demod_state.lostframes,
		demod_state.lostframes2);
}


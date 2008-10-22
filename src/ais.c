#if HAVE_CONFIG_H
#include "config.h"
#endif


#include <stdio.h>
#include <stdlib.h>

#include "ais.h"
#include "input.h"
#include "signalin.h"
#include "protodec.h"


void parse_configfile(struct mysql_data_t *, struct demod_state_t *);

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
	short *buffer;
	int buffer_l;
	float *buff_f, *buff_fs;
	char *buff_b;
	char lastbit = 0;
	struct mysql_data_t mysql_data;
	struct demod_state_t demod_state;

	parse_configfile(&mysql_data, &demod_state);
	signal(SIGINT, closedown);

	protodec_initialize(&demod_state, &mysql_data);

	if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf(stderr, "Error opening sound device\n");
		return -1;
	}

	if (input_initialize(handle, &buffer, &buffer_l) < 0) {
		return -1;
	}
	buff_f = (float *) malloc(sizeof(float) * buffer_l);
	if (buff_f == NULL) {
		fprintf(stderr, "Error allocating memory\n");
		return -1;
	}
	buff_fs = (float *) malloc(sizeof(float) * buffer_l / 5);
	if (buff_fs == NULL) {
		fprintf(stderr, "Error allocating memory\n");
		return -1;
	}
	buff_b = (char *) malloc(sizeof(char) * buffer_l / 5);
	if (buff_b == NULL) {
		fprintf(stderr, "Error allocating memory\n");
		return -1;
	}
#ifdef HAVE_MYSQL
	if (mysql_data.mysql_dbname[0] != 0) {
		printf(" (Saving to MySQL database \"%s\")", mysql_data.mysql_dbname);
		if (strcmp(mysql_data.mysql_keepsmall, "yes") == 0 || strcmp(mysql_data.mysql_keepsmall, "YES") == 0)
			printf(", updating data.\n");
		else
			printf(", inserting data.\n");
	}
	if (mysql_data.mysql_oldlimit[0] != 0 && demod_state.enable_mysql)
		printf("Deleting data older than %ssec\n",
		       mysql_data.mysql_oldlimit);
#endif
	if (mysql_data.serial_port[0] != 0) {
		if (demod_state.my_fd != -1)
			printf("Sending NMEA out to:  %s\n",
			       mysql_data.serial_port);
	}
	printf("Starts demodulating and decoding AIS...");

	printf("\n\n");

	while (!done) {
		input_read(handle, buffer, buffer_l);
		signal_filter(buffer, buffer_l, buff_f);
		signal_clockrecovery(buff_f, buffer_l, buff_fs);
		signal_bitslice(buff_fs, buffer_l, buff_b, &lastbit);
		protodec_decode(buff_b, buffer_l, &demod_state);
	}

	printf("Closing down...\n");
	input_cleanup(handle);
	free(buffer);
	free(buff_f);
	free(buff_fs);
	free(buff_b);
	if (demod_state.my_fd != -1)
		close(demod_state.my_fd);
	printf("Received correctly: %d\nWrong CRC: %d\nWrong Size: %d\n",
	       demod_state.receivedframes, demod_state.lostframes,
	       demod_state.lostframes2);
}


void parse_configfile(struct mysql_data_t *d, struct demod_state_t *s)
{
	FILE *in;
	char buffer[100];
	d->mysql_dbname[0] = 0;
	d->mysql_username[0] = 0;
	d->mysql_password[0] = 0;
	d->mysql_table[0] = 0;
	d->mysql_keepsmall[0] = 0;
	d->mysql_oldlimit[0] = 0;
	d->serial_port[0] = 0;
	int tmp;
	int i;
	memset(s->skip_type, 0, sizeof(s->skip_type));

	if (in = fopen("/etc/gnuais.conf", "r")) {
		while (1) {
			if ((fscanf(in, "%s", buffer)) == EOF)
				break;
			if (!strcmp(buffer, "mysql_host")) {
				if ((fscanf(in, "%s", buffer) == EOF)) {
					fprintf(stderr, "Error reading config file\n");
					exit(-1);
				}
				strcpy(d->mysql_host, buffer);
			} else if (!strcmp(buffer, "mysql_db")) {
				if ((fscanf(in, "%s", buffer) == EOF)) {
					fprintf(stderr, "Error reading config file\n");
					exit(-1);
				}
				strcpy(d->mysql_dbname, buffer);
			} else if (!strcmp(buffer, "mysql_user")) {
				if ((fscanf(in, "%s", buffer) == EOF)) {
					fprintf(stderr, "Error reading config file\n");
					exit(-1);
				}
				strcpy(d->mysql_username, buffer);
			} else if (!strcmp(buffer, "mysql_password")) {
				if ((fscanf(in, "%s", buffer) == EOF)) {
					fprintf(stderr, "Error reading config file\n");
					exit(-1);
				}
				strcpy(d->mysql_password, buffer);
			} else if (!strcmp(buffer, "mysql_keepsmall")) {
				if ((fscanf(in, "%s", buffer) == EOF)) {
					fprintf(stderr, "Error reading config file\n");
					exit(-1);
				}
				strcpy(d->mysql_keepsmall, buffer);
			} else if (!strcmp(buffer, "mysql_oldlimit")) {
				if ((fscanf(in, "%s", buffer) == EOF)) {
					fprintf(stderr, "Error reading config file\n");
					exit(-1);
				}
				strcpy(d->mysql_oldlimit, buffer);
			} else if (!strcmp(buffer, "serial_port")) {
				if ((fscanf(in, "%s", buffer) == EOF)) {
					fprintf(stderr, "Error reading config file\n");
					exit(-1);
				}
				strcpy(d->serial_port, buffer);
			} else if (!strcmp(buffer, "skip_type")) {
				if ((fscanf(in, "%s", buffer) == EOF)) {
					fprintf(stderr, "Error reading config file\n");
					exit(-1);
				}
				tmp = atoi(buffer);
				if (tmp > 0 && tmp < 10)
					s->skip_type[(char) tmp] = 1;
			}
		}
		fclose(in);
	}

}

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

void closedown(int sig)
{
 	done = 1;
}

int main(int argc, char *argv[])
{
	if(argc != 2)
	{
		fprintf(stderr,"Usage: %s <soundfilename>\n",argv[0]);
		return -1;
	}
	FILE *soundfile;
	if ((soundfile = fopen(argv[1],"w")) == NULL)
	{
		printf("Could not open sound file\n");
		return -1;
	}
	int err,i;
	done=0;
	snd_pcm_t *handle;
	short *buffer;	
	int buffer_l = 1024;
	float *buff_f, *buff_fs;
	char *buff_b;
	char lastbit = 0;
	struct mysql_data_t mysql_data;
	struct demod_state_t demod_state;
	signal(SIGINT,closedown);
	printf("Recording sound to: %s\n",argv[1]);
	if (( err = snd_pcm_open(&handle,"default",SND_PCM_STREAM_CAPTURE,0)) < 0)
	{
		fprintf(stderr,"Error opening sound device\n");
		return -1;
	}
	if (input_initialize(handle,&buffer,&buffer_l) < 0)
	{
		return -1;
	}
	
	while(!done)
	{
		input_read(handle,buffer,buffer_l);
		fwrite(buffer,2,buffer_l,soundfile);
	}

	printf("Closing down...\n");	
	fclose(soundfile);
	input_cleanup(handle);
}


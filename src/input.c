#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "input.h"




int input_initialize(snd_pcm_t * handle, short **buffer, int *buffer_l)
{
	int err;

	snd_pcm_hw_params_t *hwparams;

	snd_pcm_hw_params_alloca(&hwparams);

	if ((err = snd_pcm_hw_params_any(handle, hwparams)) < 0) {
		fprintf(stderr, "Error initializing hwparams\n");
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_access(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf(stderr, "Error setting access mode\n");
		return -1;
	}
	if ((err = snd_pcm_hw_params_set_format(handle, hwparams, SND_PCM_FORMAT_S16_LE)) < 0) {
		fprintf(stderr, "Error setting format\n");
		return -1;
	}
	int channels = 1;
	if ((err = snd_pcm_hw_params_set_channels(handle, hwparams, channels)) < 0) {
		fprintf(stderr, "Error setting mono mode\n");
		return -1;
	}

	int rate = 48000;

	if ((err = snd_pcm_hw_params_set_rate_near(handle, hwparams, &rate, 0)) < 0) {
		fprintf(stderr, "Error setting samplerate\n");
		return -1;
	}
	snd_pcm_uframes_t size = 4096;
	int dir = 0;

	if ((err = snd_pcm_hw_params_set_period_size_near(handle, hwparams, &size, &dir)) < 0) {
		fprintf(stderr, "Error setting buffer size\n");
		return -1;
	}

	if ((err = snd_pcm_hw_params(handle, hwparams)) < 0) {
		fprintf(stderr, "Error writing hwparams\n");
		return -1;
	}

	snd_pcm_hw_params_get_period_size(hwparams, &size, &dir);
	printf("Using sound buffer size: %d\n", (int) size);
	int extra = (int) size % 5;
	*buffer_l = (int) size - extra;

	if ((*buffer = (short *) malloc((*buffer_l) * sizeof(short))) == NULL) {
		fprintf(stderr, "Error allocating memory\n");
		return -1;
	}

	return 0;
}


void input_read(snd_pcm_t * handle, short *buffer, int count)
{
	int err;

	err = snd_pcm_readi(handle, buffer, count);
//      printf("leser %d\n",count);
	if (err == -EPIPE) {
		fprintf(stderr, "Overrun\n");
		snd_pcm_prepare(handle);
	} else if (err < 0) {
		fprintf(stderr, "Write error\n");
	} else if (err != count) {
		fprintf(stderr, "Short read, read %d frames\n", err);
	}

}

void input_cleanup(snd_pcm_t * handle)
{
	snd_pcm_close(handle);
}

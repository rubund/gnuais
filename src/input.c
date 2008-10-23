#include <strings.h>

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "input.h"
#include "hlog.h"
#include "hmalloc.h"

#define LOGPREFIX "Sound device: "

int input_initialize(snd_pcm_t * handle, short **buffer, int *buffer_l)
{
	int err;

	snd_pcm_hw_params_t *hwparams = NULL;

	snd_pcm_hw_params_alloca(&hwparams);

	if ((err = snd_pcm_hw_params_any(handle, hwparams)) < 0) {
		hlog(LOG_CRIT, LOGPREFIX "Error initializing hwparams");
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_access(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		hlog(LOG_CRIT, LOGPREFIX "Error setting acecss mode (SND_PCM_ACCESS_RW_INTERLEAVED)");
		return -1;
	}
	if ((err = snd_pcm_hw_params_set_format(handle, hwparams, SND_PCM_FORMAT_S16_LE)) < 0) {
		hlog(LOG_CRIT, LOGPREFIX "Error setting format (SND_PCM_FORMAT_S16_LE)");
		return -1;
	}
	int channels = 1;
	if ((err = snd_pcm_hw_params_set_channels(handle, hwparams, channels)) < 0) {
		hlog(LOG_CRIT, LOGPREFIX "Error setting mono mode (channels 1)");
		return -1;
	}

	int rate = 48000;

	if ((err = snd_pcm_hw_params_set_rate_near(handle, hwparams, &rate, 0)) < 0) {
		hlog(LOG_CRIT, LOGPREFIX "Error setting sample rate (%d)", rate);
		return -1;
	}
	snd_pcm_uframes_t size = 4096;
	int dir = 0;

	if ((err = snd_pcm_hw_params_set_period_size_near(handle, hwparams, &size, &dir)) < 0) {
		hlog(LOG_CRIT, LOGPREFIX "Error setting buffer size (%d)", size);
		return -1;
	}

	if ((err = snd_pcm_hw_params(handle, hwparams)) < 0) {
		hlog(LOG_CRIT, LOGPREFIX "Error writing hwparams");
		return -1;
	}

	snd_pcm_hw_params_get_period_size(hwparams, &size, &dir);
	hlog(LOG_DEBUG, LOGPREFIX "Using sound buffer size: %d", (int) size);
	int extra = (int) size % 5;
	*buffer_l = (int) size - extra;

	*buffer = (short *) hmalloc((*buffer_l) * sizeof(short));
	bzero(*buffer, *buffer_l * sizeof(short));

	return 0;
}


void input_read(snd_pcm_t * handle, short *buffer, int count)
{
	int err;

	err = snd_pcm_readi(handle, buffer, count);
//      printf("leser %d\n",count);
	if (err == -EPIPE) {
		hlog(LOG_ERR, LOGPREFIX "Overrun");
		snd_pcm_prepare(handle);
	} else if (err < 0) {
		hlog(LOG_ERR, LOGPREFIX "Write error");
	} else if (err != count) {
		hlog(LOG_ERR, LOGPREFIX "Short read, read %d frames", err);
	}

}

void input_cleanup(snd_pcm_t * handle)
{
	snd_pcm_close(handle);
}

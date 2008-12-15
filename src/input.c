#include <strings.h>

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "input.h"
#include "hlog.h"
#include "hmalloc.h"
#include "cfg.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define LOGPREFIX "Sound device: "

int input_initialize(snd_pcm_t * handle, short **buffer, int *buffer_l)
{
	int err;
	int channels, dir;

	snd_pcm_hw_params_t *hwparams = NULL;

	snd_pcm_hw_params_alloca(&hwparams);

	if ((err = snd_pcm_hw_params_any(handle, hwparams)) < 0) {
		hlog(LOG_CRIT, LOGPREFIX "Error initializing hwparams");
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_access(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		hlog(LOG_CRIT, LOGPREFIX "Error setting acecss mode (SND_PCM_ACCESS_RW_INTERLEAVED): %s", snd_strerror(err));
		return -1;
	}
	if ((err = snd_pcm_hw_params_set_format(handle, hwparams, SND_PCM_FORMAT_S16_LE)) < 0) {
		hlog(LOG_CRIT, LOGPREFIX "Error setting format (SND_PCM_FORMAT_S16_LE): %s", snd_strerror(err));
		return -1;
	}
	
	if (sound_channels == SOUND_CHANNELS_MONO)
		channels = 1;
	else
		channels = 2;
	
	if ((err = snd_pcm_hw_params_set_channels(handle, hwparams, channels)) < 0) {
		hlog(LOG_CRIT, LOGPREFIX "Error setting channels %d: %s", channels, snd_strerror(err));
		hlog(LOG_CRIT, LOGPREFIX "Maybe your sound card does not support this SoundChannels setting (mono-only or stereo-only card).");
		return -1;
	}

	unsigned int rate = 48000;
	if ((err = snd_pcm_hw_params_set_rate_near(handle, hwparams, &rate, 0)) < 0) {
		hlog(LOG_CRIT, LOGPREFIX "Error setting sample rate (%d): %s", rate, snd_strerror(err));
		return -1;
	}
	
	snd_pcm_uframes_t size = 4096; /* number of frames */
	
	dir = 0;
	if ((err = snd_pcm_hw_params_set_period_size_near(handle, hwparams, &size, &dir)) < 0) {
		hlog(LOG_CRIT, LOGPREFIX "Error setting buffer size (%d): %s", size, snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_hw_params(handle, hwparams)) < 0) {
		hlog(LOG_CRIT, LOGPREFIX "Error writing hwparams: %s", snd_strerror(err));
		return -1;
	}
	
	snd_pcm_hw_params_get_period_size(hwparams, &size, &dir);
	int extra = (int) size % 5;
	*buffer_l = (int) size - extra;
	int buffer_len_in_bytes = *buffer_l * sizeof(short) * channels;
	
	hlog(LOG_DEBUG, LOGPREFIX "Using sound buffer size: %d frames of %d channels: %d bytes",
		*buffer_l, channels, buffer_len_in_bytes);
	
	*buffer = (short *) hmalloc(buffer_len_in_bytes);
	bzero(*buffer, buffer_len_in_bytes);
	
	return 0;
}


int input_read(snd_pcm_t * handle, short *buffer, int count)
{
	int err;

	err = snd_pcm_readi(handle, buffer, count);
	
	if (err == -EPIPE) {
		hlog(LOG_ERR, LOGPREFIX "Overrun");
		snd_pcm_prepare(handle);
	} else if (err < 0) {
		hlog(LOG_ERR, LOGPREFIX "Read error");
	} else if (err != count) {
		hlog(LOG_ERR, LOGPREFIX "Short read, read %d frames", err);
	} else {
		/*hlog(LOG_DEBUG, LOGPREFIX "Read %d samples", err); */
	}
	
	return err;
}

void input_cleanup(snd_pcm_t *handle)
{
	snd_pcm_close(handle);
}

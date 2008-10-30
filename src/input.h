#ifndef INC_INPUT_H
#define INC_INPUT_H

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>



int input_initialize(snd_pcm_t * handle, short **, int *);
int input_read(snd_pcm_t * handle, short *buffer, int count);
void input_cleanup(snd_pcm_t *handle);

#endif

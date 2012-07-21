#ifndef INC_PULSEAUDIO_H
#define INC_PULSEAUDIO_H

#include <pulse/simple.h>

pa_simple *pulseaudio_initialize();

void pulseaudio_cleanup(pa_simple *s);

int pulseaudio_read(pa_simple *s, short *buffer, int count);


#endif /* INC_PULSEAUDIO_H */

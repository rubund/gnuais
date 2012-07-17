#ifndef INC_PULSEAUDIO_H
#define INC_PULSEAUDIO_H

#include <pulse/simple.h>

pa_simple *pulseaudio_initialize();

void pulseaudio_cleanup(pa_simple *s);



#endif /* INC_PULSEAUDIO_H */

#include <stdio.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include "pulseaudio.h"



pa_simple *pulseaudio_initialize(){

    pa_simple *s;
    pa_sample_spec ss;

    ss.format = PA_SAMPLE_S16NE;
    ss.channels = 2;
    ss.rate = 48000;

    s = pa_simple_new(NULL,"gnuais",PA_STREAM_RECORD,NULL,"AIS data",&ss,NULL,NULL,NULL);

    return s;
}


void pulseaudio_cleanup(pa_simple *s){

    pa_simple_free(s);

}

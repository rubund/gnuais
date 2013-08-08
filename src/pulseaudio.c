
/*
 *	pulseaudio.c
 *
 *	(c) Ruben Undheim 2012
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <stdio.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include "pulseaudio.h"
#include "cfg.h"



pa_simple *pulseaudio_initialize(){

	pa_simple *s;
	pa_sample_spec ss;

	ss.format = PA_SAMPLE_S16NE;
	if (sound_channels == SOUND_CHANNELS_MONO)
		ss.channels = 1;
	else
		ss.channels = 2;
	ss.rate = 48000;

	s = pa_simple_new(NULL,"gnuais",PA_STREAM_RECORD,NULL,"AIS data",&ss,NULL,NULL,NULL);

	return s;
}


void pulseaudio_cleanup(pa_simple *s){

	pa_simple_free(s);

}

int pulseaudio_read(pa_simple *s, short *buffer, int count){
	int number_read;
	int channels;
	if (sound_channels == SOUND_CHANNELS_MONO)
		channels = 1;
	else
		channels = 2;
	number_read = pa_simple_read(s,buffer,(size_t)(count * sizeof(short) * channels) ,NULL);
	if(number_read < 0)
		return -1;
	return count; 
}

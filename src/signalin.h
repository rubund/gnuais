#ifndef INC_SIGNAL_H
#define INC_SIGNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>



void signal_filter(short *buffer, int count, float *bufout);
void signal_clockrecovery(float *bufin, int count, float *bufout);
void signal_bitslice(float *bufin, int count, char *bufout, char *last);



#endif

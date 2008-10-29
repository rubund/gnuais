#ifndef INC_SIGNAL_H
#define INC_SIGNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>



extern void signal_filter(short *buffer, int buf_ch_num, int buf_ch_ofs, int count, float *bufout);
extern void signal_clockrecovery(float *bufin, int count, float *bufout);
extern void signal_bitslice(float *bufin, int count, char *bufout, char *last);



#endif

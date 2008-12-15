
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <time.h>

#include "signalin.h"
#include "hlog.h"
#include "cfg.h"
#include "hmalloc.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static float coeffs[] =  {
	0.00084686721675097942, 0.00091766606783494353, -1.2112550466221181e-18, -0.001315040048211813, -0.0016600260278210044,
	2.0942120342892902e-18, 0.0026929380837827921, 0.003403655719012022, -3.6154720406197441e-18, -0.0052816560491919518,
	-0.0064831161871552467, 2.1757727044315087e-17, 0.0095260320231318474, 0.0114327073097229, -7.7247841418234466e-18,
	-0.016265831887722015, -0.019352054223418236, 9.7787579752150947e-18, 0.027568245306611061, 0.033225845545530319,
	-1.1472294474131661e-17, -0.050564087927341461, -0.065182454884052277, 1.2585288322198299e-17, 0.13577644526958466,
	0.27430874109268188, 0.33281025290489197, 0.27430874109268188, 0.13577644526958466, 1.2585288322198299e-17,
	-0.065182454884052277, -0.050564087927341461, -1.1472294474131661e-17, 0.033225845545530319, 0.027568245306611061,
	9.7787579752150947e-18, -0.019352054223418236, -0.016265831887722015, -7.7247841418234466e-18, 0.0114327073097229,
	0.0095260320231318474, 2.1757727044315087e-17, -0.0064831161871552467, -0.0052816560491919518, -3.6154720406197441e-18,
	0.003403655719012022, 0.0026929380837827921, 2.0942120342892902e-18, -0.0016600260278210044, -0.001315040048211813,
	-1.2112550466221181e-18, 0.00091766606783494353, 0.00084686721675097942
};

#define COEFFS_L 53
short *tmpbuf[2] = { 0, 0 };

void signalin_deinit(void)
{
	if (tmpbuf[0])
		hfree(tmpbuf[0]);
	if (tmpbuf[1])
		hfree(tmpbuf[1]);
}

void signal_filter(short *buffer, int buf_ch_num, int buf_ch_ofs, int count, float *bufout)
{
	//static short tmpbuf[2][53];
	static time_t last_levellog[2] = { 0, 0 };
	time_t level_distance;
	int i, j, tmp, tmp2;
	double sum;
	
	if (!tmpbuf[buf_ch_ofs]) {
		printf("allocating tmpbuf for ch %d\n", buf_ch_ofs);
		tmpbuf[buf_ch_ofs] = hmalloc(COEFFS_L*sizeof(short));
	}
	
	/* check level */
	short v;
	short max = 0;
	for (i = 0; i < count; i++) {
		v = abs(buffer[ (i*buf_ch_num)+buf_ch_ofs ]);
		if (v > max)
			max = v;
	}
	
	float level = (float)max / (float)32768 * (float)100;
	level_distance = time(NULL) - last_levellog[buf_ch_ofs];
	
	if (level > 98.0 && (level_distance >= 30 || level_distance >= sound_levellog)) {
		hlog(LOG_NOTICE, "Level on ch %d too high: %.0f %%", buf_ch_ofs, level);
		time(&last_levellog[buf_ch_ofs]);
	} else if (sound_levellog != 0 && level_distance >= sound_levellog) {
		hlog(LOG_INFO, "Level on ch %d: %.0f %%", buf_ch_ofs, level);
		time(&last_levellog[buf_ch_ofs]);
	}
	
	/* get on with the filtering... */
	for (i = 0; i < count; i++) {
		sum = 0;
		for (j = i; j < COEFFS_L; j++) {
			sum += coeffs[j - i] * ((float) tmpbuf[buf_ch_ofs][j] * (1.0 / 32768));
		}
		
		if (i > COEFFS_L)
			tmp = COEFFS_L;
		else
			tmp = i;
			
		tmp2 = i - COEFFS_L;
		
		if (tmp2 > 0)
			tmp2 = tmp2;
		else
			tmp2 = 0;
			
		for (j = 0; j < tmp; j++) {
			sum += coeffs[j + COEFFS_L - tmp] * ((float) buffer[ ((tmp2 + j) * buf_ch_num) + buf_ch_ofs ] * (1.0 / 32768));
		}
		
		bufout[i] = sum;
	}
	
	/* copy last COEFFS_L samples to tmpbuf */
	for (j = 0; j < COEFFS_L; j++) {
		tmpbuf[buf_ch_ofs][j] = buffer[ (count - COEFFS_L + j) * buf_ch_num + buf_ch_ofs ];
	}
}

void signal_clockrecovery(float *bufin, int count, float *bufout)
{
	/* This function needs improvements!!! */
	float sum = 0;
	int i, j;
	for (i = 0; i < (count / 5); i++) {
		sum = 0;
/*		for (j = 0; j < 5; j++)
			sum += bufin[i * 5 + j];
		bufout[i] = sum;*/
		bufout[i] = bufin[i*5];   // This actually works better than take the average of 5 samples.
	}
}

void signal_bitslice(float *bufin, int count, char *bufout, char *last)
{
	char now;
	int i;
	for (i = 0; i < (count / 5); i++) {
		if (bufin[i] > 0)
			now = 1;
		else
			now = 0;

		bufout[i] = (now - (*last)) % 2;

		if (bufout[i] == 0)
			bufout[i] = 1;
		else
			bufout[i] = 0;
		*last = now;
	}
}

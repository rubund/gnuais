
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <time.h>

#include "receiver.h"
#include "hlog.h"
#include "cfg.h"
#include "hmalloc.h"
#include "filter.h"

static float coeffs[] =  {
	 0.00084686721675097942,  0.00091766606783494353, 
	-1.2112550466221181e-18, -0.001315040048211813, 
	-0.0016600260278210044,	  2.0942120342892902e-18, 
	 0.0026929380837827921,   0.003403655719012022, 
	-3.6154720406197441e-18, -0.0052816560491919518,
	-0.0064831161871552467,   2.1757727044315087e-17, 
	 0.0095260320231318474,   0.0114327073097229, 
	-7.7247841418234466e-18, -0.016265831887722015, 
	-0.019352054223418236,    9.7787579752150947e-18, 
	 0.027568245306611061,    0.033225845545530319,
	-1.1472294474131661e-17, -0.050564087927341461, 
	-0.065182454884052277,    1.2585288322198299e-17, 
	 0.13577644526958466,     0.27430874109268188, 
	 0.33281025290489197,     0.27430874109268188, 
	 0.13577644526958466,     1.2585288322198299e-17,
	-0.065182454884052277,   -0.050564087927341461, 
	-1.1472294474131661e-17,  0.033225845545530319, 
	 0.027568245306611061,    9.7787579752150947e-18, 
	-0.019352054223418236,   -0.016265831887722015, 
	-7.7247841418234466e-18,  0.0114327073097229,
	 0.0095260320231318474,   2.1757727044315087e-17, 
	-0.0064831161871552467,  -0.0052816560491919518, 
	-3.6154720406197441e-18,  0.003403655719012022, 
	 0.0026929380837827921,   2.0942120342892902e-18, 
	-0.0016600260278210044,  -0.001315040048211813,
	-1.2112550466221181e-18,  0.00091766606783494353, 
	 0.00084686721675097942
};

#define COEFFS_L 53

struct receiver *init_receiver(char name, int num_ch, int ch_ofs)
{
	struct receiver *rx;

	rx = (struct receiver *) hmalloc(sizeof(struct receiver));
	memset(rx, 0, sizeof(struct receiver));

	rx->filter = filter_init(COEFFS_L, coeffs);

	rx->decoder = hmalloc(sizeof(struct demod_state_t));
	protodec_initialize(rx->decoder, NULL, name);

	rx->name = name;
	rx->lastbit = 0;
	rx->num_ch = num_ch;
	rx->ch_ofs = ch_ofs;
	rx->pll = 0;
	rx->pllinc = 0x10000 / 5;
	rx->prev = 0;

	return rx;
}

void free_receiver(struct receiver *rx)
{
	if (rx) {
		filter_free(rx->filter);
		hfree(rx);
	}
}

#define	INC	16

void receiver_run(struct receiver *rx, short *buf, int len)
{
	float out;
	int curr, bit;
	char b;

	len = (len * rx->num_ch) - rx->ch_ofs;
	buf += rx->ch_ofs;

	while (len > 0) {
		static int u = 0;

		rx->decoder->cntr++;

		filter_run(rx->filter, *buf / 32768.0, &out);

		curr = (out > 0);
		if ((curr ^ rx->prev) == 1) {
			if (rx->pll < (0x10000 / 2)) {
				rx->pll += rx->pllinc / INC;
				u += rx->pllinc / INC;
			} else {
				rx->pll -= rx->pllinc / INC;
				u -= rx->pllinc / INC;
			}
		}
		rx->prev = curr;

		rx->pll += rx->pllinc;

		if (rx->pll > 0xffff) {
			/* slice */
			bit = (out > 0);
			/* nrzi decode */
			b = !(bit ^ rx->lastbit);
			/* feed to the decoder */
			protodec_decode(&b, 1, rx->decoder);

			rx->lastbit = bit;
			rx->pll &= 0xffff;
		}

		buf += rx->num_ch;
		len -= rx->num_ch;
	}
}


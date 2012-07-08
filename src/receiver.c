
#if HAVE_CONFIG_H
#include "../config.h"
#endif

#include <string.h>
#include <time.h>

#include "receiver.h"
#include "hlog.h"
#include "cfg.h"
#include "hmalloc.h"
#include "filter.h"


static float coeffs[]={
   2.5959e-55, 2.9479e-49, 1.4741e-43, 3.2462e-38, 3.1480e-33,
   1.3443e-28, 2.5280e-24, 2.0934e-20, 7.6339e-17, 1.2259e-13,
   8.6690e-11, 2.6996e-08, 3.7020e-06, 2.2355e-04, 5.9448e-03,
   6.9616e-02, 3.5899e-01, 8.1522e-01, 8.1522e-01, 3.5899e-01,
   6.9616e-02, 5.9448e-03, 2.2355e-04, 3.7020e-06, 2.6996e-08,
   8.6690e-11, 1.2259e-13, 7.6339e-17, 2.0934e-20, 2.5280e-24,
   1.3443e-28, 3.1480e-33, 3.2462e-38, 1.4741e-43, 2.9479e-49,
   2.5959e-55
};
#define COEFFS_L 36 


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
	rx->last_levellog = 0;

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
	short maxval = 0;
	int level_distance;
	float level;

	len = (len * rx->num_ch) - rx->ch_ofs;
	buf += rx->ch_ofs;

	while (len > 0) {
		static int u = 0;

		rx->decoder->cntr++;
		
		// look for peak volume
		if (*buf > maxval)
			maxval = *buf;

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
	
	/* calculate level, and log it */
	level = (float)maxval / (float)32768 * (float)100;
	level_distance = time(NULL) - rx->last_levellog;
	
	if (level > 95.0 && (level_distance >= 30 || level_distance >= sound_levellog)) {
		hlog(LOG_NOTICE, "Level on ch %d too high: %.0f %%", rx->ch_ofs, level);
		time(&rx->last_levellog);
	} else if (sound_levellog != 0 && level_distance >= sound_levellog) {
		hlog(LOG_INFO, "Level on ch %d: %.0f %%", rx->ch_ofs, level);
		time(&rx->last_levellog);
	}
}


/*
 *    filter.c  --  FIR filter
 *
 *    Copyright (C) 2001, 2002, 2003
 *      Tomi Manninen (oh2bns@sral.fi)
 *
 *    This file is part of gMFSK.
 *
 *    gMFSK is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    gMFSK is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with gMFSK; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdlib.h>
#include <string.h>

#include "hmalloc.h"
#include "filter.h"

#undef	DEBUG

#ifdef DEBUG
#include <stdio.h>
#endif

/* ---------------------------------------------------------------------- */

/*
 * This gets used when not optimising
 */
#ifndef	__OPTIMIZE__
float mac(const float *a, const float *b, int len)
{ 
	float sum = 0;
	int i;

	for (i = 0; i < len; i++)
		sum += (*a++) * (*b++);
	return sum;
}
#endif

/* ---------------------------------------------------------------------- */

struct filter *filter_init(int len, float *taps)
{
	struct filter *f;

	f = (struct filter *) hmalloc(sizeof(struct filter));
	memset(f, 0, sizeof(struct filter));

	f->taps = (float *) hmalloc(len * sizeof(float));
	memcpy(f->taps, taps, len * sizeof(float));

	f->length = len;
	f->pointer = f->length;

	return f;
}

void filter_free(struct filter *f)
{
	if (f) {
		hfree(f->taps);
		hfree(f);
	}
}

/* ---------------------------------------------------------------------- */

void filter_run(struct filter *f, float in, float *out)
{
	float *ptr = f->buffer + f->pointer++;

	*ptr = in;
	
	// TODO: optimize: pass filter length as constant to enable
	// using optimized __mac_c and fix the number of rounds there!
	*out = mac(ptr - f->length, f->taps, f->length);
	//*out = mac(ptr - f->length, f->taps, 53);

	if (f->pointer == BufferLen) {
		memcpy(f->buffer, 
		       f->buffer + BufferLen - f->length,
		       f->length * sizeof(float));
		f->pointer = f->length;
	}
}

short filter_run_buf(struct filter *f, short *in, float *out, int step, int len)
{
	int id = 0;
	int od = 0;
	short maxval = 0;
	int pointer = f->pointer;
	float *buffer = f->buffer;
	
	while (od < len) {
	        buffer[pointer] = in[id];
		
		// look for peak volume
		if (in[id] > maxval)
			maxval = in[id];
		
		out[od] = mac(&buffer[pointer - f->length], f->taps, f->length);
		pointer++;
		
		/* the buffer is much smaller than the incoming chunks */
		if (pointer == BufferLen) {
			memcpy(buffer, 
			       buffer + BufferLen - f->length,
			       f->length * sizeof(float));
			pointer = f->length;
		}
		
		id += step;
		od++;
	}
	
	f->pointer = pointer;
	
	return maxval;
}

/* ---------------------------------------------------------------------- */

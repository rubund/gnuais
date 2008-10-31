
#include <stdio.h>
#include <time.h>
#include <string.h>		/* String function definitions */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "protodec.h"
#include "ais.h"
#include "out_mysql.h"
#include "hmalloc.h"
#include "cfg.h"
#include "hlog.h"

#define SERBUFFER_LEN	100
char *serbuffer = NULL;
char *nmea = NULL;

void protodec_initialize(struct demod_state_t *d, struct serial_state_t *serial, char chanid)
{
	d->chanid = chanid;
	d->serial = serial;
	
	d->receivedframes = 0;
	d->lostframes = 0;
	d->lostframes2 = 0;
	
	protodec_reset(d);
	
	d->seqnr = 0;
	
	d->buffer = hmalloc(DEMOD_BUFFER_LEN);
	d->rbuffer = hmalloc(DEMOD_BUFFER_LEN);
	
	if (!serbuffer)
		serbuffer = hmalloc(SERBUFFER_LEN);
	if (!nmea)
		nmea = hmalloc(SERBUFFER_LEN);
}


void protodec_reset(struct demod_state_t *d)
{
	d->state = ST_SKURR;
	d->nskurr = 0;
	d->ndata = 0;
	d->npreamble = 0;
	d->nstartsign = 0;
	d->nstopsign = 0;
	d->antallpreamble = 0;
	d->antallenner = 0;
	d->last = 0;
	d->bitstuff = 0;
	d->bufferpos = 0;
}

void protodec_getdata(int bufferlengde, struct demod_state_t *d)
{
	int serbuffer_l;
	
	unsigned char type = protodec_henten(0, 6, d->rbuffer);
	if (type < 1 || type > 9)
		return;
	unsigned long mmsi = protodec_henten(8, 30, d->rbuffer);
	unsigned long day, hour, minute, second, year, month;
	int longitude, latitude;
	unsigned short coarse, sog, heading;
	char name[21];
	char destination[21];
	char rateofturn, underway;
	unsigned int A, B;
	unsigned char C, D;
	unsigned char draught;
	int m;
	unsigned char sentences, sentencenum, nmeachk;
	int senlen;
	unsigned char fillbits;
	float longit, latit;
	int k, hvor, letter;
	time_t tid;
	time(&tid);

	DBG(printf("Bufferlen: %d,", bufferlengde));
	
	if (bufferlengde % 6 > 0) {
		fillbits = 6 - (bufferlengde % 6);
		for (m = bufferlengde; m < bufferlengde + fillbits; m++) {
			d->rbuffer[m] = 0;
		}
		bufferlengde = bufferlengde + fillbits;
	}

	DBG(printf(" fixed Bufferlen: %d with %d fillbits\n", bufferlengde, fillbits));
	
	//6bits to nmea-ascii. One sentence len max 82char 
	//inc. head + tail.This makes inside datamax 62char multipart, 62 single
	senlen = 61;		//this is normally not needed.For testing only. May be fixed number
	if (bufferlengde <= (senlen * 6)) {
		sentences = 1;
	} else {
		sentences = bufferlengde / (senlen * 6);
		//sentences , if overflow put one more
		if (bufferlengde % (senlen * 6) != 0)
			sentences++;
	};
	DBG(printf("%d sentences with max data of %d ascii chrs\n", sentences, senlen));
	sentencenum = 0;
	hvor = 0;
	do {
		k = 13;		//leave room for nmea header
		while (k < (senlen + 13) && bufferlengde > hvor) {
			letter = protodec_henten(hvor, 6, d->rbuffer);
			//6bit-to-ascii conversion by IEC
			if (letter < 40)
				letter = letter + 48;
			else
				letter = letter + 56;
			nmea[k] = letter;
			hvor += 6;
			k++;
		}
		DBG(printf("Drop from loop with k:%d hvor:%d senlen:%d bufferlengde\n",
			k, hvor, senlen, bufferlengde));
		//set nmea trailer with 00 checksum (calculate later)    
		nmea[k] = 44;
		nmea[k + 1] = 48;
		nmea[k + 2] = 42;
		nmea[k + 3] = 48;
		nmea[k + 4] = 48;
		nmea[k + 5] = 0;
		sentencenum++;

		// printout one frame starts here
		//AIVDM,x,x,,, - header comes here first

		nmea[0] = 65;
		nmea[1] = 73;
		nmea[2] = 86;
		nmea[3] = 68;
		nmea[4] = 77;
		nmea[5] = 44;
		nmea[6] = 48 + sentences;
		nmea[7] = 44;
		nmea[8] = 48 + sentencenum;
		nmea[9] = 44;

		//if multipart message it needs sequential id number
		if (sentences > 1) {
			DBG(printf("It is multipart (%d/%d) add seq.nr.(%d) to header\n",
				sentences, sentencenum, d->seqnr));
			nmea[10] = d->seqnr + 48;
			nmea[11] = 44;
			nmea[12] = 44;
			//and if the last of multipart we need to show fillbits at trailer
			if (sentencenum == sentences) {
				DBG(printf("It is last of multipart (%d/%d) add fillbits (%d) to trailer\n",
					sentences, sentencenum, fillbits));
				nmea[k + 1] = 48 + fillbits;
			}
		} else {	//else put channel A & no seqnr to keep equal lenght (foo!)
			nmea[10] = 44;
			nmea[11] = 65;
			nmea[12] = 44;
		}

		//strcpy(nmea,"!AIVDM,1,1,,,");
		//calculate xor checksum in hex for nmea[0] until nmea[m]='*'(42)
		char nchk[2];
		nmeachk = nmea[0];
		m = 1;
		while (nmea[m] != 42) {	//!="*"
			nmeachk = nmeachk ^ nmea[m];
			m++;
		}
		// convert calculated checksum to 2 digit hex there are 00 as base
		// so if only 1 digit put it to later position to get 0-header 01,02...
		nchk[0] = 0;
		nchk[1] = 0;
		sprintf(nchk, "%X", nmeachk);
		if (nchk[1] == 0) {
			nmea[k + 4] = nchk[0];
		} else {
			nmea[k + 3] = nchk[0];
			nmea[k + 4] = nchk[1];
		}
		//In final. Add header "!" and trailer <cr><lf>
		// here it could be sent to /dev/ttySx
		serbuffer_l = snprintf(serbuffer, SERBUFFER_LEN, "!%s\r\n", nmea);
		if (d->serial)
			serial_write(d->serial, serbuffer, serbuffer_l);
		DBG(printf("End of nmea->ascii-loop with sentences:%d sentencenum:%d\n",
			sentences, sentencenum));
		if (my)
			myout_nmea(my, (int) tid, nmea);
	}
	while (sentencenum < sentences);
	//multipart message ready. Increase seqnr for next one
	//rolling 1-9. Single msg ready may also increase this, no matter.
	d->seqnr++;
	if (d->seqnr > 9)
		d->seqnr = 0;
	
	if (type < 0 || type > MAX_AIS_PACKET_TYPE)
		return; // unsupported packet type
		
	if (skip_type[type])
		return; // ignored by configuration
	
	printf("(ch %c cntr %ld type %d): ", d->chanid, cntr, type);
	switch (type) {
	case 1:
		longitude = protodec_henten(61, 28, d->rbuffer);
		if (((longitude >> 27) & 1) == 1)
			longitude |= 0xF0000000;
		latitude = protodec_henten(38 + 22 + 29, 27, d->rbuffer);
		if (((latitude >> 26) & 1) == 1)
			latitude |= 0xf8000000;
		coarse = protodec_henten(38 + 22 + 28 + 28, 12, d->rbuffer);
		sog = protodec_henten(50, 10, d->rbuffer);
		rateofturn = protodec_henten(38 + 2, 8, d->rbuffer);
		underway = protodec_henten(38, 2, d->rbuffer);
		heading = protodec_henten(38 + 22 + 28 + 28 + 12, 9, d->rbuffer);
		printf("%09ld %10f %10f %5f %5f %5i %5d %5d",
			mmsi, (float) latitude / 600000,
			(float) longitude / 600000,
			(float) coarse / 10, (float) sog / 10,
			rateofturn, underway, heading);
		printf("  ( !%s )", nmea);
		
		if (my) {
			myout_ais_position(my, (int) tid, (int) mmsi,
				(float) latitude / 600000,
				(float) longitude / 600000,
				(float) heading, (float) coarse / 10,
				(float) sog / 10);
		}
		
		break;
		
	case 2:
		longitude = protodec_henten(61, 28, d->rbuffer);
		if (((longitude >> 27) & 1) == 1)
			longitude |= 0xF0000000;
		latitude = protodec_henten(38 + 22 + 29, 27, d->rbuffer);
		if (((latitude >> 26) & 1) == 1)
			latitude |= 0xf8000000;
		coarse = protodec_henten(38 + 22 + 28 + 28, 12, d->rbuffer);
		sog = protodec_henten(50, 10, d->rbuffer);
		rateofturn = protodec_henten(38 + 2, 8, d->rbuffer);
		underway = protodec_henten(38, 2, d->rbuffer);
		heading = protodec_henten(38 + 22 + 28 + 28 + 12, 9, d->rbuffer);
		printf("%09ld %10f %10f %5f %5f %5i %5d %5d",
			mmsi, (float) latitude / 600000,
			(float) longitude / 600000,
			(float) coarse / 10, (float) sog / 10,
			rateofturn, underway, heading);
		printf("  ( !%s )", nmea);
		
		if (my) {
			myout_ais_position(my, (int) tid, (int) mmsi,
				(float) latitude / 600000,
				(float) longitude / 600000,
				(float) heading, (float) coarse / 10,
				(float) sog / 10);
		}
		
		break;
		
	case 3:
		longitude = protodec_henten(61, 28, d->rbuffer);
		if (((longitude >> 27) & 1) == 1)
			longitude |= 0xF0000000;
		latitude = protodec_henten(38 + 22 + 29, 27, d->rbuffer);
		if (((latitude >> 26) & 1) == 1)
			latitude |= 0xf8000000;
		coarse = protodec_henten(38 + 22 + 28 + 28, 12, d->rbuffer);
		sog = protodec_henten(50, 10, d->rbuffer);
		rateofturn = protodec_henten(38 + 2, 8, d->rbuffer);
		underway = protodec_henten(38, 2, d->rbuffer);
		heading = protodec_henten(38 + 22 + 28 + 28 + 12, 9, d->rbuffer);
		printf("%09ld %10f %10f %5f %5f %5i %5d %5d",
			mmsi, (float) latitude / 600000,
			(float) longitude / 600000,
			(float) coarse / 10, (float) sog / 10,
			rateofturn, underway, heading);
		printf("  ( !%s )", nmea);
		
		if (my) {
			myout_ais_position(my, (int) tid, (int) mmsi,
				(float) latitude / 600000,
				(float) longitude / 600000,
				(float) heading, (float) coarse / 10,
				(float) sog / 10);
		}
		
		break;
		
	case 4:
		year = protodec_henten(40, 12, d->rbuffer);
		month = protodec_henten(52, 4, d->rbuffer);
		day = protodec_henten(56, 5, d->rbuffer);
		hour = protodec_henten(61, 5, d->rbuffer);
		minute = protodec_henten(66, 6, d->rbuffer);
		second = protodec_henten(72, 6, d->rbuffer);
		longitude = protodec_henten(79, 28, d->rbuffer);
		if (((longitude >> 27) & 1) == 1)
			longitude |= 0xF0000000;
		longit = ((float) longitude) / 10000 / 60;
		latitude = protodec_henten(107, 27, d->rbuffer);
		if (((latitude >> 26) & 1) == 1)
			latitude |= 0xf8000000;
		latit = ((float) latitude) / 10000 / 60;
		printf("%09ld %ld %ld %ld %ld %ld %ld %f %f",
			mmsi, year, month, day, hour, minute,
			second, latit, longit);
		printf("  ( !%s )", nmea);
		
		if (my) {
			myout_ais_basestation(my, (int) tid, (int) mmsi,
				(float) latitude / 600000,
				(float) longitude / 600000);
		}
		
		break;
		
	case 5:
		hvor = 112;
		for (k = 0; k < 20; k++) {
			letter = protodec_henten(hvor, 6, d->rbuffer);
			protodec_bokstavtabell(letter, name, k);
			hvor += 6;
		}
		name[20] = 0;
		// printf("Name: %s\n", name);
		hvor = 120 + 106 + 68 + 8;
		for (k = 0; k < 20; k++) {
			letter = protodec_henten(hvor, 6, d->rbuffer);
			protodec_bokstavtabell(letter, destination, k);
			hvor += 6;
		}
		destination[20] = 0;
		// printf("Destination: %s\n",destination);
		A = protodec_henten(240, 9, d->rbuffer);
		B = protodec_henten(240 + 9, 9, d->rbuffer);
		C = protodec_henten(240 + 9 + 9, 6, d->rbuffer);
		D = protodec_henten(240 + 9 + 9 + 6, 6, d->rbuffer);
		draught = protodec_henten(294, 8, d->rbuffer);
		// printf("Length: %d\nWidth: %d\nDraught: %f\n",A+B,C+D,(float)draught/10);
		printf("%09ld %s %s %d %d %f", mmsi,
			name, destination, A + B, C + D,
			(float) draught / 10);
		printf("  ( !%s )", nmea);
		
		if (my) {
			myout_ais_vesseldata(my, (int) tid, (int) mmsi,
				name, destination,
				(float) draught / 10,
				(int) A, (int) B, (int) C, (int) D);
		}
		
		break;
		
	default:
		printf("  ( !%s )", nmea);
		break;
	}
	printf("\n");
}



void protodec_decode(char *in, int count, struct demod_state_t *d)
{
	int i = 0;
	int bufferlengde, correct;
	while (i < (count / 5)) {
		switch (d->state) {
		case ST_DATA:
			if (d->bitstuff) {
				if (in[i] == 1) {
					d->state = ST_STOPSIGN;
					d->ndata = 0;
					DBG(printf("%d", in[i]));
					d->bitstuff = 0;
				} else {
					d->ndata++;
					d->last = in[i];
					d->bitstuff = 0;
				}
			} else {
				if (in[i] == d->last && in[i] == 1) {
					d->antallenner++;
					if (d->antallenner == 4) {
						d->bitstuff = 1;
						d->antallenner = 0;
					}

				} else
					d->antallenner = 0;

				DBG(printf("%d", in[i]));
				
				d->buffer[d->bufferpos] = in[i];
				d->bufferpos++;
				d->ndata++;
				if (d->bufferpos >= 449) {
					protodec_reset(d);
				}
			}
			break;

		case ST_SKURR:	// The state when no reasonable input is coming
			if (in[i] != d->last)
				d->antallpreamble++;
			else
				d->antallpreamble = 0;
			d->last = in[i];
			if (d->antallpreamble > 14 && in[i] == 0) {
				d->state = ST_PREAMBLE;
				d->nskurr = 0;
				d->antallpreamble = 0;
				DBG(printf("Preamble\n"));
			}
			d->nskurr++;
			break;

		case ST_PREAMBLE:	// Switches to this state when preamble has been discovered
			DBG(printf("..%d..", in[i]));
			if (in[i] != d->last && d->nstartsign == 0) {
				d->antallpreamble++;
			} else {
				if (in[i] == 1)	//To ettal har kommet etter hverandre 
				{
					if (d->nstartsign == 0) {	// Forste gang det skjer
						d->nstartsign = 3;
						d->last = in[i];
					} else if (d->nstartsign == 5)	// Har oppdaget start av startsymbol
					{
						d->nstartsign++;
						d->npreamble = 0;
						d->antallpreamble = 0;
						d->state = ST_STARTSIGN;
					} else {
						d->nstartsign++;
					}

				} else	//To nuller har kommet etter hverandre
				{
					if (d->nstartsign == 0) {
						d->nstartsign = 1;

					} else {
						protodec_reset(d);
					}
				}
			}
			d->npreamble++;
			break;

		case ST_STARTSIGN:
			//printf("..%d..",in[i]);
			//printf("Startsign: %d\n",d->nstartsign);
			if (d->nstartsign >= 7) {
				if (in[i] == 0) {
					DBG(printf("\nData:\n"));
					d->state = ST_DATA;
					d->nstartsign = 0;
					d->antallenner = 0;
					memset(d->buffer, 0, sizeof(d->buffer));
					d->bufferpos = 0;
				} else {
					protodec_reset(d);
				}

			} else if (in[i] == 0) {
				protodec_reset(d);
			}
			d->nstartsign++;
			break;

		case ST_STOPSIGN:
			bufferlengde = d->bufferpos - 6 - 16;
			if (in[i] == 0) {
				DBG(printf("%d\n\nFrame received OK.  %d bits\n",
					in[i], bufferlengde));
				correct = protodec_calculate_crc(bufferlengde, d);
				if (correct) {
					DBG(printf("CRC Checksum correct! Frame Successfully Received!!!\n"));
					d->receivedframes++;
					protodec_getdata(bufferlengde, d);
				} else {
					DBG(printf("CRC Checksum FALSE!!!\n"));
					d->lostframes++;
				}
				DBG(printf("_________________________________________________________\n\n"));
			} else {
				DBG(printf("\n\nERROR in Frame\n__________________________________________________________\n\n"));
				d->lostframes2++;
			}
			protodec_reset(d);
			break;


		}
		d->last = in[i];
		i++;
	}
}


unsigned long protodec_henten(int from, int size, unsigned char *frame)
{
	int i = 0;
	unsigned long tmp = 0;
	
	for (i = 0; i < size; i++)
		tmp |= (frame[from + i]) << (size - 1 - i);
	
	return tmp;
}

void protodec_bokstavtabell(char bokstav, char *name, int pos)
{
	name[pos] = bokstav + 64;
	if (name[pos] == 64 || name[pos] == 96)
		name[pos] = ' ';

//      if (letter < 40) letter = letter + 48;
//          else letter = letter + 56;
}

unsigned short protodec_sdlc_crc(unsigned char *data, unsigned len)	// Calculates CRC-checksum
{
	unsigned short c, crc = 0xffff;

	while (len--)
		for (c = 0x100 + *data++; c > 1; c >>= 1)
			if ((crc ^ c) & 1)
				crc = (crc >> 1) ^ 0x8408;
			else
				crc >>= 1;
	return ~crc;

}

int protodec_calculate_crc(int lengde, struct demod_state_t *d)
{
	int antallbytes;
	unsigned char *data;
	int i, j, x;
	unsigned char tmp;
	
	if (lengde <= 0) {
		hlog(LOG_ERR, "protodec_calculate_crc: length <= 0!");
		return 0;
	}
	
	antallbytes = lengde / 8;
	data = (unsigned char *) hmalloc(sizeof(unsigned char) * (antallbytes + 2));
	for (j = 0; j < antallbytes + 2; j++) {
		tmp = 0;
		for (i = 0; i < 8; i++)
			tmp |= (((d->buffer[i + 8 * j]) << (i)));
		data[j] = tmp;
	}
	unsigned short crc = protodec_sdlc_crc(data, antallbytes + 2);
//DBG(printf("CRC: %04x\n",crc));
	memset(d->rbuffer, 0, sizeof(d->rbuffer));
	for (j = 0; j < antallbytes; j++) {
		for (i = 0; i < 8; i++) {
			x = j * 8 + i;
			if (x >= DEMOD_BUFFER_LEN) {
				hlog(LOG_ERR, "protodec_calculate_crc: would run over rbuffer length");
				hfree(data);
				return 0;
			} else {
				d->rbuffer[x] = (data[j] >> (7 - i)) & 1;
			}
		}
	}
	hfree(data);
	return (crc == 0x0f47);
}


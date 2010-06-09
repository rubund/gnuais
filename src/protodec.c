
#include <pthread.h>
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
#include "cache.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif
//#define DEBUG_NMEA
#ifdef DEBUG_NMEA
#define NMEA_DBG(x) x
#else
#define NMEA_DBG(x)
#endif


#define SERBUFFER_LEN	100

void protodec_initialize(struct demod_state_t *d, struct serial_state_t *serial, char chanid)
{
	memset(d, 0, sizeof(struct demod_state_t));

	d->chanid = chanid;
	d->serial = serial;
	
	d->receivedframes = 0;
	d->lostframes = 0;
	d->lostframes2 = 0;
	
	protodec_reset(d);
	
	d->seqnr = 0;
	
	d->buffer = hmalloc(DEMOD_BUFFER_LEN);
	d->rbuffer = hmalloc(DEMOD_BUFFER_LEN);
	
	d->serbuffer = hmalloc(SERBUFFER_LEN);
	d->nmea = hmalloc(SERBUFFER_LEN);
}

void protodec_deinit(struct demod_state_t *d)
{
	hfree(d->buffer);
	hfree(d->rbuffer);
	hfree(d->serbuffer);
	hfree(d->nmea);
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

/*
 *	Mark trailing spaces as NULL bytes in a string
 */

static void remove_trailing_spaces(char *s, int len)
{
	int i;
	
	s[len] = 0;
	for (i = len-1; i >= 0; i--) {
		if (s[i] == ' ' || s[i] == 0)
			s[i] = 0;
		else
			i = -1;
	}
}

/*
 *	Decode 6-bit ASCII to normal 8-bit ASCII
 */

void protodec_decode_sixbit_ascii(char sixbit, char *name, int pos)
{
	if (sixbit >= 1 && sixbit <= 31) {
		name[pos] = sixbit + 64;
		return;
	}
	
	if (sixbit >= 32 && sixbit <= 63) {
		name[pos] = sixbit;
		return;
	}
	
	name[pos] = ' ';
}

unsigned long protodec_henten(int from, int size, unsigned char *frame)
{
	int i = 0;
	unsigned long tmp = 0;
	
	for (i = 0; i < size; i++)
		tmp |= (frame[from + i]) << (size - 1 - i);
	
	return tmp;
}

#define NCHK_LEN 3

void protodec_getdata(int bufferlengde, struct demod_state_t *d)
{
	int serbuffer_l;
	
	unsigned char type = protodec_henten(0, 6, d->rbuffer);
	if (type < 1 || type > MAX_AIS_PACKET_TYPE /* 9 */)
		return;
	unsigned long mmsi = protodec_henten(8, 30, d->rbuffer);
	unsigned long imo;
	unsigned int shiptype;
	unsigned long day, hour, minute, second, year, month;
	int longitude, latitude;
	unsigned short course, sog, heading;
	char callsign[7];
	char name[21];
	char destination[21];
	char rateofturn, navstat;
	unsigned int A, B;
	unsigned char C, D;
	unsigned char draught;
	int m;
	unsigned char sentences, sentencenum, nmeachk, partnr;
	int senlen;
	unsigned char fillbits = 0;
	float longit, latit;
	int k, hvor, letter;
	char nchk[NCHK_LEN];
	time_t received_t;
	time(&received_t);
	
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
	NMEA_DBG(printf("NMEA: %d sentences with max data of %d ascii chrs\n", sentences, senlen));
	sentencenum = 0;
	hvor = 0;
	do {
		k = 13;		//leave room for nmea header
		while (k < senlen + 13 && bufferlengde > hvor) {
			letter = protodec_henten(hvor, 6, d->rbuffer);
			// 6bit-to-ascii conversion by IEC
			if (letter < 40)
				letter = letter + 48;
			else
				letter = letter + 56;
			d->nmea[k] = letter;
			hvor += 6;
			k++;
		}
		NMEA_DBG(printf("NMEA: Drop from loop with k:%d hvor:%d senlen:%d bufferlengde\n",
			k, hvor, senlen, bufferlengde));
		//set nmea trailer with 00 checksum (calculate later)
		d->nmea[k] = 44;
		d->nmea[k + 1] = 48;
		d->nmea[k + 2] = 42;
		d->nmea[k + 3] = 48;
		d->nmea[k + 4] = 48;
		d->nmea[k + 5] = 0;
		sentencenum++;
		
		// printout one frame starts here
		//AIVDM,x,x,,, - header comes here first
		
		d->nmea[0] = 65;
		d->nmea[1] = 73;
		d->nmea[2] = 86;
		d->nmea[3] = 68;
		d->nmea[4] = 77;
		d->nmea[5] = 44;
		d->nmea[6] = 48 + sentences;
		d->nmea[7] = 44;
		d->nmea[8] = 48 + sentencenum;
		d->nmea[9] = 44;
		
		//if multipart message it needs sequential id number
		if (sentences > 1) {
			NMEA_DBG(printf("NMEA: It is multipart (%d/%d), add sequence number (%d) to header\n",
				sentences, sentencenum, d->seqnr));
			d->nmea[10] = d->seqnr + 48;
			d->nmea[11] = 44;
			d->nmea[12] = 44;
			//and if the last of multipart we need to show fillbits at trailer
			if (sentencenum == sentences) {
				NMEA_DBG(printf("NMEA: It is last of multipart (%d/%d), add fillbits (%d) to trailer\n",
					sentences, sentencenum, fillbits));
				d->nmea[k + 1] = 48 + fillbits;
			}
		} else {	//else put channel A & no seqnr to keep equal lenght (foo!)
			d->nmea[10] = 44;
			d->nmea[11] = 65;
			d->nmea[12] = 44;
		}

		//strcpy(nmea,"!AIVDM,1,1,,,");
		//calculate xor checksum in hex for nmea[0] until nmea[m]='*'(42)
		nmeachk = d->nmea[0];
		m = 1;
		while (d->nmea[m] != 42) {	//!="*"
			nmeachk = nmeachk ^ d->nmea[m];
			m++;
		}
		// convert calculated checksum to 2 digit hex there are 00 as base
		// so if only 1 digit put it to later position to get 0-header 01,02...
		nchk[0] = 0;
		nchk[1] = 0;
		snprintf(nchk, NCHK_LEN, "%X", nmeachk);
		if (nchk[1] == 0) {
			d->nmea[k + 4] = nchk[0];
		} else {
			d->nmea[k + 3] = nchk[0];
			d->nmea[k + 4] = nchk[1];
		}
		//In final. Add header "!" and trailer <cr><lf>
		// here it could be sent to /dev/ttySx
		serbuffer_l = snprintf(d->serbuffer, SERBUFFER_LEN, "!%s\r\n", d->nmea);
		if (d->serial)
			serial_write(d->serial, d->serbuffer, serbuffer_l);
		NMEA_DBG(printf("NMEA: End of nmea->ascii-loop with sentences:%d sentencenum:%d\n",
			sentences, sentencenum));
		if (my)
			myout_nmea(my, received_t, d->nmea);
	} while (sentencenum < sentences);
	//multipart message ready. Increase seqnr for next one
	//rolling 1-9. Single msg ready may also increase this, no matter.
	d->seqnr++;
	if (d->seqnr > 9)
		d->seqnr = 0;
	
	if (type < 1 || type > MAX_AIS_PACKET_TYPE)
		return; // unsupported packet type
		
	if (skip_type[type])
		return; // ignored by configuration
	
	printf("(ch %c cntr %ld type %d): ", d->chanid, d->cntr, type);
	switch (type) {
	case 1:
	case 2:
	case 3:
		longitude = protodec_henten(61, 28, d->rbuffer);
		if (((longitude >> 27) & 1) == 1)
			longitude |= 0xF0000000;
		latitude = protodec_henten(38 + 22 + 29, 27, d->rbuffer);
		if (((latitude >> 26) & 1) == 1)
			latitude |= 0xf8000000;
		course = protodec_henten(38 + 22 + 28 + 28, 12, d->rbuffer);
		sog = protodec_henten(50, 10, d->rbuffer);
		rateofturn = protodec_henten(38 + 2, 8, d->rbuffer);
		navstat = protodec_henten(38, 2, d->rbuffer);
		heading = protodec_henten(38 + 22 + 28 + 28 + 12, 9, d->rbuffer);
		printf("%09ld %10f %10f %5f %5f %5i %5d %5d",
			mmsi, (float) latitude / 600000.0,
			(float) longitude / 600000.0,
			(float) course / 10.0, (float) sog / 10.0,
			rateofturn, navstat, heading);
		printf("  ( !%s )", d->nmea);
		
		if (my)
			myout_ais_position(my, received_t, mmsi,
				(float) latitude / 600000.0,
				(float) longitude / 600000.0,
				(float) heading, (float) course / 10.0,
				(float) sog / 10.0);
		
		if (cache_positions)
			cache_position(received_t, mmsi, navstat,
				(float) latitude / 600000.0,
				(float) longitude / 600000.0,
				heading,
				(float) course / 10.0,
				rateofturn,
				(float) sog / 10.0);
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
		longit = ((float) longitude) / 10000.0 / 60.0;
		latitude = protodec_henten(107, 27, d->rbuffer);
		if (((latitude >> 26) & 1) == 1)
			latitude |= 0xf8000000;
		latit = ((float) latitude) / 10000.0 / 60.0;
		printf("%09ld %ld %ld %ld %ld %ld %ld %f %f",
			mmsi, year, month, day, hour, minute,
			second, latit, longit);
		printf("  ( !%s )", d->nmea);
		
		if (my)
			myout_ais_basestation(my, received_t, mmsi,
				(float) latitude / 600000.0,
				(float) longitude / 600000.0);
		
		if (cache_positions)
			cache_position(received_t, mmsi, 0,
				(float) latitude / 600000.0,
				(float) longitude / 600000.0,
				0, 0.0, 0, 0.0);
		
		break;
		
	case 5:
		/* get IMO number */
		imo = protodec_henten(40, 30, d->rbuffer);
		//printf("--- 5: mmsi %lu imo %lu\n", mmsi, imo);
		
		/* get callsign */
		hvor = 70;
		for (k = 0; k < 6; k++) {
			letter = protodec_henten(hvor, 6, d->rbuffer);
			protodec_decode_sixbit_ascii(letter, callsign, k);
			hvor += 6;
		}
		callsign[6] = 0;
		remove_trailing_spaces(callsign, 6);
		//printf("Callsign: '%s'\n", callsign);
		
		/* get name */
		hvor = 112;
		for (k = 0; k < 20; k++) {
			letter = protodec_henten(hvor, 6, d->rbuffer);
			protodec_decode_sixbit_ascii(letter, name, k);
			hvor += 6;
		}
		name[20] = 0;
		remove_trailing_spaces(name, 20);
		//printf("Name: '%s'\n", name);
		
		/* get destination */
		hvor = 120 + 106 + 68 + 8;
		for (k = 0; k < 20; k++) {
			letter = protodec_henten(hvor, 6, d->rbuffer);
			protodec_decode_sixbit_ascii(letter, destination, k);
			hvor += 6;
		}
		destination[20] = 0;
		remove_trailing_spaces(destination, 20);
		//printf("Destination: '%s'\n",destination);
		
		/* type of ship and cargo */
		shiptype = protodec_henten(232, 8, d->rbuffer);
		
		/* dimensions and reference GPS position */
		A = protodec_henten(240, 9, d->rbuffer);
		B = protodec_henten(240 + 9, 9, d->rbuffer);
		C = protodec_henten(240 + 9 + 9, 6, d->rbuffer);
		D = protodec_henten(240 + 9 + 9 + 6, 6, d->rbuffer);
		draught = protodec_henten(294, 8, d->rbuffer);
		// printf("Length: %d\nWidth: %d\nDraught: %f\n",A+B,C+D,(float)draught/10);
		printf("%09ld %s %s %d %d %f", mmsi,
			name, destination, A + B, C + D,
			(float) draught / 10.0);
		printf("  ( !%s )", d->nmea);
		
		if (my)
			myout_ais_vesseldata(my, received_t, mmsi,
				name, destination,
				(float) draught / 10.0,
				(int) A, (int) B, (int) C, (int) D);
		
		if (cache_positions)
			cache_vesseldata(received_t, mmsi, imo, callsign,
				name, destination, shiptype, A, B, C, D, draught / 10.0);
		
		break;

	case 18: // class B transmitter
		longitude = protodec_henten(57, 28, d->rbuffer);
		if (((longitude >> 27) & 1) == 1)
			longitude |= 0xF0000000;
		latitude = protodec_henten(85, 27, d->rbuffer);
		if (((latitude >> 26) & 1) == 1)
			latitude |= 0xf8000000;
		course = protodec_henten(112, 12, d->rbuffer);
		sog = protodec_henten(46, 10, d->rbuffer);

		rateofturn = 0; //NOT in B
		navstat = 15;   //NOT in B
		heading = protodec_henten(124, 9, d->rbuffer);
		printf("%09ld %10f %10f %5f %5f %5i %5d %5d",
			mmsi, (float) latitude / 600000.0,
			(float) longitude / 600000.0,
			(float) course / 10.0, (float) sog / 10.0,
			rateofturn, navstat, heading);
		printf("  ( !%s )", d->nmea);
		
		if (my)
			myout_ais_position(my, received_t, mmsi,
				(float) latitude / 600000.0,
				(float) longitude / 600000.0,
				(float) heading, (float) course / 10.0,
				(float) sog / 10.0);
		
		if (cache_positions)
			cache_position(received_t, mmsi, navstat,
				(float) latitude / 600000.0,
				(float) longitude / 600000.0,
				heading,
				(float) course / 10.0,
				rateofturn,
				(float) sog / 10.0);

		break;

	case 19: // class B transmitter
		
		/* get name */
		hvor = 143;
		for (k = 0; k < 20; k++) {
			letter = protodec_henten(hvor, 6, d->rbuffer);
			protodec_decode_sixbit_ascii(letter, name, k);
			hvor += 6;
		}
		name[20] = 0;
		remove_trailing_spaces(name, 20);
		//printf("Name: '%s'\n", name);
		
		//Class B does not have destination, use "CLASS B TX" instead
                destination[0] = 0;
		strcat(destination,"CLASS B TX");
		destination[10] = 0; //just to be sure ;)
		
		/* type of ship and cargo */
		shiptype = protodec_henten(263, 8, d->rbuffer);
		
		/* dimensions and reference GPS position */
		A = protodec_henten(271, 9, d->rbuffer);
		B = protodec_henten(271 + 9, 9, d->rbuffer);
		C = protodec_henten(271 + 9 + 9, 6, d->rbuffer);
		D = protodec_henten(271 + 9 + 9 + 6, 6, d->rbuffer);
	
		// printf("Length: %d\nWidth: %d\n",A+B,C+D);
		//printf("%09ld %d %d %f", mmsi, A + B, C + D);
		printf("  ( !%s )", d->nmea);
		
			        
		if (my)
			myout_ais_vesselname(my, received_t, mmsi, name, destination);

		if (cache_positions)
			cache_vesselname(received_t, mmsi, name, destination);
		
				     
		if (my)
			myout_ais_vesseldatab(my, received_t, mmsi,
				(int) A, (int) B, (int) C, (int) D);
		
		if (cache_positions)
			cache_vesseldatabb(received_t, mmsi,
			 shiptype, A, B, C, D);
		

		break;
		
		

	case 24: // class B transmitter

                /* resolve type 24 frame's part A or B */
		partnr = protodec_henten(38, 2, d->rbuffer);

          	//printf("(partnr %d type %d): ",partnr, type);
                if (partnr == 0) {
		//printf("(Now in name:partnr %d type %d): ",partnr, type);
 		     /* get name */
		     hvor = 40;
		     for (k = 0; k < 20; k++) {
			letter = protodec_henten(hvor, 6, d->rbuffer);
			protodec_decode_sixbit_ascii(letter, name, k);
			hvor += 6;
		     }
		     name[20] = 0;
		     remove_trailing_spaces(name, 20);
		     //printf("Name: '%s'\n", name);
	        
		//Class B does not have destination, use "CLASS B TX" instead
                destination[0] = 0;
		strcat(destination,"CLASS B TX");
		destination[10] = 0; //just to be sure ;)
		
		if (my)
			myout_ais_vesselname(my, received_t, mmsi, name, destination);
			
		if (cache_positions)
			cache_vesselname(received_t, mmsi, name, destination);
			
		};
		                     
                if (partnr == 1) {
		     //printf("(Now in data:partnr %d type %d): ",partnr, type);
 		     /* get callsign */
		     hvor = 90; 
		     for (k = 0; k < 6; k++) {
			letter = protodec_henten(hvor, 6, d->rbuffer);
			protodec_decode_sixbit_ascii(letter, callsign, k);
			hvor += 6;
		     }
		     callsign[6] = 0;
		     remove_trailing_spaces(callsign, 6);
		     //printf("Callsign: '%s'\n", callsign);
		
		     /* type of ship and cargo */
		     shiptype = protodec_henten(40, 8, d->rbuffer);
		
		     /* dimensions and reference GPS position */
		     A = protodec_henten(132, 9, d->rbuffer);
	 	     B = protodec_henten(132 + 9, 9, d->rbuffer);
		     C = protodec_henten(132 + 9 + 9, 6, d->rbuffer);
		     D = protodec_henten(132 + 9 + 9 + 6, 6, d->rbuffer);
		    
		     /* 
		     printf("Length: %d\nWidth: %d\n",A+B,C+D);
		     printf("%09ld %d %d", mmsi, A + B, C + D);
		     */
		     printf("  ( !%s )", d->nmea);
		     
		     if (my)
			myout_ais_vesseldatab(my, received_t, mmsi,
				(int) A, (int) B, (int) C, (int) D);
		
		     if (cache_positions)
			cache_vesseldatab(received_t, mmsi, callsign,
			 shiptype, A, B, C, D);
		
		};
		
	
		break;
		
			default:
		printf("  ( !%s )", d->nmea);
		break;
	}
	printf("\n");
}



void protodec_decode(char *in, int count, struct demod_state_t *d)
{
	int i = 0;
	int bufferlength, correct;
	
	while (i < count) {
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
				if (in[i] == 1)	{ //To ettal har kommet etter hverandre 
					if (d->nstartsign == 0) {	// Forste gang det skjer
						d->nstartsign = 3;
						d->last = in[i];
					} else if (d->nstartsign == 5) {	// Har oppdaget start av startsymbol
						d->nstartsign++;
						d->npreamble = 0;
						d->antallpreamble = 0;
						d->state = ST_STARTSIGN;
					} else {
						d->nstartsign++;
					}

				} else { //To nuller har kommet etter hverandre
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
			bufferlength = d->bufferpos - 6 - 16;
			if (in[i] == 0 && bufferlength > 0) {
				DBG(printf("%d\n\nFrame received OK.  %d bits\n",
					in[i], bufferlength));
				correct = protodec_calculate_crc(bufferlength, d);
				if (correct) {
					DBG(printf("CRC Checksum correct! Frame Successfully Received!!!\n"));
					d->receivedframes++;
					protodec_getdata(bufferlength, d);
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

int protodec_calculate_crc(int length_bits, struct demod_state_t *d)
{
	int length_bytes;
	unsigned char *buf;
	int buflen;
	int i, j, x;
	unsigned char tmp;
	
	if (length_bits <= 0) {
		hlog(LOG_ERR, "protodec_calculate_crc: length_bits <= 0!");
		return 0;
	}
	
	length_bytes = length_bits / 8;
	buflen = length_bytes + 2;
	
	/* what is this? */
	buf = (unsigned char *) hmalloc(sizeof(*buf) * buflen);
	for (j = 0; j < buflen; j++) {
		tmp = 0;
		for (i = 0; i < 8; i++)
			tmp |= (((d->buffer[i + 8 * j]) << (i)));
		buf[j] = tmp;
	}
	
	/* ok, here's the actual CRC calculation */
	unsigned short crc = protodec_sdlc_crc(buf, buflen);
	//DBG(printf("CRC: %04x\n",crc));
	
	/* what is this? */
	memset(d->rbuffer, 0, sizeof(d->rbuffer));
	for (j = 0; j < length_bytes; j++) {
		for (i = 0; i < 8; i++) {
			x = j * 8 + i;
			if (x >= DEMOD_BUFFER_LEN) {
				hlog(LOG_ERR, "protodec_calculate_crc: would run over rbuffer length");
				hfree(buf);
				return 0;
			} else {
				d->rbuffer[x] = (buf[j] >> (7 - i)) & 1;
			}
		}
	}
	
	hfree(buf);
	
	return (crc == 0x0f47);
}


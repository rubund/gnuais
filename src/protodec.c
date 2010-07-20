
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
 * Calculates CRC-checksum
 */
 
unsigned short protodec_sdlc_crc(const unsigned char *data, unsigned len)
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

/*
 *	decode position packets (types 1,2,3)
 */

void protodec_pos(struct demod_state_t *d, int bufferlen, time_t received_t, unsigned long mmsi)
{
	int longitude, latitude;
	unsigned short course, sog, heading;
	char rateofturn, navstat;
	
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
	
	printf(" lat %.6f lon %.6f course %.0f speed %.1f rateofturn %d navstat %d heading %d",
		(float) latitude / 600000.0,
		(float) longitude / 600000.0,
		(float) course / 10.0, (float) sog / 10.0,
		rateofturn, navstat, heading);
	
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
}

void protodec_4(struct demod_state_t *d, int bufferlen, time_t received_t, unsigned long mmsi)
{
	unsigned long day, hour, minute, second, year, month;
	int longitude, latitude;
	float longit, latit;
	
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
	
	printf(" date %ld-%ld-%ld time %02ld:%02ld:%02ld lat %.6f lon %.6f",
		year, month, day, hour, minute,
		second, latit, longit);
	
	if (my)
		myout_ais_basestation(my, received_t, mmsi,
			(float) latitude / 600000.0,
			(float) longitude / 600000.0);
	
	if (cache_positions)
		cache_position(received_t, mmsi, 0,
			(float) latitude / 600000.0,
			(float) longitude / 600000.0,
			0, 0.0, 0, 0.0);
}

void protodec_5(struct demod_state_t *d, int bufferlen, time_t received_t, unsigned long mmsi)
{
	int pos;
	unsigned long imo;
	char callsign[7];
	char name[21];
	char destination[21];
	unsigned int A, B;
	unsigned char C, D;
	unsigned char draught;
	int k;
	int letter;
	unsigned int shiptype;
	
	/* get IMO number */
	imo = protodec_henten(40, 30, d->rbuffer);
	//printf("--- 5: mmsi %lu imo %lu\n", mmsi, imo);
	
	/* get callsign */
	pos = 70;
	for (k = 0; k < 6; k++) {
		letter = protodec_henten(pos, 6, d->rbuffer);
		protodec_decode_sixbit_ascii(letter, callsign, k);
		pos += 6;
	}
	
	callsign[6] = 0;
	remove_trailing_spaces(callsign, 6);
	
	/* get name */
	pos = 112;
	for (k = 0; k < 20; k++) {
		letter = protodec_henten(pos, 6, d->rbuffer);
		protodec_decode_sixbit_ascii(letter, name, k);
		pos += 6;
	}
	name[20] = 0;
	remove_trailing_spaces(name, 20);
	
	/* get destination */
	pos = 120 + 106 + 68 + 8;
	for (k = 0; k < 20; k++) {
		letter = protodec_henten(pos, 6, d->rbuffer);
		protodec_decode_sixbit_ascii(letter, destination, k);
		pos += 6;
	}
	destination[20] = 0;
	remove_trailing_spaces(destination, 20);
	
	/* type of ship and cargo */
	shiptype = protodec_henten(232, 8, d->rbuffer);
	
	/* dimensions and reference GPS position */
	A = protodec_henten(240, 9, d->rbuffer);
	B = protodec_henten(240 + 9, 9, d->rbuffer);
	C = protodec_henten(240 + 9 + 9, 6, d->rbuffer);
	D = protodec_henten(240 + 9 + 9 + 6, 6, d->rbuffer);
	draught = protodec_henten(294, 8, d->rbuffer);
	// printf("Length: %d\nWidth: %d\nDraught: %f\n",A+B,C+D,(float)draught/10);
	
	printf(" name \"%s\" destination \"%s\" type %d length %d width %d draught %.1f",
		name, destination, shiptype,
		A + B, C + D,
		(float) draught / 10.0);
	
	if (my)
		myout_ais_vesseldata(my, received_t, mmsi,
			name, destination,
			(float) draught / 10.0,
			(int) A, (int) B, (int) C, (int) D);
	
	if (cache_positions)
		cache_vesseldata(received_t, mmsi, imo, callsign,
			name, destination, shiptype, A, B, C, D, draught / 10.0);
}

/*
 *	6: addressed binary message
 */
 
void protodec_6(struct demod_state_t *d, int bufferlen, time_t received_t, unsigned long mmsi)
{
	int sequence = protodec_henten(38, 2, d->rbuffer);
	unsigned long dst_mmsi = protodec_henten(40, 30, d->rbuffer);
	int retransmitted = protodec_henten(70, 1, d->rbuffer);
	int appid = protodec_henten(72, 16, d->rbuffer);
	int appid_dac = protodec_henten(72, 10, d->rbuffer);
	int appid_func = protodec_henten(82, 6, d->rbuffer);
	
	printf(" dst_mmsi %09ld seq %d retransmitted %d appid %d app_dac %d app_func %d",
		dst_mmsi, sequence, retransmitted, appid, appid_dac, appid_func);
}

/*
 *	7: Binary acknowledge
 *	13: Safety related acknowledge
 */
 
void protodec_7_13(struct demod_state_t *d, int bufferlen, time_t received_t, unsigned long mmsi)
{
	unsigned long dst_mmsi;
	int sequence;
	int i;
	int pos;
	
	pos = 40;
	
	printf(" buflen %d pos+32 %d", bufferlen, pos + 32);
	for (i = 0; i < 4 && pos + 32 <= bufferlen; pos += 32) {
		dst_mmsi = protodec_henten(pos, 30, d->rbuffer);
		sequence = protodec_henten(pos + 30, 2, d->rbuffer);
		
		printf(" ack %d (to %09ld seq %d)",
			i+1, dst_mmsi, sequence);
		i++;
	}
}

void protodec_18(struct demod_state_t *d, int bufferlen, time_t received_t, unsigned long mmsi)
{
	int longitude, latitude;
	unsigned short course, sog, heading;
	char rateofturn, navstat;
	
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
	printf(" lat %.6f lon %.6f course %.0f speed %.1f rateofturn %d navstat %d heading %d",
		(float) latitude / 600000.0,
		(float) longitude / 600000.0,
		(float) course / 10.0, (float) sog / 10.0,
		rateofturn, navstat, heading);
	
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
}

void protodec_19(struct demod_state_t *d, int bufferlen, time_t received_t, unsigned long mmsi)
{
	int pos, k;
	unsigned int A, B;
	unsigned char C, D;
	unsigned int shiptype;
	int letter;
	char name[21];
	/*
	 * Class B does not have destination, use "CLASS B" instead
	 * (same as ShipPlotter)
	 */
	char destination[21] = "CLASS B";
	
	/* get name */
	pos = 143;
	for (k = 0; k < 20; k++) {
		letter = protodec_henten(pos, 6, d->rbuffer);
		protodec_decode_sixbit_ascii(letter, name, k);
		pos += 6;
	}
	name[20] = 0;
	remove_trailing_spaces(name, 20);
	//printf("Name: '%s'\n", name);
	
	/* type of ship and cargo */
	shiptype = protodec_henten(263, 8, d->rbuffer);
	
	/* dimensions and reference GPS position */
	A = protodec_henten(271, 9, d->rbuffer);
	B = protodec_henten(271 + 9, 9, d->rbuffer);
	C = protodec_henten(271 + 9 + 9, 6, d->rbuffer);
	D = protodec_henten(271 + 9 + 9 + 6, 6, d->rbuffer);
	
	// printf("Length: %d\nWidth: %d\n",A+B,C+D);
	//printf("%09ld %d %d %f", mmsi, A + B, C + D);
	printf(" name \"%s\" type %d length %d  width %d", name, shiptype, A+B, C+D);
	
	if (my) {
		myout_ais_vesselname(my, received_t, mmsi, name, destination);
		myout_ais_vesseldatab(my, received_t, mmsi,
			(int) A, (int) B, (int) C, (int) D);
	}
	
	if (cache_positions) {
		cache_vesselname(received_t, mmsi, name, destination);
		cache_vesseldatabb(received_t, mmsi, shiptype, A, B, C, D);
	}
}

void protodec_20(struct demod_state_t *d, int bufferlen)
{
	int ofs, slots, timeout, incr;
	int i;
	int pos;
	
	pos = 40;
	
	for (i = 0; i < 4 && pos + 30 < bufferlen; pos += 30) {
		ofs = protodec_henten(pos, 12, d->rbuffer);
		slots = protodec_henten(pos + 12, 4, d->rbuffer);
		timeout = protodec_henten(pos + 12 + 4, 3, d->rbuffer);
		incr = protodec_henten(pos + 12 + 4 + 3, 11, d->rbuffer);
		
		printf(" reserve %d (ofs %d slots %d timeout %d incr %d)",
			i+1, ofs, slots, timeout, incr);
		i++;
	}
}

void protodec_24(struct demod_state_t *d, int bufferlen, time_t received_t, unsigned long mmsi)
{
	int partnr;
	int pos;
	int k, letter;
	unsigned int A, B;
	unsigned char C, D;
	unsigned int shiptype;
	char name[21];
	char callsign[7];
	/*
	 * Class B does not have destination, use "CLASS B" instead
	 * (same as ShipPlotter)
	 */
	const char destination[21] = "CLASS B";
	
	/* resolve type 24 frame's part A or B */
	partnr = protodec_henten(38, 2, d->rbuffer);
	
	//printf("(partnr %d type %d): ",partnr, type);
	if (partnr == 0) {
		//printf("(Now in name:partnr %d type %d): ",partnr, type);
		/* get name */
		pos = 40;
		for (k = 0; k < 20; k++) {
			letter = protodec_henten(pos, 6, d->rbuffer);
			protodec_decode_sixbit_ascii(letter, name, k);
			pos += 6;
		}
		
		name[20] = 0;
		remove_trailing_spaces(name, 20);
		
		printf(" name \"%s\"", name);
		
		if (my)
			myout_ais_vesselname(my, received_t, mmsi, name, destination);
		
		if (cache_positions)
			cache_vesselname(received_t, mmsi, name, destination);
	}
	
	if (partnr == 1) {
		//printf("(Now in data:partnr %d type %d): ",partnr, type);
		/* get callsign */
		pos = 90; 
		for (k = 0; k < 6; k++) {
			letter = protodec_henten(pos, 6, d->rbuffer);
			protodec_decode_sixbit_ascii(letter, callsign, k);
			pos += 6;
		}
		callsign[6] = 0;
		remove_trailing_spaces(callsign, 6);
		
		/* type of ship and cargo */
		shiptype = protodec_henten(40, 8, d->rbuffer);
		
		/* dimensions and reference GPS position */
		A = protodec_henten(132, 9, d->rbuffer);
		B = protodec_henten(132 + 9, 9, d->rbuffer);
		C = protodec_henten(132 + 9 + 9, 6, d->rbuffer);
		D = protodec_henten(132 + 9 + 9 + 6, 6, d->rbuffer);
		
		printf(" callsign \"%s\" type %d length %d width %d",
			callsign, shiptype, A+B, C+D);
		
		if (my)
			myout_ais_vesseldatab(my, received_t, mmsi,
				(int) A, (int) B, (int) C, (int) D);
		
		if (cache_positions)
			cache_vesseldatab(received_t, mmsi, callsign,
				shiptype, A, B, C, D);
	}
}

#define NCHK_LEN 3

void protodec_generate_nmea(struct demod_state_t *d, int bufferlen, int fillbits, time_t received_t)
{
	int senlen;
	int pos;
	int k, letter;
	int m;
	unsigned char sentences, sentencenum, nmeachk;
	char nchk[NCHK_LEN];
	int serbuffer_l;
	
	//6bits to nmea-ascii. One sentence len max 82char
	//inc. head + tail.This makes inside datamax 62char multipart, 62 single
	senlen = 61;		//this is normally not needed.For testing only. May be fixed number
	if (bufferlen <= (senlen * 6)) {
		sentences = 1;
	} else {
		sentences = bufferlen / (senlen * 6);
		//sentences , if overflow put one more
		if (bufferlen % (senlen * 6) != 0)
			sentences++;
	};
	NMEA_DBG(printf("NMEA: %d sentences with max data of %d ascii chrs\n", sentences, senlen));
	sentencenum = 0;
	pos = 0;
	do {
		k = 13;		//leave room for nmea header
		while (k < senlen + 13 && bufferlen > pos) {
			letter = protodec_henten(pos, 6, d->rbuffer);
			// 6bit-to-ascii conversion by IEC
			if (letter < 40)
				letter = letter + 48;
			else
				letter = letter + 56;
			d->nmea[k] = letter;
			pos += 6;
			k++;
		}
		NMEA_DBG(printf("NMEA: Drop from loop with k:%d pos:%d senlen:%d bufferlen\n",
			k, pos, senlen, bufferlen));
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
}

void protodec_getdata(int bufferlen, struct demod_state_t *d)
{
	unsigned char type = protodec_henten(0, 6, d->rbuffer);
	if (type < 1 || type > MAX_AIS_PACKET_TYPE /* 9 */)
		return;
	unsigned long mmsi = protodec_henten(8, 30, d->rbuffer);
	unsigned short appid;
	int fillbits = 0;
	int k;
	time_t received_t;
	time(&received_t);
	
	DBG(printf("Bufferlen: %d,", bufferlen));
	
	if (bufferlen % 6 > 0) {
		fillbits = 6 - (bufferlen % 6);
		for (k = bufferlen; k < bufferlen + fillbits; k++)
			d->rbuffer[k] = 0;
		
		bufferlen = bufferlen + fillbits;
	}

	DBG(printf(" fixed Bufferlen: %d with %d fillbits\n", bufferlen, fillbits));
	
	/* generate an NMEA string out of the binary packet */
	protodec_generate_nmea(d, bufferlen, fillbits, received_t);
	
	//multipart message ready. Increase seqnr for next one
	//rolling 1-9. Single msg ready may also increase this, no matter.
	d->seqnr++;
	if (d->seqnr > 9)
		d->seqnr = 0;
	
	if (type < 1 || type > MAX_AIS_PACKET_TYPE)
		return; // unsupported packet type
		
	if (skip_type[type])
		return; // ignored by configuration
	
	printf("ch %c cntr %ld type %d mmsi %09ld:", d->chanid, d->cntr, type, mmsi);
	
	switch (type) {
	case 1: /* position packets */
	case 2:
	case 3:
		protodec_pos(d, bufferlen, received_t, mmsi);
		break;
		
	case 4: /* base station position */
		protodec_4(d, bufferlen, received_t, mmsi);
		break;
		
	case 5: /* vessel info */
		protodec_5(d, bufferlen, received_t, mmsi);
		break;
	
	case 6: /* Addressed binary message */
		protodec_6(d, bufferlen, received_t, mmsi);
		break;
	
	case 7: /* Binary acknowledge */
	case 13: /* Safety related acknowledge */
		protodec_7_13(d, bufferlen, received_t, mmsi);
		break;
	
	case 8: /* Binary broadcast message */
		appid = protodec_henten(40, 16, d->rbuffer);
		printf(" appid %d", appid);
		break;

	case 18: /* class B transmitter position report */
		protodec_18(d, bufferlen, received_t, mmsi);
		break;

	case 19: /* class B transmitter vessel info */
		protodec_19(d, bufferlen, received_t, mmsi);
		break;

	case 24: /* class B transmitter info */
		protodec_24(d, bufferlen, received_t, mmsi);
		break;
	
	case 20:
		protodec_20(d, bufferlen);
		break;
	
	default:
		break;
	}
	
	printf(" (!%s)\n", d->nmea);
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


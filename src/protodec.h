#ifndef INC_PROTODEC_H
#define INC_PROTODEC_H

#include "ais.h"

#include "serial.h"

#define ST_SKURR 1
#define ST_PREAMBLE 2
#define ST_STARTSIGN 3
#define ST_DATA 4
#define ST_STOPSIGN 5

// change this to  
//#define DBG(x) x 
// if you want to see all debug text
#define DBG(x)

#define DEMOD_BUFFER_LEN 450


struct demod_state_t {
	char chanid;
	unsigned long cntr;
	int state;
	unsigned int offset;
	int nskurr, npreamble, nstartsign, ndata, nstopsign;
	
	int antallenner;
	unsigned char *buffer;
	unsigned char *rbuffer;
	char *tbuffer;
	int bufferpos;
	char last;
	int antallpreamble;
	int bitstuff;
	int receivedframes;
	int lostframes;
	int lostframes2;
	unsigned char seqnr;
	
	struct serial_state_t *serial;
	
	char *serbuffer;
	char *nmea;
};

void protodec_initialize(struct demod_state_t *d, struct serial_state_t *serial, char chanid);
void protodec_reset(struct demod_state_t *d);
void protodec_getdata(int bufferlengde, struct demod_state_t *d);
void protodec_decode(char *in, int count, struct demod_state_t *d);
void protodec_bokstavtabell(char bokstav, char *name, int pos);
void protodec_bokstavtabell(char bokstav, char *name, int pos);
unsigned short protodec_sdlc_crc(unsigned char *data, unsigned len);
int protodec_calculate_crc(int lengde, struct demod_state_t *d);
unsigned long protodec_henten(int from, int size, unsigned char *frame);

#endif

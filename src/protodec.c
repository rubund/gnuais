#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "protodec.h"
#include "ais.h"

#include "cfg.h"



void protodec_initialize(struct demod_state_t *d)
{
	d->receivedframes = 0;
	d->lostframes = 0;
	d->lostframes2 = 0;

	protodec_reset(d);

#ifdef HAVE_MYSQL

	if ((mysql_keepsmall) && (strcmp(mysql_keepsmall, "yes") == 0
	    || strcmp(mysql_keepsmall, "YES") == 0)) {
		d->keepsmall = 1;
	} else {
		d->keepsmall = 0;
	}

	if (mysql_oldlimit) {
		d->oldlimit = atoi(mysql_oldlimit);
	} else {
		d->oldlimit = 0;
	}

	if (mysql_db)
		d->enable_mysql = 1;
	else
		d->enable_mysql = 0;

	if (d->enable_mysql) {
		if (!mysql_init(&d->conn)) {
			fprintf(stderr, "Error initializing mysql\n");
			exit(-1);
		}
		//printf("Host: %s\nDB: %s\nuser: %s\npassword: %s\ntable: %s\n",mysql_host,mysql_dbname,mysql_username,mysql_password,mysql_table);
		if (!mysql_real_connect(&d->conn, mysql_host, mysql_user, mysql_password, mysql_db, 0, NULL, 0)) {
			fprintf(stderr, "Error opening mysql connection\n");
			exit(-1);
		}

	}
#endif
	if (serial_port) {
		DBG(printf("Serial device is %s ,", serial_port));
		
		//Returns the file descriptor on success or -1 on error.
		/* File descriptor for the port */
		d->my_fd = open(serial_port, O_RDWR | O_NOCTTY | O_NDELAY);
		if (d->my_fd == -1) {	// Could not open the port.
			perror("open_port: Unable to open serial port");
		} else {
			DBG(printf("serial opened. My_fd is:%d\n", d->my_fd));
			fcntl(d->my_fd, F_SETFL, 0);

			struct termios options;
			/* get the current options */
			tcgetattr(d->my_fd, &options);

			//set speed 4800
			cfsetispeed(&options, B4800);
			cfsetospeed(&options, B4800);
			/* set raw input, 1 second timeout */
			options.c_cflag |= (CLOCAL | CREAD);
			options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
			options.c_oflag &= ~OPOST;
			options.c_cc[VMIN] = 0;
			options.c_cc[VTIME] = 10;

			//No parity 8N1
			options.c_cflag &= ~PARENB;
			options.c_cflag &= ~CSTOPB;
			options.c_cflag &= ~CSIZE;
			options.c_cflag |= CS8;
			/* set the options */
			tcsetattr(d->my_fd, TCSANOW, &options);
		}
	}
	//needed for NMEA sending
	else
		d->my_fd = -1;
	d->seqnr = 0;
	d->serbuffer[0] = 0;

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
#ifdef HAVE_MYSQL
#define MAX_SQL_LEN 2000
	static char sqlquery[MAX_SQL_LEN];
	static char sqlquery2[MAX_SQL_LEN];
	static char sqlquery3[MAX_SQL_LEN];
	int qrows;
#endif
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
	unsigned char height;
	int m;
	char nmea[100];
	unsigned char sentences, sentencenum, nmeachk;
	int senlen;
	unsigned char fillbits;
	float longit, latit;
	int k, hvor, letter;
#ifdef HAVE_MYSQL
	MYSQL_RES *res;		// mysql
#endif
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
		sprintf(d->serbuffer, "!%s\r\n", nmea);
		//printf("!%s\r\n",nmea);
		if (d->my_fd != -1) {
			int n =
			    write(d->my_fd, d->serbuffer,
				  strlen(d->serbuffer));
			if (n < 0) {
				fputs("write() of bytes failed!\n",
				      stderr);
			}
			DBG(printf("Write serial out:%d\n", n));
		}
		DBG(printf("End of nmea->ascii-loop with sentences:%d sentencenum:%d\n",
			sentences, sentencenum));
#ifdef HAVE_MYSQL
		snprintf(sqlquery, MAX_SQL_LEN,
			"insert into ais_nmea (time, message) values (%d,\"!%s\")",
			(int) tid, nmea);
		if (d->enable_mysql) {
			if (mysql_query(&d->conn, sqlquery))	//try update data
			{
				fprintf(stderr, "Couldn't do query (ais_nmea)\n");	//if fails exit program
				exit(-1);
			}
		}
#endif
	}
	while (sentencenum < sentences);
	//multipart message ready. Increase seqnr for next one
	//rolling 1-9. Single msg ready may also increase this, no matter.
	d->seqnr++;
	if (d->seqnr > 9)
		d->seqnr = 0;


	printf("(%d):  ", cntr);
	switch (type) {
	case 1:
		if (d->skip_type[1] == 0) {
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
			printf("%d:  %09d %10f %10f %5f %5f %5i %5d %5d",
			       type, mmsi, (float) latitude / 600000,
			       (float) longitude / 600000,
			       (float) coarse / 10, (float) sog / 10,
			       rateofturn, underway, heading);
			printf("  ( !%s )\r\n", nmea);
#ifdef HAVE_MYSQL
			snprintf(sqlquery, MAX_SQL_LEN,
				"insert into ais_position (time,mmsi,latitude,longitude,heading,coarse,speed) values (%d,%d,%.7f,%.7f,%.5f,%f,%f)",
				(int) tid, (int) mmsi,
				(float) latitude / 600000,
				(float) longitude / 600000,
				(float) heading, (float) coarse / 10,
				(float) sog / 10);

			snprintf(sqlquery2, MAX_SQL_LEN,
				"update ais_position set time=%d, latitude=%.7f,longitude=%.7f,heading=%f,coarse=%.5f,speed=%f where mmsi=%d",
				(int) tid, (float) latitude / 600000,
				(float) longitude / 600000,
				(float) heading, (float) coarse / 10,
				(float) sog / 10, (int) mmsi);
#endif
		}
		break;
	case 2:
		if (d->skip_type[1] == 0) {
			longitude = protodec_henten(61, 28, d->rbuffer);
			if (((longitude >> 27) & 1) == 1)
				longitude |= 0xF0000000;
			latitude =
			    protodec_henten(38 + 22 + 29, 27, d->rbuffer);
			if (((latitude >> 26) & 1) == 1)
				latitude |= 0xf8000000;
			coarse =
			    protodec_henten(38 + 22 + 28 + 28, 12,
					    d->rbuffer);
			sog = protodec_henten(50, 10, d->rbuffer);
			rateofturn =
			    protodec_henten(38 + 2, 8, d->rbuffer);
			underway = protodec_henten(38, 2, d->rbuffer);
			heading =
			    protodec_henten(38 + 22 + 28 + 28 + 12, 9,
					    d->rbuffer);
			printf("%d:  %09d %10f %10f %5f %5f %5i %5d %5d",
			       type, mmsi, (float) latitude / 600000,
			       (float) longitude / 600000,
			       (float) coarse / 10, (float) sog / 10,
			       rateofturn, underway, heading);
			printf("  ( !%s )\r\n", nmea);
#ifdef HAVE_MYSQL
			snprintf(sqlquery, MAX_SQL_LEN,
				"insert into ais_position (time,mmsi,latitude,longitude,heading,coarse,speed) values (%d,%d,%.7f,%.7f,%.5f,%f,%f)",
				(int) tid, (int) mmsi,
				(float) latitude / 600000,
				(float) longitude / 600000,
				(float) heading, (float) coarse / 10,
				(float) sog / 10);

			snprintf(sqlquery2, MAX_SQL_LEN,
				"update ais_position set time=%d, latitude=%.7f,longitude=%.7f,heading=%f,coarse=%.5f,speed=%f where mmsi=%d",
				(int) tid, (float) latitude / 600000,
				(float) longitude / 600000,
				(float) heading, (float) coarse / 10,
				(float) sog / 10, (int) mmsi);

#endif
		}
		break;
	case 3:
		if (d->skip_type[1] == 0) {
			longitude = protodec_henten(61, 28, d->rbuffer);
			if (((longitude >> 27) & 1) == 1)
				longitude |= 0xF0000000;
			latitude =
			    protodec_henten(38 + 22 + 29, 27, d->rbuffer);
			if (((latitude >> 26) & 1) == 1)
				latitude |= 0xf8000000;
			coarse =
			    protodec_henten(38 + 22 + 28 + 28, 12,
					    d->rbuffer);
			sog = protodec_henten(50, 10, d->rbuffer);
			rateofturn =
			    protodec_henten(38 + 2, 8, d->rbuffer);
			underway = protodec_henten(38, 2, d->rbuffer);
			heading =
			    protodec_henten(38 + 22 + 28 + 28 + 12, 9,
					    d->rbuffer);
			printf("%d:  %09d %10f %10f %5f %5f %5i %5d %5d",
			       type, mmsi, (float) latitude / 600000,
			       (float) longitude / 600000,
			       (float) coarse / 10, (float) sog / 10,
			       rateofturn, underway, heading);
			printf("  ( !%s )\r\n", nmea);
#ifdef HAVE_MYSQL
			snprintf(sqlquery, MAX_SQL_LEN,
				"insert into ais_position (time,mmsi,latitude,longitude,heading,coarse,speed) values (%d,%d,%.7f,%.7f,%.5f,%f,%f)",
				(int) tid, (int) mmsi,
				(float) latitude / 600000,
				(float) longitude / 600000,
				(float) heading, (float) coarse / 10,
				(float) sog / 10);

			snprintf(sqlquery2, MAX_SQL_LEN,
				"update ais_position set time=%d, latitude=%.7f,longitude=%.7f,heading=%f,coarse=%.5f,speed=%f where mmsi=%d",
				(int) tid, (float) latitude / 600000,
				(float) longitude / 600000,
				(float) heading, (float) coarse / 10,
				(float) sog / 10, (int) mmsi);
#endif
		}
		break;
	case 4:
		if (d->skip_type[4] == 0) {
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
			printf("%d:  %09d %d %d %d %d %d %d %f %f", type,
			       mmsi, year, month, day, hour, minute,
			       second, latit, longit);
			printf("  ( !%s )\r\n", nmea);
#ifdef HAVE_MYSQL
			snprintf(sqlquery, MAX_SQL_LEN,
				"insert into ais_basestation (time,mmsi,longitude,latitude) values (%d,%d,%.7f,%.7f)",
				(int) tid, (int) mmsi,
				(float) latitude / 600000,
				(float) longitude / 600000);

			snprintf(sqlquery2, MAX_SQL_LEN,
				"update ais_basestation set time=%d,latitude=%.7f,longitude=%.7f where mmsi=%d",
				(int) tid, (float) latitude / 600000,
				(float) longitude / 600000, (int) mmsi);


#endif			 /*MYSQL*/
		}
		break;
	case 5:
		if (d->skip_type[5] == 0) {
			hvor = 112;
			for (k = 0; k < 20; k++) {
				letter =
				    protodec_henten(hvor, 6, d->rbuffer);
				protodec_bokstavtabell(letter, name, k);
				hvor += 6;
			}
			name[20] = 0;
//      printf("Name: %s\n", name);
			hvor = 120 + 106 + 68 + 8;
			for (k = 0; k < 20; k++) {
				letter =
				    protodec_henten(hvor, 6, d->rbuffer);
				protodec_bokstavtabell(letter, destination,
						       k);
				hvor += 6;
			}
			destination[20] = 0;
//      printf("Destination: %s\n",destination);
			A = protodec_henten(240, 9, d->rbuffer);
			B = protodec_henten(240 + 9, 9, d->rbuffer);
			C = protodec_henten(240 + 9 + 9, 6, d->rbuffer);
			D = protodec_henten(240 + 9 + 9 + 6, 6,
					    d->rbuffer);
			height = protodec_henten(294, 8, d->rbuffer);
//      printf("Length: %d\nWidth: %d\nHeight: %f\n",A+B,C+D,(float)height/10);
			printf("%d:  %09d %s %s %d %d %f", type, mmsi,
			       name, destination, A + B, C + D,
			       (float) height / 10);
			printf("  ( !%s )\r\n", nmea);
#ifdef HAVE_MYSQL

			snprintf(sqlquery, MAX_SQL_LEN,
				"insert into ais_vesseldata (time,mmsi,name,destination,height,A,B,C,D) values (%d,%d,\"%s\",\"%s\",%f,%d,%d,%d,%d)",
				(int) tid, (int) mmsi, name, destination,
				(float) height / 10, (int) A, (int) B,
				(int) C, (int) D);

			snprintf(sqlquery2, MAX_SQL_LEN,
				"update ais_vesseldata set time=%d, name=\"%s\",destination=\"%s\",A=%d,B=%d,C=%d,D=%d,height=%f where mmsi=%d",
				(int) tid, name, destination, (int) A,
				(int) B, (int) C, (int) D,
				(float) height / 10, (int) mmsi);

#endif
		}

		break;
	default:
		return;
		break;

	}
//      std::cout << sqlquery.str() << "\n";
#ifdef HAVE_MYSQL
	DBG(printf("%s\n", sqlquery));
	
	if (d->enable_mysql && (d->skip_type[type] == 0)) {
		if (d->keepsmall) {
			DBG(printf("Trying to update MySql..."));
			if (mysql_query(&d->conn, sqlquery2)) {	//try update data
				fprintf(stderr, "Couldn't do query (update)\n");	//if fails exit program
				exit(-1);
			} else {
				qrows = mysql_affected_rows(&d->conn);
			}
			if (qrows) {
				DBG(printf("%d Updated!\n", qrows));
				res = mysql_use_result(&d->conn);
				mysql_free_result(res);	// MYSQL
			} else {
				DBG(printf("Nothing to update. Trying insert new entry..."));
				if (mysql_query(&d->conn, sqlquery))	//if fails create new entry
				{
					fprintf(stderr, "Couldn't do query (insert)\n");	//if fails exit program
					exit(-1);
				} else {
					DBG(printf("Inserted!\n"));
					res = mysql_use_result(&d->conn);
					mysql_free_result(res);	// MYSQL
				}
			}
		} else {
			if (mysql_query(&d->conn, sqlquery))	//if fails create new entry
			{
				fprintf(stderr, "Couldn't do query (basic insert)\n");	//if fails exit program
				exit(-1);
			} else {
				res = mysql_use_result(&d->conn);
				mysql_free_result(res);	// MYSQL
			}

		}
//end  of update/new

// delete too old entries;
		if (d->oldlimit > 0) {
			snprintf(sqlquery3, MAX_SQL_LEN,
				"delete from ais_position where time<%d",
				(int) tid - d->oldlimit);
			DBG(printf("%s     Time now:%d\n", sqlquery3, (int) tid));
			DBG(printf("Trying to delete oldies..."));
			if (mysql_query(&d->conn, sqlquery3)) {
				//try update data
				fprintf(stderr, "Couldn't do query (delete)\n");	//if fails exit program
				exit(-1);
			} else {
				qrows = mysql_affected_rows(&d->conn);
			}
			if (qrows) {
				DBG(printf("%d Removed!\n", qrows));
				res = mysql_use_result(&d->conn);
				mysql_free_result(res);	// MYSQL
			} else {
				DBG(printf("none.\n", qrows));
			}
		}
// end of old entries

	}
#endif

}



void protodec_decode(char *in, int count, struct demod_state_t *d)
{
	int i = 0;
	unsigned long n;
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
					memset(d->buffer, 0,
					       250 * sizeof(char));
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
	for (i = 0; i < size; i++) {
		tmp |= (frame[from + i]) << (size - 1 - i);

	}
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
	int antallbytes = lengde / 8;
	unsigned char *data =
	    (unsigned char *) malloc(sizeof(unsigned char) *
				     (antallbytes + 2));
	int i, j;

	unsigned char tmp;

	for (j = 0; j < antallbytes + 2; j++) {
		tmp = 0;
		for (i = 0; i < 8; i++)
			tmp |= (((d->buffer[i + 8 * j]) << (i)));
		data[j] = tmp;
	}
	unsigned short crc = protodec_sdlc_crc(data, antallbytes + 2);
//DBG(printf("CRC: %04x\n",crc));
	memset(d->rbuffer, 0, 450 * sizeof(char));
	for (j = 0; j < antallbytes; j++) {
		for (i = 0; i < 8; i++)
			d->rbuffer[j * 8 + i] = (data[j] >> (7 - i)) & 1;
	}
	free(data);
	return (crc == 0x0f47);
}



/*
 *	out_mysql.h
 *
 *	(c) Heikki Hannikainen 2008
 *
 *	Store position reports in a MySQL database.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <time.h>
#ifdef HAVE_MYSQL
#include <mysql/mysql.h>
#endif

struct mysql_state_t {
#ifdef HAVE_MYSQL
	MYSQL conn;
	int connected;
	int inserts;
#endif
} *my;

extern struct mysql_state_t *myout_init();

extern int myout_ais_position(struct mysql_state_t *my,
	time_t tid, int mmsi, float lat, float lon,
	float hdg, float course, float sog);

extern int myout_ais_basestation(struct mysql_state_t *my,
	time_t tid, int mmsi, float lat, float lon);

extern int myout_ais_vesseldata(struct mysql_state_t *my,
	time_t tid, int mmsi, char *name, char *destination,
	float draught, int A, int B, int C, int D);

extern int myout_ais_vesselname(struct mysql_state_t *my,
	time_t tid, int mmsi, const char *name, const char *destination);
	
extern int myout_ais_vesseldatab(struct mysql_state_t *my,
	time_t tid, int mmsi, int A, int B, int C, int D);

extern int myout_nmea(struct mysql_state_t *my, time_t tid, char *nmea);


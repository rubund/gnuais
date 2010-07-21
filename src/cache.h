
/*
 *	cache.h
 *
 *	(c) Heikki Hannikainen 2008
 *
 *	Cache received AIS position reports, storing the most current
 *	up-to-date information for each MMSI, so that it can be sent out
 *	at regular intervals.
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

#include "splay.h"

/* an entry in the cache */
struct cache_ent {
	int mmsi;
	
	/* position message */
	time_t received_pos;
	float lat;
	float lon;
	int hdg;
	float course;
	float sog;
	int navstat;
	
	/* vessel data message */
	time_t received_data;
	int imo;
	char *callsign;
	char *name;
	char *destination;
	int shiptype;
	int A;
	int B;
	int C;
	int D;
	float draught;
	
	/* persons on board */
	time_t received_persons_on_board;
	int persons_on_board;
};

extern int cache_positions;

extern int cache_init(void);
extern int cache_deinit(void);

extern void cache_free_entry(struct cache_ent *e);
extern int cache_free(struct sptree *sp);
extern struct sptree *cache_rotate(void);

extern int cache_position(
	int received_t, int mmsi, int navstat, float lat, float lon,
	int hdg, float course, int rateofturn, float sog);

extern int cache_vesseldata(
	int received_t, int mmsi, int imo,
	char *callsign, char *name, char *destination,
	int shiptype, int A, int B, int C, int D, float draught);

extern int cache_vessel_persons(int received_t, int mmsi, int persons_on_board);

extern int cache_vesselname(int received_t, int mmsi, char *name, const char *destination);

extern int cache_vesseldatab(int received_t, int mmsi,
	char *callsign,
	int shiptype, int A, int B, int C, int D);
	
extern int cache_vesseldatabb(int received_t, int mmsi,
        int shiptype, int A, int B, int C, int D);

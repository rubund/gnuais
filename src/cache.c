
/*
 *	cache.c
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

int cache_positions = 0;

int cache_init()
{
	cache_positions = 1;
	
	return 0;
}

int cache_free()
{
	return 0;
}

int cache_position(int received_t, int mmsi, float lat, float lon, float hdg, float course, float sog)
{
	return 0;
}

int cache_vesseldata(int received_t, int mmsi, char *name, char *destination, int A, int B, int C, int D)
{
	return 0;
}



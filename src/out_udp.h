/*
 *	out_udp.h
 *
 *	(c) Heikki Hannikainen, OH7LZB <hessu@hes.iki.fi>
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

#ifndef INC_OUT_UDP_H
#define INC_OUT_UDP_H

#include "cfg.h"

struct udp_dest_t {
	struct sockaddr_storage addr;
	socklen_t addr_len;
	int fd;
	char *host;
	char *port;
};

struct udp_state_t {
	struct udp_dest_t *dests;
	int dest_count;
};

extern struct udp_state_t *udpout_init(struct udp_config_t *cfg);
extern int udpout_nmea(struct udp_state_t *udp, const char *nmea, int len);
extern void udpout_close(struct udp_state_t *udp);

#endif

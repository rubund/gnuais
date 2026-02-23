/*
 *	out_udp.c: NMEA output via UDP
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

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

#include "out_udp.h"
#include "cfg.h"
#include "hlog.h"
#include "hmalloc.h"

struct udp_state_t *udpout_init(struct udp_config_t *cfg)
{
	struct udp_state_t *udp;
	struct udp_config_t *c;
	struct addrinfo hints, *ai, *result;
	int count, i, ret;

	/* count destinations */
	count = 0;
	for (c = cfg; c; c = c->next)
		count++;

	if (count == 0)
		return NULL;

	udp = hmalloc(sizeof(*udp));
	memset(udp, 0, sizeof(*udp));
	udp->dests = hmalloc(sizeof(struct udp_dest_t) * count);
	memset(udp->dests, 0, sizeof(struct udp_dest_t) * count);
	udp->dest_count = 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	i = 0;
	for (c = cfg; c; c = c->next) {
		ret = getaddrinfo(c->host, c->port, &hints, &result);
		if (ret != 0) {
			hlog(LOG_ERR, "UDP: getaddrinfo(%s, %s) failed: %s",
				c->host, c->port, gai_strerror(ret));
			continue;
		}

		ai = result;

		memcpy(&udp->dests[i].addr, ai->ai_addr, ai->ai_addrlen);
		udp->dests[i].addr_len = ai->ai_addrlen;

		udp->dests[i].fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (udp->dests[i].fd < 0) {
			hlog(LOG_ERR, "UDP: socket() for %s:%s failed: %s",
				c->host, c->port, strerror(errno));
			freeaddrinfo(result);
			continue;
		}

		udp->dests[i].host = hstrdup(c->host);
		udp->dests[i].port = hstrdup(c->port);

		hlog(LOG_INFO, "UDP: Sending NMEA to %s:%s (af %d fd %d)",
			c->host, c->port, ai->ai_family, udp->dests[i].fd);

		freeaddrinfo(result);
		i++;
	}

	udp->dest_count = i;
	hlog(LOG_INFO, "UDP: %d destinations configured", udp->dest_count);

	if (udp->dest_count == 0) {
		hfree(udp->dests);
		hfree(udp);
		return NULL;
	}

	return udp;
}

int udpout_nmea(struct udp_state_t *udp, const char *nmea, int len)
{
	int i;
	ssize_t ret;

	for (i = 0; i < udp->dest_count; i++) {
		ret = sendto(udp->dests[i].fd, nmea, len, 0,
			(struct sockaddr *)&udp->dests[i].addr,
			udp->dests[i].addr_len);
		if (ret < 0) {
			hlog(LOG_ERR, "UDP: sendto() to %s:%s fd %d failed: %s",
				udp->dests[i].host, udp->dests[i].port,
				udp->dests[i].fd, strerror(errno));
		}
	}

	return 0;
}

void udpout_close(struct udp_state_t *udp)
{
	int i;

	if (!udp)
		return;

	for (i = 0; i < udp->dest_count; i++) {
		if (udp->dests[i].fd >= 0)
			close(udp->dests[i].fd);
		hfree(udp->dests[i].host);
		hfree(udp->dests[i].port);
	}

	hfree(udp->dests);
	hfree(udp);
}

/*
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
 *	
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "../config.h"

#define PROGNAME PACKAGE
#define VERSTR  PROGNAME " v" VERSION
#define SERVERID PROGNAME "-" VERSION
#define CRLF "\r\n"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <netdb.h>

#ifndef AI_PASSIVE
#include "netdb6.h"
#endif

extern int fork_a_daemon;	/* fork a daemon */

extern int dump_requests;	/* print requests */
extern int stats_interval;
extern int expiry_interval;

#define MAX_AIS_PACKET_TYPE 24
extern int skip_type[];

extern int verbose;

extern char *mycall;
extern char *myemail;

extern char *sound_device;
extern char *sound_in_file;
extern char *sound_out_file;
extern char *serial_port;

#define SOUND_CHANNELS_MONO 1
#define SOUND_CHANNELS_BOTH 2
#define SOUND_CHANNELS_LEFT 3
#define SOUND_CHANNELS_RIGHT 4
extern int sound_channels;
extern int sound_levellog;

extern char *cfgfile;
extern char *pidfile;
extern char *logdir;
extern char *logname;

extern char *mysql_host;
extern char *mysql_db;
extern char *mysql_user;
extern char *mysql_password;
extern int mysql_keepsmall;
extern int mysql_oldlimit;

#define UPLINK_JSON 1

struct uplink_config_t {
	struct uplink_config_t *next;
	struct uplink_config_t **prevp; /* pointer to the *next pointer in the previous node */
	
	int proto;
	const char *name;			/* name of socket */
	const char *url;
};

extern struct uplink_config_t *uplink_config;

extern int read_config(void);
extern void free_config(void);

extern void parse_cmdline(int argc, char *argv[]);

#endif


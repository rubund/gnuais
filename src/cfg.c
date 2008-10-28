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

/*
 *	config.c: configuration parsing, based on Tomi's code
 */

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/resource.h>

#include "cfg.h"
#include "hmalloc.h"
#include "hlog.h"
#include "cfgfile.h"

#define HELPS	"Usage: " PACKAGE " [-c cfgfile] [-f (fork)] [-n <logname>] [-e <loglevel>] [-o <logdest>] [-r <logdir>] [-l <inputsoundfile>] [-h (help)]\n"


char def_cfgfile[] = PROGNAME ".conf";
char def_logname[] = PROGNAME;
char def_sound_device[] = "default";

char *cfgfile = def_cfgfile;
char *pidfile;
char *logdir;	/* access logs go here */

char *logname = def_logname;	/* syslog entries use this program name */

char *mycall;
char *myemail;

char *sound_device;
char *sound_file;
int sound_channels = SOUND_CHANNELS_MONO;

char *mysql_host;
char *mysql_db;
char *mysql_user;
char *mysql_password;
int mysql_keepsmall;
int mysql_oldlimit;

char *serial_port;

struct uplink_config_t *uplink_config;
struct uplink_config_t *new_uplink_config;

int fork_a_daemon;	/* fork a daemon */
int stats_interval;
int expiry_interval;

int skip_type[MAX_AIS_PACKET_TYPE+1];

int verbose;

int do_interval(int *dest, int argc, char **argv);
int do_uplink(struct uplink_config_t **lq, int argc, char **argv);
int do_skip_type(int *dest, int argc, char **argv);
int do_sound_ch(int *dest, int argc, char **argv);

/*
 *	Configuration file commands
 */

#define _CFUNC_ (int (*)(void *dest, int argc, char **argv))

static struct cfgcmd cfg_cmds[] = {
	{ "logdir",		_CFUNC_ do_string,	&logdir			},
	{ "mycall",		_CFUNC_ do_string,	&mycall			},
	{ "myemail",		_CFUNC_ do_string,	&myemail		},
	{ "statsinterval",	_CFUNC_ do_interval,	&stats_interval		},
	{ "expiryinterval",	_CFUNC_ do_interval,	&expiry_interval	},
	{ "uplink",		_CFUNC_ do_uplink,	&new_uplink_config	},

	{ "mysql_host",		_CFUNC_ do_string,	&mysql_host		},
	{ "mysql_db",		_CFUNC_ do_string,	&mysql_db		},
	{ "mysql_user",		_CFUNC_ do_string,	&mysql_user		},
	{ "mysql_password",	_CFUNC_ do_string,	&mysql_password		},
	{ "mysql_keepsmall",	_CFUNC_ do_toggle,	&mysql_keepsmall	},
	{ "mysql_oldlimit",	_CFUNC_ do_int,		&mysql_oldlimit		},
	
	{ "sounddevice",	_CFUNC_ do_string,	&sound_device		},
	{ "soundfile",		_CFUNC_ do_string,	&sound_file		},
	{ "soundchannels",	_CFUNC_ do_sound_ch,	&sound_channels		},
	{ "serialport",		_CFUNC_ do_string,	&serial_port		},
	{ "serial_port",	_CFUNC_ do_string,	&serial_port		},
	
	{ "skip_type",		_CFUNC_ do_skip_type,	&skip_type		},
	
	{ NULL,			NULL,			NULL			}
};

/*
 *	Free a uplink config tree
 */

void free_uplink_config(struct uplink_config_t **lc)
{
	struct uplink_config_t *this;

	while (*lc) {
		this = *lc;
		*lc = this->next;
		hfree((void*)this->name);
		hfree((void*)this->proto);
		hfree((void*)this->host);
		hfree((void*)this->port);
		hfree(this);
	}
}

/*
 *	parse an interval specification
 */
 
time_t parse_interval(char *origs)
{
	time_t t = 0;
	int i;
	char *s, *np, *p, c;
	
	np = p = s = hstrdup(origs);
	
	while (*p) {
		if (!isdigit((int)*p)) {
			c = tolower(*p);
			*p = '\0';
			i = atoi(np);
			if (c == 's')
				t += i;
			else if (c == 'm')
				t += 60 * i;
			else if (c == 'h')
				t += 60 * 60 * i;
			else if (c == 'd')
				t += 24 * 60 * 60 * i;
			np = p + 1;
		}
		p++;
	}
	
	if (*np)
		t += atoi(np);
		
	hfree(s);
	return t;
}


/*
 *	Parse an interval configuration entry
 */

int do_interval(int *dest, int argc, char **argv)
{
	if (argc < 2)
		return -1;
		
	*dest = parse_interval(argv[1]);
	return 0;
}

/*
 *	Parse the skip_type configuration entry
 */

int do_skip_type(int *dest, int argc, char **argv)
{
	int i;
	
	if (argc < 2)
		return -1;
	
	i = atoi(argv[1]);
	
	if (i > 0 && i <= MAX_AIS_PACKET_TYPE) {
		skip_type[i] = 1;
	} else {
		hlog(LOG_CRIT, "skip_type value out of range: %d", i);
		return -1;
	}
	
	return 0;
}

/*
 *	Parse the soundchannels configuration entry
 */

int do_sound_ch(int *dest, int argc, char **argv)
{
	if (argc < 2)
		return -1;
	
	if (strcasecmp(argv[1], "mono") == 0) {
		*dest = SOUND_CHANNELS_MONO;
	} else if (strcasecmp(argv[1], "both") == 1) {
		*dest = SOUND_CHANNELS_BOTH;
	} else if (strcasecmp(argv[1], "left") == 1) {
		*dest = SOUND_CHANNELS_LEFT;
	} else if (strcasecmp(argv[1], "right") == 1) {
		*dest = SOUND_CHANNELS_RIGHT;
	} else {
		hlog(LOG_CRIT, "SoundChannels value unknown: %s", argv[1]);
		return -1;
	}
	
	return 0;
}

/*
 *	Parse a uplink definition directive
 *
 *	uplink <label> <token> {tcp|udp|sctp} <hostname> <portnum>
 *
 */

int do_uplink(struct uplink_config_t **lq, int argc, char **argv)
{
	struct uplink_config_t *l;
	int port;
	struct addrinfo req, *ai;

	if (argc < 5)
		return -1;

	/* argv[1] is  name label  for this uplink */
	/*
	if (strcasecmp(argv[2],"ro")==0) {
	  clflags |= 1;
	}*/

	memset(&req, 0, sizeof(req));
	req.ai_family   = 0;
	req.ai_socktype = SOCK_STREAM;
	req.ai_protocol = IPPROTO_TCP;
	req.ai_flags    = 0;
	ai = NULL;

	if (strcasecmp(argv[3], "tcp") == 0) {
		// well, do nothing for now.
	} else if (strcasecmp(argv[3], "udp") == 0) {
		req.ai_socktype = SOCK_DGRAM;
		req.ai_protocol = IPPROTO_UDP;
#if defined(SOCK_SEQPACKET) && defined(IPPROTO_SCTP)
	} else if (strcasecmp(argv[3], "sctp") == 0) {
		req.ai_socktype = SOCK_SEQPACKET;
		req.ai_protocol = IPPROTO_SCTP;
#endif
	} else {
		hlog(LOG_ERR, "Uplink: Unsupported protocol '%s'\n", argv[3]);
		return -2;
	}
	
	port = atoi(argv[5]);
	if (port < 1 || port > 65535) {
		hlog(LOG_ERR, "Uplink: Invalid port number '%s'\n", argv[5]);
		return -2;
	}

#if 0
	i = getaddrinfo(argv[4], argv[5], &req, &ai);
	if (i != 0) {
		hlog(LOG_INFO,"Uplink: address resolving failure of '%s' '%s'",argv[4],argv[5]);
		/* But do continue, this is perhaps a temporary glitch ? */
	}
	if (ai)
		freeaddrinfo(ai);
#endif
	l = hmalloc(sizeof(*l));

	l->name  = hstrdup(argv[1]);
	l->proto = hstrdup(argv[3]);
	l->host  = hstrdup(argv[4]);
	l->port  = hstrdup(argv[5]);

	/* put in the list */
	l->next = *lq;
	if (l->next)
		l->next->prevp = &l->next;
	*lq = l;
	
	return 0;
}

/*
 *	Validate an APRS-IS callsign
 */

int valid_aprsis_call(char *s)
{
	if (strlen(s) > 12)
		return 0;
	if (strlen(s) < 3)
		return 0;
	
	return 1;
}

/*
 *	upcase
 */
 
char *strupr(char *s)
{
	char *p;
	
	for (p = s; (*p); p++)
		*p = toupper(*p);
		
	return s;
}

/*
 *	Read configuration files, should add checks for this program's
 *	specific needs and obvious misconfigurations!
 */

int read_config(void)
{
	int failed = 0;
	char *s;
	
	if (read_cfgfile(cfgfile, cfg_cmds))
		return -1;
	
	/* these parameters will only be used when reading the configuration
	 * for the first time.
	 */
	if (!logdir) {
		hlog(LOG_CRIT, "Config: logdir not defined.");
		failed = 1;
	}
	
	/* mycall is only applied when running for the first time. */
	if (!mycall) {
		hlog(LOG_CRIT, "Config: mycall is not defined.");
		failed = 1;
	} else if (!valid_aprsis_call(mycall)) {
		hlog(LOG_CRIT, "Config: mycall '%s' is not valid.", mycall);
		failed = 1;
	}
	
	if (!myemail) {
		hlog(LOG_WARNING, "Config: myemail is not defined.");
		failed = 1;
	}
	
	if (!sound_file && !sound_device) {
		sound_device = def_sound_device;
		hlog(LOG_WARNING, "Config: SoundDevice is not defined - using: %s", sound_device);
	}
	
	if (sound_file && sound_device) {
		if (sound_device != def_sound_device)
			hfree(sound_device);
		sound_device = NULL;
	}
	
	/* put in the new uplink config */
	free_uplink_config(&uplink_config);
	uplink_config = new_uplink_config;
	if (uplink_config)
		uplink_config->prevp = &uplink_config;
	new_uplink_config = NULL;

	if (failed)
		return -1;
	
	if (!pidfile) {
		s = hmalloc(strlen(logdir) + 10 + 3);
		sprintf(s, "%s/%s.pid", logdir, logname);
		
		pidfile = s;
	}
	
	return 0;
}

/*
 *	Free configuration variables
 */

void free_config(void)
{
	if (logdir)
		hfree(logdir);
	logdir = NULL;
	if (pidfile)
		hfree(pidfile);
	pidfile = NULL;
	if (cfgfile != def_cfgfile)
		hfree(cfgfile);
	cfgfile = NULL;
	if (logname != def_logname)
		hfree(logname);
	hfree(mycall);
	hfree(myemail);
	mycall = myemail = NULL;
	logname = NULL;
	free_uplink_config(&uplink_config);
}

/*
 *	Parse command line arguments
 */

void parse_cmdline(int argc, char *argv[])
{
	int s, i;
	int failed = 0;
	
	while ((s = getopt(argc, argv, "c:fn:r:e:o:l:?h")) != -1) {
	switch (s) {
		case 'c':
			cfgfile = hstrdup(optarg);
			break;
		case 'f':
			fork_a_daemon = 1;
			break;
		case 'n':
			logname = hstrdup(optarg);
			break;
		case 'r':
			log_dir = hstrdup(optarg);
			break;
		case 'e':
			i = pick_loglevel(optarg, log_levelnames);
			if (i > -1)
				log_level = i;
			else {
				fprintf(stderr, "Log level unknown: \"%s\"\n", optarg);
				failed = 1;
			}
			break;
		case 'o':
			i = pick_loglevel(optarg, log_destnames);
			if (i > -1)
				log_dest = i;
			else {
				fprintf(stderr, "Log destination unknown: \"%s\"\n", optarg);
				failed = 1;
			}
			break;
		case 'l':
			sound_file = hstrdup(optarg);
			break;
		case '?':
		case 'h':
			fprintf(stderr, "%s", VERSTR);
			failed = 1;
	}
	}
	
	if ((log_dest == L_FILE) && (!log_dir)) {
		fprintf(stderr, "Log destination set to 'file' but no log directory specified!\n");
		failed = 1;
	}
	
	if (failed) {
		fputs(HELPS, stderr);
		exit(failed);
	}
}


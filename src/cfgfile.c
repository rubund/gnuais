/*
 *	Configuration file parsing
 *
 *	(c) Tomi Manninen <tomi.manninen@hut.fi>
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include "cfgfile.h"
#include "hmalloc.h"
#include "hlog.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int cpfile(const char *to, const char *from)
{
	int fd_to, fd_from;
	char buf[4096];
	ssize_t nread;
	int saved_errno;

	fd_from = open(from, O_RDONLY);
	if (fd_from < 0)
		return -1;

	fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (fd_to < 0)
		goto out_error;

	while (nread = read(fd_from, buf, sizeof buf), nread > 0)
	{
	  char *out_ptr = buf;
	  ssize_t nwritten;

	  do {
		  nwritten = write(fd_to, out_ptr, nread);

		  if (nwritten >= 0)
		  {
		  nread -= nwritten;
		  out_ptr += nwritten;
	  }
		  else if (errno != EINTR)
		  {
		  goto out_error;
	  }
	  } while (nread > 0);
  }

	if (nread == 0)
	{
	  if (close(fd_to) < 0)
	  {
		fd_to = -1;
		goto out_error;
	}
	  close(fd_from);

	  /* Success! */
	  return 0;
  }

out_error:
	saved_errno = errno;

	close(fd_from);
	if (fd_to >= 0)
		close(fd_to);

	errno = saved_errno;
	return -1;
}

 /* ***************************************************************** */

/*
 *	Convert string to long long
 */
 
long long hatoll(char *s)
{
	long long res = 0LL;
	
	while (isdigit((int)*s))
		res = res * 10 + (*s++ - '0');
	
	return res;
}

 /* ***************************************************************** */

/*
 *	String to lower case
 */

extern char *strlwr(char *s)
{
	char *c;
	for (c = s; (*c); c++) {
		*c = tolower(*c);
	}
	return s;
}

 /* ***************************************************************** */

int do_string(char **dest, int argc, char **argv)
{
	if (argc < 2)
		return -1;
	if (*dest)
		hfree(*dest);
	*dest = hstrdup(argv[1]);
	return 0;
}

int do_int(int *dest, int argc, char **argv)
{
	if (argc < 2)
		return -1;
	*dest = atoi(argv[1]);
	return 0;
}

int do_toggle(int *dest, int argc, char **argv)
{
	if (argc < 2)
		return -1;
		
	if (
		strcasecmp(argv[1], "true") == 0
		|| strcasecmp(argv[1], "on") == 0
		|| strcasecmp(argv[1], "1") == 0
		|| strcasecmp(argv[1], "enable") == 0
		|| strcasecmp(argv[1], "enabled") == 0
		|| strcasecmp(argv[1], "yes") == 0
		) {
		*dest = 1;
			return 1;
	}
	*dest = 0;
	return 0;
}

 /* ***************************************************************** */

/*
 *	Parse c-style escapes (neat to have!)
 */
 
static char *parse_string(char *str)
{
	char *cp = str;
	unsigned long num;

	while (*str != '\0' && *str != '\"') {
		if (*str == '\\') {
			str++;
			switch (*str++) {
			case 'n':
				*cp++ = '\n';
				break;
			case 't':
				*cp++ = '\t';
				break;
			case 'v':
				*cp++ = '\v';
				break;
			case 'b':
				*cp++ = '\b';
				break;
			case 'r':
				*cp++ = '\r';
				break;
			case 'f':
				*cp++ = '\f';
				break;
			case 'a':
				*cp++ = '\007';
				break;
			case '\\':
				*cp++ = '\\';
				break;
			case '\"':
				*cp++ = '\"';
				break;
			case 'x':
				str--;
				num = strtoul(str, &str, 16);
				*cp++ = (char) num;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				str--;
				num = strtoul(str, &str, 8);
				*cp++ = (char) num;
				break;
			case '\0':
				return NULL;
			default:
				*cp++ = *(str - 1);
				break;
			};
		} else {
			*cp++ = *str++;
		}
	}
	if (*str == '\"')
		str++;		/* skip final quote */
	*cp = '\0';		/* terminate string */
	return str;
}

/*
 *	Parse command line to argv, honoring quotes and such
 */
 
int parse_args(char *argv[],char *cmd)
{
	int ct = 0;
	int quoted;
	
	while (ct < 255)
	{
		quoted = 0;
		while (*cmd && isspace((int)*cmd))
			cmd++;
		if (*cmd == 0)
			break;
		if (*cmd == '"') {
			quoted++;
			cmd++;
		}
		argv[ct++] = cmd;
		if (quoted) {
			if ((cmd = parse_string(cmd)) == NULL)
				return 0;
		} else {
			while (*cmd && !isspace((int)*cmd))
				cmd++;
		}
		if (*cmd)
			*cmd++ = 0;
	}
	argv[ct] = NULL;
	return ct;
}

/*
 *	Combine arguments back to a string
 */
 
char *argstr(int arg, int argc, char **argv)
{
	static char s[CFGLINE_LEN];
	int i;
	
	s[0] = '\0';
	
	for (i = arg; i < argc; i++) {
		strncat(s, argv[i], sizeof(s)-strlen(s)-1);
		strncat(s, " ", sizeof(s)-strlen(s)-1);
	}
	
	if ((i = strlen(s)) > 0)
		s[i-1] = '\0';
	
	return s;
}

/*
 *	Find the command from the command table and execute it.
 */
 
int cmdparse(struct cfgcmd *cmds, char *cmdline)
{
	struct cfgcmd *cmdp;
	int argc;
	char *argv[256];

	if ((argc = parse_args(argv, cmdline)) == 0 || *argv[0] == '#')
		return 0;
	strlwr(argv[0]);
	for (cmdp = cmds; cmdp->function != NULL; cmdp++)
		if (strncasecmp(cmdp->name, argv[0], strlen(argv[0])) == 0)
			break;
	if (cmdp->function == NULL) {
		fprintf(stderr, "No such configuration file directive: %s\n", argv[0]);
		return -1;
	}
	return (*cmdp->function)(cmdp->dest, argc, argv);
}

 /* ***************************************************************** */

/*
 *	Read configuration
 */
 
int read_cfgfile(char *f, struct cfgcmd *cmds)
{
	FILE *fp;
	FILE *tmp_file;
	char line[CFGLINE_LEN];
	int ret, n = 0;
	char *conf_home_folder;

	fp = fopen(f, "r");
    if(fp == NULL){
		conf_home_folder = hstrdup(getenv("HOME"));	
		conf_home_folder = str_append(conf_home_folder,"/.gnuais.conf");
		fp = fopen(conf_home_folder,"r");
		ret = 0;
		if(fp == NULL){
			tmp_file = fopen("/etc/gnuais.conf","r");	
			if(tmp_file == NULL){
				tmp_file = fopen("/usr/local/share/doc/gnuais/gnuais.conf-example","r");	
				if(tmp_file == NULL){
					tmp_file = fopen("/usr/share/doc/gnuais/gnuais.conf-example","r");	
					if(tmp_file == NULL){
						hlog(LOG_ERR,"No gnuais.conf-example found to be copied to ~/.gnuais.conf");
					}
					else {
						hlog(LOG_NOTICE, "Using gnuais.conf-example as a starting point for ~/.gnuais.conf...");
						ret = cpfile(conf_home_folder,"/usr/share/doc/gnuais/gnuais.conf-example");
						if(ret == -1) hlog(LOG_ERR, "Could not copy configuration file to the home folder");
						else hlog(LOG_NOTICE, "DONE creating configuration file (~/.gnuais.conf)");
					}
				}
				else {
					hlog(LOG_NOTICE, "Using gnuais.conf-example as a starting point for ~/.gnuais.conf...");
					ret = cpfile(conf_home_folder,"/usr/local/share/doc/gnuais/gnuais.conf-example");
					if(ret == -1) hlog(LOG_ERR, "Could not copy configuration file to the home folder");
					else hlog(LOG_NOTICE, "DONE creating configuration file (~/.gnuais.conf)");
				}
			}
			else {
				hlog(LOG_WARNING, "gnuais does not use the configuration file /etc/gnuais.conf anymore");
				hlog(LOG_WARNING, "It will be copied to your home directory ~/.gnuais.conf...");
				ret = cpfile(conf_home_folder,"/etc/gnuais.conf");
				if(ret == -1) hlog(LOG_ERR, "Could not copy configuration file from /etc/gnuais.conf to the home folder");
				else hlog(LOG_NOTICE, "DONE");
			}
			if(ret != -1) {
				fp = fopen(conf_home_folder,"r");
				if(fp == NULL){
					hlog(LOG_ERR, "Could not open configuration file after copying it to the home directory");
				}
			}
		}
	}
    if(fp == NULL){
		hlog(LOG_ERR, "No configuration file found! Running with the default configuration. You should create a file ~/.gnuais.conf. There should be an example to use in the source archive called gnuais.conf-example");
	}
	else {
		while (fgets(line, CFGLINE_LEN, fp) != NULL) {
			n++;
			ret = cmdparse(cmds, line);
			if (ret < 0) {
				fprintf(stderr, "Problem in %s at line %d: %s\n", f, n, line);
				fclose(fp);
				return 2;
			}
		}
		fclose(fp);
	}
	
	return 0;
}



/*
 *	serial port handling for NMEA AIS output
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "serial.h"
#include "hlog.h"
#include "hmalloc.h"
#include "cfg.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/*
 *	allocate serial state structure, open the serial device, configure
 *	terminal parameters, returns the structure
 */

struct serial_state_t *serial_init()
{
	struct serial_state_t *state = NULL;
	
	state = (struct serial_state_t *) hmalloc(sizeof(*state));
	
	//Returns the file descriptor on success or -1 on error.
	/* File descriptor for the port */
	state->fd = open(serial_port, O_RDWR | O_NOCTTY | O_NDELAY);
	if (state->fd == -1) {	// Could not open the port.
		hlog(LOG_CRIT, "Could not open serial port %s: %s",
			serial_port, strerror(errno));
		return NULL;
	}
	
	hlog(LOG_INFO, "Opened serial port %s for NMEA output", serial_port);
	
	/* set file status flags to 0 - why? */
	fcntl(state->fd, F_SETFL, 0);
	
	struct termios options;
	/* get the current options */
	if (tcgetattr(state->fd, &options))
		hlog(LOG_ERR,
			"Could not read serial port parameters on port %s: %s",
			serial_port, strerror(errno));
	
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
	if (tcsetattr(state->fd, TCSANOW, &options))
		hlog(LOG_ERR,
			"Could not configure serial port parameters on port %s: %s",
			serial_port, strerror(errno));
	
	return state;
}

/*
 *	write a string to the serial port
 */

int serial_write(struct serial_state_t *state, char *s, int len)
{
	int n = write(state->fd, s, len);
	
	if (n < 0)
		hlog(LOG_ERR, "Could not write to serial port %s: %s",
			serial_port, strerror(errno));
#ifdef DEBUG_SERIAL
	else
		hlog(LOG_DEBUG, "Wrote %d bytes to serial port", n);
#endif
	return n;
}

/*
 *	close the serial port and free the state structure
 */

int serial_close(struct serial_state_t *state)
{
	int ret = 0;
	
	if (state->fd >= 0) {
		if (close(state->fd)) {
			hlog(LOG_ERR, "Could not close serial port %s: %s", serial_port, strerror(errno));
			ret = -1;
		}
		state->fd = -1;
	}
	
	hfree(state);
	
	return ret;
}


/*
 *	protodec.c
 *
 *	(c) Ruben Undheim 2013
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

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "ipc.h"
#include "hlog.h"

#define UNIX_PATH_MAX 100

int socket_fd;
struct sockaddr_un address;
static pthread_t ipc_th;

static void gnuais_ipc_socketlistener(void *asdf){
	int connection_fd;
	socklen_t address_length;
	hlog(LOG_INFO,"IPC thread started");
	while((connection_fd = accept(socket_fd, (struct sockaddr *) &address, &address_length)) > - 1){
		hlog(LOG_INFO,"Client connected to IPC socket");
	}
	hlog(LOG_INFO,"Done in IPC thread");
}


int gnuais_ipc_startthread(){
	if(pthread_create(&ipc_th, NULL, (void *)gnuais_ipc_socketlistener, NULL)) {
		hlog(LOG_CRIT, "pthread_create failed for gnuais_ipc_socketlistener");
		return -1;
	}
	return 0;
}

void gnuais_ipc_deinit(){
	close(socket_fd);
	unlink("/tmp/gnuais.socket");
}

int gnuais_ipc_init(){
	int connection_fd;
	pid_t child;

	socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if(socket_fd < 0){
		hlog(LOG_ERR,"socket() failed");
		return -1;
	}

	unlink("/tmp/gnuais.socket");

	memset(&address, 0, sizeof(struct sockaddr_un));
	address.sun_family = AF_UNIX;
	snprintf(address.sun_path, UNIX_PATH_MAX,"/tmp/gnuais.socket");

	if(bind(socket_fd,(struct sockaddr *) &address, sizeof(struct sockaddr_un)) != 0) {
		hlog(LOG_ERR, "bind() failed");
		return -1;
	}

	if(listen(socket_fd, 1) != 0){
		hlog(LOG_ERR, "listen() failed");
		return -1;
	}
	if(gnuais_ipc_startthread() != 0)
		return -1;
	return 0;
}


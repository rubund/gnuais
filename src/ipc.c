
/*
 *	ipc.c
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
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include "ipc.h"
#include "hlog.h"
#include "hmalloc.h"

#define UNIX_PATH_MAX 100

int socket_fd;
struct sockaddr_un address;
static pthread_t ipc_th;

pthread_mutex_t ipc_mut = PTHREAD_MUTEX_INITIALIZER;

static void gnuais_ipc_socketlistener(void *asdf){
	int connection_fd;
	struct ipc_state_t *ipc;
	ipc = (struct ipc_state_t *)asdf;
	socklen_t address_length;
	hlog(LOG_INFO,"IPC thread started");
	while(((connection_fd = accept(socket_fd, (struct sockaddr *) &address, &address_length)) > - 1) && ipc->numclientsockets < (MAX_CLIENT_SOCKETS - 2)){
		pthread_mutex_lock(&ipc_mut);
		ipc->clientsocket[ipc->numclientsockets] = connection_fd;
		ipc->numclientsockets++;
		pthread_mutex_unlock(&ipc_mut);
		hlog(LOG_INFO,"numclientsockets: %d\n",ipc->numclientsockets);
	}
	hlog(LOG_INFO,"Done in IPC thread");
}


int gnuais_ipc_startthread(struct ipc_state_t* ipc){
	if(pthread_create(&ipc_th, NULL, (void *)gnuais_ipc_socketlistener, (void*)ipc)) {
		hlog(LOG_CRIT, "pthread_create failed for gnuais_ipc_socketlistener");
		return -1;
	}
	return 0;
}

void gnuais_ipc_deinit(struct ipc_state_t * ipc){
	int ret;
	shutdown(socket_fd,2);
	if ((ret = pthread_join(ipc_th, NULL))) {
		hlog(LOG_CRIT, "pthread_join of gnuais_ipc_socketlistener failed: %s", strerror(ret));
		return;
	}
	unlink("/tmp/gnuais.socket");
	hfree(ipc);	
}

struct ipc_state_t* gnuais_ipc_init(){
	struct ipc_state_t *s;
	s = (struct ipc_state_t*)malloc(sizeof(struct ipc_state_t));
	int returnvalue=0;
	s->numclientsockets = 0;

	socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if(socket_fd < 0){
		hlog(LOG_ERR,"socket() failed");
		returnvalue= -1;
		goto failed;
	}

	unlink("/tmp/gnuais.socket");

	memset(&address, 0, sizeof(struct sockaddr_un));
	address.sun_family = AF_UNIX;
	snprintf(address.sun_path, UNIX_PATH_MAX,"/tmp/gnuais.socket");

	if(bind(socket_fd,(struct sockaddr *) &address, sizeof(struct sockaddr_un)) != 0) {
		hlog(LOG_ERR, "bind() failed");
		returnvalue= -1;
		goto failed;
	}

	if(listen(socket_fd, 1) != 0){
		hlog(LOG_ERR, "listen() failed");
		returnvalue= -1;
		goto failed;
	}
	if(gnuais_ipc_startthread(s) != 0)
		returnvalue= -1;

 failed:	
	if(returnvalue == -1){
		free(s);
		s = 0;
	}
	return s;
}

int ipc_write(struct ipc_state_t *ipc, char *buffer, int buflength){
	hlog(LOG_DEBUG,"IPC buffer: %s\n",buffer);
	int nbytes=0;
	int i;
	pthread_mutex_lock(&ipc_mut);
	for(i=0;i<(ipc->numclientsockets);i++) {
		nbytes = write(ipc->clientsocket[i], buffer, buflength);
		if(nbytes == 0){
			hlog(LOG_INFO,"One gnuaisgui client is disconnected\n");
		}
	}
	pthread_mutex_unlock(&ipc_mut);
	return 0;
}

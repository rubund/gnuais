#ifndef INC_AIS_H
#define INC_AIS_H

#include <signal.h>

extern long int cntr;

struct mysql_data_t {
	char mysql_host[50];
	char mysql_dbname[50];
	char mysql_username[50];
	char mysql_password[50];
	char mysql_table[50];
	char mysql_keepsmall[50];
	char mysql_oldlimit[50];
	char serial_port[50];
};



#endif

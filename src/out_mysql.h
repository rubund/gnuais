
#ifdef HAVE_MYSQL
#include <mysql/mysql.h>
#endif

struct mysql_state_t {
#ifdef HAVE_MYSQL
	MYSQL conn;
	int inserts;
#endif
} *my;

extern struct mysql_state_t *myout_init();

extern int myout_ais_position(struct mysql_state_t *my,
	int tid, int mmsi, float lat, float lon,
	float hdg, float course, float sog);

extern int myout_ais_basestation(struct mysql_state_t *my,
	int tid, int mmsi, float lat, float lon);

extern int myout_ais_vesseldata(struct mysql_state_t *my,
	int tid, int mmsi, char *name, char *destination,
	float draught, int A, int B, int C, int D);

extern int myout_nmea(struct mysql_state_t *my, int tid, char *nmea);


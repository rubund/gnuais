
#include "config.h"
#include "out_mysql.h"
#include "hlog.h"
#include "hmalloc.h"
#include "cfg.h"

#define MAX_SQL_LEN 2000

struct mysql_state_t *my;

struct mysql_state_t *myout_init()
{
#ifdef HAVE_MYSQL
	struct mysql_state_t *my;
	
	my = hmalloc(sizeof(*my));
	my->inserts = 0;
	
	if (!mysql_init(&my->conn)) {
		hlog(LOG_CRIT, "Could not initialize MySQL library!");
		return NULL;
	}
	//printf("Host: %s\nDB: %s\nuser: %s\npassword: %s\ntable: %s\n",mysql_host,mysql_dbname,mysql_username,mysql_password,mysql_table);
	if (!mysql_real_connect(&my->conn, mysql_host, mysql_user, mysql_password, mysql_db, 0, NULL, 0)) {
		hlog(LOG_CRIT, "Could not connect to MySQL!");
		return NULL;
	}
	
	return my;
#else
	hlog(LOG_CRIT, "MySQL support not built in.");
	return NULL;
#endif
}

int myout_delete_from(struct mysql_state_t *my, int now, char *table)
{
	char q[MAX_SQL_LEN];
	int qrows;
	const char *e;
	
	snprintf(q, MAX_SQL_LEN,
		"DELETE FROM %s WHERE time < %d", table, now - mysql_oldlimit);
	
	if (mysql_query(&my->conn, q)) {
		e = mysql_error(&my->conn);
		hlog(LOG_ERR, "Could not delete old data from %s: %s", table, e);
		return 0;
	}
	
	qrows = mysql_affected_rows(&my->conn);
	if (qrows > 0)
		hlog(LOG_DEBUG, "MySQL: Deleted %d rows from %s.", qrows, table);
	
	return qrows;
}

int myout_delete_old(struct mysql_state_t *my, int now)
{
	int rows;
	
	if (mysql_oldlimit > 0) {
		rows = myout_delete_from(my, now, "ais_position");
		rows += myout_delete_from(my, now, "ais_vesseldata");
		rows += myout_delete_from(my, now, "ais_basestation");
		rows += myout_delete_from(my, now, "ais_nmea");
	}
	
	return 0;
}

int myout_update_or_insert(struct mysql_state_t *my, char *upd, char *ins)
{
	int qrows;
	const char *e;
	
	if (mysql_keepsmall) {
		/* Ok, try to update date first */
		if (mysql_query(&my->conn, upd)) {
			e = mysql_error(&my->conn);
			hlog(LOG_ERR, "Could not update data in MySQL database: %s", e);
			return -1;
		}
		
		/* if we managed to update some rows, we're done
		 * - else, continue with insert 
		 */
		qrows = mysql_affected_rows(&my->conn);
		
		if (qrows > 0)
			return 0;
	}
	
	if (mysql_query(&my->conn, ins)) {
		e = mysql_error(&my->conn);
		hlog(LOG_ERR, "Could not insert data in MySQL database: %s", e);
		return -1;
	}
	
	return 0;
}

int myout_ais_position(struct mysql_state_t *my, int tid, int mmsi, float lat, float lon, float hdg, float course, float sog)
{
#ifdef HAVE_MYSQL
	char ins[MAX_SQL_LEN], upd[MAX_SQL_LEN];
	
	my->inserts++;
	if (my->inserts % 10 == 0) {
		myout_delete_old(my, tid);
		my->inserts = 0;
	}
	
	snprintf(ins, MAX_SQL_LEN,
		"INSERT INTO ais_position (time,mmsi,latitude,longitude,heading,course,speed) VALUES (%d,%d,%.7f,%.7f,%.5f,%f,%f)",
		tid, mmsi, lat, lon, hdg, course, sog);
	snprintf(upd, MAX_SQL_LEN,
		"UPDATE ais_position SET time=%d, latitude=%.7f, longitude=%.7f, heading=%f, course=%.5f, speed=%f WHERE mmsi=%d",
		tid, lat, lon, hdg, course, sog, mmsi);
	
	return myout_update_or_insert(my, upd, ins);
#else
	return -1;
#endif
}

int myout_ais_basestation(struct mysql_state_t *my, int tid, int mmsi, float lat, float lon)
{
#ifdef HAVE_MYSQL
	char ins[MAX_SQL_LEN], upd[MAX_SQL_LEN];
	
	snprintf(ins, MAX_SQL_LEN,
		"INSERT INTO ais_basestation (time,mmsi,longitude,latitude) VALUES (%d,%d,%.7f,%.7f)",
		tid, mmsi, lat, lon);
	snprintf(upd, MAX_SQL_LEN,
		"UPDATE ais_basestation SET time=%d, latitude=%.7f, longitude=%.7f WHERE mmsi=%d",
		tid, lat, lon, mmsi);
	
	return myout_update_or_insert(my, upd, ins);
#else
	return -1;
#endif
}

int myout_ais_vesseldata(struct mysql_state_t *my,
	int tid, int mmsi, char *name, char *destination,
	float draught, int A, int B, int C, int D)
{
#ifdef HAVE_MYSQL
	char ins[MAX_SQL_LEN], upd[MAX_SQL_LEN];
	
	snprintf(ins, MAX_SQL_LEN,
		"INSERT INTO ais_vesseldata (time,mmsi,name,destination,draught,A,B,C,D) VALUES (%d,%d,\"%s\",\"%s\",%f,%d,%d,%d,%d)",
		tid, mmsi, name, destination, draught, A, B, C, D);
	
	snprintf(upd, MAX_SQL_LEN,
		"UPDATE ais_vesseldata SET time=%d, name=\"%s\", destination=\"%s\", A=%d, B=%d, C=%d, D=%d, draught=%f WHERE mmsi=%d",
		tid, name, destination, A, B, C, D, draught, mmsi);
		
	return myout_update_or_insert(my, upd, ins);
#else
	return -1;
#endif
}


int myout_nmea(struct mysql_state_t *my, int tid, char *nmea)
{
#ifdef HAVE_MYSQL
	char q[MAX_SQL_LEN];
	const char *e;
	
	snprintf(q, MAX_SQL_LEN,
		"INSERT INTO ais_nmea (time, message) VALUES (%d,\"!%s\")",
		tid, nmea);
		
	if (mysql_query(&my->conn, q)) {
		e = mysql_error(&my->conn);
		hlog(LOG_ERR, "MySQL: Could not insert into ais_nmea: %s", e);
		return -1;
	}
	
	return 0;
#endif
}


/*
 *	out_json.c
 *
 *	(c) Heikki Hannikainen 2008
 *
 *	Send ship position data out in the JSON AIS format:
 *	http://wiki.ham.fi/JSON_AIS.en
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

#include "config.h"

#include <pthread.h>
#include <unistd.h>

#ifdef HAVE_CURL
#define ENABLE_JSONAIS_CURL
#include <curl/curl.h>
#endif

#include "out_json.h"
#include "splay.h"
#include "cache.h"
#include "hlog.h"
#include "hmalloc.h"
#include "cfg.h"

pthread_t jsonout_th;

#ifdef ENABLE_JSONAIS_CURL

/*
 *	export the contents of the buffer splaytree
 */

static void jsonout_export(void)
{
	unsigned int entries = 0;
	unsigned int exported = 0;
	struct spblk *x, *nextx;
	//char *post = hstrdup("c=dnshits");
	struct sptree *sp;
	struct cache_ent *e;
	char *json = NULL;
	int got_pos;
	
	/* fill in initial json */
	json = str_append(json,
		"{\n"
		"\t\"protocol\": \"jsonais\",\n"
		"\t\"groups\": [\n" /* start of groups */
		"\t\t{\n" /* start of group */
		"\t\t\t\"path\": [ { \"name\": \"%s\" } ],\n"
		"\t\t\t\"msgs\": [\n",
		mycall
		);
	
	/* get the current position cache */
	sp = cache_rotate();
	
	/* expire old entries */
	for (x = sp_fhead(sp); x != NULL; x = nextx) {
		entries++;
		nextx = sp_fnext(x);
		e = (struct cache_ent *)x->data;
		
		got_pos = ((e->lat > 0.0001 || e->lat < -0.0001) && (e->lon > 0.0001 || e->lon < -0.0001));
		
		if ((e->mmsi) && ( (got_pos) || (e->name) ) ) {
			hlog(LOG_DEBUG, "jsonout: exporting MMSI %d", e->mmsi);
			json = str_append(json,
				"%s{\"msgtype\": \"sp\", \"mmsi\": %d",
				(exported == 0) ? "" : ",\n",
				e->mmsi
				);
			
			if (got_pos)
				json = str_append(json, ", \"lat\": %.7f, \"lon\": %.7f",
					e->lat,
					e->lon
					);
			
			if (e->course >= 0)
				str_append(json, ", \"course\": %.1f", e->course);
			if (e->hdg >= 0)
				str_append(json, ", \"heading\": %.1f", e->hdg);
			if (e->sog >= 0)
				str_append(json, ", \"speed\": %.1f", e->sog);
			if (e->name)
				str_append(json, ", \"shipname\": \"%s\"", e->name);
			if (e->destination)
				str_append(json, ", \"destination\": \"%s\"", e->destination);
			
			str_append(json, "}");
			
			exported++;
		}
		
		cache_free_entry(e);
		
		sp_delete(x, sp);
	}
	
	//printf("POST data: %s\n", post);
	/*
	if ((url) && (entries)) {
		while (post_stats(url, post))
			sleep(60);
	}
	hfree(post);
	*/
	
	json = str_append(json,
		"\n\n"
		"\t\t\t]\n" /* end of message array */
		"\t\t}\n" /* end of the message group */
		"\t]\n" /* end of groups */
		"}\n" /* end of the whole json blob */
		);
	
	/* clean up */
	sp_null(sp);
	hfree(sp);
	
	hlog(LOG_DEBUG, "jsonout: %s", json);
	
	if (exported) {
		/* send out */
	}
	
	hfree(json);
}

/*
 *	exporting thread
 */

static void jsonout_thread(void *asdf)
{
	hlog(LOG_DEBUG, "jsonout: thread started");
	
	//curl_global_init(CURL_GLOBAL_NOTHING);
	
	while (1) {
		sleep(60);
		
		hlog(LOG_DEBUG, "jsonout: exporting");
		
		jsonout_export();
	}
}

int jsonout_init(void)
{
	if (pthread_create(&jsonout_th, NULL, (void *)jsonout_thread, NULL)) {
		hlog(LOG_CRIT, "pthread_create failed for jsonout_thread");
		return -1;
	}
                
	return 0;
}

#else // ENABLE_JSONAIS_CURL

int jsonout_init(void)
{
	hlog(LOG_CRIT, "jsonout_init: JSON AIS export not available in this build");
	return -1;
}

#endif

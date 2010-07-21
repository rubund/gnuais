
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
#include <time.h>
#include <errno.h>
#include <string.h>

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

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static pthread_t jsonout_th;
static int jsonout_die = 0;

#ifdef ENABLE_JSONAIS_CURL

/*
 *	a dummy curl response data handler - it'll get to handle whatever
 *	the upstream server gives us back
 */

size_t curl_wdata(void *ptr, size_t size, size_t nmemb, void *stream)
{
	return size * nmemb;
}


/*
 *	send a single json POST request to an upstream server
 */

static int jsonout_post_single(struct curl_httppost *post, const char *url)
{
	CURL *ch;
	CURLcode r;
	long retcode = 200;
	
	if (!(ch = curl_easy_init())) {
		hlog(LOG_ERR, "curl_easy_init() returned NULL");
		return 1;
	}
	
	do {
		if ((r = curl_easy_setopt(ch, CURLOPT_HTTPPOST, post))) {
			hlog(LOG_ERR, "curl_easy_setopt(CURLOPT_HTTPPOST) failed: %s", curl_easy_strerror(r));
			break;
		}
		
		if ((r = curl_easy_setopt(ch, CURLOPT_URL, url))) {
			hlog(LOG_ERR, "curl_easy_setopt(CURLOPT_URL) failed: %s (%s)", curl_easy_strerror(r), url);
			break;
		}
		
		if ((r = curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, &curl_wdata))) {
			hlog(LOG_ERR, "curl_easy_setopt(CURLOPT_WRITEFUNCTION) failed: %s", curl_easy_strerror(r));
			break;
		}
		
		if ((r = curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1))) {
			hlog(LOG_ERR, "curl_easy_setopt(CURLOPT_NOPROGRESS) failed: %s", curl_easy_strerror(r));
			break;
		}
		if ((r = curl_easy_setopt(ch, CURLOPT_VERBOSE, 0))) {
			hlog(LOG_ERR, "curl_easy_setopt(CURLOPT_VERBOSE) failed: %s", curl_easy_strerror(r));
			break;
		}
		
		if ((r = curl_easy_perform(ch))) {
			hlog(LOG_ERR, "curl_easy_perform() failed: %s (%s)", curl_easy_strerror(r), url);
			break;
		}
		
		if ((r = curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &retcode))) {
			hlog(LOG_ERR, "curl_easy_getinfo(CURLINFO_RESPONSE_CODE) failed: %s (%s)", curl_easy_strerror(r), url);
			break;
		}
	} while (0);
	
	curl_easy_cleanup(ch);
	
	if (retcode != 200) {
		hlog(LOG_ERR, "JSON AIS export: server for %s returned %ld\n", url, retcode);
		r = -1;
	}
	
	return (r);
}

/*
 *	Encode an unix timestamp in JSON AIS format
 *
 *	YYYYMMDDHHMMSS
 *	01234567890123
 */

int time_jsonais(time_t *t, char *buf, int buflen)
{
	int i;
	struct tm dt;
	
	/* check that the buffer is large enough - we use
	 * 14 bytes plus the NULL
	 */
	if (buflen < 15) {
		hlog(LOG_ERR, "time_jsonais: not enough space to produce JSON AIS timestamp");
		return -1;
	}
	
	/* thread-safe UTC */
	if (gmtime_r(t, &dt) == NULL) {
		hlog(LOG_ERR, "time_jsonais: gmtime_r failed");
		return -1;
	}
	
	i = snprintf(buf, buflen, "%04d%02d%02d%02d%02d%02d",
		dt.tm_year + 1900,
		dt.tm_mon + 1,
		dt.tm_mday,
		dt.tm_hour,
		dt.tm_min,
		dt.tm_sec);
	
	//hlog(LOG_DEBUG, "time_jsonais: %d => %s", *t, buf);
	
	return i;
}

/*
 *	produce a curl form structure containing the JSON data, and send
 *	it to all upstream servers one by one
 */

static void jsonout_post_all(char *json)
{
	struct uplink_config_t *up;
	struct curl_httppost *cpost = NULL, *last = NULL;
	
	curl_formadd(&cpost, &last,
		CURLFORM_COPYNAME, "jsonais",
		CURLFORM_CONTENTTYPE, "application/json",
		CURLFORM_PTRCONTENTS, json,
		CURLFORM_END);
	
	for (up = uplink_config; (up); up = up->next)
		if (up->proto == UPLINK_JSON)
			jsonout_post_single(cpost, up->url);
	
	curl_formfree(cpost);
}

/*
 *	export the contents of the buffer splaytree
 */

#define TBUF_LEN 15

static void jsonout_export(void)
{
	unsigned int entries = 0;
	unsigned int exported = 0;
	struct spblk *x, *nextx;
	struct sptree *sp;
	struct cache_ent *e;
	char *json = NULL;
	int got_pos;
	char tbuf[TBUF_LEN];
	time_t now;
	
	time(&now);
	time_jsonais(&now, tbuf, TBUF_LEN);
	
	/* fill in initial json */
	json = str_append(json,
		"{\n"
		"\t\"protocol\": \"jsonais\",\n"
		"\t\"encodetime\": \"%s\",\n"
		"\t\"groups\": [\n" /* start of groups */
		"\t\t{\n" /* start of group */
		"\t\t\t\"path\": [ { \"name\": \"%s\" } ],\n"
		"\t\t\t\"msgs\": [\n",
		tbuf,
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
		
		if ((e->mmsi) && (got_pos) ) {
			hlog(LOG_DEBUG, "jsonout: exporting MMSI %d position", e->mmsi);
			time_jsonais(&e->received_pos, tbuf, TBUF_LEN);
			json = str_append(json,
				"%s{\"msgtype\": 3, \"mmsi\": %d, \"rxtime\": \"%s\"",
				(exported == 0) ? "" : ",\n",
				e->mmsi, tbuf
				);
			
			json = str_append(json, ", \"lat\": %.7f, \"lon\": %.7f",
				e->lat,
				e->lon
				);
			
			if (e->course >= 0)
				json = str_append(json, ", \"course\": %.1f", e->course);
			if (e->hdg >= 0)
				json = str_append(json, ", \"heading\": %d", e->hdg);
			if (e->sog >= 0)
				json = str_append(json, ", \"speed\": %.1f", e->sog);
			if (e->navstat >= 0)
				json = str_append(json, ", \"status\": %d", e->navstat);
			
			json = str_append(json, "}");
			
			exported++;
		}
		
		if ((e->mmsi) && (e->name) ) {
			hlog(LOG_DEBUG, "jsonout: exporting MMSI %d data", e->mmsi);
			time_jsonais(&e->received_data, tbuf, TBUF_LEN);
			json = str_append(json,
				"%s{\"msgtype\": 5, \"mmsi\": %d, \"rxtime\": \"%s\"",
				(exported == 0) ? "" : ",\n",
				e->mmsi, tbuf
				);
			
			if (e->imo >= 0)
				json = str_append(json, ", \"imo\": %d", e->imo);
			if (e->shiptype >= 0)
				json = str_append(json, ", \"shiptype\": %d", e->shiptype);
			if (e->callsign)
				json = str_append(json, ", \"callsign\": \"%s\"", e->callsign);
			if (e->name)
				json = str_append(json, ", \"shipname\": \"%s\"", e->name);
			if (e->destination)
				json = str_append(json, ", \"destination\": \"%s\"", e->destination);
			
			if (e->A >= 0 && e->B >= 0) {
				json = str_append(json, ", \"length\": %d", e->A + e->B);
				json = str_append(json, ", \"ref_front\": %d", e->A);
			}
			
			if (e->draught >= 0)
				json = str_append(json, ", \"draught\": %.1f", e->draught);
			
			if (e->C >= 0 && e->D >= 0) {
				json = str_append(json, ", \"width\": %d", e->C + e->D);
				json = str_append(json, ", \"ref_left\": %d", e->C);
			}
			
			json = str_append(json, "}");
			
			exported++;
		}
		
		if (e->persons_on_board >= 0) {
			hlog(LOG_DEBUG, "jsonout: exporting MMSI %d persons_on_board %d", e->mmsi, e->persons_on_board);
			time_jsonais(&e->received_persons_on_board, tbuf, TBUF_LEN);
			json = str_append(json,
				"%s{\"msgtype\": 8, \"mmsi\": %d, \"persons_on_board\": %d, \"rxtime\": \"%s\"}",
				(exported == 0) ? "" : ",\n",
				e->mmsi, e->persons_on_board, tbuf
				);
			exported++;
		}
		
		cache_free_entry(e);
		
		sp_delete(x, sp);
	}
	
	json = str_append(json,
		"\n\n"
		"\t\t\t]\n" /* end of message array */
		"\t\t}\n" /* end of the message group */
		"\t]\n" /* end of groups */
		"}\n" /* end of the whole json blob */
		);
	
	/* clean up */
	if (sp) {
		sp_null(sp);
		hfree(sp);
	}
	
	hlog(LOG_DEBUG, "jsonout: %s", json);
	
	if (exported) {
		/* if we have some entries, send them out */
		jsonout_post_all(json);
	}
	
	hfree(json);
}

/*
 *	exporting thread
 */

static void jsonout_thread(void *asdf)
{
	int i;
	hlog(LOG_DEBUG, "jsonout: thread started");
	
	while (1) {
		for (i = 0; i < 60; i++) {
			if (jsonout_die)
				return;
			sleep(1);
		}
		
		hlog(LOG_DEBUG, "jsonout: exporting");
		
		jsonout_export();
	}
}

int jsonout_init(void)
{
	curl_global_init(CURL_GLOBAL_ALL);
	
	if (pthread_create(&jsonout_th, NULL, (void *)jsonout_thread, NULL)) {
		hlog(LOG_CRIT, "pthread_create failed for jsonout_thread");
		return -1;
	}
                
	return 0;
}

int jsonout_deinit(void)
{
	int ret;
	
	/* request death */
	jsonout_die = 1;
	
	if ((ret = pthread_join(jsonout_th, NULL))) {
		hlog(LOG_CRIT, "pthread_join of jsonout_thread failed: %s", strerror(ret));
		return -1;
	}
	
	curl_global_cleanup();
	
	return 0;
}

#else // ENABLE_JSONAIS_CURL

int jsonout_init(void)
{
	hlog(LOG_CRIT, "jsonout_init: JSON AIS export not available in this build");
	return -1;
}

int jsonout_deinit(void)
{
	return -1;
}

#endif

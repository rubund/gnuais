
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


#include <pthread.h>
#include <curl/curl.h>
#include <unistd.h>

#include "out_json.h"
#include "splay.h"
#include "cache.h"
#include "hlog.h"
#include "hmalloc.h"

pthread_t jsonout_th;

/*
 *	export the contents of the buffer splaytree
 */

static void jsonout_export(void)
{
	unsigned int entries = 0;
	uint32_t *i;
	struct spblk *x, *nextx;
	//char *post = hstrdup("c=dnshits");
	struct sptree *sp;
	struct cache_ent *e;
	
	/* get the current position cache */
	sp = cache_rotate();
	
	/* expire old entries */
	for (x = sp_fhead(sp); x != NULL; x = nextx) {
		entries++;
		nextx = sp_fnext(x);
		e = (struct cache_ent *)x->data;
		
		hlog(LOG_DEBUG, "jsonout: exporting MMSI %d", e->mmsi);
		//post = str_append(post, "%s%s/%u", (entries == 1) ? "&z=" : ",", pname(x->key), *i);
		
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
	
	/* clean up in case */
	sp_null(sp);
	hfree(sp);
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


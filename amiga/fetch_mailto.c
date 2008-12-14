/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Fetching of data from a file (implementation).
 */

#include <string.h>
#include "content/fetch.h"
#include "utils/log.h"
#include "utils/url.h"
#include <proto/dos.h>
#include <proto/exec.h>
#include "amiga/object.h"
#include <malloc.h>
#include "content/content.h"
#include <time.h>
#include <proto/utility.h>
#include "utils/messages.h"
#include <proto/openurl.h>

static struct MinList *ami_mailto_fetcher_list;
static UBYTE *ami_mailto_fetcher_buffer = NULL;

struct Library *OpenURLBase;
struct OpenURLIFace *IOpenURL;

/** Information for a single fetch. */
struct ami_mailto_fetch_info {
	struct fetch *fetch_handle; /**< The fetch handle we're parented by. */
	BPTR fh;	/** File handle */
	bool only_2xx;		/**< Only HTTP 2xx responses acceptable. */
	char *path;
	char *url;		/**< URL of this fetch. */
	bool aborted;
	bool locked;
	struct nsObject *obj;
	int httpcode;
	ULONG len;
	char *mimetype;
	struct cache_data cachedata;
};

static bool ami_fetch_mailto_initialise(const char *scheme);
static void ami_fetch_mailto_finalise(const char *scheme);
static void * ami_fetch_mailto_setup(struct fetch *parent_fetch, const char *url,
		 bool only_2xx, const char *post_urlenc,
		 struct form_successful_control *post_multipart,
		 const char **headers);
static bool ami_fetch_mailto_start(void *vfetch);
static void ami_fetch_mailto_abort(void *vf);
static void ami_fetch_mailto_free(void *f);
static void ami_fetch_mailto_poll(const char *scheme_ignored);

/**
 * Initialise the fetcher.
 *
 * Must be called once before any other function.
 */

void ami_fetch_mailto_register(void)
{
	if(OpenURLBase = OpenLibrary("openurl.library",0))
	{
		if(IOpenURL = (struct OpenURLIFace *)GetInterface(OpenURLBase,"main",1,NULL))
		{
			if (!fetch_add_fetcher("mailto",
				       ami_fetch_mailto_initialise,
				       ami_fetch_mailto_setup,
				       ami_fetch_mailto_start,
				       ami_fetch_mailto_abort,
				       ami_fetch_mailto_free,
				       ami_fetch_mailto_poll,
				       ami_fetch_mailto_finalise)) {
				LOG(("Unable to register Amiga fetcher for mailto:"));
			}
		}
	}
}


/**
 * Initialise a cURL fetcher.
 */

bool ami_fetch_mailto_initialise(const char *scheme)
{
	LOG(("Initialise Amiga fetcher for %s", scheme));
	ami_mailto_fetcher_list = NewObjList();

	if(ami_mailto_fetcher_list) return true;
		else return false;
}


/**
 * Finalise a cURL fetcher
 */

void ami_fetch_mailto_finalise(const char *scheme)
{
	LOG(("Finalise Amiga fetcher %s", scheme));
	FreeObjList(ami_mailto_fetcher_list);
	if(IOpenURL) DropInterface((struct Interface *)IOpenURL);
	if(OpenURLBase) CloseLibrary(OpenURLBase);

}


/**
 * Start fetching data for the given URL.
 *
 * The function returns immediately. The fetch may be queued for later
 * processing.
 *
 * A pointer to an opaque struct curl_fetch_info is returned, which can be passed to
 * fetch_abort() to abort the fetch at any time. Returns 0 if memory is
 * exhausted (or some other fatal error occurred).
 *
 * The caller must supply a callback function which is called when anything
 * interesting happens. The callback function is first called with msg
 * FETCH_TYPE, with the Content-Type header in data, then one or more times
 * with FETCH_DATA with some data for the url, and finally with
 * FETCH_FINISHED. Alternatively, FETCH_ERROR indicates an error occurred:
 * data contains an error message. FETCH_REDIRECT may replace the FETCH_TYPE,
 * FETCH_DATA, FETCH_FINISHED sequence if the server sends a replacement URL.
 *
 * Some private data can be passed as the last parameter to fetch_start, and
 * callbacks will contain this.
 */

void * ami_fetch_mailto_setup(struct fetch *parent_fetch, const char *url,
		 bool only_2xx, const char *post_urlenc,
		 struct form_successful_control *post_multipart,
		 const char **headers)
{
	struct ami_mailto_fetch_info *fetch;

	fetch = AllocVec(sizeof (*fetch),MEMF_PRIVATE | MEMF_CLEAR);
	if (!fetch)
		return 0;

	fetch->fetch_handle = parent_fetch;

	/* construct a new fetch structure */
	fetch->fh = 0;
	fetch->only_2xx = only_2xx;
	fetch->url = strdup(url);
//	fetch->path = url_to_path(url);

//	LOG(("fetch %p, url '%s', path '%s'", fetch, url,fetch->path));

	fetch->obj = AddObject(ami_mailto_fetcher_list,AMINS_FETCHER);
	fetch->obj->objstruct = fetch;

	return fetch;
}


/**
 * Dispatch a single job
 */
bool ami_fetch_mailto_start(void *vfetch)
{
	struct ami_mailto_fetch_info *fetch = (struct ami_mailto_fetch_info*)vfetch;

	LOG(("ami file fetcher start"));

	fetch->cachedata.req_time = time(NULL);
	fetch->cachedata.res_time = time(NULL);
	fetch->cachedata.date = 0;
	fetch->cachedata.expires = 0;
	fetch->cachedata.age = INVALID_AGE;
	fetch->cachedata.max_age = 0;
	fetch->cachedata.no_cache = true;
	fetch->cachedata.etag = NULL;
	fetch->cachedata.last_modified = 0;

	return true;
}

void ami_fetch_mailto_abort(void *vf)
{
	struct ami_mailto_fetch_info *fetch = (struct ami_mailto_fetch_info*)vf;

	LOG(("ami mailto fetcher abort"));

	fetch->aborted = true;
}


/**
 * Free a fetch structure and associated resources.
 */

void ami_fetch_mailto_free(void *vf)
{
	struct ami_mailto_fetch_info *fetch = (struct ami_mailto_fetch_info*)vf;
	LOG(("ami file fetcher free %lx",fetch));

	free(fetch->url);
	DelObject(fetch->obj); // delobject frees fetch
}

static void ami_fetch_mailto_send_callback(fetch_msg msg,
		struct ami_mailto_fetch_info *fetch, const void *data,
		unsigned long size)
{
	fetch->locked = true;
	LOG(("ami file fetcher callback %ld",msg));
	fetch_send_callback(msg,fetch->fetch_handle,data,size);
	fetch->locked = false;
}

/**
 * Do some work on current fetches.
 *
 * Must be called regularly to make progress on fetches.
 */

void ami_fetch_mailto_poll(const char *scheme_ignored)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct ami_mailto_fetch_info *fetch;
	
	if(IsMinListEmpty(ami_mailto_fetcher_list)) return;

	node = (struct nsObject *)GetHead((struct List *)ami_mailto_fetcher_list);

	do
	{
		nnode=(struct nsObject *)GetSucc((struct Node *)node);

		fetch = (struct ami_mailto_fetch_info *)node->objstruct;

		if(fetch->locked) continue;

		URL_OpenA(fetch->url,NULL);

		fetch_set_http_code(fetch->fetch_handle,302);
		ami_fetch_mailto_send_callback(FETCH_REDIRECT, fetch,
					fetch_get_referer(fetch->fetch_handle), 0);

		fetch_remove_from_queues(fetch->fetch_handle);
		fetch_free(fetch->fetch_handle);

	}while(node=nnode);
}

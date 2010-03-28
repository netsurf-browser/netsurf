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

static struct MinList *ami_file_fetcher_list = NULL;
static UBYTE *ami_file_fetcher_buffer = NULL;

/** Information for a single fetch. */
struct ami_file_fetch_info {
	struct fetch *fetch_handle; /**< The fetch handle we're parented by. */
	BPTR fh;	/** File handle */
	bool only_2xx;		/**< Only HTTP 2xx responses acceptable. */
	char *path;
	char *url;		/**< URL of this fetch. */
	bool aborted;
	bool locked;
	struct nsObject *obj;
	int httpcode;
	int64 len;
	char *mimetype;
	struct cache_data cachedata;
};

static bool ami_fetch_file_initialise(const char *scheme);
static void ami_fetch_file_finalise(const char *scheme);
static void * ami_fetch_file_setup(struct fetch *parent_fetch, const char *url,
		 bool only_2xx, const char *post_urlenc,
		 struct fetch_multipart_data *post_multipart,
		 const char **headers);
static bool ami_fetch_file_start(void *vfetch);
static void ami_fetch_file_abort(void *vf);
static void ami_fetch_file_free(void *f);
static void ami_fetch_file_poll(const char *scheme_ignored);

/**
 * Initialise the fetcher.
 *
 * Must be called once before any other function.
 */

void ami_fetch_file_register(void)
{
	if (!fetch_add_fetcher("file",
				       ami_fetch_file_initialise,
				       ami_fetch_file_setup,
				       ami_fetch_file_start,
				       ami_fetch_file_abort,
				       ami_fetch_file_free,
				       ami_fetch_file_poll,
				       ami_fetch_file_finalise)) {
			LOG(("Unable to register Amiga fetcher for file:"));
	}
}


/**
 * Initialise a cURL fetcher.
 */

bool ami_fetch_file_initialise(const char *scheme)
{
	LOG(("Initialise Amiga fetcher for %s", scheme));
	ami_file_fetcher_list = NewObjList();
	ami_file_fetcher_buffer = AllocVec(1024,MEMF_PRIVATE);

	if(ami_file_fetcher_list && ami_file_fetcher_buffer) return true;
		else return false;
}


/**
 * Finalise a cURL fetcher
 */

void ami_fetch_file_finalise(const char *scheme)
{
	LOG(("Finalise Amiga fetcher %s", scheme));
	FreeObjList(ami_file_fetcher_list);
	FreeVec(ami_file_fetcher_buffer);
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

void * ami_fetch_file_setup(struct fetch *parent_fetch, const char *url,
		 bool only_2xx, const char *post_urlenc,
		 struct fetch_multipart_data *post_multipart,
		 const char **headers)
{
	struct ami_file_fetch_info *fetch;

	fetch = AllocVec(sizeof (*fetch),MEMF_PRIVATE | MEMF_CLEAR);
	if (!fetch)
		return 0;

	fetch->fetch_handle = parent_fetch;

	/* construct a new fetch structure */
	fetch->fh = 0;
	fetch->only_2xx = only_2xx;
//	fetch->url = strdup(url);
	fetch->path = url_to_path(url);

	LOG(("fetch %p, url '%s', path '%s'", fetch, url,fetch->path));

	fetch->obj = AddObject(ami_file_fetcher_list,AMINS_FETCHER);
	fetch->obj->objstruct = fetch;

	return fetch;
}


/**
 * Dispatch a single job
 */
bool ami_fetch_file_start(void *vfetch)
{
	struct ami_file_fetch_info *fetch = (struct ami_file_fetch_info*)vfetch;

	/* LOG(("ami file fetcher start")); */

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

void ami_fetch_file_abort(void *vf)
{
	struct ami_file_fetch_info *fetch = (struct ami_file_fetch_info*)vf;

	/* LOG(("ami file fetcher abort")); */

	if (fetch->fh) {
		FClose(fetch->fh);
		fetch->fh = 0;
	}
	fetch->aborted = true;
}


/**
 * Free a fetch structure and associated resources.
 */

void ami_fetch_file_free(void *vf)
{
	struct ami_file_fetch_info *fetch = (struct ami_file_fetch_info*)vf;
	/* LOG(("ami file fetcher free %lx",fetch)); */

	if(fetch->fh) FClose(fetch->fh);
	if(fetch->mimetype) free(fetch->mimetype);
	if(fetch->path) free(fetch->path);

	DelObject(fetch->obj); // delobject frees fetch
}

static void ami_fetch_file_send_callback(fetch_msg msg,
		struct ami_file_fetch_info *fetch, const void *data,
		unsigned long size, fetch_error_code errorcode)
{
	fetch->locked = true;
	/* LOG(("ami file fetcher callback %ld",msg)); */
	fetch_send_callback(msg,fetch->fetch_handle,data,size,errorcode);
	fetch->locked = false;
}

/**
 * Do some work on current fetches.
 *
 * Must be called regularly to make progress on fetches.
 */

void ami_fetch_file_poll(const char *scheme_ignored)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct ami_file_fetch_info *fetch;
	fetch_error_code errorcode;
	
	if(IsMinListEmpty(ami_file_fetcher_list)) return;

	node = (struct nsObject *)GetHead((struct List *)ami_file_fetcher_list);

	do
	{
		errorcode = FETCH_ERROR_NO_ERROR;
		nnode=(struct nsObject *)GetSucc((struct Node *)node);

		fetch = (struct ami_file_fetch_info *)node->objstruct;

		if(fetch->locked) continue;

		if(!fetch->aborted)
		{
			if(fetch->fh)
			{
				ULONG len;

				len = FRead(fetch->fh,ami_file_fetcher_buffer,1,1024);

				if (len == (ULONG)-1)
					errorcode = FETCH_ERROR_MISC;
				else if (len > 0)
					ami_fetch_file_send_callback(
							FETCH_DATA, fetch,
							ami_file_fetcher_buffer,
							len, errorcode);

				if((len<1024) && (!fetch->aborted))
				{
					ami_fetch_file_send_callback(FETCH_FINISHED,
						fetch, &fetch->cachedata, 0,
						errorcode);

					fetch->aborted = true;
				}
			}
			else
			{
				fetch->fh = FOpen(fetch->path,MODE_OLDFILE,0);

				if(fetch->fh)
				{
					char header[64];
					struct ExamineData *fib;
					if(fib = ExamineObjectTags(EX_FileHandleInput,fetch->fh,TAG_DONE))
					{
						fetch->len = fib->FileSize;
						FreeDosObject(DOS_EXAMINEDATA,fib);
					}

					fetch_set_http_code(fetch->fetch_handle,200);
					fetch->mimetype = fetch_mimetype(fetch->path);
					LOG(("mimetype %s len %ld",fetch->mimetype,fetch->len));

					snprintf(header, sizeof header,
							"Content-Type: %s",
							fetch->mimetype);
					ami_fetch_file_send_callback(FETCH_HEADER,
						fetch, header, strlen(header), errorcode);

					snprintf(header, sizeof header,
							"Content-Length: %ld",
							fetch->len);
					ami_fetch_file_send_callback(FETCH_HEADER,
						fetch, header, strlen(header), errorcode);

				}
				else
				{
					STRPTR errorstring;

					errorstring = ASPrintf("%s %s",messages_get("FileError"),fetch->path);
					fetch_set_http_code(fetch->fetch_handle,404);
					
					errorcode = FETCH_ERROR_HTTP_NOT2;
					ami_fetch_file_send_callback(FETCH_ERROR, fetch,
						errorstring, 0,
						errorcode);
					fetch->aborted = true;
					FreeVec(errorstring);
				}
			}
		}

		if(fetch && fetch->aborted)
		{
			fetch_remove_from_queues(fetch->fetch_handle);
			fetch_free(fetch->fetch_handle);
			return;
		}
	}while(node=nnode);
}

/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf.
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

/** Information for a single fetch. */
struct ami_file_fetch_info {
	struct fetch *fetch_handle; /**< The fetch handle we're parented by. */
	BPTR fh;	/** File handle */
	bool only_2xx;		/**< Only HTTP 2xx responses acceptable. */
	char *path;
	char *url;		/**< URL of this fetch. */
};

static bool ami_fetch_file_initialise(const char *scheme);
static void ami_fetch_file_finalise(const char *scheme);
static void * ami_fetch_file_setup(struct fetch *parent_fetch, const char *url,
		 bool only_2xx, const char *post_urlenc,
		 struct form_successful_control *post_multipart,
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
	return true; /* Always succeeds */
}


/**
 * Finalise a cURL fetcher
 */

void ami_fetch_file_finalise(const char *scheme)
{
	LOG(("Finalise Amiga fetcher %s", scheme));
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
		 struct form_successful_control *post_multipart,
		 const char **headers)
{
	struct ami_file_fetch_info *fetch;

	fetch = AllocVec(sizeof (*fetch),MEMF_PRIVATE | MEMF_CLEAR);
	if (!fetch)
		return 0;

	fetch->fetch_handle = parent_fetch;

	LOG(("fetch %p, url '%s'", fetch, url));

	/* construct a new fetch structure */
	fetch->fh = 0;
	fetch->only_2xx = only_2xx;
//	fetch->url = strdup(url);
	fetch->path = url_to_path(url);

	return fetch;
}


/**
 * Dispatch a single job
 */
bool ami_fetch_file_start(void *vfetch)
{
	struct ami_file_fetch_info *fetch = (struct ami_file_fetch_info*)vfetch;

	fetch->fh = FOpen(fetch->path,MODE_OLDFILE,0);

	if(fetch->fh) return true;
		else return false;
}

void ami_fetch_file_abort(void *vf)
{
	struct ami_file_fetch_info *fetch = (struct ami_file_fetch_info*)vf;

	if (fetch->fh) {
		FClose(fetch->fh);
		fetch->fh = 0;
//		f->abort = true;
	} else {
		fetch_remove_from_queues(fetch->fetch_handle);
		fetch_free(fetch->fetch_handle);
	}
}


/**
 * Free a fetch structure and associated resources.
 */

void ami_fetch_file_free(void *vf)
{
	struct ami_file_fetch_info *fetch = (struct ami_file_fetch_info*)vf;

	if(fetch->fh)
	{
		FClose(fetch->fh);
	}

	FreeVec(fetch);
}


/**
 * Do some work on current fetches.
 *
 * Must be called regularly to make progress on fetches.
 */

void ami_fetch_file_poll(const char *scheme_ignored)
{
}


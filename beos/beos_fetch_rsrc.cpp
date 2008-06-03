/*
 * Copyright 2008 Rob Kendrick <rjek@netsurf-browser.org>
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

/* rsrc: URL handling. */
#warning XXX: WRITEME

#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <curl/curl.h>		/* for URL unescaping functions */
extern "C" {
#include "utils/config.h"
#include "content/fetch.h"
#include "content/urldb.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "render/form.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/ring.h"
#include "utils/base64.h"
}
#include "beos/beos_fetch_rsrc.h"

#include <image.h>
#include <Resources.h>

struct fetch_rsrc_context {
	struct fetch *parent_fetch;
	char *name;
	char *url;
	char *mimetype;
	char *data;
	size_t datalen;

	bool aborted;
	bool locked;
	
	struct fetch_rsrc_context *r_next, *r_prev;
};

static struct fetch_rsrc_context *ring = NULL;

static BResources *gAppResources = NULL;

static bool fetch_rsrc_initialise(const char *scheme)
{
	LOG(("fetch_rsrc_initialise called for %s", scheme));
	return true;
}

static void fetch_rsrc_finalise(const char *scheme)
{
	LOG(("fetch_rsrc_finalise called for %s", scheme));
}

static void *fetch_rsrc_setup(struct fetch *parent_fetch, const char *url,
		 bool only_2xx, const char *post_urlenc,
		 struct form_successful_control *post_multipart,
		 const char **headers)
{
	struct fetch_rsrc_context *ctx;
	ctx = (struct fetch_rsrc_context *)calloc(1, sizeof(*ctx));
	
	if (ctx == NULL)
		return NULL;
		
	ctx->parent_fetch = parent_fetch;
	ctx->url = strdup(url);
	
	if (ctx->url == NULL) {
		free(ctx);
		return NULL;
	}

	RING_INSERT(ring, ctx);
	
	return ctx;
}

static bool fetch_rsrc_start(void *ctx)
{
	return true;
}

static void fetch_rsrc_free(void *ctx)
{
	struct fetch_rsrc_context *c = (struct fetch_rsrc_context *)ctx;

	free(c->name);
	free(c->url);
	free(c->data);
	free(c->mimetype);
	RING_REMOVE(ring, c);
	free(ctx);
}

static void fetch_rsrc_abort(void *ctx)
{
	struct fetch_rsrc_context *c = (struct fetch_rsrc_context *)ctx;

	/* To avoid the poll loop having to deal with the fetch context
	 * disappearing from under it, we simply flag the abort here. 
	 * The poll loop itself will perform the appropriate cleanup.
	 */
	c->aborted = true;
}

static void fetch_rsrc_send_callback(fetch_msg msg, 
		struct fetch_rsrc_context *c, const void *data, 
		unsigned long size)
{
	c->locked = true;
	fetch_send_callback(msg, c->parent_fetch, data, size);
	c->locked = false;
}

static bool fetch_rsrc_process(struct fetch_rsrc_context *c)
{
	char *params;
	char *at = NULL;
	char *slash;
	char *comma = NULL;
	char *unescaped;
	uint32 type = 'data'; // default for embeded files
	int32 id = 0;
	
	/* format of a rsrc: URL is:
	 *   rsrc:[TYPE][@NUM]/name[,mime]
	 */
	
	LOG(("*** Processing %s", c->url));
	
	if (strlen(c->url) < 6) {
		/* 6 is the minimum possible length (rsrc:/) */
		fetch_rsrc_send_callback(FETCH_ERROR, c, 
			"Malformed rsrc: URL", 0);
		return false;
	}
	
	/* skip the rsrc: part */
	params = c->url + sizeof("rsrc:") - 1;
	
	/* find the slash */
	if ( (slash = strchr(params, '/')) == NULL) {
		fetch_rsrc_send_callback(FETCH_ERROR, c,
			"Malformed rsrc: URL", 0);
		return false;
	}
	comma = strchr(slash, ',');
	c->name = strdup(slash + 1);
	
	if (!comma) {
		/* there is no mimetype here, assume text/plain */
		c->mimetype = strdup("text/plain;charset=US-ASCII");
	} else {	
		/* make a copy of everything after the comma */
		c->mimetype = strdup(comma + 1);
		c->name[strlen(c->name) - strlen(comma)] = '\0';
	}
	
	if (c->mimetype == NULL) {
		fetch_rsrc_send_callback(FETCH_ERROR, c,
			"Unable to allocate memory for mimetype in rsrc: URL",
			0);
		return false;
	}

	if (params[0] != '/') {
		uint8 c1, c2, c3, c4;
		if (sscanf(params, "%c%c%c%c", &c1, &c2, &c3, &c4) > 3) {
			type = c1 << 24 | c2 << 16 | c3 << 8 | c4;
			printf("type:%4.4s\n", &type);
		}
	}

	fprintf(stderr, "fetch_rsrc: 0x%08lx, %ld, '%s'\n", type, id, c->name);

	bool found;
	if (id)
		found = gAppResources->HasResource(type, id);
	else
		found = gAppResources->HasResource(type, c->name);
	if (!found) {
		fetch_rsrc_send_callback(FETCH_ERROR, c,
			"Cannot locate rsrc: URL",
			0);
		return false;
	}

	size_t len;
	const void *data;
	if (id)
		data = gAppResources->LoadResource(type, id, &len);
	else
		data = gAppResources->LoadResource(type, c->name, &len);

	if (!data) {
		fetch_rsrc_send_callback(FETCH_ERROR, c,
			"Cannot load rsrc: URL",
			0);
		return false;
	}

	c->datalen = len;
	c->data = (char *)malloc(c->datalen);
	if (c->data == NULL) {
		fetch_rsrc_send_callback(FETCH_ERROR, c,
			"Unable to allocate memory for rsrc: URL", 0);
		return false;
	}
	memcpy(c->data, data, c->datalen);

	return true;
}

static void fetch_rsrc_poll(const char *scheme)
{
	struct fetch_rsrc_context *c, *next;
	struct cache_data cachedata;
	
	if (ring == NULL) return;
	
	cachedata.req_time = time(NULL);
	cachedata.res_time = time(NULL);
	cachedata.date = 0;
	cachedata.expires = 0;
	cachedata.age = INVALID_AGE;
	cachedata.max_age = 0;
	cachedata.no_cache = true;
	cachedata.etag = NULL;
	cachedata.last_modified = 0;

	/* Iterate over ring, processing each pending fetch */
	c = ring;
	do {
		/* Take a copy of the next pointer as we may destroy
		 * the ring item we're currently processing */
		next = c->r_next;

		/* Ignore fetches that have been flagged as locked.
		 * This allows safe re-entrant calls to this function.
		 * Re-entrancy can occur if, as a result of a callback,
		 * the interested party causes fetch_poll() to be called 
		 * again.
		 */
		if (c->locked == true) {
			continue;
		}

		/* Only process non-aborted fetches */
		if (!c->aborted && fetch_rsrc_process(c) == true) {
			fetch_set_http_code(c->parent_fetch, 200);
			LOG(("setting rsrc: MIME type to %s, length to %zd",
					c->mimetype, c->datalen));
			/* Any callback can result in the fetch being aborted.
			 * Therefore, we _must_ check for this after _every_
			 * call to fetch_rsrc_send_callback().
			 */
			fetch_rsrc_send_callback(FETCH_TYPE,
				c, c->mimetype, c->datalen);
			if (!c->aborted) {
				fetch_rsrc_send_callback(FETCH_DATA, 
					c, c->data, c->datalen);
			}
			if (!c->aborted) {
				fetch_rsrc_send_callback(FETCH_FINISHED, 
					c, &cachedata, 0);
			}
		} else {
			LOG(("Processing of %s failed!", c->url));

			/* Ensure that we're unlocked here. If we aren't, 
			 * then fetch_rsrc_process() is broken.
			 */
			assert(c->locked == false);
		}

		fetch_remove_from_queues(c->parent_fetch);
		fetch_free(c->parent_fetch);

		/* Advance to next ring entry, exiting if we've reached
		 * the start of the ring or the ring has become empty
		 */
	} while ( (c = next) != ring && ring != NULL);
}

/* BAppFileInfo is supposed to find the app's resources for us,
 * but this won't work if we ever want to be used as a replicant.
 * This trick should work regardless,
 */
static int find_app_resources()
{
	image_info info;
	const char *path = NULL;
	int32 cookie = 0;
	while (get_next_image_info(0, &cookie, &info) == B_OK) {
//fprintf(stderr, "%p <> %p, %p\n", (char *)&find_app_resources, (char *)info.text, (char *)info.text + info.text_size);
		if (((char *)&find_app_resources >= (char *)info.text)
		 && ((char *)&find_app_resources < (char *)info.text + info.text_size)) {
//fprintf(stderr, "match\n");
		 	path = info.name;
		 	break;
		}
	}
	if (path == NULL)
		return B_ERROR;

//fprintf(stderr, "loading resources from '%s'\n", path);

	BFile file(path, B_READ_ONLY);
	if (file.InitCheck() < 0)
		return file.InitCheck();
	gAppResources = new BResources;
	status_t err;
	err = gAppResources->SetTo(&file);
	if (err >= B_OK)
		return B_OK;
	delete gAppResources;
	gAppResources = NULL;
	return err;
}

void fetch_rsrc_register(void)
{
	int err;
	err = find_app_resources();
	if (err < B_OK) {
		warn_user("Resources", strerror(err));
		return;
	}
	fetch_add_fetcher("rsrc",
		fetch_rsrc_initialise,
		fetch_rsrc_setup,
		fetch_rsrc_start,
		fetch_rsrc_abort,
		fetch_rsrc_free,
		fetch_rsrc_poll,
		fetch_rsrc_finalise);
}

void fetch_rsrc_unregister(void)
{
	delete gAppResources;
	gAppResources = NULL;
}

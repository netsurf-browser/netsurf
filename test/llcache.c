#include <stdio.h>
#include <stdlib.h>

#include "content/fetch.h"
#include "content/llcache.h"
#include "utils/ring.h"
#include "utils/url.h"

/******************************************************************************
 * Things that we'd reasonably expect to have to implement                    *
 ******************************************************************************/

/* desktop/netsurf.h */
bool verbose_log;

/* utils/utils.h */
void die(const char * const error)
{
	fprintf(stderr, "%s\n", error);

	exit(1);
}

/* utils/utils.h */
void warn_user(const char *warning, const char *detail)
{
	fprintf(stderr, "%s %s\n", warning, detail);
}

/* content/fetch.h */
const char *fetch_filetype(const char *unix_path)
{
	return NULL;
}

/* content/fetch.h */
char *fetch_mimetype(const char *ro_path)
{
	return NULL;
}

/******************************************************************************
 * Things that are absolutely not reasonable, and should disappear            *
 ******************************************************************************/

#include "desktop/cookies.h"
#include "desktop/tree.h"

/* desktop/cookies.h -- used by urldb 
 *
 * URLdb should have a cookies update event + handler registration
 */
bool cookies_update(const char *domain, const struct cookie_data *data)
{
	return true;
}

/* image/bitmap.h -- used by urldb 
 *
 * URLdb shouldn't care about bitmaps. 
 * This is because the legacy RO thumbnail stuff was hacked in and must die.
 */
void bitmap_destroy(void *bitmap)
{
}

/* desktop/tree.h -- used by options.c 
 *
 * Why on earth is tree loading and saving in options.c?
 */
void tree_initialise(struct tree *tree)
{
}

/* desktop/tree.h */
struct node *tree_create_folder_node(struct node *parent, const char *title)
{
	return NULL;
}

/* desktop/tree.h */
struct node *tree_create_URL_node(struct node *parent, const char *url,
		const struct url_data *data, const char *title)
{
	return NULL;
}

/* desktop/tree.h */
struct node_element *tree_find_element(struct node *node, node_element_data d)
{
	return NULL;
}

/******************************************************************************
 * test: protocol handler                                                     *
 ******************************************************************************/

typedef struct test_context {
	struct fetch *parent;

	bool aborted;
	bool locked;

	struct test_context *r_prev;
	struct test_context *r_next;
} test_context;

static test_context *ring;

bool test_initialise(const char *scheme)
{
	/* Nothing to do */
	return true;
}

void test_finalise(const char *scheme)
{
	/* Nothing to do */
}

void *test_setup_fetch(struct fetch *parent, const char *url, bool only_2xx, 
		const char *post_urlenc, 
		struct fetch_multipart_data *post_multipart, 
		const char **headers)
{
	test_context *ctx = calloc(1, sizeof(test_context));

	if (ctx == NULL)
		return NULL;

	ctx->parent = parent;

	RING_INSERT(ring, ctx);

	return ctx;
}

bool test_start_fetch(void *handle)
{
	/* Nothing to do */
	return true;
}

void test_abort_fetch(void *handle)
{
	test_context *ctx = handle;

	ctx->aborted = true;
}

void test_free_fetch(void *handle)
{
	test_context *ctx = handle;

	RING_REMOVE(ring, ctx);

	free(ctx);
}

void test_process(test_context *ctx)
{
	/** \todo Implement */
}

void test_poll(const char *scheme)
{
	test_context *ctx, *next;

	if (ring == NULL)
		return;

	ctx = ring;
	do {
		next = ctx->r_next;

		if (ctx->locked)
			continue;

		if (ctx->aborted == false) {
			test_process(ctx);
		}

		fetch_remove_from_queues(ctx->parent);
		fetch_free(ctx->parent);
	} while ((ctx = next) != ring && ring != NULL);
}

/******************************************************************************
 * The actual test code                                                       *
 ******************************************************************************/

nserror query_handler(const llcache_query *query, void *pw,
		llcache_query_response cb, void *cbpw)
{
	/* I'm too lazy to actually implement this. It should queue the query, 
	 * then deliver the response from main(). */

	return NSERROR_OK;
}

nserror event_handler(const llcache_handle *handle, 
		const llcache_event *event, void *pw)
{
	static char *event_names[] = {
		"HAD_HEADERS", "HAD_DATA", "DONE", "ERROR", "PROGRESS"
	};
	bool *done = pw;

	if (event->type != LLCACHE_EVENT_PROGRESS)
		fprintf(stdout, "%p : %s\n", handle, event_names[event->type]);

	/* Inform main() that the fetch completed */
	if (event->type == LLCACHE_EVENT_DONE)
		*done = true;

	return NSERROR_OK;
}

int main(int argc, char **argv)
{
	nserror error;
	llcache_handle *handle;
	llcache_handle *handle2;
	bool done = false;

	/* Initialise subsystems */
	url_init();
	fetch_init();
	fetch_add_fetcher("test", test_initialise, test_setup_fetch, 
			test_start_fetch, test_abort_fetch, test_free_fetch, 
			test_poll, test_finalise);

	/* Initialise low-level cache */
	error = llcache_initialise(query_handler, NULL);
	if (error != NSERROR_OK) {
		fprintf(stderr, "llcache_initialise: %d\n", error);
		return 1;
	}

	/* Retrieve an URL from the low-level cache (may trigger fetch) */
	error = llcache_handle_retrieve("http://www.netsurf-browser.org/",
			LLCACHE_RETRIEVE_VERIFIABLE, NULL, NULL,
			event_handler, &done, &handle);
	if (error != NSERROR_OK) {
		fprintf(stderr, "llcache_handle_retrieve: %d\n", error);
		return 1;
	}

	/* Poll relevant components */
	while (done == false) {
		fetch_poll();
		llcache_poll();
	}

	done = false;
	error = llcache_handle_retrieve("http://www.netsurf-browser.org/",
			LLCACHE_RETRIEVE_VERIFIABLE, NULL, NULL,
			event_handler, &done, &handle2);
	if (error != NSERROR_OK) {
		fprintf(stderr, "llcache_handle_retrieve: %d\n", error);
		return 1;
	}

	while (done == false) {
		fetch_poll();
		llcache_poll();
	}

	fprintf(stdout, "%p -> %p\n", handle, 
			llcache_object_from_handle(handle));
	fprintf(stdout, "%p -> %p\n", handle2, 
			llcache_object_from_handle(handle2));

	/* Cleanup */
	llcache_handle_release(handle2);
	llcache_handle_release(handle);

	fetch_quit();

	return 0;
}


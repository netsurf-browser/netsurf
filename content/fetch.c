/**
 * $Id: fetch.c,v 1.2 2003/02/25 21:00:27 bursa Exp $
 */

#include <assert.h>
#include <time.h>
#include "curl/curl.h"
#include "netsurf/content/fetch.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/log.h"

struct fetch
{
	time_t start_time;
	CURL * curl_handle;
	void (*callback)(fetch_msg msg, void *p, char *data, unsigned long size);
	int had_headers : 1;
	int in_callback : 1;
	int aborting : 1;
	char *url;
	char error_buffer[CURL_ERROR_SIZE];
	void *p;
};

static const char * const user_agent = "NetSurf";
static CURLM * curl_multi;

static size_t fetch_curl_data(void * data, size_t size, size_t nmemb, struct fetch *f);


/**
 * fetch_init -- initialise the fetcher
 */

void fetch_init(void)
{
	CURLcode code;

	code = curl_global_init(CURL_GLOBAL_ALL);
	if (code != CURLE_OK)
		die("curl_global_init failed");

	curl_multi = curl_multi_init();
	if (curl_multi == 0)
		die("curl_multi_init failed");
}


/**
 * fetch_quit -- clean up for quit
 */

void fetch_quit(void)
{
	CURLMcode codem;

	codem = curl_multi_cleanup(curl_multi);
	if (codem != CURLM_OK)
		LOG(("curl_multi_cleanup failed: ignoring"));

	curl_global_cleanup();
}


/**
 * fetch_start -- start fetching data for the given url
 *
 * Returns immediately. The callback function will be called when
 * something interesting happens.
 */

struct fetch * fetch_start(char *url, char *referer,
                 void (*callback)(fetch_msg msg, void *p, char *data, unsigned long size), void *p)
{
	struct fetch* fetch = (struct fetch*) xcalloc(1, sizeof(struct fetch));
	CURLcode code;
	CURLMcode codem;

	LOG(("fetch %p, url '%s'", fetch, url));
  
	fetch->start_time = time(&fetch->start_time);
	fetch->callback = callback;
	fetch->had_headers = 0;
	fetch->in_callback = 0;
	fetch->aborting = 0;
	fetch->url = xstrdup(url);
	fetch->p = p;

	/* create the curl easy handle */
	fetch->curl_handle = curl_easy_init();
	assert(fetch->curl_handle != 0);  /* TODO: handle curl errors */
	code = curl_easy_setopt(fetch->curl_handle, CURLOPT_VERBOSE, 1);
	assert(code == CURLE_OK);
	code = curl_easy_setopt(fetch->curl_handle, CURLOPT_URL, fetch->url);
	assert(code == CURLE_OK);
	code = curl_easy_setopt(fetch->curl_handle, CURLOPT_PRIVATE, fetch);
	assert(code == CURLE_OK);
	code = curl_easy_setopt(fetch->curl_handle, CURLOPT_ERRORBUFFER, fetch->error_buffer);
	assert(code == CURLE_OK);
	code = curl_easy_setopt(fetch->curl_handle, CURLOPT_WRITEFUNCTION, fetch_curl_data);
	assert(code == CURLE_OK);
	code = curl_easy_setopt(fetch->curl_handle, CURLOPT_WRITEDATA, fetch);
	assert(code == CURLE_OK);
	code = curl_easy_setopt(fetch->curl_handle, CURLOPT_USERAGENT, user_agent);
	assert(code == CURLE_OK);
	if (referer != 0) {
		code = curl_easy_setopt(fetch->curl_handle, CURLOPT_REFERER, referer);
		assert(code == CURLE_OK);
	} 

	/* add to the global curl multi handle */
	codem = curl_multi_add_handle(curl_multi, fetch->curl_handle);
	assert(codem == CURLM_OK || codem == CURLM_CALL_MULTI_PERFORM);

	/* do any possible work on the fetch */
	while (codem == CURLM_CALL_MULTI_PERFORM) {
		int running;
		codem = curl_multi_perform(curl_multi, &running);
		assert(codem == CURLM_OK || codem == CURLM_CALL_MULTI_PERFORM);
	}

	return fetch;
}


/**
 * fetch_abort -- stop a fetch
 */

void fetch_abort(struct fetch *f)
{
	CURLMcode codem;

	assert(f != 0);
	LOG(("fetch %p, url '%s'", f, f->url));
  
	if (f->in_callback) {
		LOG(("in callback: will abort later"));
		f->aborting = 1;
		return;
	}
  
	/* remove from curl */
	codem = curl_multi_remove_handle(curl_multi, f->curl_handle);
	assert(codem == CURLM_OK);
	curl_easy_cleanup(f->curl_handle);

	xfree(f->url);
	xfree(f);
}


/**
 * fetch_poll -- do some work on current fetches
 *
 * Must return as soon as possible.
 */

void fetch_poll(void)
{
	CURLcode code;
	CURLMcode codem;
	int running, queue;
	CURLMsg * curl_msg;
	struct fetch *f;

	/* do any possible work on the current fetches */
	do {
		codem = curl_multi_perform(curl_multi, &running);
		assert(codem == CURLM_OK || codem == CURLM_CALL_MULTI_PERFORM);
	} while (codem == CURLM_CALL_MULTI_PERFORM);

	/* process curl results */
	curl_msg = curl_multi_info_read(curl_multi, &queue);
	while (curl_msg) {
		switch (curl_msg->msg) {
			case CURLMSG_DONE:
				/* find the structure associated with this fetch */
				code = curl_easy_getinfo(curl_msg->easy_handle, CURLINFO_PRIVATE, &f);
				assert(code == CURLE_OK);

				LOG(("CURLMSG_DONE, result %i", curl_msg->data.result));

				/* inform the caller that the fetch is done */
				if (curl_msg->data.result == CURLE_OK)
					f->callback(FETCH_FINISHED, f->p, 0, 0);
				else if (curl_msg->data.result != CURLE_WRITE_ERROR)
					f->callback(FETCH_ERROR, f->p, f->error_buffer, 0);

				/* clean up fetch */
				fetch_abort(f);

				break;

			default:
				assert(0);
		}
		curl_msg = curl_multi_info_read(curl_multi, &queue);
	}
}


/**
 * fetch_curl_data -- callback function for curl (internal)
 */

size_t fetch_curl_data(void * data, size_t size, size_t nmemb, struct fetch *f)
{
	f->in_callback = 1;

	LOG(("fetch %p, size %u", f, size * nmemb));

	if (!f->had_headers) {
		/* find the content type and inform the caller */
		char *type;
		CURLcode code;

		code = curl_easy_getinfo(f->curl_handle, CURLINFO_CONTENT_TYPE, &type);
		assert(code == CURLE_OK);

		if (type == 0)
			type = "text/html";  /* TODO: find type of file: urls */

		LOG(("FETCH_TYPE, '%s'", type));
		f->callback(FETCH_TYPE, f->p, type, 0);
		if (f->aborting) {
			f->in_callback = 0;
			return 0;
		}

		f->had_headers = 1;
	}

	/* send data to the caller */
	LOG(("FETCH_DATA"));
	f->callback(FETCH_DATA, f->p, data, size * nmemb);

	f->in_callback = 0;
	return size * nmemb;
}


/**
 * testing framework
 */

#ifdef TEST
#include <unistd.h>

struct test {char *url; struct fetch *f;};

void callback(fetch_msg msg, struct test *t, char *data, unsigned long size)
{
	printf("%s: ", t->url);
	switch (msg) {
		case FETCH_TYPE:
			printf("FETCH_TYPE '%s'", data);
			break;
		case FETCH_DATA:
			printf("FETCH_DATA %lu", size);
			break;
		case FETCH_FINISHED:
			printf("FETCH_FINISHED");
			break;
		case FETCH_ERROR:
			printf("FETCH_ERROR '%s'", data);
			break;
		default:
			assert(0);
	}
	printf("\n");
}

struct test test[] = {
	{"http://127.0.0.1/", 0},
	{"http://netsurf.strcprstskrzkrk.co.uk/", 0},
	{"http://www.oxfordstudent.com/", 0},
	{"http://www.google.co.uk/", 0},
	{"http://news.bbc.co.uk/", 0},
	{"http://doesnt.exist/", 0},
	{"blah://blah", 0},
};

int main(void)
{
	int i;
	fetch_init();
	for (i = 0; i != sizeof(test) / sizeof(test[0]); i++)
		test[i].f = fetch_start(test[i].url, 0, callback, &test[i]);
	while (1) {
		fetch_poll();
		sleep(1);
	}
	return 0;
}
#endif


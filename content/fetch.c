/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

/**
 * This module handles fetching of data from any url.
 *
 * Implementation:
 * This implementation uses libcurl's 'multi' interface.
 *
 * Active fetches are held in the linked list fetch_list. There may be at most
 * one fetch from each host. Any further fetches are queued until the previous
 * one ends.
 */

#include <assert.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "curl/curl.h"
#include "libxml/uri.h"
#include "netsurf/content/fetch.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/log.h"
#include "netsurf/desktop/options.h"
#ifdef riscos
#include "netsurf/desktop/gui.h"
#endif

struct fetch
{
	time_t start_time;
	CURL * curl_handle;
	void (*callback)(fetch_msg msg, void *p, char *data, unsigned long size);
	int had_headers : 1;
	int in_callback : 1;
	int aborting : 1;
	char *url;
	char *referer;
	char error_buffer[CURL_ERROR_SIZE];
	void *p;
	struct curl_slist *headers;
	char *host;
	int status_code;
	char *location;
	struct fetch *queue;
	struct fetch *prev;
	struct fetch *next;
};

static const char * const user_agent = "NetSurf";
static char * ca_bundle;
static CURLM * curl_multi;
static struct fetch *fetch_list = 0;

static size_t fetch_curl_data(void * data, size_t size, size_t nmemb, struct fetch *f);
static size_t fetch_curl_header(char * data, size_t size, size_t nmemb, struct fetch *f);
static int fetch_process_headers(struct fetch *f);

#ifdef riscos
extern const char * const NETSURF_DIR;
#endif


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

#ifdef riscos
	ca_bundle = xcalloc(strlen(NETSURF_DIR) + 100, 1);
	sprintf(ca_bundle, "%s.Resources.ca-bundle", NETSURF_DIR);
	LOG(("ca_bundle '%s'", ca_bundle));
#endif
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
	struct fetch *fetch = xcalloc(1, sizeof(*fetch)), *host_fetch;
	CURLcode code;
	CURLMcode codem;
	xmlURI *uri;

	LOG(("fetch %p, url '%s'", fetch, url));

	uri = xmlParseURI(url);
	if (uri == 0) {
		LOG(("warning: failed to parse url"));
		return 0;
	}

	/* construct a new fetch structure */
	fetch->start_time = time(0);
	fetch->callback = callback;
	fetch->had_headers = 0;
	fetch->in_callback = 0;
	fetch->aborting = 0;
	fetch->url = xstrdup(url);
	fetch->referer = 0;
	if (referer != 0)
		fetch->referer = xstrdup(referer);
	fetch->p = p;
	fetch->headers = 0;
	fetch->host = 0;
	if (uri->server != 0)
		fetch->host = xstrdup(uri->server);
	fetch->status_code = 0;
	fetch->queue = 0;
	fetch->prev = 0;
	fetch->next = 0;

	xmlFreeURI(uri);

	/* look for a fetch from the same host */
	if (fetch->host != 0) {
		for (host_fetch = fetch_list;
				host_fetch != 0 && (host_fetch->host == 0 ||
					strcasecmp(host_fetch->host, fetch->host) != 0);
				host_fetch = host_fetch->next)
			;
		if (host_fetch != 0) {
			/* fetch from this host in progress: queue the new fetch */
			LOG(("queueing"));
			fetch->queue = host_fetch->queue;
			host_fetch->queue = fetch;
			return fetch;
		}
	}

	fetch->next = fetch_list;
	if (fetch_list != 0)
		fetch_list->prev = fetch;
	fetch_list = fetch;

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
	code = curl_easy_setopt(fetch->curl_handle, CURLOPT_HEADERFUNCTION, fetch_curl_header);
	assert(code == CURLE_OK);
	code = curl_easy_setopt(fetch->curl_handle, CURLOPT_WRITEHEADER, fetch);
	assert(code == CURLE_OK);
	code = curl_easy_setopt(fetch->curl_handle, CURLOPT_USERAGENT, user_agent);
	assert(code == CURLE_OK);
	if (referer != 0) {
		code = curl_easy_setopt(fetch->curl_handle, CURLOPT_REFERER, referer);
		assert(code == CURLE_OK);
	}
#ifdef riscos
	code = curl_easy_setopt(fetch->curl_handle, CURLOPT_CAINFO, ca_bundle);
	assert(code == CURLE_OK);
#endif

	/* custom request headers */
	fetch->headers = 0;
	/* remove curl default headers */
	fetch->headers = curl_slist_append(fetch->headers, "Accept:");
	fetch->headers = curl_slist_append(fetch->headers, "Pragma:");
	code = curl_easy_setopt(fetch->curl_handle, CURLOPT_HTTPHEADER, fetch->headers);
	assert(code == CURLE_OK);

	/* use proxy if options dictate this */
	if (OPTIONS.http)
	{
	code = curl_easy_setopt(fetch->curl_handle, CURLOPT_PROXY, OPTIONS.http_proxy);
	assert(code == CURLE_OK);
	code = curl_easy_setopt(fetch->curl_handle, CURLOPT_PROXYPORT, (long)OPTIONS.http_port);
	assert(code == CURLE_OK);
	}


	/* add to the global curl multi handle */
	codem = curl_multi_add_handle(curl_multi, fetch->curl_handle);
	assert(codem == CURLM_OK || codem == CURLM_CALL_MULTI_PERFORM);

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

	/* remove from list of fetches */
	if (f->prev == 0)
		fetch_list = f->next;
	else
		f->prev->next = f->next;
	if (f->next != 0)
		f->next->prev = f->prev;

	/* remove from curl multi handle */
	codem = curl_multi_remove_handle(curl_multi, f->curl_handle);
	assert(codem == CURLM_OK);

	if (f->queue != 0) {
		/* start a queued fetch for this host, reusing the handle for this host */
		struct fetch *fetch = f->queue;
		CURLcode code;
		CURLMcode codem;

		LOG(("starting queued %p '%s'", fetch, fetch->url));

		fetch->prev = 0;
		fetch->next = fetch_list;
		if (fetch_list != 0)
			fetch_list->prev = fetch;
		fetch_list = fetch;

		fetch->curl_handle = f->curl_handle;
		code = curl_easy_setopt(fetch->curl_handle, CURLOPT_URL, fetch->url);
		assert(code == CURLE_OK);
		code = curl_easy_setopt(fetch->curl_handle, CURLOPT_PRIVATE, fetch);
		assert(code == CURLE_OK);
		code = curl_easy_setopt(fetch->curl_handle, CURLOPT_ERRORBUFFER, fetch->error_buffer);
		assert(code == CURLE_OK);
		code = curl_easy_setopt(fetch->curl_handle, CURLOPT_WRITEDATA, fetch);
		assert(code == CURLE_OK);
		code = curl_easy_setopt(fetch->curl_handle, CURLOPT_WRITEHEADER, fetch);
		assert(code == CURLE_OK);
		/* TODO: remove referer header if fetch->referer == 0 */
		if (fetch->referer != 0) {
			code = curl_easy_setopt(fetch->curl_handle, CURLOPT_REFERER, fetch->referer);
			assert(code == CURLE_OK);
		}

		/* add to the global curl multi handle */
		codem = curl_multi_add_handle(curl_multi, fetch->curl_handle);
		assert(codem == CURLM_OK || codem == CURLM_CALL_MULTI_PERFORM);

	} else {
		curl_easy_cleanup(f->curl_handle);
		curl_slist_free_all(f->headers);
	}

	xfree(f->url);
	free(f->host);
	free(f->referer);
	free(f->location);
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
	int running, queue, finished;
	CURLMsg * curl_msg;
	struct fetch *f;
	void *p;
	void (*callback)(fetch_msg msg, void *p, char *data, unsigned long size);

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
				finished = 0;
				callback = f->callback;
				p = f->p;
				if (curl_msg->data.result == CURLE_OK) {
					/* fetch completed normally */
					if (!f->had_headers && fetch_process_headers(f))
						; /* redirect with no body or similar */
					else
						finished = 1;
				} else if (curl_msg->data.result != CURLE_WRITE_ERROR) {
					/* CURLE_WRITE_ERROR occurs when fetch_curl_data
					 * returns 0, which we use to abort intentionally */
					callback(FETCH_ERROR, f->p, f->error_buffer, 0);
				}

				/* clean up fetch */
				fetch_abort(f);

				/* postponed until after abort so that queue fetches are started */
				if (finished)
					callback(FETCH_FINISHED, p, 0, 0);

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

	if (!f->had_headers && fetch_process_headers(f))
		return 0;

	/* send data to the caller */
	LOG(("FETCH_DATA"));
	f->callback(FETCH_DATA, f->p, data, size * nmemb);

	f->in_callback = 0;
	return size * nmemb;
}


/**
 * fetch_curl_header -- callback function for headers
 */

size_t fetch_curl_header(char * data, size_t size, size_t nmemb, struct fetch *f)
{
	int i;
	size *= nmemb;
	if (12 < size && strncasecmp(data, "Location:", 9) == 0) {
		/* extract Location header */
		f->location = xcalloc(size, 1);
		for (i = 9; data[i] == ' ' || data[i] == '\t'; i++)
			;
		strncpy(f->location, data + i, size - i);
		for (i = size - i - 1; f->location[i] == ' ' ||
				f->location[i] == '\t' ||
				f->location[i] == '\r' ||
				f->location[i] == '\n'; i--)
			f->location[i] = '\0';
	}
	return size;
}


/**
 * Find the status code and content type and inform the caller.
 */

int fetch_process_headers(struct fetch *f)
{
	long http_code;
	const char *type;
	CURLcode code;

	f->had_headers = 1;

	code = curl_easy_getinfo(f->curl_handle, CURLINFO_HTTP_CODE, &http_code);
	assert(code == CURLE_OK); 
	LOG(("HTTP status code %li", http_code));

	if (300 <= http_code && http_code < 400 && f->location != 0) {
		/* redirect */
		LOG(("FETCH_REDIRECT, '%s'", f->location));
		f->callback(FETCH_REDIRECT, f->p, f->location, 0);
		f->in_callback = 0;
		return 1;
	}

	code = curl_easy_getinfo(f->curl_handle, CURLINFO_CONTENT_TYPE, &type);
	assert(code == CURLE_OK);

	if (type == 0) {
		type = "text/html";
		if (strncmp(f->url, "file:///", 8) == 0) {
			char *url_path;
			url_path = curl_unescape(f->url + 8, (int) strlen(f->url) - 8);
			type = fetch_filetype(url_path);
			free(url_path);
		}
	}

	LOG(("FETCH_TYPE, '%s'", type));
	f->callback(FETCH_TYPE, f->p, type, 0);
	if (f->aborting) {
		f->in_callback = 0;
		return 1;
	}

	return 0;
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


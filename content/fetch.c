/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

/** \file
 * Fetching of data from a URL (implementation).
 *
 * This implementation uses libcurl's 'multi' interface.
 *
 * Active fetches are held in the linked list fetch_list. There may be at most
 * one fetch in progress from each host. Any further fetches are queued until
 * the previous one ends.
 *
 * Invariant: only the fetch at the head of each queue is in progress, ie.
 *        queue_prev == 0  <=>  curl_handle != 0
 *   and  queue_prev != 0  <=>  curl_handle == 0.
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#ifdef riscos
#include <unixlib/local.h>
#endif
#include "curl/curl.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/fetch.h"
#ifdef riscos
#include "netsurf/desktop/gui.h"
#endif
#include "netsurf/desktop/options.h"
#ifdef WITH_AUTH
#include "netsurf/desktop/401login.h"
#endif
#ifdef WITH_POST
#include "netsurf/render/form.h"
#endif
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"


bool fetch_active;	/**< Fetches in progress, please call fetch_poll(). */

/** Information for a single fetch. */
struct fetch {
	CURL * curl_handle;	/**< cURL handle if being fetched, or 0. */
	void (*callback)(fetch_msg msg, void *p, char *data, unsigned long size);
				/**< Callback function. */
	bool had_headers;	/**< Headers have been processed. */
	int locked;		/**< Lock count. */
	bool aborting;		/**< Abort requested in callback. */
	bool stopped;		/**< Download stopped on purpose. */
	bool only_2xx;		/**< Only HTTP 2xx responses acceptable. */
	bool cookies;		/**< Send & accept cookies. */
	char *url;		/**< URL. */
	char *referer;		/**< URL for Referer header. */
	void *p;		/**< Private data for callback. */
	struct curl_slist *headers;	/**< List of request headers. */
	char *host;		/**< Host part of URL. */
	char *location;		/**< Response Location header, or 0. */
	unsigned long content_length;	/**< Response Content-Length, or 0. */
	char *realm;            /**< HTTP Auth Realm */
	char *post_urlenc;	/**< Url encoded POST string, or 0. */
	struct curl_httppost *post_multipart;	/**< Multipart post data, or 0. */
	struct fetch *queue_prev;	/**< Previous fetch for this host. */
	struct fetch *queue_next;	/**< Next fetch for this host. */
	struct fetch *prev;	/**< Previous active fetch in ::fetch_list. */
	struct fetch *next;	/**< Next active fetch in ::fetch_list. */
};

static const char * const user_agent = "NetSurf";
static CURLM *curl_multi;		/**< Global cURL multi handle. */
/** Curl handle with default options set; not used for transfers. */
static CURL *fetch_blank_curl;
static struct fetch *fetch_list = 0;	/**< List of active fetches. */
static char fetch_error_buffer[CURL_ERROR_SIZE]; /**< Error buffer for cURL. */

static CURLcode fetch_set_options(struct fetch *f);
static void fetch_free(struct fetch *f);
static void fetch_done(CURL *curl_handle, CURLcode result);
static size_t fetch_curl_data(void * data, size_t size, size_t nmemb, struct fetch *f);
static size_t fetch_curl_header(char * data, size_t size, size_t nmemb, struct fetch *f);
static bool fetch_process_headers(struct fetch *f);
#ifdef WITH_POST
static struct curl_httppost *fetch_post_convert(struct form_successful_control *control);
#endif

#ifdef riscos
static char * ca_bundle;	/**< SSL certificate bundle filename. */
extern const char * const NETSURF_DIR;
#endif


/**
 * Initialise the fetcher.
 *
 * Must be called once before any other function.
 */

void fetch_init(void)
{
	CURLcode code;

	code = curl_global_init(CURL_GLOBAL_ALL);
	if (code != CURLE_OK)
		die("Failed to initialise the fetch module "
				"(curl_global_init failed).");

	curl_multi = curl_multi_init();
	if (!curl_multi)
		die("Failed to initialise the fetch module "
				"(curl_multi_init failed).");

#ifdef riscos
	ca_bundle = malloc(strlen(NETSURF_DIR) + 40);
	if (!ca_bundle)
		die("NoMemory");
	sprintf(ca_bundle, "%s.Resources.ca-bundle", NETSURF_DIR);
#endif

	/* Create a curl easy handle with the options that are common to all
	   fetches. */
	fetch_blank_curl = curl_easy_init();
	if (!fetch_blank_curl)
		die("Failed to initialise the fetch module "
				"(curl_easy_init failed).");

#define SETOPT(option, value) \
	code = curl_easy_setopt(fetch_blank_curl, option, value);	\
	if (code != CURLE_OK)						\
		goto curl_easy_setopt_failed;

	SETOPT(CURLOPT_VERBOSE, 1);
	SETOPT(CURLOPT_ERRORBUFFER, fetch_error_buffer);
	SETOPT(CURLOPT_WRITEFUNCTION, fetch_curl_data);
	SETOPT(CURLOPT_HEADERFUNCTION, fetch_curl_header);
	SETOPT(CURLOPT_USERAGENT, user_agent);
	SETOPT(CURLOPT_ENCODING, "");
	SETOPT(CURLOPT_LOW_SPEED_LIMIT, 1L);
	SETOPT(CURLOPT_LOW_SPEED_TIME, 60L);
	SETOPT(CURLOPT_NOSIGNAL, 1L);
	SETOPT(CURLOPT_CONNECTTIMEOUT, 60L);
#ifdef riscos
	SETOPT(CURLOPT_CAINFO, ca_bundle);
#endif

	return;

curl_easy_setopt_failed:
	die("Failed to initialise the fetch module "
			"(curl_easy_setopt failed).");
}


/**
 * Clean up for quit.
 *
 * Must be called before exiting.
 */

void fetch_quit(void)
{
	CURLMcode codem;

	curl_easy_cleanup(fetch_blank_curl);

	codem = curl_multi_cleanup(curl_multi);
	if (codem != CURLM_OK)
		LOG(("curl_multi_cleanup failed: ignoring"));

	curl_global_cleanup();
}


/**
 * Start fetching data for the given URL.
 *
 * The function returns immediately. The fetch may be queued for later
 * processing.
 *
 * A pointer to an opaque struct fetch is returned, which can be passed to
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

struct fetch * fetch_start(char *url, char *referer,
		void (*callback)(fetch_msg msg, void *p, char *data,
				unsigned long size),
		void *p, bool only_2xx, char *post_urlenc,
		struct form_successful_control *post_multipart, bool cookies)
{
	char *host;
	struct fetch *fetch;
	struct fetch *host_fetch;
	CURLcode code;
	CURLMcode codem;
	struct curl_slist *slist;

	fetch = malloc(sizeof (*fetch));
	if (!fetch)
		return 0;

	host = url_host(url);

	LOG(("fetch %p, url '%s'", fetch, url));

	/* construct a new fetch structure */
	fetch->curl_handle = 0;
	fetch->callback = callback;
	fetch->had_headers = false;
	fetch->locked = 0;
	fetch->aborting = false;
	fetch->stopped = false;
	fetch->only_2xx = only_2xx;
	fetch->cookies = cookies;
	fetch->url = strdup(url);
	fetch->referer = 0;
	if (referer)
		fetch->referer = strdup(referer);
	fetch->p = p;
	fetch->headers = 0;
	fetch->host = host;
	fetch->location = 0;
	fetch->content_length = 0;
	fetch->realm = 0;
	fetch->post_urlenc = 0;
	fetch->post_multipart = 0;
	if (post_urlenc)
		fetch->post_urlenc = strdup(post_urlenc);
	else if (post_multipart)
		fetch->post_multipart = fetch_post_convert(post_multipart);
	fetch->queue_prev = 0;
	fetch->queue_next = 0;
	fetch->prev = 0;
	fetch->next = 0;

	if (!fetch->url || (referer && !fetch->referer) ||
			(post_urlenc && !fetch->post_urlenc) ||
			(post_multipart && !fetch->post_multipart))
		goto failed;

#define APPEND(list, value) \
	slist = curl_slist_append(list, value);		\
	if (!slist)					\
		goto failed;				\
	list = slist;

	/* remove curl default headers */
	APPEND(fetch->headers, "Accept:");
	APPEND(fetch->headers, "Pragma:");
	if (option_accept_language) {
		char s[80];
		snprintf(s, sizeof s, "Accept-Language: %s, *;q=0.1",
				option_accept_language);
		s[sizeof s - 1] = 0;
		APPEND(fetch->headers, s);
	}

	/* look for a fetch from the same host */
	if (host) {
		for (host_fetch = fetch_list;
				host_fetch && (host_fetch->host == 0 ||
				strcasecmp(host_fetch->host, host) != 0);
				host_fetch = host_fetch->next)
			;
		if (host_fetch) {
			/* fetch from this host in progress:
			   queue the new fetch */
			LOG(("queueing"));
			fetch->curl_handle = 0;
			/* queue at end */
			for (; host_fetch->queue_next;
					host_fetch = host_fetch->queue_next)
				;
			fetch->queue_prev = host_fetch;
			host_fetch->queue_next = fetch;
			return fetch;
		}
	}

	fetch->next = fetch_list;
	if (fetch_list != 0)
		fetch_list->prev = fetch;
	fetch_list = fetch;
	fetch_active = true;

	/* create the curl easy handle */
	fetch->curl_handle = curl_easy_duphandle(fetch_blank_curl);
	if (!fetch->curl_handle)
		goto failed;

	code = fetch_set_options(fetch);
	if (code != CURLE_OK)
		goto failed;

	/* add to the global curl multi handle */
	codem = curl_multi_add_handle(curl_multi, fetch->curl_handle);
	assert(codem == CURLM_OK || codem == CURLM_CALL_MULTI_PERFORM);

	return fetch;

failed:
	free(host);
	free(fetch->url);
	free(fetch->referer);
	free(fetch->post_urlenc);
	if (fetch->post_multipart)
		curl_formfree(fetch->post_multipart);
	curl_slist_free_all(fetch->headers);
	free(fetch);
	return 0;
}


/**
 * Set options specific for a fetch.
 */

CURLcode fetch_set_options(struct fetch *f)
{
	CURLcode code;
	struct login *li;

#undef SETOPT
#define SETOPT(option, value) \
	code = curl_easy_setopt(f->curl_handle, option, value);	\
	if (code != CURLE_OK)					\
		return code;

	SETOPT(CURLOPT_URL, f->url);
	SETOPT(CURLOPT_PRIVATE, f);
	SETOPT(CURLOPT_WRITEDATA, f);
	SETOPT(CURLOPT_WRITEHEADER, f);
	SETOPT(CURLOPT_REFERER, f->referer);
	SETOPT(CURLOPT_HTTPHEADER, f->headers);
	if (f->post_urlenc) {
		SETOPT(CURLOPT_POSTFIELDS, f->post_urlenc);
	} else if (f->post_multipart) {
		SETOPT(CURLOPT_HTTPPOST, f->post_multipart);
	} else {
		SETOPT(CURLOPT_HTTPGET, 1L);
	}
	if (f->cookies) {
		SETOPT(CURLOPT_COOKIEFILE, messages_get("cookiefile"));
		SETOPT(CURLOPT_COOKIEJAR, messages_get("cookiejar"));
	} else {
		SETOPT(CURLOPT_COOKIEFILE, 0);
		SETOPT(CURLOPT_COOKIEJAR, 0);
	}
	if ((li = login_list_get(f->url))) {
		SETOPT(CURLOPT_HTTPAUTH, CURLAUTH_ANY);
		SETOPT(CURLOPT_USERPWD, li->logindetails);
	} else {
		SETOPT(CURLOPT_USERPWD, 0);
	}
	if (option_http_proxy && option_http_proxy_host) {
		SETOPT(CURLOPT_PROXY, option_http_proxy_host);
		SETOPT(CURLOPT_PROXYPORT, (long) option_http_proxy_port);
	}

	return CURLE_OK;
}


/**
 * Stop a fetch.
 */

void fetch_abort(struct fetch *f)
{
	CURLcode code;
	CURLMcode codem;
	struct fetch *fetch;
	struct fetch *next_fetch;

	assert(f != 0);
	LOG(("fetch %p, url '%s'", f, f->url));

	if (f->locked) {
		LOG(("locked: will abort later"));
		f->aborting = true;
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
	if (f->curl_handle) {
		codem = curl_multi_remove_handle(curl_multi, f->curl_handle);
		assert(codem == CURLM_OK);
	}

	if (f->curl_handle && f->queue_next) {
		/* start a queued fetch for this host, reusing the handle */
		fetch = f->queue_next;

		LOG(("starting queued %p '%s'", fetch, fetch->url));

		fetch->curl_handle = f->curl_handle;
		f->curl_handle = 0;
		code = fetch_set_options(fetch);
		if (code == CURLE_OK)
			/* add to the global curl multi handle */
			codem = curl_multi_add_handle(curl_multi,
					fetch->curl_handle);

		if (code == CURLE_OK && (codem == CURLM_OK ||
				codem == CURLM_CALL_MULTI_PERFORM)) {
			/* add to list of fetches */
			fetch->prev = 0;
			fetch->next = fetch_list;
			if (fetch_list != 0)
				fetch_list->prev = fetch;
			fetch_list = fetch;
			fetch->queue_prev = 0;
		} else {
			/* destroy all queued fetches for this host */
			do {
				fetch->locked++;
				fetch->callback(FETCH_ERROR, fetch->p,
						messages_get("FetchError"), 0);
				fetch->locked--;
				next_fetch = fetch->queue_next;
				fetch_free(fetch);
				fetch = next_fetch;
			} while (fetch);
		}

	} else {
		if (f->queue_prev)
			f->queue_prev->queue_next = f->queue_next;
		if (f->queue_next)
			f->queue_next->queue_prev = f->queue_prev;
	}

	fetch_free(f);
}


/**
 * Free a fetch structure and associated resources.
 */

void fetch_free(struct fetch *f)
{
	if (f->curl_handle)
		curl_easy_cleanup(f->curl_handle);
	free(f->url);
	free(f->host);
	free(f->referer);
	free(f->location);
	free(f->realm);
	if (f->headers)
		curl_slist_free_all(f->headers);
	free(f->post_urlenc);
	if (f->post_multipart)
		curl_formfree(f->post_multipart);
	free(f);
}


/**
 * Do some work on current fetches.
 *
 * Must be called regularly to make progress on fetches.
 */

void fetch_poll(void)
{
	int running, queue;
	CURLMcode codem;
	CURLMsg *curl_msg;

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
				fetch_done(curl_msg->easy_handle,
						curl_msg->data.result);
				break;
			default:
				break;
		}
		curl_msg = curl_multi_info_read(curl_multi, &queue);
	}

	if (!fetch_list)
		fetch_active = false;
}


/**
 * Handle a completed fetch (CURLMSG_DONE from curl_multi_info_read()).
 *
 * \param  curl_handle  curl easy handle of fetch
 */

void fetch_done(CURL *curl_handle, CURLcode result)
{
	bool finished = false;
	bool error = false;
	struct fetch *f;
	void *p;
	void (*callback)(fetch_msg msg, void *p, char *data,
			unsigned long size);
	CURLcode code;

	/* find the structure associated with this fetch */
	code = curl_easy_getinfo(curl_handle, CURLINFO_PRIVATE, &f);
	assert(code == CURLE_OK);

	callback = f->callback;
	p = f->p;

	if (result == CURLE_OK) {
		/* fetch completed normally */
		if (!f->had_headers && fetch_process_headers(f))
			; /* redirect with no body or similar */
		else
			finished = true;
	} else if (result == CURLE_WRITE_ERROR && f->stopped)
		/* CURLE_WRITE_ERROR occurs when fetch_curl_data
		 * returns 0, which we use to abort intentionally */
		;
	else
		error = true;

	/* clean up fetch */
	fetch_abort(f);

	/* postponed until after abort so that queue fetches are started */
	if (finished)
		callback(FETCH_FINISHED, p, 0, 0);
	else if (error)
		callback(FETCH_ERROR, p, fetch_error_buffer, 0);
}


/**
 * Callback function for cURL.
 */

size_t fetch_curl_data(void * data, size_t size, size_t nmemb, struct fetch *f)
{
	f->locked++;

	LOG(("fetch %p, size %u", f, size * nmemb));

	if (!f->had_headers && fetch_process_headers(f)) {
		f->locked--;
		f->stopped = true;
		return 0;
	}

	/* send data to the caller */
	LOG(("FETCH_DATA"));
	f->callback(FETCH_DATA, f->p, data, size * nmemb);

	f->locked--;
	return size * nmemb;
}


/**
 * Callback function for headers.
 */

size_t fetch_curl_header(char * data, size_t size, size_t nmemb, struct fetch *f)
{
	int i;
	size *= nmemb;
	if (12 < size && strncasecmp(data, "Location:", 9) == 0) {
		/* extract Location header */
		free(f->location);
		f->location = malloc(size);
		if (!f->location) {
			LOG(("malloc failed"));
			return size;
		}
		for (i = 9; data[i] == ' ' || data[i] == '\t'; i++)
			;
		strncpy(f->location, data + i, size - i);
		for (i = size - i - 1; f->location[i] == ' ' ||
				f->location[i] == '\t' ||
				f->location[i] == '\r' ||
				f->location[i] == '\n'; i--)
			f->location[i] = '\0';
	} else if (15 < size && strncasecmp(data, "Content-Length:", 15) == 0) {
		/* extract Content-Length header */
		for (i = 15; data[i] == ' ' || data[i] == '\t'; i++)
			;
		if ('0' <= data[i] && data[i] <= '9')
			f->content_length = atol(data + i);
#ifdef WITH_AUTH
	} else if (16 < size && strncasecmp(data, "WWW-Authenticate",16) == 0) {
		/* extract Realm from WWW-Authenticate header */
		free(f->realm);
		f->realm = malloc(size);
		if (!f->realm) {
			LOG(("malloc failed"));
			return size;
		}
		/** \todo  this code looks dangerous */
	        for (i=16;(unsigned int)i!=strlen(data);i++)
	               if(data[i]=='=')break;
	        strncpy(f->realm, data+i+2, size-i-5);
#endif
	}
	return size;
}


/**
 * Find the status code and content type and inform the caller.
 *
 * Return true if the fetch is being aborted.
 */

bool fetch_process_headers(struct fetch *f)
{
	long http_code;
	const char *type;
	CURLcode code;

	f->locked++;
	f->had_headers = true;

	code = curl_easy_getinfo(f->curl_handle, CURLINFO_HTTP_CODE, &http_code);
	assert(code == CURLE_OK);
	LOG(("HTTP status code %li", http_code));

	/* handle HTTP redirects (3xx response codes) */
	if (300 <= http_code && http_code < 400 && f->location != 0) {
		LOG(("FETCH_REDIRECT, '%s'", f->location));
		f->callback(FETCH_REDIRECT, f->p, f->location, 0);
		f->locked--;
		return true;
	}

        /* handle HTTP 401 (Authentication errors) */
#ifdef WITH_AUTH
	if (http_code == 401) {
		f->callback(FETCH_AUTH, f->p, f->realm,0);
		f->locked--;
		return true;
	}
#endif

	/* handle HTTP errors (non 2xx response codes) */
	if (f->only_2xx && strncmp(f->url, "http", 4) == 0 &&
			(http_code < 200 || 299 < http_code)) {
		f->callback(FETCH_ERROR, f->p, messages_get("Not2xx"), 0);
		f->locked--;
		return true;
	}

	/* find MIME type from headers or filetype for local files */
	code = curl_easy_getinfo(f->curl_handle, CURLINFO_CONTENT_TYPE, &type);
	assert(code == CURLE_OK);

	if (type == 0) {
		type = "text/html";
		if (strncmp(f->url, "file:///", 8) == 0) {
			char *url_path;
			url_path = curl_unescape(f->url + 8, (int) strlen(f->url) - 8);
			type = fetch_filetype(url_path);
			curl_free(url_path);
		} else if (strncmp(f->url, "file:/", 6) == 0) {
			char *url_path;
			url_path = curl_unescape(f->url + 6, (int) strlen(f->url) - 6);
			type = fetch_filetype(url_path);
			curl_free(url_path);
		}
	}

	LOG(("FETCH_TYPE, '%s'", type));
	f->callback(FETCH_TYPE, f->p, type, f->content_length);
	f->locked--;
	if (f->aborting)
		return true;

	return false;
}


/**
 * Convert a list of struct ::form_successful_control to a list of
 * struct curl_httppost for libcurl.
 */
#ifdef WITH_POST
struct curl_httppost *fetch_post_convert(struct form_successful_control *control)
{
	struct curl_httppost *post = 0, *last = 0;
	char *mimetype = 0;
	char *leafname = 0, *temp = 0;

	for (; control; control = control->next) {
		if (control->file) {
			mimetype = fetch_mimetype(control->value);
#ifdef riscos
			temp = strrchr(control->value, '.');
			if (!temp)
				temp = control->value; /* already leafname */
			else
				temp += 1;
			leafname = malloc(strlen(temp));
			if (!leafname) {
				LOG(("malloc failed"));
				free(mimetype);
				continue;
			}
			__unixify_std(temp, leafname, strlen(temp), 0xfff);
#else
			leafname = strrchr(control->value, '/') ;
			if (!leafname)
				leafname = control->value;
			else
				leafname += 1;
#endif
			curl_formadd(&post, &last,
					CURLFORM_COPYNAME, control->name,
					CURLFORM_FILE, leafname,
					CURLFORM_CONTENTTYPE,
					(mimetype != 0 ? mimetype : "text/plain"),
					CURLFORM_END);
#ifdef riscos
			free(leafname);
#endif
			free(mimetype);
		}
		else {
			curl_formadd(&post, &last,
					CURLFORM_COPYNAME, control->name,
					CURLFORM_COPYCONTENTS, control->value,
					CURLFORM_END);
		}
	}

	return post;
}
#endif

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


/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

/** \file
 * Fetching of data from a URL (implementation).
 *
 * This implementation uses libcurl's 'multi' interface.
 *
 * Active fetches are held in the circular linked list ::fetch_ring. There may
 * be at most ::option_max_fetchers_per_host active requests per Host: header.
 * There may be at most ::option_max_fetchers active requests overall. Inactive
 * fetchers are stored in the ::queue_ring and there are at most
 * ::option_max_cached_fetch_handles kept in there at any one time.
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/stat.h>
#ifdef riscos
#include <unixlib/local.h>
#endif
#include "curl/curl.h"
#include "netsurf/utils/config.h"
#ifdef WITH_SSL
#include "openssl/ssl.h"
#endif
#include "netsurf/content/fetch.h"
#include "netsurf/content/urldb.h"
#include "netsurf/desktop/options.h"
#include "netsurf/render/form.h"
#define NDEBUG
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"


bool fetch_active;	/**< Fetches in progress, please call fetch_poll(). */

#ifdef WITH_SSL
/** SSL certificate info */
struct cert_info {
	X509 *cert;		/**< Pointer to certificate */
	long err;		/**< OpenSSL error code */
};
#endif

/** Information for a single fetch. */
struct fetch {
	CURL * curl_handle;	/**< cURL handle if being fetched, or 0. */
	void (*callback)(fetch_msg msg, void *p, const void *data,
			unsigned long size);
				/**< Callback function. */
	bool had_headers;	/**< Headers have been processed. */
	bool abort;		/**< Abort requested. */
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
	long http_code;		/**< HTTP response code, or 0. */
	char *cookie_string;	/**< Cookie string for this fetch */
	char *realm;		/**< HTTP Auth Realm */
	char *post_urlenc;	/**< Url encoded POST string, or 0. */
	struct curl_httppost *post_multipart;	/**< Multipart post data, or 0. */
	struct cache_data cachedata;	/**< Cache control data */
	time_t last_modified;		/**< If-Modified-Since time */
	time_t file_etag;		/**< ETag for local objects */
#ifdef WITH_SSL
#define MAX_CERTS 10
	struct cert_info cert_data[MAX_CERTS];	/**< HTTPS certificate data */
#endif
	struct fetch *r_prev;	/**< Previous active fetch in ::fetch_ring. */
	struct fetch *r_next;	/**< Next active fetch in ::fetch_ring. */
};

struct cache_handle {
	CURL *handle; /**< The cached cURL handle */
	char *host;	   /**< The host for which this handle is cached */

	struct cache_handle *r_prev; /**< Previous cached handle in ring. */
	struct cache_handle *r_next; /**< Next cached handle in ring. */
};

static const char * const user_agent = "NetSurf";
CURLM *fetch_curl_multi;		/**< Global cURL multi handle. */
/** Curl handle with default options set; not used for transfers. */
static CURL *fetch_blank_curl;
static struct fetch *fetch_ring = 0;	/**< Ring of active fetches. */
static struct fetch *queue_ring = 0;	/**< Ring of queued fetches */
static struct cache_handle *handle_ring = 0; /**< Ring of cached handles */

static char fetch_error_buffer[CURL_ERROR_SIZE]; /**< Error buffer for cURL. */
static char fetch_progress_buffer[256]; /**< Progress buffer for cURL */
static char fetch_proxy_userpwd[100];	/**< Proxy authentication details. */

static CURLcode fetch_set_options(struct fetch *f);
#ifdef WITH_SSL
static CURLcode fetch_sslctxfun(CURL *curl_handle, SSL_CTX *sslctx, void *p);
#endif
static void fetch_free(struct fetch *f);
static void fetch_stop(struct fetch *f);
static void fetch_done(CURL *curl_handle, CURLcode result);
static int fetch_curl_progress(void *clientp, double dltotal, double dlnow,
		double ultotal, double ulnow);
static size_t fetch_curl_data(void *data, size_t size, size_t nmemb,
		struct fetch *f);
static size_t fetch_curl_header(char *data, size_t size, size_t nmemb,
		struct fetch *f);
static bool fetch_process_headers(struct fetch *f);
static struct curl_httppost *fetch_post_convert(
		struct form_successful_control *control);
#ifdef WITH_SSL
static int fetch_verify_callback(int preverify_ok, X509_STORE_CTX *x509_ctx);
static int fetch_cert_verify_callback(X509_STORE_CTX *x509_ctx, void *parm);
#endif

/** Insert the given item into the specified ring.
 * Assumes that the element is zeroed as appropriate.
 */
#define RING_INSERT(ring,element) \
	LOG(("RING_INSERT(%s, %p(%s))", #ring, element, element->host)); \
	if (ring) { \
		element->r_next = ring; \
		element->r_prev = ring->r_prev; \
		ring->r_prev = element; \
		element->r_prev->r_next = element; \
	} else \
		ring = element->r_prev = element->r_next = element

/** Remove the given element from the specified ring.
 * Will zero the element as needed
 */
#define RING_REMOVE(ring, element) \
	LOG(("RING_REMOVE(%s, %p(%s)", #ring, element, element->host)); \
	if (element->r_next != element ) { \
		/* Not the only thing in the ring */ \
		element->r_next->r_prev = element->r_prev; \
		element->r_prev->r_next = element->r_next; \
		if (ring == element) ring = element->r_next; \
	} else { \
		/* Only thing in the ring */ \
		ring = 0; \
	} \
	element->r_next = element->r_prev = 0

/** Find the element (by hostname) in the given ring, leave it in the
 * provided element variable
 */
#define RING_FINDBYHOST(ring, element, hostname) \
	LOG(("RING_FINDBYHOST(%s, %s)", #ring, hostname)); \
	if (ring) { \
		element = ring; \
		do { \
			if (strcasecmp(element->host, hostname) == 0) \
				break; \
			element = element->r_next; \
		} while (element != ring); \
		element = 0; \
	} else element = 0

/** Measure the size of a ring and put it in the supplied variable */
#define RING_GETSIZE(ringtype, ring, sizevar) \
	LOG(("RING_GETSIZE(%s)", #ring)); \
	if (ring) { \
		ringtype *p = ring; \
		sizevar = 0; \
		do { \
			sizevar++; \
			p = p->r_next; \
		} while (p != ring); \
	} else sizevar = 0

/** Count the number of elements in the ring which match the provided hostname */
#define RING_COUNTBYHOST(ringtype, ring, sizevar, hostname) \
	LOG(("RING_COUNTBYHOST(%s, %s)", #ring, hostname)); \
	if (ring) { \
		ringtype *p = ring; \
		sizevar = 0; \
		do { \
			if (strcasecmp(p->host, hostname) == 0) \
				sizevar++; \
			p = p->r_next; \
		} while (p != ring); \
	} else sizevar = 0

static void fetch_cache_handle(CURL *handle, char *hostname);
static void fetch_dispatch_jobs(void);

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

	fetch_curl_multi = curl_multi_init();
	if (!fetch_curl_multi)
		die("Failed to initialise the fetch module "
				"(curl_multi_init failed).");

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
	SETOPT(CURLOPT_PROGRESSFUNCTION, fetch_curl_progress);
	SETOPT(CURLOPT_NOPROGRESS, 0);
	SETOPT(CURLOPT_USERAGENT, user_agent);
	SETOPT(CURLOPT_ENCODING, "gzip");
	SETOPT(CURLOPT_LOW_SPEED_LIMIT, 1L);
	SETOPT(CURLOPT_LOW_SPEED_TIME, 180L);
	SETOPT(CURLOPT_NOSIGNAL, 1L);
	SETOPT(CURLOPT_CONNECTTIMEOUT, 30L);

	if (option_ca_bundle)
		SETOPT(CURLOPT_CAINFO, option_ca_bundle);

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

	codem = curl_multi_cleanup(fetch_curl_multi);
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
		void (*callback)(fetch_msg msg, void *p, const void *data,
				unsigned long size),
		void *p, bool only_2xx, char *post_urlenc,
		struct form_successful_control *post_multipart, bool cookies,
		char *headers[])
{
	char *host;
	struct fetch *fetch;
	struct curl_slist *slist;
	url_func_result res;
	char *ref1 = 0, *ref2 = 0;
	int i;

	fetch = malloc(sizeof (*fetch));
	if (!fetch)
		return 0;

	res = url_host(url, &host);
	if (res != URL_FUNC_OK) {
		/* we only fail memory exhaustion */
		if (res == URL_FUNC_NOMEM)
			goto failed;

		host = strdup("");
		if (!host)
			goto failed;
	}

	res = url_scheme(url, &ref1);
	if (res != URL_FUNC_OK) {
		/* we only fail memory exhaustion */
		if (res == URL_FUNC_NOMEM)
			goto failed;
		ref1 = NULL;
	}

	if (referer) {
		res = url_scheme(referer, &ref2);
		if (res != URL_FUNC_OK) {
			/* we only fail memory exhaustion */
			if (res == URL_FUNC_NOMEM)
				goto failed;
			ref2 = NULL;
		}
	}

	LOG(("fetch %p, url '%s'", fetch, url));

	/* construct a new fetch structure */
	fetch->curl_handle = 0;
	fetch->callback = callback;
	fetch->had_headers = false;
	fetch->abort = false;
	fetch->stopped = false;
	fetch->only_2xx = only_2xx;
	fetch->cookies = cookies;
	fetch->url = strdup(url);
	fetch->referer = 0;
	/* only send the referer if the schemes match */
	if (referer) {
		if (ref1 && ref2 && strcasecmp(ref1, ref2) == 0)
			fetch->referer = strdup(referer);
	}
	fetch->p = p;
	fetch->headers = 0;
	fetch->host = host;
	fetch->location = 0;
	fetch->content_length = 0;
	fetch->http_code = 0;
	fetch->cookie_string = 0;
	fetch->realm = 0;
	fetch->post_urlenc = 0;
	fetch->post_multipart = 0;
	if (post_urlenc)
		fetch->post_urlenc = strdup(post_urlenc);
	else if (post_multipart)
		fetch->post_multipart = fetch_post_convert(post_multipart);
	fetch->cachedata.req_time = time(0);
	fetch->cachedata.res_time = 0;
	fetch->cachedata.date = 0;
	fetch->cachedata.expires = 0;
	fetch->cachedata.age = INVALID_AGE;
	fetch->cachedata.max_age = INVALID_AGE;
	fetch->cachedata.no_cache = false;
	fetch->cachedata.etag = 0;
	fetch->cachedata.last_modified = 0;
	fetch->last_modified = 0;
	fetch->file_etag = 0;
#ifdef WITH_SSL
	memset(fetch->cert_data, 0, sizeof(fetch->cert_data));
#endif
	fetch->r_prev = 0;
	fetch->r_next = 0;

	if (!fetch->url || (referer &&
			(ref1 && ref2 && strcasecmp(ref1, ref2) == 0) &&
			!fetch->referer) ||
			(post_urlenc && !fetch->post_urlenc) ||
			(post_multipart && !fetch->post_multipart))
		goto failed;

	/* these aren't needed past here */
	if (ref1) {
		free(ref1);
		ref1 = 0;
	}
	if (ref2) {
		free(ref2);
		ref2 = 0;
	}

#define APPEND(list, value) \
	slist = curl_slist_append(list, value);		\
	if (!slist)					\
		goto failed;				\
	list = slist;

	/* remove curl default headers */
	APPEND(fetch->headers, "Accept:");
	APPEND(fetch->headers, "Pragma:");

	/* when doing a POST libcurl sends Expect: 100-continue" by default
	 * which fails with lighttpd, so disable it (see bug 1429054) */
	APPEND(fetch->headers, "Expect:");

	if (option_accept_language) {
		char s[80];
		snprintf(s, sizeof s, "Accept-Language: %s, *;q=0.1",
				option_accept_language);
		s[sizeof s - 1] = 0;
		APPEND(fetch->headers, s);
	}

	if (option_accept_charset) {
		char s[80];
		snprintf(s, sizeof s, "Accept-Charset: %s, *;q=0.1",
				option_accept_charset);
		s[sizeof s - 1] = 0;
		APPEND(fetch->headers, s);
	}

	/* And add any headers specified by the caller */
	for (i = 0; headers[i]; i++) {
		if (strncasecmp(headers[i], "If-Modified-Since:", 18) == 0) {
			char *d = headers[i] + 18;
			for (; *d && (*d == ' ' || *d == '\t'); d++)
				/* do nothing */;
			fetch->last_modified = curl_getdate(d, NULL);
		}
		else if (strncasecmp(headers[i], "If-None-Match:", 14) == 0) {
			char *d = headers[i] + 14;
			for (; *d && (*d == ' ' || *d == '\t' || *d == '"');
					d++)
				/* do nothing */;
			fetch->file_etag = atoi(d);
		}
		APPEND(fetch->headers, headers[i]);
	}

	/* Dump us in the queue and ask the queue to run. */
	RING_INSERT(queue_ring, fetch);
	fetch_dispatch_jobs();
	return fetch;

failed:
	free(host);
	if (ref1)
		free(ref1);
	if (ref2)
		free(ref2);
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
 * Initiate a fetch from the queue.
 *
 * Called with a fetch structure and a CURL handle to be used to fetch the content.
 *
 * This will return whether or not the fetch was successfully initiated.
 */
static bool fetch_initiate_fetch(struct fetch *fetch, CURL *handle)
{
	CURLcode code;
	CURLMcode codem;

	fetch->curl_handle = handle;

	/* Initialise the handle */
	code = fetch_set_options(fetch);
	if (code != CURLE_OK) {
		fetch->curl_handle = 0;
		return false;
	}

	/* add to the global curl multi handle */
	codem = curl_multi_add_handle(fetch_curl_multi, fetch->curl_handle);
	assert(codem == CURLM_OK || codem == CURLM_CALL_MULTI_PERFORM);

	fetch_active = true;
	return true;
}

/**
 * Find a CURL handle to use to dispatch a job
 */
static CURL *fetch_get_handle(char *host)
{
	struct cache_handle *h;
	CURL *ret;
	RING_FINDBYHOST(handle_ring, h, host);
	if (h) {
		ret = h->handle;
		free(h->host);
		RING_REMOVE(handle_ring, h);
		free(h);
	} else {
		ret = curl_easy_duphandle(fetch_blank_curl);
	}
	return ret;
}

/**
 * Dispatch a single job
 */
static bool fetch_dispatch_job(struct fetch *fetch)
{
	RING_REMOVE(queue_ring, fetch);
	if (!fetch_initiate_fetch(fetch, fetch_get_handle(fetch->host))) {
		RING_INSERT(queue_ring, fetch); /* Put it back on the end of the queue */
		return false;
	} else {
		RING_INSERT(fetch_ring, fetch);
		return true;
	}
}

/**
 * Choose and dispatch a single job. Return false if we failed to dispatch anything.
 *
 * We don't check the overall dispatch size here because we're not called unless
 * there is room in the fetch queue for us.
 */
static bool fetch_choose_and_dispatch(void)
{
	struct fetch *queueitem;
	queueitem = queue_ring;
	do {
		/* We can dispatch the selected item if there is room in the
		 * fetch ring
		 */
		int countbyhost;
		RING_COUNTBYHOST(struct fetch, fetch_ring, countbyhost, queueitem->host);
		if (countbyhost < option_max_fetchers_per_host) {
			/* We can dispatch this item in theory */
			return fetch_dispatch_job(queueitem);
		}
		queueitem = queueitem->r_next;
	} while (queueitem != queue_ring);
	return false;
}

/**
 * Dispatch as many jobs as we have room to dispatch.
 */
static void fetch_dispatch_jobs(void)
{
	int all_active, all_queued;

	if (!queue_ring) return; /* Nothing to do, the queue is empty */
	RING_GETSIZE(struct fetch, queue_ring, all_queued);
	RING_GETSIZE(struct fetch, fetch_ring, all_active);
	while( all_queued && all_active < option_max_fetchers ) {
		LOG(("%d queued, %d fetching", all_queued, all_active));
		if (fetch_choose_and_dispatch()) {
			all_queued--;
			all_active++;
		} else {
			/* Either a dispatch failed or we ran out. Just stop */
			break;
		}
	}
}

/**
 * Cache a CURL handle for the provided host (if wanted)
 *
 */
static void fetch_cache_handle(CURL *handle, char *host)
{
	struct cache_handle *h = 0;
	int c;
	RING_FINDBYHOST(handle_ring, h, host);
	if (h) {
		/* Already have a handle cached for this hostname */
		curl_easy_cleanup(handle);
		return;
	}
	/* We do not have a handle cached, first up determine if the cache is full */
	RING_GETSIZE(struct cache_handle, handle_ring, c);
	if (c >= option_max_cached_fetch_handles) {
		/* Cache is full, so, we rotate the ring by one and replace the
		 * oldest handle with this one. We do this without freeing/allocating
		 * memory (except the hostname) and without removing the entry from the
		 * ring and then re-inserting it, in order to be as efficient as we can.
		 */
		h = handle_ring;
		handle_ring = h->r_next;
		curl_easy_cleanup(h->handle);
		h->handle = handle;
		free(h->host);
		h->host = strdup(host);
		return;
	}
	/* The table isn't full yet, so make a shiny new handle to add to the ring */
	h = (struct cache_handle*)malloc(sizeof(struct cache_handle));
	h->handle = handle;
	h->host = strdup(host);
	RING_INSERT(handle_ring, h);
}

/**
 * Set options specific for a fetch.
 */

CURLcode fetch_set_options(struct fetch *f)
{
	CURLcode code;
	const char *auth;

#undef SETOPT
#define SETOPT(option, value) \
	code = curl_easy_setopt(f->curl_handle, option, value);	\
	if (code != CURLE_OK)					\
		return code;

	SETOPT(CURLOPT_URL, f->url);
	SETOPT(CURLOPT_PRIVATE, f);
	SETOPT(CURLOPT_WRITEDATA, f);
	SETOPT(CURLOPT_WRITEHEADER, f);
	SETOPT(CURLOPT_PROGRESSDATA, f);
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
		f->cookie_string = urldb_get_cookie(f->url, f->referer);
		if (f->cookie_string)
			SETOPT(CURLOPT_COOKIE, f->cookie_string);
	}
#ifdef WITH_AUTH
	if ((auth = urldb_get_auth_details(f->url)) != NULL) {
		SETOPT(CURLOPT_HTTPAUTH, CURLAUTH_ANY);
		SETOPT(CURLOPT_USERPWD, auth);
	} else {
#endif
		SETOPT(CURLOPT_USERPWD, 0);
#ifdef WITH_AUTH
	}
#endif
	if (option_http_proxy && option_http_proxy_host) {
		SETOPT(CURLOPT_PROXY, option_http_proxy_host);
		SETOPT(CURLOPT_PROXYPORT, (long) option_http_proxy_port);
		if (option_http_proxy_auth != OPTION_HTTP_PROXY_AUTH_NONE) {
			SETOPT(CURLOPT_PROXYAUTH,
					option_http_proxy_auth ==
					OPTION_HTTP_PROXY_AUTH_BASIC ?
					(long) CURLAUTH_BASIC :
					(long) CURLAUTH_NTLM);
			snprintf(fetch_proxy_userpwd,
					sizeof fetch_proxy_userpwd,
					"%s:%s",
					option_http_proxy_auth_user,
					option_http_proxy_auth_pass);
			SETOPT(CURLOPT_PROXYUSERPWD, fetch_proxy_userpwd);
		}
	}

#ifdef WITH_SSL
	if (urldb_get_cert_permissions(f->url)) {
		/* Disable certificate verification */
		SETOPT(CURLOPT_SSL_VERIFYPEER, 0L);
		SETOPT(CURLOPT_SSL_VERIFYHOST, 0L);
	} else {
		/* do verification */
		SETOPT(CURLOPT_SSL_CTX_FUNCTION, fetch_sslctxfun);
		SETOPT(CURLOPT_SSL_CTX_DATA, f);
	}
#endif

	return CURLE_OK;
}


#ifdef WITH_SSL
/**
 * cURL SSL setup callback
 */

CURLcode fetch_sslctxfun(CURL *curl_handle, SSL_CTX *sslctx, void *parm)
{
	SSL_CTX_set_verify(sslctx, SSL_VERIFY_PEER, fetch_verify_callback);
	SSL_CTX_set_cert_verify_callback(sslctx, fetch_cert_verify_callback,
			parm);
	return CURLE_OK;
}
#endif


/**
 * Abort a fetch.
 */

void fetch_abort(struct fetch *f)
{
	assert(f);
	LOG(("fetch %p, url '%s'", f, f->url));
	if (f->curl_handle) {
		f->abort = true;
	} else {
		RING_REMOVE(queue_ring, f);
		fetch_free(f);
	}
}


/**
 * Clean up the provided fetch object and free it.
 *
 * Will prod the queue afterwards to allow pending requests to be initiated.
 */

void fetch_stop(struct fetch *f)
{
	CURLMcode codem;

	assert(f);
	LOG(("fetch %p, url '%s'", f, f->url));

	if (f->curl_handle) {
		/* remove from curl multi handle */
		codem = curl_multi_remove_handle(fetch_curl_multi,
				f->curl_handle);
		assert(codem == CURLM_OK);
		/* Put this curl handle into the cache if wanted. */
		fetch_cache_handle(f->curl_handle, f->host);
		f->curl_handle = 0;
		/* Remove this from the active set of fetches (if it's still there) */
		RING_REMOVE(fetch_ring, f);
	} else {
		/* Remove this from the queued set of fetches (if it's still there) */
		RING_REMOVE(queue_ring, f);
	}

	fetch_free(f);
	if (!fetch_ring && !queue_ring)
		fetch_active = false;
	else if (queue_ring)
		fetch_dispatch_jobs();
}


/**
 * Free a fetch structure and associated resources.
 */

void fetch_free(struct fetch *f)
{
#ifdef WITH_SSL
	int i;
#endif

	if (f->curl_handle)
		curl_easy_cleanup(f->curl_handle);
	free(f->url);
	free(f->host);
	free(f->referer);
	free(f->location);
	free(f->cookie_string);
	free(f->realm);
	if (f->headers)
		curl_slist_free_all(f->headers);
	free(f->post_urlenc);
	if (f->post_multipart)
		curl_formfree(f->post_multipart);
	free(f->cachedata.etag);

#ifdef WITH_SSL
	for (i = 0; i < MAX_CERTS && f->cert_data[i].cert; i++) {
		f->cert_data[i].cert->references--;
		if (f->cert_data[i].cert->references == 0)
			X509_free(f->cert_data[i].cert);
	}
#endif

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
		codem = curl_multi_perform(fetch_curl_multi, &running);
		assert(codem == CURLM_OK || codem == CURLM_CALL_MULTI_PERFORM);
	} while (codem == CURLM_CALL_MULTI_PERFORM);

	/* process curl results */
	curl_msg = curl_multi_info_read(fetch_curl_multi, &queue);
	while (curl_msg) {
		switch (curl_msg->msg) {
			case CURLMSG_DONE:
				fetch_done(curl_msg->easy_handle,
						curl_msg->data.result);
				break;
			default:
				break;
		}
		curl_msg = curl_multi_info_read(fetch_curl_multi, &queue);
	}
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
#ifdef WITH_SSL
	bool cert = false;
#endif
	bool abort;
	struct fetch *f;
	void *p;
	void (*callback)(fetch_msg msg, void *p, const void *data,
			unsigned long size);
	CURLcode code;
	struct cache_data cachedata;
#ifdef WITH_SSL
	struct cert_info certs[MAX_CERTS];
	memset(certs, 0, sizeof(certs));
#endif

	/* find the structure associated with this fetch */
	code = curl_easy_getinfo(curl_handle, CURLINFO_PRIVATE, &f);
	assert(code == CURLE_OK);

	abort = f->abort;
	callback = f->callback;
	p = f->p;

	if (!abort && result == CURLE_OK) {
		/* fetch completed normally */
		if (f->stopped ||
				(!f->had_headers &&
					fetch_process_headers(f)))
			; /* redirect with no body or similar */
		else
			finished = true;
	} else if (result == CURLE_WRITE_ERROR && f->stopped)
		/* CURLE_WRITE_ERROR occurs when fetch_curl_data
		 * returns 0, which we use to abort intentionally */
		;
#ifdef WITH_SSL
	else if (result == CURLE_SSL_PEER_CERTIFICATE ||
			result == CURLE_SSL_CACERT) {
		memcpy(certs, f->cert_data, sizeof(certs));
		memset(f->cert_data, 0, sizeof(f->cert_data));
		cert = true;
	}
#endif
	else
		error = true;

	/* If finished, acquire cache info to pass to callback */
	if (finished) {
		memcpy(&cachedata, &f->cachedata, sizeof(struct cache_data));
		f->cachedata.etag = 0;
	}

	/* clean up fetch and start any queued fetch for this host */
	fetch_stop(f);

	/* postponed until after stop so that queue fetches are started */
	if (abort)
		; /* fetch was aborted: no callback */
	else if (finished) {
		callback(FETCH_FINISHED, p, &cachedata, 0);
		free(cachedata.etag);
	}
#ifdef WITH_SSL
	else if (cert) {
		int i;
		BIO *mem;
		BUF_MEM *buf;
		struct ssl_cert_info ssl_certs[MAX_CERTS];

		for (i = 0; i < MAX_CERTS && certs[i].cert; i++) {
			ssl_certs[i].version =
				X509_get_version(certs[i].cert);

			mem = BIO_new(BIO_s_mem());
			ASN1_TIME_print(mem,
					X509_get_notBefore(certs[i].cert));
			BIO_get_mem_ptr(mem, &buf);
			BIO_set_close(mem, BIO_NOCLOSE);
			BIO_free(mem);
			snprintf(ssl_certs[i].not_before,
					min(sizeof ssl_certs[i].not_before,
						(unsigned) buf->length + 1),
					"%s", buf->data);
			BUF_MEM_free(buf);

			mem = BIO_new(BIO_s_mem());
			ASN1_TIME_print(mem,
					X509_get_notAfter(certs[i].cert));
			BIO_get_mem_ptr(mem, &buf);
			BIO_set_close(mem, BIO_NOCLOSE);
			BIO_free(mem);
			snprintf(ssl_certs[i].not_after,
					min(sizeof ssl_certs[i].not_after,
						(unsigned) buf->length + 1),
					"%s", buf->data);
			BUF_MEM_free(buf);

			ssl_certs[i].sig_type =
				X509_get_signature_type(certs[i].cert);
			ssl_certs[i].serial =
				ASN1_INTEGER_get(
					X509_get_serialNumber(certs[i].cert));
			mem = BIO_new(BIO_s_mem());
			X509_NAME_print_ex(mem,
				X509_get_issuer_name(certs[i].cert),
				0, XN_FLAG_SEP_CPLUS_SPC |
					XN_FLAG_DN_REV | XN_FLAG_FN_NONE);
			BIO_get_mem_ptr(mem, &buf);
			BIO_set_close(mem, BIO_NOCLOSE);
			BIO_free(mem);
			snprintf(ssl_certs[i].issuer,
					min(sizeof ssl_certs[i].issuer,
						(unsigned) buf->length + 1),
					"%s", buf->data);
			BUF_MEM_free(buf);

			mem = BIO_new(BIO_s_mem());
			X509_NAME_print_ex(mem,
				X509_get_subject_name(certs[i].cert),
				0, XN_FLAG_SEP_CPLUS_SPC |
					XN_FLAG_DN_REV | XN_FLAG_FN_NONE);
			BIO_get_mem_ptr(mem, &buf);
			BIO_set_close(mem, BIO_NOCLOSE);
			BIO_free(mem);
			snprintf(ssl_certs[i].subject,
					min(sizeof ssl_certs[i].subject,
						(unsigned) buf->length + 1),
					"%s", buf->data);
			BUF_MEM_free(buf);

			ssl_certs[i].cert_type =
				X509_certificate_type(certs[i].cert,
					X509_get_pubkey(certs[i].cert));

			/* and clean up */
			certs[i].cert->references--;
			if (certs[i].cert->references == 0)
				X509_free(certs[i].cert);
		}

		callback(FETCH_CERT_ERR, p, &ssl_certs, i);

	}
#endif
	else if (error)
		callback(FETCH_ERROR, p, fetch_error_buffer, 0);
}


/**
 * Callback function for fetch progress.
 */

int fetch_curl_progress(void *clientp, double dltotal, double dlnow,
		double ultotal, double ulnow)
{
	struct fetch *f = (struct fetch *) clientp;
	double percent;

	if (f->abort)
		return 0;

	if (dltotal > 0) {
		percent = dlnow * 100.0f / dltotal;
		snprintf(fetch_progress_buffer, 255,
				messages_get("Progress"),
				human_friendly_bytesize(dlnow),
				human_friendly_bytesize(dltotal));
		f->callback(FETCH_PROGRESS, f->p, fetch_progress_buffer,
				(unsigned long) percent);
	} else {
		snprintf(fetch_progress_buffer, 255,
				messages_get("ProgressU"),
				human_friendly_bytesize(dlnow));
		f->callback(FETCH_PROGRESS, f->p, fetch_progress_buffer, 0);
	}

	return 0;
}


/**
 * Callback function for cURL.
 */

size_t fetch_curl_data(void *data, size_t size, size_t nmemb,
		struct fetch *f)
{
	CURLcode code;

	/* ensure we only have to get this information once */
	if (!f->http_code)
	{
		code = curl_easy_getinfo(f->curl_handle, CURLINFO_HTTP_CODE,
				&f->http_code);
		assert(code == CURLE_OK);
	}

	/* ignore body if this is a 401 reply by skipping it and reset
	   the HTTP response code to enable follow up fetches */
	if (f->http_code == 401)
	{
		f->http_code = 0;
		return size * nmemb;
	}

	LOG(("fetch %p, size %u", f, size * nmemb));

	if (f->abort || (!f->had_headers && fetch_process_headers(f))) {
		f->stopped = true;
		return 0;
	}

	/* send data to the caller */
	LOG(("FETCH_DATA"));
	f->callback(FETCH_DATA, f->p, data, size * nmemb);

	if (f->abort) {
		f->stopped = true;
		return 0;
	}

	return size * nmemb;
}


/**
 * Callback function for headers.
 */

size_t fetch_curl_header(char *data, size_t size, size_t nmemb,
		struct fetch *f)
{
	int i;
	size *= nmemb;

#define SKIP_ST(o) for (i = (o); i < (int) size && (data[i] == ' ' || data[i] == '\t'); i++)

	/* Set fetch response time if not already set */
	if (f->cachedata.res_time == 0)
		f->cachedata.res_time = time(0);

	if (12 < size && strncasecmp(data, "Location:", 9) == 0) {
		/* extract Location header */
		free(f->location);
		f->location = malloc(size);
		if (!f->location) {
			LOG(("malloc failed"));
			return size;
		}
		SKIP_ST(9);
		strncpy(f->location, data + i, size - i);
		f->location[size - i] = '\0';
		for (i = size - i - 1; i >= 0 &&
				(f->location[i] == ' ' ||
				f->location[i] == '\t' ||
				f->location[i] == '\r' ||
				f->location[i] == '\n'); i--)
			f->location[i] = '\0';
	} else if (15 < size && strncasecmp(data, "Content-Length:", 15) == 0) {
		/* extract Content-Length header */
		SKIP_ST(15);
		if (i < (int)size && '0' <= data[i] && data[i] <= '9')
			f->content_length = atol(data + i);
#ifdef WITH_AUTH
	} else if (17 < size && strncasecmp(data, "WWW-Authenticate:", 17) == 0) {
		/* extract the first Realm from WWW-Authenticate header */
		free(f->realm);
		f->realm = malloc(size);
		if (!f->realm) {
			LOG(("malloc failed"));
			return size;
		}
		SKIP_ST(17);

		while (i < (int) size && strncasecmp(data + i, "realm", 5))
			i++;
		while (i < (int)size && data[++i] != '"')
			/* */;
		i++;

		strncpy(f->realm, data + i, size - i);
		f->realm[size - i] = '\0';
		for (i = size - i - 1; i >= 0 &&
				(f->realm[i] == ' ' ||
				f->realm[i] == '"' ||
				f->realm[i] == '\t' ||
				f->realm[i] == '\r' ||
				f->realm[i] == '\n'); --i)
			f->realm[i] = '\0';
#endif
	} else if (5 < size && strncasecmp(data, "Date:", 5) == 0) {
		/* extract Date header */
		SKIP_ST(5);
		if (i < (int) size)
			f->cachedata.date = curl_getdate(&data[i], NULL);
	} else if (4 < size && strncasecmp(data, "Age:", 4) == 0) {
		/* extract Age header */
		SKIP_ST(4);
		if (i < (int) size && '0' <= data[i] && data[i] <= '9')
			f->cachedata.age = atoi(data + i);
	} else if (8 < size && strncasecmp(data, "Expires:", 8) == 0) {
		/* extract Expires header */
		SKIP_ST(8);
		if (i < (int) size)
			f->cachedata.expires = curl_getdate(&data[i], NULL);
	} else if (14 < size && strncasecmp(data, "Cache-Control:", 14) == 0) {
		/* extract and parse Cache-Control header */
		int comma;
		SKIP_ST(14);

		while (i < (int) size) {
			for (comma = i; comma < (int) size; comma++)
				if (data[comma] == ',')
					break;

			SKIP_ST(i);

			if (8 < comma - i && (strncasecmp(data + i, "no-cache", 8) == 0 || strncasecmp(data + i, "no-store", 8) == 0))
				/* When we get a disk cache we should
				 * distinguish between these two */
				f->cachedata.no_cache = true;
			else if (7 < comma - i && strncasecmp(data + i, "max-age", 7) == 0) {
				for (; i < comma; i++)
					if (data[i] == '=')
						break;
				SKIP_ST(i+1);
				if (i < comma)
					f->cachedata.max_age =
							atoi(data + i);
			}

			i = comma + 1;
		}
	} else if (5 < size && strncasecmp(data, "ETag:", 5) == 0) {
		/* extract ETag header */
		free(f->cachedata.etag);
		f->cachedata.etag = malloc(size);
		if (!f->cachedata.etag) {
			LOG(("malloc failed"));
			return size;
		}
		SKIP_ST(5);
		strncpy(f->cachedata.etag, data + i, size - i);
		f->cachedata.etag[size - i] = '\0';
		for (i = size - i - 1; i >= 0 &&
				(f->cachedata.etag[i] == ' ' ||
				f->cachedata.etag[i] == '\t' ||
				f->cachedata.etag[i] == '\r' ||
				f->cachedata.etag[i] == '\n'); --i)
			f->cachedata.etag[i] = '\0';
	} else if (14 < size && strncasecmp(data, "Last-Modified:", 14) == 0) {
		/* extract Last-Modified header */
		SKIP_ST(14);
		if (i < (int) size) {
			f->cachedata.last_modified =
					curl_getdate(&data[i], NULL);
		}
	} else if (f->cookies && 11 < size &&
			strncasecmp(data, "Set-Cookie:", 11) == 0) {
		/* extract Set-Cookie header */
		SKIP_ST(11);
		urldb_set_cookie(&data[i], f->url);
	}

	return size;
#undef SKIP_ST
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
	struct stat s;
	char *url_path = 0;

	f->had_headers = true;

	/* Set fetch response time if not already set */
	if (f->cachedata.res_time == 0)
		f->cachedata.res_time = time(0);

	if (!f->http_code)
	{
		code = curl_easy_getinfo(f->curl_handle, CURLINFO_HTTP_CODE,
				&f->http_code);
		assert(code == CURLE_OK);
	}
	http_code = f->http_code;
	LOG(("HTTP status code %li", http_code));

	if (http_code == 304 && !f->post_urlenc && !f->post_multipart) {
		/* Not Modified && GET request */
		f->callback(FETCH_NOTMODIFIED, f->p,
				(const char *)&f->cachedata, 0);
		return true;
	}

	/* handle HTTP redirects (3xx response codes) */
	if (300 <= http_code && http_code < 400 && f->location != 0) {
		LOG(("FETCH_REDIRECT, '%s'", f->location));
		f->callback(FETCH_REDIRECT, f->p, f->location, 0);
		return true;
	}

	/* handle HTTP 401 (Authentication errors) */
#ifdef WITH_AUTH
	if (http_code == 401) {
		f->callback(FETCH_AUTH, f->p, f->realm,0);
		return true;
	}
#endif

	/* handle HTTP errors (non 2xx response codes) */
	if (f->only_2xx && strncmp(f->url, "http", 4) == 0 &&
			(http_code < 200 || 299 < http_code)) {
		f->callback(FETCH_ERROR, f->p, messages_get("Not2xx"), 0);
		return true;
	}

	/* find MIME type from headers or filetype for local files */
	code = curl_easy_getinfo(f->curl_handle, CURLINFO_CONTENT_TYPE, &type);
	assert(code == CURLE_OK);

	if (strncmp(f->url, "file:///", 8) == 0)
		url_path = curl_unescape(f->url + 7,
				(int) strlen(f->url) - 7);

	if (url_path && stat(url_path, &s) == 0) {
		/* file: URL and file exists */
		/* create etag */
		free(f->cachedata.etag);
		f->cachedata.etag = malloc(13);
		if (f->cachedata.etag)
			sprintf(f->cachedata.etag,
					"\"%10d\"", (int)s.st_mtime);

		/* don't set last modified time so as to ensure that local
		 * files are revalidated at all times. */

		/* If performed a conditional request and unmodified ... */
		if (f->last_modified && f->file_etag &&
				f->last_modified > s.st_mtime &&
				f->file_etag == s.st_mtime) {
			f->callback(FETCH_NOTMODIFIED, f->p,
					(const char *)&f->cachedata, 0);
			curl_free(url_path);
			return true;
		}
	}

	if (type == 0) {
		type = "text/plain";
		if (url_path) {
			type = fetch_filetype(url_path);
		}
	}

	curl_free(url_path);

	LOG(("FETCH_TYPE, '%s'", type));
	f->callback(FETCH_TYPE, f->p, type, f->content_length);
	if (f->abort)
		return true;

	return false;
}


/**
 * Convert a list of struct ::form_successful_control to a list of
 * struct curl_httppost for libcurl.
 */
struct curl_httppost *fetch_post_convert(struct form_successful_control *control)
{
	struct curl_httppost *post = 0, *last = 0;
	char *mimetype = 0;
	char *leafname = 0;
#ifdef riscos
	char *temp;
	int leaflen;
#endif

	for (; control; control = control->next) {
		if (control->file) {
			mimetype = fetch_mimetype(control->value);
#ifdef riscos
			temp = strrchr(control->value, '.');
			if (!temp)
				temp = control->value; /* already leafname */
			else
				temp += 1;

			leaflen = strlen(temp);

			leafname = malloc(leaflen + 1);
			if (!leafname) {
				LOG(("malloc failed"));
				free(mimetype);
				continue;
			}
			memcpy(leafname, temp, leaflen + 1);

			/* and s/\//\./g */
			for (temp = leafname; *temp; temp++)
				if (*temp == '/')
					*temp = '.';
#else
			leafname = strrchr(control->value, '/') ;
			if (!leafname)
				leafname = control->value;
			else
				leafname += 1;
#endif
			curl_formadd(&post, &last,
					CURLFORM_COPYNAME, control->name,
					CURLFORM_FILE, control->value,
					CURLFORM_FILENAME, leafname,
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


/**
 * Check if a URL's scheme can be fetched.
 *
 * \param  url  URL to check
 * \return  true if the scheme is supported
 */

bool fetch_can_fetch(const char *url)
{
	unsigned int i;
	const char *semi;
	size_t len;
	curl_version_info_data *data;

	if ((semi = strchr(url, ':')) == NULL)
		return false;
	len = semi - url;

	data = curl_version_info(CURLVERSION_NOW);

	for (i = 0; data->protocols[i]; i++)
		if (strlen(data->protocols[i]) == len &&
				strncasecmp(url, data->protocols[i], len) == 0)
			return true;

	return false;
}


/**
 * Change the callback function for a fetch.
 */

void fetch_change_callback(struct fetch *fetch,
		void (*callback)(fetch_msg msg, void *p, const void *data,
				unsigned long size),
		void *p)
{
	assert(fetch);
	fetch->callback = callback;
	fetch->p = p;
}


/**
 * Get the HTTP response code.
 */

long fetch_http_code(struct fetch *fetch)
{
	return fetch->http_code;
}


#ifdef WITH_SSL
/**
 * OpenSSL Certificate verification callback
 * Stores certificate details in fetch struct.
 */

int fetch_verify_callback(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
	X509 *cert = X509_STORE_CTX_get_current_cert(x509_ctx);
	int depth = X509_STORE_CTX_get_error_depth(x509_ctx);
	int err = X509_STORE_CTX_get_error(x509_ctx);
	struct fetch *f = X509_STORE_CTX_get_app_data(x509_ctx);

	/* save the certificate by incrementing the reference count and
	 * keeping a pointer */
	if (depth < MAX_CERTS && !f->cert_data[depth].cert) {
		f->cert_data[depth].cert = cert;
		f->cert_data[depth].err = err;
		cert->references++;
	}

	return preverify_ok;
}


/**
 * OpenSSL certificate chain verification callback
 * Verifies certificate chain, setting up context for fetch_verify_callback
 */

int fetch_cert_verify_callback(X509_STORE_CTX *x509_ctx, void *parm)
{
	int ok;

	/* Store fetch struct in context for verify callback */
	ok = X509_STORE_CTX_set_app_data(x509_ctx, parm);

	/* and verify the certificate chain */
	if (ok)
		ok = X509_verify_cert(x509_ctx);

	return ok;
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


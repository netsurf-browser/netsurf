/*
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
 * Copyright 2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 *
 * This file is part of NetSurf.
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Fetching of data from a URL (implementation).
 *
 * This implementation uses libcurl's 'multi' interface.
 *
 *
 * The CURL handles are cached in the curl_handle_ring. There are at most
 * ::option_max_cached_fetch_handles in this ring.
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include "utils/config.h"
#include <openssl/ssl.h>
#include "content/fetch.h"
#include "content/fetchers/fetch_curl.h"
#include "content/urldb.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "render/form.h"
#undef NDEBUG
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/ring.h"
#include "utils/useragent.h"

/** SSL certificate info */
struct cert_info {
	X509 *cert;		/**< Pointer to certificate */
	long err;		/**< OpenSSL error code */
};

/** Information for a single fetch. */
struct curl_fetch_info {
	struct fetch *fetch_handle; /**< The fetch handle we're parented by. */
	CURL * curl_handle;	/**< cURL handle if being fetched, or 0. */
	bool had_headers;	/**< Headers have been processed. */
	bool abort;		/**< Abort requested. */
	bool stopped;		/**< Download stopped on purpose. */
	bool only_2xx;		/**< Only HTTP 2xx responses acceptable. */
	char *url;		/**< URL of this fetch. */
	char *host;		/**< The hostname of this fetch. */
	struct curl_slist *headers;	/**< List of request headers. */
	char *location;		/**< Response Location header, or 0. */
	unsigned long content_length;	/**< Response Content-Length, or 0. */
	char *cookie_string;	/**< Cookie string for this fetch */
	char *realm;		/**< HTTP Auth Realm */
	char *post_urlenc;	/**< Url encoded POST string, or 0. */
	long http_code; /**< HTTP result code from cURL. */
	struct curl_httppost *post_multipart;	/**< Multipart post data, or 0. */
	time_t last_modified;		/**< If-Modified-Since time */
	time_t file_etag;		/**< ETag for local objects */
#define MAX_CERTS 10
	struct cert_info cert_data[MAX_CERTS];	/**< HTTPS certificate data */
};

struct cache_handle {
	CURL *handle; /**< The cached cURL handle */
	char *host;	   /**< The host for which this handle is cached */

	struct cache_handle *r_prev; /**< Previous cached handle in ring. */
	struct cache_handle *r_next; /**< Next cached handle in ring. */
};

CURLM *fetch_curl_multi;		/**< Global cURL multi handle. */
/** Curl handle with default options set; not used for transfers. */
static CURL *fetch_blank_curl;
static struct cache_handle *curl_handle_ring = 0; /**< Ring of cached handles */
static int curl_fetchers_registered = 0;
static bool curl_with_openssl;

static char fetch_error_buffer[CURL_ERROR_SIZE]; /**< Error buffer for cURL. */
static char fetch_progress_buffer[256]; /**< Progress buffer for cURL */
static char fetch_proxy_userpwd[100];	/**< Proxy authentication details. */

static bool fetch_curl_initialise(const char *scheme);
static void fetch_curl_finalise(const char *scheme);
static void * fetch_curl_setup(struct fetch *parent_fetch, const char *url,
		 bool only_2xx, const char *post_urlenc,
		 struct form_successful_control *post_multipart,
		 const char **headers);
static bool fetch_curl_start(void *vfetch);
static bool fetch_curl_initiate_fetch(struct curl_fetch_info *fetch,
		CURL *handle);
static CURL *fetch_curl_get_handle(char *host);
static void fetch_curl_cache_handle(CURL *handle, char *hostname);
static CURLcode fetch_curl_set_options(struct curl_fetch_info *f);
static CURLcode fetch_curl_sslctxfun(CURL *curl_handle, void *_sslctx,
				     void *p);
static void fetch_curl_abort(void *vf);
static void fetch_curl_stop(struct curl_fetch_info *f);
static void fetch_curl_free(void *f);
static void fetch_curl_poll(const char *scheme_ignored);
static void fetch_curl_done(CURL *curl_handle, CURLcode result);
static int fetch_curl_progress(void *clientp, double dltotal, double dlnow,
		double ultotal, double ulnow);
static int fetch_curl_ignore_debug(CURL *handle,
				   curl_infotype type,
				   char *data,
				   size_t size,
				   void *userptr);
static size_t fetch_curl_data(char *data, size_t size, size_t nmemb,
			      void *_f);
static size_t fetch_curl_header(char *data, size_t size, size_t nmemb,
				void *_f);
static bool fetch_curl_process_headers(struct curl_fetch_info *f);
static struct curl_httppost *fetch_curl_post_convert(
		struct form_successful_control *control);
static int fetch_curl_verify_callback(int preverify_ok,
		X509_STORE_CTX *x509_ctx);
static int fetch_curl_cert_verify_callback(X509_STORE_CTX *x509_ctx,
		void *parm);


/**
 * Initialise the fetcher.
 *
 * Must be called once before any other function.
 */

void fetch_curl_register(void)
{
	CURLcode code;
	curl_version_info_data *data;
	int i;

	LOG(("curl_version %s", curl_version()));

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

#undef SETOPT
#define SETOPT(option, value) \
	code = curl_easy_setopt(fetch_blank_curl, option, value);	\
	if (code != CURLE_OK)						\
		goto curl_easy_setopt_failed;

	if (verbose_log) {
	    SETOPT(CURLOPT_VERBOSE, 1);
	} else {
	    SETOPT(CURLOPT_VERBOSE, 0);
	}
	SETOPT(CURLOPT_ERRORBUFFER, fetch_error_buffer);
	if (option_suppress_curl_debug)
		SETOPT(CURLOPT_DEBUGFUNCTION, fetch_curl_ignore_debug);
	SETOPT(CURLOPT_WRITEFUNCTION, fetch_curl_data);
	SETOPT(CURLOPT_HEADERFUNCTION, fetch_curl_header);
	SETOPT(CURLOPT_PROGRESSFUNCTION, fetch_curl_progress);
	SETOPT(CURLOPT_NOPROGRESS, 0);
	SETOPT(CURLOPT_USERAGENT, user_agent_string());
	SETOPT(CURLOPT_ENCODING, "gzip");
	SETOPT(CURLOPT_LOW_SPEED_LIMIT, 1L);
	SETOPT(CURLOPT_LOW_SPEED_TIME, 180L);
	SETOPT(CURLOPT_NOSIGNAL, 1L);
	SETOPT(CURLOPT_CONNECTTIMEOUT, 30L);

	if (option_ca_bundle && strcmp(option_ca_bundle, ""))
		SETOPT(CURLOPT_CAINFO, option_ca_bundle);
	if (option_ca_path && strcmp(option_ca_path, ""))
		SETOPT(CURLOPT_CAPATH, option_ca_path);

	/* Detect whether the SSL CTX function API works */
	curl_with_openssl = true;
	code = curl_easy_setopt(fetch_blank_curl, 
			CURLOPT_SSL_CTX_FUNCTION, NULL);
	if (code != CURLE_OK) {
		curl_with_openssl = false;
	}

	LOG(("cURL %slinked against openssl", curl_with_openssl ? "" : "not "));

	/* cURL initialised okay, register the fetchers */

	data = curl_version_info(CURLVERSION_NOW);

	for (i = 0; data->protocols[i]; i++)
		if (!fetch_add_fetcher(data->protocols[i],
				       fetch_curl_initialise,
				       fetch_curl_setup,
				       fetch_curl_start,
				       fetch_curl_abort,
				       fetch_curl_free,
				       fetch_curl_poll,
				       fetch_curl_finalise)) {
			LOG(("Unable to register cURL fetcher for %s",
					data->protocols[i]));
		}
	return;

curl_easy_setopt_failed:
	die("Failed to initialise the fetch module "
			"(curl_easy_setopt failed).");
}


/**
 * Initialise a cURL fetcher.
 */

bool fetch_curl_initialise(const char *scheme)
{
	LOG(("Initialise cURL fetcher for %s", scheme));
	curl_fetchers_registered++;
	return true; /* Always succeeds */
}


/**
 * Finalise a cURL fetcher
 */

void fetch_curl_finalise(const char *scheme)
{
	curl_fetchers_registered--;
	LOG(("Finalise cURL fetcher %s", scheme));
	if (curl_fetchers_registered == 0) {
		CURLMcode codem;
		/* All the fetchers have been finalised. */
		LOG(("All cURL fetchers finalised, closing down cURL"));

		curl_easy_cleanup(fetch_blank_curl);

		codem = curl_multi_cleanup(fetch_curl_multi);
		if (codem != CURLM_OK)
			LOG(("curl_multi_cleanup failed: ignoring"));

		curl_global_cleanup();
	}
}


/**
 * Start fetching data for the given URL.
 *
 * The function returns immediately. The fetch may be queued for later
 * processing.
 *
 * A pointer to an opaque struct curl_fetch_info is returned, which can be 
 * passed to fetch_abort() to abort the fetch at any time. Returns 0 if memory 
 * is exhausted (or some other fatal error occurred).
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

void * fetch_curl_setup(struct fetch *parent_fetch, const char *url,
		 bool only_2xx, const char *post_urlenc,
		 struct form_successful_control *post_multipart,
		 const char **headers)
{
	char *host;
	struct curl_fetch_info *fetch;
	struct curl_slist *slist;
	url_func_result res;
	int i;

	fetch = malloc(sizeof (*fetch));
	if (!fetch)
		return 0;

	fetch->fetch_handle = parent_fetch;

	res = url_host(url, &host);
	if (res != URL_FUNC_OK) {
		/* we only fail memory exhaustion */
		if (res == URL_FUNC_NOMEM)
			goto failed;

		host = strdup("");
		if (!host)
			goto failed;
	}

	LOG(("fetch %p, url '%s'", fetch, url));

	/* construct a new fetch structure */
	fetch->curl_handle = 0;
	fetch->had_headers = false;
	fetch->abort = false;
	fetch->stopped = false;
	fetch->only_2xx = only_2xx;
	fetch->url = strdup(url);
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
		fetch->post_multipart = fetch_curl_post_convert(post_multipart);
	fetch->last_modified = 0;
	fetch->file_etag = 0;
	fetch->http_code = 0;
	memset(fetch->cert_data, 0, sizeof(fetch->cert_data));

	if (!fetch->url ||
	    (post_urlenc && !fetch->post_urlenc) ||
	    (post_multipart && !fetch->post_multipart))
		goto failed;

#define APPEND(list, value) \
	slist = curl_slist_append(list, value);		\
	if (!slist)					\
		goto failed;				\
	list = slist;

	/* remove curl default headers */
	APPEND(fetch->headers, "Pragma:");

	/* when doing a POST libcurl sends Expect: 100-continue" by default
	 * which fails with lighttpd, so disable it (see bug 1429054) */
	APPEND(fetch->headers, "Expect:");

	if (option_accept_language && option_accept_language[0] != '\0') {
		char s[80];
		snprintf(s, sizeof s, "Accept-Language: %s, *;q=0.1",
				option_accept_language);
		s[sizeof s - 1] = 0;
		APPEND(fetch->headers, s);
	}

	if (option_accept_charset && option_accept_charset[0] != '\0') {
		char s[80];
		snprintf(s, sizeof s, "Accept-Charset: %s, *;q=0.1",
				option_accept_charset);
		s[sizeof s - 1] = 0;
		APPEND(fetch->headers, s);
	}

	/* And add any headers specified by the caller */
	for (i = 0; headers[i]; i++) {
		if (strncasecmp(headers[i], "If-Modified-Since:", 18) == 0) {
			const char *d = headers[i] + 18;
			for (; *d && (*d == ' ' || *d == '\t'); d++)
				/* do nothing */;
			fetch->last_modified = curl_getdate(d, NULL);
		}
		else if (strncasecmp(headers[i], "If-None-Match:", 14) == 0) {
			const char *d = headers[i] + 14;
			for (; *d && (*d == ' ' || *d == '\t' || *d == '"');
					d++)
				/* do nothing */;
			fetch->file_etag = atoi(d);
		}
		APPEND(fetch->headers, headers[i]);
	}

	return fetch;

failed:
	free(host);
	free(fetch->url);
	free(fetch->post_urlenc);
	if (fetch->post_multipart)
		curl_formfree(fetch->post_multipart);
	curl_slist_free_all(fetch->headers);
	free(fetch);
	return 0;
}


/**
 * Dispatch a single job
 */
bool fetch_curl_start(void *vfetch)
{
	struct curl_fetch_info *fetch = (struct curl_fetch_info*)vfetch;
	return fetch_curl_initiate_fetch(fetch,
			fetch_curl_get_handle(fetch->host));
}


/**
 * Initiate a fetch from the queue.
 *
 * Called with a fetch structure and a CURL handle to be used to fetch the
 * content.
 *
 * This will return whether or not the fetch was successfully initiated.
 */

bool fetch_curl_initiate_fetch(struct curl_fetch_info *fetch, CURL *handle)
{
	CURLcode code;
	CURLMcode codem;

	fetch->curl_handle = handle;

	/* Initialise the handle */
	code = fetch_curl_set_options(fetch);
	if (code != CURLE_OK) {
		fetch->curl_handle = 0;
		return false;
	}

	/* add to the global curl multi handle */
	codem = curl_multi_add_handle(fetch_curl_multi, fetch->curl_handle);
	assert(codem == CURLM_OK || codem == CURLM_CALL_MULTI_PERFORM);

	return true;
}


/**
 * Find a CURL handle to use to dispatch a job
 */

CURL *fetch_curl_get_handle(char *host)
{
	struct cache_handle *h;
	CURL *ret;
	RING_FINDBYHOST(curl_handle_ring, h, host);
	if (h) {
		ret = h->handle;
		free(h->host);
		RING_REMOVE(curl_handle_ring, h);
		free(h);
	} else {
		ret = curl_easy_duphandle(fetch_blank_curl);
	}
	return ret;
}


/**
 * Cache a CURL handle for the provided host (if wanted)
 */

void fetch_curl_cache_handle(CURL *handle, char *host)
{
	struct cache_handle *h = 0;
	int c;
	RING_FINDBYHOST(curl_handle_ring, h, host);
	if (h) {
		/* Already have a handle cached for this hostname */
		curl_easy_cleanup(handle);
		return;
	}
	/* We do not have a handle cached, first up determine if the cache is full */
	RING_GETSIZE(struct cache_handle, curl_handle_ring, c);
	if (c >= option_max_cached_fetch_handles) {
		/* Cache is full, so, we rotate the ring by one and replace the
		 * oldest handle with this one. We do this without freeing/allocating
		 * memory (except the hostname) and without removing the entry from the
		 * ring and then re-inserting it, in order to be as efficient as we can.
		 */
		h = curl_handle_ring;
		curl_handle_ring = h->r_next;
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
	RING_INSERT(curl_handle_ring, h);
}


/**
 * Set options specific for a fetch.
 */

CURLcode
fetch_curl_set_options(struct curl_fetch_info *f)
{
	CURLcode code;
	const char *auth;

#undef SETOPT
#define SETOPT(option, value) { \
	code = curl_easy_setopt(f->curl_handle, option, value);	\
	if (code != CURLE_OK)					\
		return code;					\
	}

	SETOPT(CURLOPT_URL, f->url);
	SETOPT(CURLOPT_PRIVATE, f);
	SETOPT(CURLOPT_WRITEDATA, f);
	SETOPT(CURLOPT_WRITEHEADER, f);
	SETOPT(CURLOPT_PROGRESSDATA, f);
	SETOPT(CURLOPT_REFERER, fetch_get_referer_to_send(f->fetch_handle));
	SETOPT(CURLOPT_HTTPHEADER, f->headers);
	if (f->post_urlenc) {
		SETOPT(CURLOPT_HTTPPOST, NULL);
		SETOPT(CURLOPT_HTTPGET, 0L);
		SETOPT(CURLOPT_POSTFIELDS, f->post_urlenc);
	} else if (f->post_multipart) {
		SETOPT(CURLOPT_POSTFIELDS, NULL);
		SETOPT(CURLOPT_HTTPGET, 0L);
		SETOPT(CURLOPT_HTTPPOST, f->post_multipart);
	} else {
		SETOPT(CURLOPT_POSTFIELDS, NULL);
		SETOPT(CURLOPT_HTTPPOST, NULL);
		SETOPT(CURLOPT_HTTPGET, 1L);
	}

	f->cookie_string = urldb_get_cookie(f->url);
	if (f->cookie_string) {
		SETOPT(CURLOPT_COOKIE, f->cookie_string);
	} else {
		SETOPT(CURLOPT_COOKIE, NULL);
	}

	if ((auth = urldb_get_auth_details(f->url)) != NULL) {
		SETOPT(CURLOPT_HTTPAUTH, CURLAUTH_ANY);
		SETOPT(CURLOPT_USERPWD, auth);
	} else {
		SETOPT(CURLOPT_USERPWD, NULL);
	}

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

	if (urldb_get_cert_permissions(f->url)) {
		/* Disable certificate verification */
		SETOPT(CURLOPT_SSL_VERIFYPEER, 0L);
		SETOPT(CURLOPT_SSL_VERIFYHOST, 0L);
		if (curl_with_openssl) {
			SETOPT(CURLOPT_SSL_CTX_FUNCTION, NULL);
			SETOPT(CURLOPT_SSL_CTX_DATA, NULL);
		}
	} else {
		/* do verification */
		SETOPT(CURLOPT_SSL_VERIFYPEER, 1L);
		SETOPT(CURLOPT_SSL_VERIFYHOST, 2L);
		if (curl_with_openssl) {
			SETOPT(CURLOPT_SSL_CTX_FUNCTION, fetch_curl_sslctxfun);
			SETOPT(CURLOPT_SSL_CTX_DATA, f);
		}
	}

	return CURLE_OK;
}


/**
 * cURL SSL setup callback
 */

CURLcode
fetch_curl_sslctxfun(CURL *curl_handle, void *_sslctx, void *parm)
{
	SSL_CTX *sslctx = _sslctx;
	SSL_CTX_set_verify(sslctx, SSL_VERIFY_PEER, fetch_curl_verify_callback);
	SSL_CTX_set_cert_verify_callback(sslctx, fetch_curl_cert_verify_callback,
					 parm);
	return CURLE_OK;
}


/**
 * Abort a fetch.
 */

void fetch_curl_abort(void *vf)
{
	struct curl_fetch_info *f = (struct curl_fetch_info *)vf;
	assert(f);
	LOG(("fetch %p, url '%s'", f, f->url));
	if (f->curl_handle) {
		f->abort = true;
	} else {
		fetch_remove_from_queues(f->fetch_handle);
		fetch_free(f->fetch_handle);
	}
}


/**
 * Clean up the provided fetch object and free it.
 *
 * Will prod the queue afterwards to allow pending requests to be initiated.
 */

void fetch_curl_stop(struct curl_fetch_info *f)
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
		fetch_curl_cache_handle(f->curl_handle, f->host);
		f->curl_handle = 0;
	}

	fetch_remove_from_queues(f->fetch_handle);
}


/**
 * Free a fetch structure and associated resources.
 */

void fetch_curl_free(void *vf)
{
	struct curl_fetch_info *f = (struct curl_fetch_info *)vf;
	int i;

	if (f->curl_handle)
		curl_easy_cleanup(f->curl_handle);
	free(f->url);
	free(f->host);
	free(f->location);
	free(f->cookie_string);
	free(f->realm);
	if (f->headers)
		curl_slist_free_all(f->headers);
	free(f->post_urlenc);
	if (f->post_multipart)
		curl_formfree(f->post_multipart);

	for (i = 0; i < MAX_CERTS && f->cert_data[i].cert; i++) {
		f->cert_data[i].cert->references--;
		if (f->cert_data[i].cert->references == 0)
			X509_free(f->cert_data[i].cert);
	}

	free(f);
}


/**
 * Do some work on current fetches.
 *
 * Must be called regularly to make progress on fetches.
 */

void fetch_curl_poll(const char *scheme_ignored)
{
	int running, queue;
	CURLMcode codem;
	CURLMsg *curl_msg;

	/* do any possible work on the current fetches */
	do {
		codem = curl_multi_perform(fetch_curl_multi, &running);
		if (codem != CURLM_OK && codem != CURLM_CALL_MULTI_PERFORM) {
			LOG(("curl_multi_perform: %i %s",
					codem, curl_multi_strerror(codem)));
			warn_user("MiscError", curl_multi_strerror(codem));
			return;
		}
	} while (codem == CURLM_CALL_MULTI_PERFORM);

	/* process curl results */
	curl_msg = curl_multi_info_read(fetch_curl_multi, &queue);
	while (curl_msg) {
		switch (curl_msg->msg) {
			case CURLMSG_DONE:
				fetch_curl_done(curl_msg->easy_handle,
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
 * \param  curl_handle	curl easy handle of fetch
 */

void fetch_curl_done(CURL *curl_handle, CURLcode result)
{
	bool finished = false;
	bool error = false;
	bool cert = false;
	bool abort_fetch;
	struct curl_fetch_info *f;
	char **_hideous_hack = (char **) (void *) &f;
	CURLcode code;
	struct cert_info certs[MAX_CERTS];
	memset(certs, 0, sizeof(certs));

	/* find the structure associated with this fetch */
	/* For some reason, cURL thinks CURLINFO_PRIVATE should be a string?! */
	code = curl_easy_getinfo(curl_handle, CURLINFO_PRIVATE, _hideous_hack);
	assert(code == CURLE_OK);

	abort_fetch = f->abort;
	LOG(("done %s", f->url));

	if (abort_fetch == false && result == CURLE_OK) {
		/* fetch completed normally */
		if (f->stopped ||
				(!f->had_headers &&
					fetch_curl_process_headers(f)))
			; /* redirect with no body or similar */
		else
			finished = true;
	} else if (result == CURLE_PARTIAL_FILE) {
		/* CURLE_PARTIAL_FILE occurs if the received body of a
		 * response is smaller than that specified in the
		 * Content-Length header. */
		if (!f->had_headers && fetch_curl_process_headers(f))
			; /* redirect with partial body, or similar */
		else
			error = true;
	} else if (result == CURLE_WRITE_ERROR && f->stopped)
		/* CURLE_WRITE_ERROR occurs when fetch_curl_data
		 * returns 0, which we use to abort intentionally */
		;
	else if (result == CURLE_SSL_PEER_CERTIFICATE ||
			result == CURLE_SSL_CACERT) {
		memcpy(certs, f->cert_data, sizeof(certs));
		memset(f->cert_data, 0, sizeof(f->cert_data));
		cert = true;
	}
	else {
		LOG(("Unknown cURL response code %d", result));
		error = true;
	}

	fetch_curl_stop(f);

	if (abort_fetch)
		; /* fetch was aborted: no callback */
	else if (finished)
		fetch_send_callback(FETCH_FINISHED, f->fetch_handle, 0, 0);
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
			(void) BIO_set_close(mem, BIO_NOCLOSE);
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
			(void) BIO_set_close(mem, BIO_NOCLOSE);
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
			(void) BIO_set_close(mem, BIO_NOCLOSE);
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
			(void) BIO_set_close(mem, BIO_NOCLOSE);
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

		fetch_send_callback(FETCH_CERT_ERR, f->fetch_handle,
				&ssl_certs, i);

	}
	else if (error)
		fetch_send_callback(FETCH_ERROR, f->fetch_handle,
				fetch_error_buffer, 0);

	fetch_free(f->fetch_handle);
}


/**
 * Callback function for fetch progress.
 */

int fetch_curl_progress(void *clientp, double dltotal, double dlnow,
			double ultotal, double ulnow)
{
	struct curl_fetch_info *f = (struct curl_fetch_info *) clientp;
	double percent;

	if (f->abort)
		return 0;

	if (dltotal > 0) {
		percent = dlnow * 100.0f / dltotal;
		snprintf(fetch_progress_buffer, 255,
				messages_get("Progress"),
				human_friendly_bytesize(dlnow),
				human_friendly_bytesize(dltotal));
		fetch_send_callback(FETCH_PROGRESS, f->fetch_handle,
				    fetch_progress_buffer,
				    (unsigned long) percent);
	} else {
		snprintf(fetch_progress_buffer, 255,
				messages_get("ProgressU"),
				human_friendly_bytesize(dlnow));
		fetch_send_callback(FETCH_PROGRESS, f->fetch_handle,
				    fetch_progress_buffer, 0);
	}

	return 0;
}



/**
 * Ignore everything given to it.
 *
 * Used to ignore cURL debug.
 */

int fetch_curl_ignore_debug(CURL *handle,
			    curl_infotype type,
			    char *data,
			    size_t size,
			    void *userptr)
{
	return 0;
}


/**
 * Callback function for cURL.
 */

size_t fetch_curl_data(char *data, size_t size, size_t nmemb,
		       void *_f)
{
	struct curl_fetch_info *f = _f;
	CURLcode code;

	/* ensure we only have to get this information once */
	if (!f->http_code)
	{
		code = curl_easy_getinfo(f->curl_handle, CURLINFO_HTTP_CODE,
					 &f->http_code);
		fetch_set_http_code(f->fetch_handle, f->http_code);
		assert(code == CURLE_OK);
	}

	/* ignore body if this is a 401 reply by skipping it and reset
	   the HTTP response code to enable follow up fetches */
	if (f->http_code == 401)
	{
		f->http_code = 0;
		return size * nmemb;
	}

	/*LOG(("fetch %p, size %lu", f, size * nmemb));*/

	if (f->abort || (!f->had_headers && fetch_curl_process_headers(f))) {
		f->stopped = true;
		return 0;
	}

	/* send data to the caller */
	/*LOG(("FETCH_DATA"));*/
	fetch_send_callback(FETCH_DATA, f->fetch_handle, data, size * nmemb);

	if (f->abort) {
		f->stopped = true;
		return 0;
	}

	return size * nmemb;
}


/**
 * Callback function for headers.
 *
 * See RFC 2616 4.2.
 */

size_t fetch_curl_header(char *data, size_t size, size_t nmemb,
			 void *_f)
{
	struct curl_fetch_info *f = _f;
	int i;
	size *= nmemb;

	if (f->abort) {
		f->stopped = true;
		return 0;
	}

	fetch_send_callback(FETCH_HEADER, f->fetch_handle, data, size);

#define SKIP_ST(o) for (i = (o); i < (int) size && (data[i] == ' ' || data[i] == '\t'); i++)

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
	} else if (17 < size && strncasecmp(data, "WWW-Authenticate:", 17) == 0) {
		/* extract the first Realm from WWW-Authenticate header */
		free(f->realm);
		f->realm = malloc(size);
		if (!f->realm) {
			LOG(("malloc failed"));
			return size;
		}
		SKIP_ST(17);

		while (i < (int) size - 5 &&
				strncasecmp(data + i, "realm", 5))
			i++;
		while (i < (int) size - 1 && data[++i] != '"')
			/* */;
		i++;

		if (i < (int) size) {
			strncpy(f->realm, data + i, size - i);
			f->realm[size - i] = '\0';
			for (i = size - i - 1; i >= 0 &&
					(f->realm[i] == ' ' ||
					f->realm[i] == '"' ||
					f->realm[i] == '\t' ||
					f->realm[i] == '\r' ||
					f->realm[i] == '\n'); --i)
				f->realm[i] = '\0';
		}
	} else if (11 < size && strncasecmp(data, "Set-Cookie:", 11) == 0) {
		/* extract Set-Cookie header */
		SKIP_ST(11);

		fetch_set_cookie(f->fetch_handle, &data[i]);
	}

	return size;
#undef SKIP_ST
}


/**
 * Find the status code and content type and inform the caller.
 *
 * Return true if the fetch is being aborted.
 */

bool fetch_curl_process_headers(struct curl_fetch_info *f)
{
	long http_code;
	const char *type;
	CURLcode code;
	struct stat s;
	char *url_path = 0;

	f->had_headers = true;

	if (!f->http_code)
	{
		code = curl_easy_getinfo(f->curl_handle, CURLINFO_HTTP_CODE,
					 &f->http_code);
		fetch_set_http_code(f->fetch_handle, f->http_code);
		assert(code == CURLE_OK);
	}
	http_code = f->http_code;
	LOG(("HTTP status code %li", http_code));

	if (http_code == 304 && !f->post_urlenc && !f->post_multipart) {
		/* Not Modified && GET request */
		fetch_send_callback(FETCH_NOTMODIFIED, f->fetch_handle, 0, 0);
		return true;
	}

	/* handle HTTP redirects (3xx response codes) */
	if (300 <= http_code && http_code < 400 && f->location != 0) {
		LOG(("FETCH_REDIRECT, '%s'", f->location));
		fetch_send_callback(FETCH_REDIRECT, f->fetch_handle, f->location, 0);
		return true;
	}

	/* handle HTTP 401 (Authentication errors) */
	if (http_code == 401) {
		fetch_send_callback(FETCH_AUTH, f->fetch_handle, f->realm,0);
		return true;
	}

	/* handle HTTP errors (non 2xx response codes) */
	if (f->only_2xx && strncmp(f->url, "http", 4) == 0 &&
			(http_code < 200 || 299 < http_code)) {
		fetch_send_callback(FETCH_ERROR, f->fetch_handle,
				    messages_get("Not2xx"), 0);
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
		char etag_buf[20];
		snprintf(etag_buf, sizeof etag_buf,
				"ETag: \"%10d\"", (int) s.st_mtime);
		/* And send it to the header handler */
		fetch_send_callback(FETCH_HEADER, f->fetch_handle, etag_buf,
				strlen(etag_buf));

		/* don't set last modified time so as to ensure that local
		 * files are revalidated at all times. */

		/* If performed a conditional request and unmodified ... */
		if (f->last_modified && f->file_etag &&
				f->last_modified > s.st_mtime &&
				f->file_etag == s.st_mtime) {
			fetch_send_callback(FETCH_NOTMODIFIED, f->fetch_handle,
					    0, 0);
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
	fetch_send_callback(FETCH_TYPE, f->fetch_handle, type, f->content_length);
	if (f->abort)
		return true;

	return false;
}


/**
 * Convert a list of struct ::form_successful_control to a list of
 * struct curl_httppost for libcurl.
 */
struct curl_httppost *
fetch_curl_post_convert(struct form_successful_control *control)
{
	struct curl_httppost *post = 0, *last = 0;
	CURLFORMcode code;

	for (; control; control = control->next) {
		if (control->file) {
			char *leafname = 0;
#ifdef riscos
			char *temp;
			int leaflen;

			temp = strrchr(control->value, '.');
			if (!temp)
				temp = control->value; /* already leafname */
			else
				temp += 1;

			leaflen = strlen(temp);

			leafname = malloc(leaflen + 1);
			if (!leafname) {
				LOG(("malloc failed"));
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
			/* We have to special case filenames of "", so curl
			 * a) actually attempts the fetch and
			 * b) doesn't attempt to open the file ""
			 */
			if (control->value[0] == '\0') {
				/* dummy buffer - needs to be static so
				 * pointer's still valid when we go out
				 * of scope (not that libcurl should be
				 * attempting to access it, of course). */
				static char buf;

				code = curl_formadd(&post, &last,
					CURLFORM_COPYNAME, control->name,
					CURLFORM_BUFFER, control->value,
					/* needed, as basename("") == "." */
					CURLFORM_FILENAME, "",
					CURLFORM_BUFFERPTR, &buf,
					CURLFORM_BUFFERLENGTH, 0,
					CURLFORM_CONTENTTYPE,
						"application/octet-stream",
					CURLFORM_END);
				if (code != CURL_FORMADD_OK)
					LOG(("curl_formadd: %d (%s)",
						code, control->name));
			} else {
				char *mimetype = fetch_mimetype(control->value);
				code = curl_formadd(&post, &last,
					CURLFORM_COPYNAME, control->name,
					CURLFORM_FILE, control->value,
					CURLFORM_FILENAME, leafname,
					CURLFORM_CONTENTTYPE,
					(mimetype != 0 ? mimetype : "text/plain"),
					CURLFORM_END);
				if (code != CURL_FORMADD_OK)
					LOG(("curl_formadd: %d (%s=%s)",
						code, control->name,
						control->value));
				free(mimetype);
			}
#ifdef riscos
			free(leafname);
#endif
		}
		else {
			code = curl_formadd(&post, &last,
					CURLFORM_COPYNAME, control->name,
					CURLFORM_COPYCONTENTS, control->value,
					CURLFORM_END);
			if (code != CURL_FORMADD_OK)
				LOG(("curl_formadd: %d (%s=%s)", code,
						control->name,
						control->value));
		}
	}

	return post;
}


/**
 * OpenSSL Certificate verification callback
 * Stores certificate details in fetch struct.
 */

int fetch_curl_verify_callback(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
	X509 *cert = X509_STORE_CTX_get_current_cert(x509_ctx);
	int depth = X509_STORE_CTX_get_error_depth(x509_ctx);
	int err = X509_STORE_CTX_get_error(x509_ctx);
	struct curl_fetch_info *f = X509_STORE_CTX_get_app_data(x509_ctx);

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
 * Verifies certificate chain, setting up context for fetch_curl_verify_callback
 */

int fetch_curl_cert_verify_callback(X509_STORE_CTX *x509_ctx, void *parm)
{
	int ok;

	/* Store fetch struct in context for verify callback */
	ok = X509_STORE_CTX_set_app_data(x509_ctx, parm);

	/* and verify the certificate chain */
	if (ok)
		ok = X509_verify_cert(x509_ctx);

	return ok;
}

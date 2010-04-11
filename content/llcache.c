/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
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
 * Low-level resource cache (implementation)
 */

#define _GNU_SOURCE /* For strndup. Ugh. */
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <curl/curl.h>

#include "content/fetch.h"
#include "content/llcache.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"

/** Define to enable tracing of llcache operations. */
#undef LLCACHE_TRACE

/** State of a low-level cache object fetch */
typedef enum {
	LLCACHE_FETCH_INIT,		/**< Initial state, before fetch */
	LLCACHE_FETCH_HEADERS,		/**< Fetching headers */
	LLCACHE_FETCH_DATA,		/**< Fetching object data */
	LLCACHE_FETCH_COMPLETE		/**< Fetch completed */
} llcache_fetch_state;

/** Type of low-level cache object */
typedef struct llcache_object llcache_object;

/** Handle to low-level cache object */
struct llcache_handle {
	llcache_object *object;		/**< Pointer to associated object */

	llcache_handle_callback cb;	/**< Client callback */
	void *pw;			/**< Client data */

	llcache_fetch_state state;	/**< Last known state of object fetch */
	size_t bytes;			/**< Last reported byte count */
};

/** Low-level cache object user record */
typedef struct llcache_object_user {
	/* Must be first in struct */
	llcache_handle handle;		/**< Handle data for client */

	bool iterator_target;		/**< This is the an iterator target */
	bool queued_for_delete;		/**< This user is queued for deletion */

	struct llcache_object_user *prev;	/**< Previous in list */
	struct llcache_object_user *next;	/**< Next in list */
} llcache_object_user;

/** Low-level cache object fetch context */
typedef struct {
	uint32_t flags;			/**< Fetch flags */
	char *referer;			/**< Referring URL, or NULL if none */
	llcache_post_data *post;	/**< POST data, or NULL for GET */	

	struct fetch *fetch;		/**< Fetch handle for this object */

	llcache_fetch_state state;	/**< Current state of object fetch */

	uint32_t redirect_count;	/** Count of redirects followed */
} llcache_fetch_ctx;

/** Cache control data */
typedef struct {
	time_t req_time;	/**< Time of request */
	time_t res_time;	/**< Time of response */
	time_t date;		/**< Date: response header */
	time_t expires;		/**< Expires: response header */
#define INVALID_AGE -1
	int age;		/**< Age: response header */
	int max_age;		/**< Max-Age Cache-control parameter */
	bool no_cache;		/**< No-Cache Cache-control parameter */
	char *etag;		/**< Etag: response header */
	time_t last_modified;	/**< Last-Modified: response header */
} llcache_cache_control;

/** Representation of a fetch header */
typedef struct {
	char *name;		/**< Header name */
	char *value;		/**< Header value */
} llcache_header;

/** Low-level cache object */
/** \todo Consider whether a list is a sane container */
struct llcache_object {
	llcache_object *prev;		/**< Previous in list */
	llcache_object *next;		/**< Next in list */

	char *url;			/**< Post-redirect URL for object */
	bool has_query;			/**< URL has a query segment */
  
	/** \todo We need a generic dynamic buffer object */
	uint8_t *source_data;		/**< Source data for object */
	size_t source_len;		/**< Byte length of source data */
	size_t source_alloc;		/**< Allocated size of source buffer */

	llcache_object_user *users;	/**< List of users */

	llcache_fetch_ctx fetch;	/**< Fetch context for object */

	llcache_cache_control cache;	/**< Cache control data for object */
	llcache_object *candidate;	/**< Object to use, if fetch determines
					 * that it is still fresh */
	uint32_t candidate_count;	/**< Count of objects this is a 
					 * candidate for */

	llcache_header *headers;	/**< Fetch headers */
	size_t num_headers;		/**< Number of fetch headers */
};

/** Handler for fetch-related queries */
static llcache_query_callback query_cb;
/** Data for fetch-related query handler */
static void *query_cb_pw;

/** Head of the low-level cached object list */
static llcache_object *llcache_cached_objects;
/** Head of the low-level uncached object list */
static llcache_object *llcache_uncached_objects;

static nserror llcache_object_user_new(llcache_handle_callback cb, void *pw,
		llcache_object_user **user);
static nserror llcache_object_user_destroy(llcache_object_user *user);

static nserror llcache_object_retrieve(const char *url, uint32_t flags,
		const char *referer, const llcache_post_data *post,
		uint32_t redirect_count, llcache_object **result);
static nserror llcache_object_retrieve_from_cache(const char *url, 
		uint32_t flags, const char *referer, 
		const llcache_post_data *post, uint32_t redirect_count,
		llcache_object **result);
static bool llcache_object_is_fresh(const llcache_object *object);
static nserror llcache_object_cache_update(llcache_object *object);
static nserror llcache_object_clone_cache_data(const llcache_object *source,
		llcache_object *destination, bool deep);
static nserror llcache_object_fetch(llcache_object *object, uint32_t flags,
		const char *referer, const llcache_post_data *post,
		uint32_t redirect_count);
static nserror llcache_object_refetch(llcache_object *object);

static nserror llcache_object_new(const char *url, llcache_object **result);
static nserror llcache_object_destroy(llcache_object *object);
static nserror llcache_object_add_user(llcache_object *object,
		llcache_object_user *user);
static nserror llcache_object_remove_user(llcache_object *object, 
		llcache_object_user *user);

static nserror llcache_object_add_to_list(llcache_object *object,
		llcache_object **list);
static nserror llcache_object_remove_from_list(llcache_object *object,
		llcache_object **list);
static bool llcache_object_in_list(const llcache_object *object, 
		const llcache_object *list);

static nserror llcache_object_notify_users(llcache_object *object);

static nserror llcache_object_snapshot(llcache_object *object,
		llcache_object **snapshot);

static nserror llcache_clean(void);

static nserror llcache_post_data_clone(const llcache_post_data *orig, 
		llcache_post_data **clone);

static nserror llcache_query_handle_response(bool proceed, void *cbpw);

static void llcache_fetch_callback(fetch_msg msg, void *p, const void *data, 
		unsigned long size, fetch_error_code errorcode);
static nserror llcache_fetch_redirect(llcache_object *object, 
		const char *target, llcache_object **replacement);
static nserror llcache_fetch_notmodified(llcache_object *object,
		llcache_object **replacement);
static nserror llcache_fetch_split_header(const char *data, size_t len, 
		char **name, char **value);
static nserror llcache_fetch_parse_header(llcache_object *object,
		const char *data, size_t len, char **name, char **value);
static nserror llcache_fetch_process_header(llcache_object *object, 
		const char *data, size_t len);
static nserror llcache_fetch_process_data(llcache_object *object, 
		const uint8_t *data, size_t len);
static nserror llcache_fetch_auth(llcache_object *object,
		const char *realm);
static nserror llcache_fetch_cert_error(llcache_object *object,
		const struct ssl_cert_info *certs, size_t num);


/******************************************************************************
 * Public API								      *
 ******************************************************************************/

/* See llcache.h for documentation */
nserror llcache_initialise(llcache_query_callback cb, void *pw)
{
	query_cb = cb;
	query_cb_pw = pw;

	return NSERROR_OK;
}

/* See llcache.h for documentation */
nserror llcache_poll(void)
{
	llcache_object *object;
	
	fetch_poll();
	
	/* Catch new users up with state of objects */
	for (object = llcache_cached_objects; object != NULL; 
			object = object->next) {
		llcache_object_notify_users(object);
	}

	for (object = llcache_uncached_objects; object != NULL;
			object = object->next) {
		llcache_object_notify_users(object);
	}

	/* Attempt to clean the cache */
	llcache_clean();

	return NSERROR_OK;
}

/* See llcache.h for documentation */
nserror llcache_handle_retrieve(const char *url, uint32_t flags,
		const char *referer, const llcache_post_data *post,
		llcache_handle_callback cb, void *pw,
		llcache_handle **result)
{
	nserror error;
	llcache_object_user *user;
	llcache_object *object;

	/* Can we fetch this URL at all? */
	if (fetch_can_fetch(url) == false)
		return NSERROR_NO_FETCH_HANDLER;

	/* Create a new object user */
	error = llcache_object_user_new(cb, pw, &user);
	if (error != NSERROR_OK)
		return error;

	/* Retrieve a suitable object from the cache,
	 * creating a new one if needed. */
	error = llcache_object_retrieve(url, flags, referer, post, 0, &object);
	if (error != NSERROR_OK) {
		llcache_object_user_destroy(user);
		return error;
	}

	/* Add user to object */
	llcache_object_add_user(object, user);

	*result = &user->handle;

	return NSERROR_OK;
}

/* See llcache.h for documentation */
nserror llcache_handle_change_callback(llcache_handle *handle,
		llcache_handle_callback cb, void *pw)
{
	handle->cb = cb;
	handle->pw = pw;

	return NSERROR_OK;
}

/* See llcache.h for documentation */
nserror llcache_handle_release(llcache_handle *handle)
{
	nserror error = NSERROR_OK;
	llcache_object *object = handle->object;
	llcache_object_user *user = (llcache_object_user *) handle;

	/* Remove the user from the object and destroy it */
	error = llcache_object_remove_user(object, user);
	if (error == NSERROR_OK) {
		/* Can't delete user object if it's the target of an iterator */
		if (user->iterator_target)
			user->queued_for_delete = true;
		else
			error = llcache_object_user_destroy(user);
	}

	return error; 
}

/* See llcache.h for documentation */
nserror llcache_handle_clone(llcache_handle *handle, llcache_handle **result)
{
	nserror error;
	llcache_object_user *newuser;
		
	error = llcache_object_user_new(handle->cb, handle->pw, &newuser);
	if (error == NSERROR_OK) {
		llcache_object_add_user(handle->object, newuser);
		newuser->handle.state = handle->state;
		*result = &newuser->handle;
	}
	
	return error;
}

/* See llcache.h for documentation */
nserror llcache_handle_abort(llcache_handle *handle)
{
	llcache_object_user *user = (llcache_object_user *) handle;
	llcache_object *object = handle->object, *newobject;
	nserror error = NSERROR_OK;
	bool all_alone = true;
	
	/* Determine if we are the only user */
	if (user->prev != NULL)
		all_alone = false;
	if (user->next != NULL)
		all_alone = false;
	
	if (all_alone == false) {
		/* We must snapshot this object */
		error = llcache_object_snapshot(object, &newobject);
		if (error != NSERROR_OK)
			return error;
		/* Move across to the new object */
		llcache_object_remove_user(object, user);
		llcache_object_add_user(newobject, user);
		
		/* Add new object to uncached list */
		llcache_object_add_to_list(newobject, 
				&llcache_uncached_objects);
		
		/* And use it from now on. */
		object = newobject;
	} else {
		/* We're the only user, so abort any fetch in progress */
		if (object->fetch.fetch != NULL) {
			fetch_abort(object->fetch.fetch);
			object->fetch.fetch = NULL;
		}
		
		object->fetch.state = LLCACHE_FETCH_COMPLETE;
		
		/* Invalidate cache control data */
		memset(&(object->cache), 0, sizeof(llcache_cache_control));
	}
	
	return error;
}

/* See llcache.h for documentation */
nserror llcache_handle_force_stream(llcache_handle *handle)
{
	llcache_object_user *user = (llcache_object_user *) handle;
	llcache_object *object = handle->object;

	/* Cannot stream if there are multiple users */
	if (user->prev != NULL || user->next != NULL)
		return NSERROR_OK;

	/* Forcibly uncache this object */
	if (llcache_object_in_list(object, llcache_cached_objects)) {
		llcache_object_remove_from_list(object, 
				&llcache_cached_objects);
		llcache_object_add_to_list(object, &llcache_uncached_objects);
	}

	object->fetch.flags |= LLCACHE_RETRIEVE_STREAM_DATA;

	return NSERROR_OK;
}

/* See llcache.h for documentation */
const char *llcache_handle_get_url(const llcache_handle *handle)
{
	return handle->object != NULL ? handle->object->url : NULL;
}

/* See llcache.h for documentation */
const uint8_t *llcache_handle_get_source_data(const llcache_handle *handle,
		size_t *size)
{
	*size = handle->object != NULL ? handle->object->source_len : 0;

	return handle->object != NULL ? handle->object->source_data : NULL;
}

/* See llcache.h for documentation */
const char *llcache_handle_get_header(const llcache_handle *handle, 
		const char *key)
{
	const llcache_object *object = handle->object;
	size_t i;

	if (object == NULL)
		return NULL;

	/* About as trivial as possible */
	for (i = 0; i < object->num_headers; i++) {
		if (strcasecmp(key, object->headers[i].name) == 0)
			return object->headers[i].value;
	}

	return NULL;
}

/* See llcache.h for documentation */
bool llcache_handle_references_same_object(const llcache_handle *a, 
		const llcache_handle *b)
{
	return a->object == b->object;
}

/******************************************************************************
 * Low-level cache internals						      *
 ******************************************************************************/

/**
 * Create a new object user
 *
 * \param cb	Callback routine
 * \param pw	Private data for callback
 * \param user	Pointer to location to receive result
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_object_user_new(llcache_handle_callback cb, void *pw,
		llcache_object_user **user)
{
	llcache_object_user *u = calloc(1, sizeof(llcache_object_user));
	if (u == NULL)
		return NSERROR_NOMEM;

	u->handle.cb = cb;
	u->handle.pw = pw;

#ifdef LLCACHE_TRACE
	LOG(("Created user %p (%p, %p)", u, (void *) cb, pw));
#endif

	*user = u;

	return NSERROR_OK;
}

/**
 * Destroy an object user
 *
 * \param user	User to destroy
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * \pre User is not attached to an object
 */
nserror llcache_object_user_destroy(llcache_object_user *user)
{
#ifdef LLCACHE_TRACE
	LOG(("Destroyed user %p", user));
#endif
	
	assert(user->next == NULL);
	assert(user->prev == NULL);
	
	free(user);

	return NSERROR_OK;
}

/**
 * Retrieve an object from the cache, fetching it if necessary.
 *
 * \param url	          URL of object to retrieve
 * \param flags	          Fetch flags
 * \param referer         Referring URL, or NULL if none
 * \param post	          POST data, or NULL for a GET request
 * \param redirect_count  Number of redirects followed so far
 * \param result          Pointer to location to recieve retrieved object
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_object_retrieve(const char *url, uint32_t flags,
		const char *referer, const llcache_post_data *post,
		uint32_t redirect_count, llcache_object **result)
{
	nserror error;
	llcache_object *obj;
	bool has_query;
	url_func_result res;
	struct url_components components;

#ifdef LLCACHE_TRACE
	LOG(("Retrieve %s (%x, %s, %p)", url, flags, referer, post));
#endif

	/**
	 * Caching Rules:
	 *
	 * 1) Forced fetches are never cached
	 * 2) POST requests are never cached
	 */

	/* Look for a query segment */
	res = url_get_components(url, &components);
	if (res == URL_FUNC_NOMEM)
		return NSERROR_NOMEM;

	has_query = (components.query != NULL);

	url_destroy_components(&components);

	if (flags & LLCACHE_RETRIEVE_FORCE_FETCH || post != NULL) {
		/* Create new object */
		error = llcache_object_new(url, &obj);
		if (error != NSERROR_OK)
			return error;

		/* Attempt to kick-off fetch */
		error = llcache_object_fetch(obj, flags, referer, post, 
				redirect_count);
		if (error != NSERROR_OK) {
			llcache_object_destroy(obj);
			return error;
		}

		/* Add new object to uncached list */
		llcache_object_add_to_list(obj, &llcache_uncached_objects);
	} else {
		error = llcache_object_retrieve_from_cache(url, flags, referer,
				post, redirect_count, &obj);
		if (error != NSERROR_OK)
			return error;

		/* Returned object is already in the cached list */
	}
	
	obj->has_query = has_query;

#ifdef LLCACHE_TRACE
	LOG(("Retrieved %p", obj));
#endif
	
	*result = obj;

	return NSERROR_OK;
}

/**
 * Retrieve a potentially cached object
 *
 * \param url	          URL of object to retrieve
 * \param flags	          Fetch flags
 * \param referer         Referring URL, or NULL if none
 * \param post	          POST data, or NULL for a GET request
 * \param redirect_count  Number of redirects followed so far
 * \param result          Pointer to location to recieve retrieved object
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_object_retrieve_from_cache(const char *url, uint32_t flags,
		const char *referer, const llcache_post_data *post,
		uint32_t redirect_count, llcache_object **result)
{
	nserror error;
	llcache_object *obj, *newest = NULL;

#ifdef LLCACHE_TRACE
	LOG(("Searching cache for %s (%x %s %p)", url, flags, referer, post));
#endif

	/* Search for the most recently fetched matching object */
	for (obj = llcache_cached_objects; obj != NULL; obj = obj->next) {
		if (strcasecmp(obj->url, url) == 0 && (newest == NULL ||
				obj->cache.req_time > newest->cache.req_time))
			newest = obj;
	}

	if (newest != NULL && llcache_object_is_fresh(newest)) {
		/* Found a suitable object, and it's still fresh, so use it */
		obj = newest;

#ifdef LLCACHE_TRACE
		LOG(("Found fresh %p", obj));
#endif

		/* The client needs to catch up with the object's state.
		 * This will occur the next time that llcache_poll is called.
		 */
	} else if (newest != NULL) {
		/* Found a candidate object but it needs freshness validation */

		/* Create a new object */
		error = llcache_object_new(url, &obj);
		if (error != NSERROR_OK)
			return error;

#ifdef LLCACHE_TRACE
		LOG(("Found candidate %p (%p)", obj, newest));
#endif

		/* Clone candidate's cache data */
		error = llcache_object_clone_cache_data(newest, obj, true);
		if (error != NSERROR_OK) {
			llcache_object_destroy(obj);
			return error;
		}			

		/* Record candidate, so we can fall back if it is still fresh */
		newest->candidate_count++;
		obj->candidate = newest;

		/* Attempt to kick-off fetch */
		error = llcache_object_fetch(obj, flags, referer, post,
				redirect_count);
		if (error != NSERROR_OK) {
			newest->candidate_count--;
			llcache_object_destroy(obj);
			return error;
		}

		/* Add new object to cache */
		llcache_object_add_to_list(obj, &llcache_cached_objects);
	} else {
		/* No object found; create a new one */
		/* Create new object */
		error = llcache_object_new(url, &obj);
		if (error != NSERROR_OK)
			return error;

#ifdef LLCACHE_TRACE
		LOG(("Not found %p", obj));
#endif

		/* Attempt to kick-off fetch */
		error = llcache_object_fetch(obj, flags, referer, post,
				redirect_count);
		if (error != NSERROR_OK) {
			llcache_object_destroy(obj);
			return error;
		}

		/* Add new object to cache */
		llcache_object_add_to_list(obj, &llcache_cached_objects);
	}

	*result = obj;

	return NSERROR_OK;
}

/**
 * Determine if an object is still fresh
 *
 * \param object  Object to consider
 * \return True if object is still fresh, false otherwise
 */
bool llcache_object_is_fresh(const llcache_object *object)
{
	const llcache_cache_control *cd = &object->cache;
	int current_age, freshness_lifetime;
	time_t now = time(NULL);

	/* Calculate staleness of cached object as per RFC 2616 13.2.3/13.2.4 */
	current_age = max(0, (cd->res_time - cd->date));
	current_age = max(current_age, (cd->age == INVALID_AGE) ? 0 : cd->age);
	current_age += cd->res_time - cd->req_time + now - cd->res_time;

	/* Determine freshness lifetime of this object */
	if (cd->max_age != INVALID_AGE)
		freshness_lifetime = cd->max_age;
	else if (cd->expires != 0)
		freshness_lifetime = cd->expires - cd->date;
	else if (cd->last_modified != 0)
		freshness_lifetime = (now - cd->last_modified) / 10;
	else
		freshness_lifetime = 0;

#ifdef LLCACHE_TRACE
	LOG(("%p: (%d > %d || %d != %d)", object, 
			freshness_lifetime, current_age,
			object->fetch.state, LLCACHE_FETCH_COMPLETE));
#endif

	/* The object is fresh if its current age is within the freshness 
	 * lifetime or if we're still fetching the object */
	return (freshness_lifetime > current_age || 
			object->fetch.state != LLCACHE_FETCH_COMPLETE);
}

/**
 * Update an object's cache state
 *
 * \param object  Object to update cache for
 * \return NSERROR_OK.
 */
nserror llcache_object_cache_update(llcache_object *object)
{
	if (object->cache.date == 0)
		object->cache.date = time(NULL);

	return NSERROR_OK;
}

/**
 * Clone an object's cache data
 *
 * \param source       Source object containing cache data to clone
 * \param destination  Destination object to clone cache data into
 * \param deep	       Whether to deep-copy the data or not
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_object_clone_cache_data(const llcache_object *source,
		llcache_object *destination, bool deep)
{
	/* ETag must be first, as it can fail when deep cloning */
	if (source->cache.etag != NULL) {
		char *etag = source->cache.etag;

		if (deep) {
			/* Copy the etag */
			etag = strdup(source->cache.etag);
			if (etag == NULL)
				return NSERROR_NOMEM;
		}

		if (destination->cache.etag != NULL)
			free(destination->cache.etag);

		destination->cache.etag = etag;
	}

	destination->cache.req_time = source->cache.req_time;
	destination->cache.res_time = source->cache.res_time;

	if (source->cache.date != 0)
		destination->cache.date = source->cache.date;

	if (source->cache.expires != 0)
		destination->cache.expires = source->cache.expires;

	if (source->cache.age != INVALID_AGE)
		destination->cache.age = source->cache.age;

	if (source->cache.max_age != INVALID_AGE)
		destination->cache.max_age = source->cache.max_age;

	if (source->cache.no_cache)
		destination->cache.no_cache = source->cache.no_cache;
	
	if (source->cache.last_modified != 0)
		destination->cache.last_modified = source->cache.last_modified;

	return NSERROR_OK;
}

/**
 * Kick-off a fetch for an object
 *
 * \param object          Object to fetch
 * \param flags	          Fetch flags
 * \param referer         Referring URL, or NULL for none
 * \param post	          POST data, or NULL for GET
 * \param redirect_count  Number of redirects followed so far
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * \pre object::url must contain the URL to fetch
 * \pre If there is a freshness validation candidate, 
 *	object::candidate and object::cache must be filled in
 * \pre There must not be a fetch in progress for \a object
 */
nserror llcache_object_fetch(llcache_object *object, uint32_t flags,
		const char *referer, const llcache_post_data *post,
		uint32_t redirect_count)
{
	nserror error;
	char *referer_clone = NULL;
	llcache_post_data *post_clone = NULL;

#ifdef LLCACHE_TRACE
	LOG(("Starting fetch for %p", object));
#endif

	if (referer != NULL) {
		referer_clone = strdup(referer);
		if (referer_clone == NULL)
			return NSERROR_NOMEM;
	}

	if (post != NULL) {
		error = llcache_post_data_clone(post, &post_clone);
		if (error != NSERROR_OK) {
			free(referer_clone);
			return error;
		}
	}

	object->fetch.flags = flags;
	object->fetch.referer = referer_clone;
	object->fetch.post = post_clone;
	object->fetch.redirect_count = redirect_count;

	return llcache_object_refetch(object);
}

/**
 * (Re)fetch an object
 *
 * \param object  Object to refetch
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * \pre The fetch parameters in object->fetch must be populated
 */ 
nserror llcache_object_refetch(llcache_object *object)
{
	const char *urlenc = NULL;
	struct fetch_multipart_data *multipart = NULL;
	char **headers = NULL;
	int header_idx = 0;

	if (object->fetch.post != NULL) {
		if (object->fetch.post->type == LLCACHE_POST_URL_ENCODED)
			urlenc = object->fetch.post->data.urlenc;
		else
			multipart = object->fetch.post->data.multipart;
	}

	/* Generate cache-control headers */
	headers = malloc(3 * sizeof(char *));
	if (headers == NULL)
		return NSERROR_NOMEM;

	if (object->cache.etag != NULL) {
		const size_t len = SLEN("If-None-Match: ") + 
				strlen(object->cache.etag) + 1;

		headers[header_idx] = malloc(len);
		if (headers[header_idx] == NULL) {
			free(headers);
			return NSERROR_NOMEM;
		}

		snprintf(headers[header_idx], len, "If-None-Match: %s",
				object->cache.etag);

		header_idx++;
	}
	if (object->cache.date != 0) {
		/* Maximum length of an RFC 1123 date is 29 bytes */
		const size_t len = SLEN("If-Modified-Since: ") + 29 + 1;

		headers[header_idx] = malloc(len);
		if (headers[header_idx] == NULL) {
			while (--header_idx >= 0)
				free(headers[header_idx]);
			free(headers);
			return NSERROR_NOMEM;
		}

		snprintf(headers[header_idx], len, "If-Modified-Since: %s",
				rfc1123_date(object->cache.date));

		header_idx++;
	}
	headers[header_idx] = NULL;

	/* Reset cache control data */
	object->cache.req_time = time(NULL);
	object->cache.res_time = 0;
	object->cache.date = 0;
	object->cache.expires = 0;
	object->cache.age = INVALID_AGE;
	object->cache.max_age = INVALID_AGE;
	object->cache.no_cache = false;
	free(object->cache.etag);
	object->cache.etag = NULL;
	object->cache.last_modified = 0;

#ifdef LLCACHE_TRACE
	LOG(("Refetching %p", object));
#endif

	/* Kick off fetch */
	object->fetch.fetch = fetch_start(object->url, object->fetch.referer,
			llcache_fetch_callback, object,
			object->fetch.flags & LLCACHE_RETRIEVE_NO_ERROR_PAGES,
			urlenc, multipart,
			object->fetch.flags & LLCACHE_RETRIEVE_VERIFIABLE,
			(const char **) headers);

	/* Clean up cache-control headers */
	while (--header_idx >= 0)
		free(headers[header_idx]);
	free(headers);

	/* Did we succeed in creating a fetch? */
	if (object->fetch.fetch == NULL)
		return NSERROR_NOMEM;

	return NSERROR_OK;
}

/**
 * Create a new low-level cache object
 *
 * \param url	  URL of object to create
 * \param result  Pointer to location to receive result
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_object_new(const char *url, llcache_object **result)
{
	llcache_object *obj = calloc(1, sizeof(llcache_object));
	if (obj == NULL)
		return NSERROR_NOMEM;

#ifdef LLCACHE_TRACE
	LOG(("Created object %p (%s)", obj, url));
#endif

	obj->url = strdup(url);
	if (obj->url == NULL) {
		free(obj);
		return NSERROR_NOMEM;
	}

	*result = obj;

	return NSERROR_OK;
}

/**
 * Destroy a low-level cache object
 *
 * \param object  Object to destroy
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * \pre Object is detached from cache list
 * \pre Object has no users
 * \pre Object is not a candidate (i.e. object::candidate_count == 0)
 */
nserror llcache_object_destroy(llcache_object *object)
{
	size_t i;

#ifdef LLCACHE_TRACE
	LOG(("Destroying object %p", object));
#endif

	free(object->url);
	free(object->source_data);

	if (object->fetch.fetch != NULL) {
		fetch_abort(object->fetch.fetch);
		object->fetch.fetch = NULL;
	}

	free(object->fetch.referer);

	if (object->fetch.post != NULL) {
		if (object->fetch.post->type == LLCACHE_POST_URL_ENCODED) {
			free(object->fetch.post->data.urlenc);
		} else {
			fetch_multipart_data_destroy(
					object->fetch.post->data.multipart);
		}

		free(object->fetch.post);
	}

	free(object->cache.etag);

	for (i = 0; i < object->num_headers; i++) {
		free(object->headers[i].name);
		free(object->headers[i].value);
	}
	free(object->headers);

	free(object);

	return NSERROR_OK;
}

/**
 * Add a user to a low-level cache object
 *
 * \param object  Object to add user to
 * \param user	  User to add
 * \return NSERROR_OK.
 */
nserror llcache_object_add_user(llcache_object *object,
		llcache_object_user *user)
{
	assert(user->next == NULL);
	assert(user->prev == NULL);

	user->handle.object = object;

	user->prev = NULL;
	user->next = object->users;

	if (object->users != NULL)
		object->users->prev = user;
	object->users = user;

#ifdef LLCACHE_TRACE
	LOG(("Adding user %p to %p", user, object));
#endif

	return NSERROR_OK;
}

/**
 * Remove a user from a low-level cache object
 *
 * \param object  Object to remove user from
 * \param user	  User to remove
 * \return NSERROR_OK.
 */
nserror llcache_object_remove_user(llcache_object *object, 
		llcache_object_user *user)
{
	assert(object->users != NULL);
	assert(user->handle.object = object);
	assert((user->next != NULL) || (user->prev != NULL) || (object->users == user));
	
	if (user == object->users)
		object->users = user->next;
	else
		user->prev->next = user->next;

	if (user->next != NULL)
		user->next->prev = user->prev;
	
#ifndef NDEBUG
	user->next = user->prev = NULL;
#endif
	
#ifdef LLCACHE_TRACE
	LOG(("Removing user %p from %p", user, object));
#endif

	return NSERROR_OK;
}

/**
 * Add a low-level cache object to a cache list
 *
 * \param object  Object to add
 * \param list	  List to add to
 * \return NSERROR_OK
 */
nserror llcache_object_add_to_list(llcache_object *object,
		llcache_object **list)
{
	object->prev = NULL;
	object->next = *list;

	if (*list != NULL)
		(*list)->prev = object;
	*list = object;

	return NSERROR_OK;
}

/**
 * Remove a low-level cache object from a cache list
 *
 * \param object  Object to remove
 * \param list	  List to remove from
 * \return NSERROR_OK
 */
nserror llcache_object_remove_from_list(llcache_object *object,
		llcache_object **list)
{
	if (object == *list)
		*list = object->next;
	else
		object->prev->next = object->next;

	if (object->next != NULL)
		object->next->prev = object->prev;

	return NSERROR_OK;
}

/**
 * Determine if a low-level cache object resides in a given list
 *
 * \param object  Object to search for
 * \param list	  List to search in
 * \return True if object resides in list, false otherwise
 */
bool llcache_object_in_list(const llcache_object *object,
		const llcache_object *list)
{
	while (list != NULL) {
		if (list == object)
			break;

		list = list->next;
	}

	return list != NULL;
}

/**
 * Notify users of an object's current state
 *
 * \param object  Object to notify users about
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_object_notify_users(llcache_object *object)
{
	nserror error;
	llcache_object_user *user, *next_user;
	llcache_event event;

#ifdef LLCACHE_TRACE
	LOG(("Notifying users of %p", object));
#endif

	/**
	 * State transitions and event emission for users. 
	 * Rows: user state. Cols: object state.
	 *
	 * User\Obj	INIT	HEADERS		DATA	COMPLETE
	 * INIT		 -	   T		 T*	   T*
	 * HEADERS	 -	   -		 T	   T*
	 * DATA		 -	   -		 M	   T
	 * COMPLETE	 -	   -		 -	   -
	 *
	 * T => transition user to object state
	 * M => no transition required, but may need to emit event
	 *
	 * The transitions marked with an asterisk can be removed by moving
	 * the user context into the subsequent state and then reevaluating.
	 *
	 * Events are issued as follows:
	 *
	 * HAD_HEADERS: on transition from HEADERS -> DATA state
	 * HAD_DATA   : in DATA state, whenever there's new source data
	 * DONE	      : on transition from DATA -> COMPLETE state
	 */

	for (user = object->users; user != NULL; user = next_user) {
		/* Emit necessary events to bring the user up-to-date */
		llcache_handle *handle = &user->handle;
		const llcache_fetch_state objstate = object->fetch.state;

		/* Save identity of next user in case client destroys 
		 * the user underneath us */
		user->iterator_target = true;
		next_user = user->next;

#ifdef LLCACHE_TRACE
		if (handle->state != objstate)
			LOG(("User %p state: %d Object state: %d", 
					user, handle->state, objstate));
#endif

		/* User: INIT, Obj: HEADERS, DATA, COMPLETE => User->HEADERS */
		if (handle->state == LLCACHE_FETCH_INIT && 
				objstate > LLCACHE_FETCH_INIT) {
			handle->state = LLCACHE_FETCH_HEADERS;
		}

		/* User: HEADERS, Obj: DATA, COMPLETE => User->DATA */
		if (handle->state == LLCACHE_FETCH_HEADERS &&
				objstate > LLCACHE_FETCH_HEADERS) {
			handle->state = LLCACHE_FETCH_DATA;

			/* Emit HAD_HEADERS event */
			event.type = LLCACHE_EVENT_HAD_HEADERS;

			error = handle->cb(handle, &event, handle->pw);
			if (error != NSERROR_OK) {
				user->iterator_target = false;
				return error;
			}

			if (user->queued_for_delete) {
				llcache_object_user_destroy(user);
				continue;
			}
		}

		/* User: DATA, Obj: DATA, COMPLETE, more source available */
		if (handle->state == LLCACHE_FETCH_DATA &&
				objstate >= LLCACHE_FETCH_DATA &&
				object->source_len > handle->bytes) {
			/* Construct HAD_DATA event */
			event.type = LLCACHE_EVENT_HAD_DATA;
			event.data.data.buf = 
					object->source_data + handle->bytes;
			event.data.data.len = 
					object->source_len - handle->bytes;

			/* Update record of last byte emitted */
			if (object->fetch.flags & 
					LLCACHE_RETRIEVE_STREAM_DATA) {
				/* Streaming, so reset to zero to 
				 * minimise amount of cached source data */
				handle->bytes = object->source_len = 0;
			} else {
				handle->bytes = object->source_len;
			}

			/* Emit event */
			error = handle->cb(handle, &event, handle->pw);
			if (error != NSERROR_OK) {
				user->iterator_target = false;
				return error;
			}

			if (user->queued_for_delete) {
				llcache_object_user_destroy(user);
				continue;
			}
		}

		/* User: DATA, Obj: COMPLETE => User->COMPLETE */
		if (handle->state == LLCACHE_FETCH_DATA &&
				objstate > LLCACHE_FETCH_DATA) {
			handle->state = LLCACHE_FETCH_COMPLETE;

			/* Emit DONE event */
			event.type = LLCACHE_EVENT_DONE;

			error = handle->cb(handle, &event, handle->pw);
			if (error != NSERROR_OK) {
				user->iterator_target = false;
				return error;
			}

			if (user->queued_for_delete) {
				llcache_object_user_destroy(user);
				continue;
			}
		}

		/* No longer the target of an iterator */
		user->iterator_target = false;
	}

	return NSERROR_OK;
}

/**
 * Make a snapshot of the current state of an llcache_object.
 *
 * This has the side-effect of the new object being non-cacheable,
 * also not-fetching and not a candidate for any other object.
 *
 * Also note that this new object has no users and at least one
 * should be assigned to it before llcache_clean is entered or it
 * will be immediately cleaned up.
 *
 * \param object  The object to take a snapshot of
 * \param snapshot  Pointer to receive snapshot of \a object
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_object_snapshot(llcache_object *object,
		llcache_object **snapshot)
{
	llcache_object *newobj;
	nserror error;
	
	error = llcache_object_new(object->url, &newobj);
	
	if (error != NSERROR_OK)
		return error;
	
	newobj->has_query = object->has_query;

	newobj->source_alloc = newobj->source_len = object->source_len;
	
	if (object->source_len > 0) {
		newobj->source_data = malloc(newobj->source_alloc);
		if (newobj->source_data == NULL) {
			llcache_object_destroy(newobj);
			return NSERROR_NOMEM;
		}
		memcpy(newobj->source_data, object->source_data, newobj->source_len);
	}
	
	if (object->num_headers > 0) {
		newobj->headers = calloc(sizeof(llcache_header), object->num_headers);
		if (newobj->headers == NULL) {
			llcache_object_destroy(newobj);
			return NSERROR_NOMEM;
		}
		while (newobj->num_headers < object->num_headers) {
			llcache_header *nh = &(newobj->headers[newobj->num_headers]);
			llcache_header *oh = &(object->headers[newobj->num_headers]);
			newobj->num_headers += 1;
			nh->name = strdup(oh->name);
			nh->value = strdup(oh->value);
			if (nh->name == NULL || nh->value == NULL) {
				llcache_object_destroy(newobj);
				return NSERROR_NOMEM;
			}
		}
	}
	
	newobj->fetch.state = LLCACHE_FETCH_COMPLETE;
	
	*snapshot = newobj;
	
	return NSERROR_OK;
}

/**
 * Attempt to clean the cache
 *
 * \return NSERROR_OK.
 */
nserror llcache_clean(void)
{
	llcache_object *object, *next;

#ifdef LLCACHE_TRACE
	LOG(("Attempting cache clean"));
#endif

	/* Candidates for cleaning are (in order of priority):
	 * 
	 * 1) Uncacheable objects with no users
	 * 2) Stale cacheable objects with no users or pending fetches
	 * 3) Fresh cacheable objects with no users or pending fetches
	 */

	/* 1) Uncacheable objects with no users or fetches */
	for (object = llcache_uncached_objects; object != NULL; object = next) {
		next = object->next;

		/* The candidate count of uncacheable objects is always 0 */
		if (object->users == NULL && object->candidate_count == 0 &&
				object->fetch.fetch == NULL) {
#ifdef LLCACHE_TRACE
			LOG(("Found victim %p", object));
#endif
			llcache_object_remove_from_list(object, 
					&llcache_uncached_objects);
			llcache_object_destroy(object);
		}
	}

	/* 2) Stale cacheable objects with no users or pending fetches */
	for (object = llcache_cached_objects; object != NULL; object = next) {
		next = object->next;

		if (object->users == NULL && object->candidate_count == 0 &&
				llcache_object_is_fresh(object) == false &&
				object->fetch.fetch == NULL) {
#ifdef LLCACHE_TRACE
			LOG(("Found victim %p", object));
#endif
			llcache_object_remove_from_list(object,
					&llcache_cached_objects);
			llcache_object_destroy(object);
		}
	}

	/* 3) Fresh cacheable objects with no users or pending fetches */
	/** \todo This one only happens if the cache is too large */

	return NSERROR_OK;
}

/**
 * Clone a POST data object
 *
 * \param orig	 Object to clone
 * \param clone	 Pointer to location to receive clone
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_post_data_clone(const llcache_post_data *orig, 
		llcache_post_data **clone)
{
	llcache_post_data *post_clone;

	post_clone = calloc(1, sizeof(llcache_post_data));
	if (post_clone == NULL)
		return NSERROR_NOMEM;

	post_clone->type = orig->type;

	/* Deep-copy the type-specific data */
	if (orig->type == LLCACHE_POST_URL_ENCODED) {
		post_clone->data.urlenc = strdup(orig->data.urlenc);
		if (post_clone->data.urlenc == NULL) {
			free(post_clone);

			return NSERROR_NOMEM;
		}
	} else {
		post_clone->data.multipart = fetch_multipart_data_clone(
				orig->data.multipart);
		if (post_clone->data.multipart == NULL) {
			free(post_clone);

			return NSERROR_NOMEM;
		}
	}

	*clone = post_clone;

	return NSERROR_OK;
}

/**
 * Handle a query response
 *
 * \param proceed  Whether to proceed with fetch
 * \param cbpw	   Our context for query
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_query_handle_response(bool proceed, void *cbpw)
{
	nserror error;
	llcache_event event;
	llcache_object_user *user;
	llcache_object *object = cbpw;

	/* Refetch, using existing fetch parameters, if client allows us to */
	if (proceed)
		return llcache_object_refetch(object);

	/* Inform client(s) that object fetch failed */
	event.type = LLCACHE_EVENT_ERROR;
	/** \todo More appropriate error message */
	event.data.msg = messages_get("FetchFailed");

	for (user = object->users; user != NULL; user = user->next) {
		error = user->handle.cb(&user->handle, &event, user->handle.pw);
		if (error != NSERROR_OK)
			return error;
	}

	return NSERROR_OK;
}

/**
 * Handler for fetch events
 *
 * \param msg	     Type of fetch event
 * \param p	     Our private data
 * \param data	     Event data
 * \param size	     Length of data in bytes
 * \param errorcode  Reason for fetch error
 */
void llcache_fetch_callback(fetch_msg msg, void *p, const void *data, 
		unsigned long size, fetch_error_code errorcode)
{
	nserror error = NSERROR_OK;
	llcache_object *object = p;
	llcache_object_user *user;
	llcache_event event;

#ifdef LLCACHE_TRACE
	LOG(("Fetch event %d for %p", msg, object));
#endif

	switch (msg) {
	/* 3xx responses */
	case FETCH_REDIRECT:
		/* Request resulted in a redirect */
		error = llcache_fetch_redirect(object, data, &object);
		break;
	case FETCH_NOTMODIFIED:
		/* Conditional request determined that cached object is fresh */
		error = llcache_fetch_notmodified(object, &object);
		break;

	/* Normal 2xx state machine */
	case FETCH_HEADER:
		/* Received a fetch header */
		object->fetch.state = LLCACHE_FETCH_HEADERS;

		error = llcache_fetch_process_header(object, data, size);
		break;
	case FETCH_DATA:
		/* Received some data */
		object->fetch.state = LLCACHE_FETCH_DATA;
		if (object->has_query && (object->cache.expires == 0 && 
				object->cache.max_age == INVALID_AGE)) {
			/* URI had query string and did not provide an explicit
			 * expiration time, thus by rfc2616 13.9 we must 
			 * invalidate the cache data to force the cache to not 
			 * retain the object.
			 */
			memset(&(object->cache), 0, 
					sizeof(llcache_cache_control));
		}
		error = llcache_fetch_process_data(object, data, size);
		break;
	case FETCH_FINISHED:
		/* Finished fetching */
		object->fetch.state = LLCACHE_FETCH_COMPLETE;
		object->fetch.fetch = NULL;

		llcache_object_cache_update(object);
		break;

	/* Out-of-band information */
	case FETCH_ERROR:
		/* An error occurred while fetching */
		/* The fetch has has already been cleaned up by the fetcher */
		object->fetch.fetch = NULL;

		/* Invalidate cache control data */
		memset(&(object->cache), 0, sizeof(llcache_cache_control));

		/** \todo Consider using errorcode for something */

		event.type = LLCACHE_EVENT_ERROR;
		event.data.msg = data;

		for (user = object->users; user != NULL; user = user->next) {
			error = user->handle.cb(&user->handle, &event,
					user->handle.pw);
			if (error != NSERROR_OK)
				break;
		}
		break;
	case FETCH_PROGRESS:
		/* Progress update */
		event.type = LLCACHE_EVENT_PROGRESS;
		event.data.msg = data;

		for (user = object->users; user != NULL; user = user->next) {
			error = user->handle.cb(&user->handle, &event, 
					user->handle.pw);
			if (error != NSERROR_OK)
				break;
		}
		break;

	/* Events requiring action */
	case FETCH_AUTH:
		/* Need Authentication */
		error = llcache_fetch_auth(object, data);
		break;
	case FETCH_CERT_ERR:
		/* Something went wrong when validating TLS certificates */
		error = llcache_fetch_cert_error(object, data, size);
		break;
	}

	/* Deal with any errors reported by event handlers */
	if (error != NSERROR_OK) {
		if (object->fetch.fetch != NULL) {
			fetch_abort(object->fetch.fetch);
			object->fetch.fetch = NULL;
		}
		return;
	}
}

/**
 * Handle FETCH_REDIRECT event
 *
 * \param object       Object being redirected
 * \param target       Target of redirect (may be relative)
 * \param replacement  Pointer to location to receive replacement object
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_fetch_redirect(llcache_object *object, const char *target,
		llcache_object **replacement)
{
	nserror error;
	llcache_object *dest;
	llcache_object_user *user, *next;
	const llcache_post_data *post = object->fetch.post;
	char *url, *absurl;
	char *scheme;
	url_func_result result;
	/* Extract HTTP response code from the fetch object */
	long http_code = fetch_http_code(object->fetch.fetch);

	/* Abort fetch for this object */
	fetch_abort(object->fetch.fetch);
	object->fetch.fetch = NULL;
	
	/* Invalidate the cache control data */
	memset(&(object->cache), 0, sizeof(llcache_cache_control));
	/* And mark it complete */
	object->fetch.state = LLCACHE_FETCH_COMPLETE;
	
	/* Forcibly stop redirecting if we've followed too many redirects */
#define REDIRECT_LIMIT 10
	if (object->fetch.redirect_count > REDIRECT_LIMIT) {
		llcache_event event;

		LOG(("Too many nested redirects"));

		event.type = LLCACHE_EVENT_ERROR;
		event.data.msg = messages_get("BadRedirect");

		for (user = object->users; user != NULL; user = user->next) {
			error = user->handle.cb(&user->handle, &event,
					user->handle.pw);
			if (error != NSERROR_OK)
				break;
		}

		return NSERROR_OK;
	}
#undef REDIRECT_LIMIT

	/* Make target absolute */
	result = url_join(target, object->url, &absurl);
	if (result != URL_FUNC_OK) {
		return NSERROR_NOMEM;
	}

	/* Ensure target is normalised */
	result = url_normalize(absurl, &url);

	/* No longer require absolute url */
	free(absurl);

	if (result != URL_FUNC_OK) {
		return NSERROR_NOMEM;
	}

	/* Ensure that redirects to file:/// don't happen */
	result = url_scheme(url, &scheme);
	if (result != URL_FUNC_OK) {
		free(url);
		return NSERROR_NOMEM;
	}

	if (strcasecmp(scheme, "file") == 0) {
		free(scheme);
		free(url);
		return NSERROR_OK;
	}

	free(scheme);

	/* Bail out if we've no way of handling this URL */
	if (fetch_can_fetch(url) == false) {
		free(url);
		return NSERROR_OK;
	}

	if (http_code == 301 || http_code == 302 || http_code == 303) {
		/* 301, 302, 303 redirects are all unconditional GET requests */
		post = NULL;
	} else if (http_code != 307 || post != NULL) {
		/** \todo 300, 305, 307 with POST */
		free(url);
		return NSERROR_OK;
	}

	/* Attempt to fetch target URL */
	error = llcache_object_retrieve(url, object->fetch.flags,
			object->fetch.referer, post, 
			object->fetch.redirect_count + 1, &dest);

	/* No longer require url */
	free(url);

	if (error != NSERROR_OK)
		return error;

	/* Move user(s) to replacement object */
	for (user = object->users; user != NULL; user = next) {
		next = user->next;

		llcache_object_remove_user(object, user);
		llcache_object_add_user(dest, user);
	}

	/* Dest is now our object */
	*replacement = dest;

	return NSERROR_OK;	
}

/**
 * Handle FETCH_NOTMODIFIED event
 *
 * \param object       Object to process
 * \param replacement  Pointer to location to receive replacement object
 * \return NSERROR_OK.
 */
nserror llcache_fetch_notmodified(llcache_object *object,
		llcache_object **replacement)
{
	llcache_object_user *user, *next;

	/* Move user(s) to candidate content */
	for (user = object->users; user != NULL; user = next) {
		next = user->next;

		llcache_object_remove_user(object, user);
		llcache_object_add_user(object->candidate, user);
	}

	/* Candidate is no longer a candidate for us */
	object->candidate->candidate_count--;

	/* Clone our cache control data into the candidate */
	llcache_object_clone_cache_data(object, object->candidate, false);
	/* Bring candidate's cache data up to date */
	llcache_object_cache_update(object->candidate);

	/* Invalidate our cache-control data */
	memset(&object->cache, 0, sizeof(llcache_cache_control));

	/* Ensure fetch has stopped */
	fetch_abort(object->fetch.fetch);
	object->fetch.fetch = NULL;

	/* Candidate is now our object */
	*replacement = object->candidate;

	/* Old object will be flushed from the cache on the next poll */

	return NSERROR_OK;
}

/**
 * Split a fetch header into name and value
 *
 * \param data	 Header string
 * \param len	 Byte length of header
 * \param name	 Pointer to location to receive header name
 * \param value	 Pointer to location to receive header value
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_fetch_split_header(const char *data, size_t len, char **name,
		char **value)
{
	char *n, *v;
	const char *colon;

	/* Find colon */
	colon = strchr(data, ':');
	if (colon == NULL) {
		/* Failed, assume a key with no value */
		n = strdup(data);
		if (n == NULL)
			return NSERROR_NOMEM;

		v = strdup("");
		if (v == NULL) {
			free(n);
			return NSERROR_NOMEM;
		}
	} else {
		/* Split header into name & value */

		/* Strip leading whitespace from name */
		while (data[0] == ' ' || data[0] == '\t' ||
				data[0] == '\r' || data[0] == '\n') {
			data++;
		}

		/* Strip trailing whitespace from name */
		while (colon > data && (colon[-1] == ' ' || 
				colon[-1] == '\t' || colon[-1] == '\r' || 
				colon[-1] == '\n'))
			colon--;

		n = strndup(data, colon - data);
		if (n == NULL)
			return NSERROR_NOMEM;

		/* Find colon again */
		while (*colon != ':') {
			colon++;
		}

		/* Skip over colon and any subsequent whitespace */
		do {
			colon++;
		} while (*colon == ' ' || *colon == '\t' || 
				*colon == '\r' || *colon == '\n');

		/* Strip trailing whitespace from value */
		while (len > 0 && (data[len - 1] == ' ' || 
				data[len - 1] == '\t' || 
				data[len - 1] == '\r' ||
				data[len - 1] == '\n')) {
			len--;
		}

		v = strndup(colon, len - (colon - data));
		if (v == NULL) {
			free(n);
			return NSERROR_NOMEM;
		}
	}

	*name = n;
	*value = v;

	return NSERROR_OK;
}

/**
 * Parse a fetch header
 *
 * \param object  Object to parse header for
 * \param data	  Header string
 * \param len	  Byte length of header
 * \param name	  Pointer to location to receive header name
 * \param value	  Pointer to location to receive header value
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_fetch_parse_header(llcache_object *object, const char *data, 
		size_t len, char **name, char **value)
{
	nserror error;

	/* Set fetch response time if not already set */
	if (object->cache.res_time == 0)
		object->cache.res_time = time(NULL);

	/* Decompose header into name-value pair */
	error = llcache_fetch_split_header(data, len, name, value);
	if (error != NSERROR_OK)
		return error;

	/* Parse cache headers to populate cache control data */
#define SKIP_ST(p) while (*p != '\0' && (*p == ' ' || *p == '\t')) p++

	if (5 < len && strcasecmp(*name, "Date") == 0) {
		/* extract Date header */
		object->cache.date = curl_getdate(*value, NULL);
	} else if (4 < len && strcasecmp(*name, "Age") == 0) {
		/* extract Age header */
		if ('0' <= **value && **value <= '9')
			object->cache.age = atoi(*value);
	} else if (8 < len && strcasecmp(*name, "Expires") == 0) {
		/* extract Expires header */
		object->cache.expires = curl_getdate(*value, NULL);
	} else if (14 < len && strcasecmp(*name, "Cache-Control") == 0) {
		/* extract and parse Cache-Control header */
		const char *start = *value;
		const char *comma = *value;

		while (*comma != '\0') {
			while (*comma != '\0' && *comma != ',')
				comma++;

			if (8 < comma - start && (strncasecmp(start, 
					"no-cache", 8) == 0 || 
					strncasecmp(start, "no-store", 8) == 0))
				/* When we get a disk cache we should
				 * distinguish between these two */
				object->cache.no_cache = true;
			else if (7 < comma - start && 
					strncasecmp(start, "max-age", 7) == 0) {
				/* Find '=' */
				while (start < comma && *start != '=')
					start++;

				/* Skip over it */
				start++;

				/* Skip whitespace */
				SKIP_ST(start);

				if (start < comma)
					object->cache.max_age = atoi(start);
			}

			if (*comma != '\0') {
				/* Skip past comma */
				comma++;
				/* Skip whitespace */
				SKIP_ST(comma);
			}

			/* Set start for next token */
			start = comma;
		}
	} else if (5 < len && strcasecmp(*name, "ETag") == 0) {
		/* extract ETag header */
		free(object->cache.etag);
		object->cache.etag = strdup(*value);
		if (object->cache.etag == NULL)
			return NSERROR_NOMEM;
	} else if (14 < len && strcasecmp(*name, "Last-Modified") == 0) {
		/* extract Last-Modified header */
		object->cache.last_modified = curl_getdate(*value, NULL);
	}

#undef SKIP_ST

	return NSERROR_OK;	
}

/**
 * Process a fetch header
 *
 * \param object  Object being fetched
 * \param data	  Header string
 * \param len	  Byte length of header
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_fetch_process_header(llcache_object *object, const char *data,
		size_t len)
{
	nserror error;
	char *name, *value;
	llcache_header *temp;

	error = llcache_fetch_parse_header(object, data, len, &name, &value);
	if (error != NSERROR_OK)
		return error;

	/* Append header data to the object's headers array */
	temp = realloc(object->headers, (object->num_headers + 1) * 
			sizeof(llcache_header));
	if (temp == NULL) {
		free(name);
		free(value);
		return NSERROR_NOMEM;
	}

	object->headers = temp;

	object->headers[object->num_headers].name = name;
	object->headers[object->num_headers].value = value;

	object->num_headers++;

	return NSERROR_OK;
}

/**
 * Process a chunk of fetched data
 *
 * \param object  Object being fetched
 * \param data	  Data to process
 * \param len	  Byte length of data
 * \return NSERROR_OK on success, appropriate error otherwise.
 */
nserror llcache_fetch_process_data(llcache_object *object, const uint8_t *data, 
		size_t len)
{
	/* Resize source buffer if it's too small */
	if (object->source_len + len >= object->source_alloc) {
		const size_t new_len = object->source_len + len + 64 * 1024;
		uint8_t *temp = realloc(object->source_data, new_len);
		if (temp == NULL)
			return NSERROR_NOMEM;

		object->source_data = temp;
		object->source_alloc = new_len;
	}

	/* Append this data chunk to source buffer */
	memcpy(object->source_data + object->source_len, data, len);
	object->source_len += len;

	return NSERROR_OK;
}

/**
 * Handle an authentication request
 *
 * \param object  Object being fetched
 * \param realm	  Authentication realm
 * \return NSERROR_OK on success, appropriate error otherwise.
 */
nserror llcache_fetch_auth(llcache_object *object, const char *realm)
{
	nserror error = NSERROR_OK;

	/* Abort fetch for this object */
	fetch_abort(object->fetch.fetch);
	object->fetch.fetch = NULL;

	if (query_cb != NULL) {
		llcache_query query;

		/* Destroy headers */
		while (object->num_headers > 0) {
			object->num_headers--;

			free(object->headers[object->num_headers].name);
			free(object->headers[object->num_headers].value);
		}
		free(object->headers);
		object->headers = NULL;

		/* Emit query for authentication details */
		query.type = LLCACHE_QUERY_AUTH;
		query.url = object->url;
		query.data.auth.realm = realm;

		error = query_cb(&query, query_cb_pw, 
				llcache_query_handle_response, object);
	} else {
		llcache_object_user *user;
		llcache_event event;

		/* Inform client(s) that object fetch failed */
		event.type = LLCACHE_EVENT_ERROR;
		/** \todo More appropriate error message */
		event.data.msg = messages_get("FetchFailed");

		for (user = object->users; user != NULL; user = user->next) {
			error = user->handle.cb(&user->handle, &event, 
					user->handle.pw);
			if (error != NSERROR_OK)
				break;
		}
	}

	return error;
}

/**
 * Handle a TLS certificate verification failure
 *
 * \param object  Object being fetched
 * \param certs	  Certificate chain
 * \param num	  Number of certificates in chain
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_fetch_cert_error(llcache_object *object,
		const struct ssl_cert_info *certs, size_t num)
{
	nserror error = NSERROR_OK;

	/* Abort fetch for this object */
	fetch_abort(object->fetch.fetch);
	object->fetch.fetch = NULL;

	if (query_cb != NULL) {
		llcache_query query;

		/* Emit query for TLS */
		query.type = LLCACHE_QUERY_SSL;
		query.url = object->url;
		query.data.ssl.certs = certs;
		query.data.ssl.num = num;

		error = query_cb(&query, query_cb_pw,
				llcache_query_handle_response, object);
	} else {
		llcache_object_user *user;
		llcache_event event;

		/* Inform client(s) that object fetch failed */
		event.type = LLCACHE_EVENT_ERROR;
		/** \todo More appropriate error message */
		event.data.msg = messages_get("FetchFailed");

		for (user = object->users; user != NULL; user = user->next) {
			error = user->handle.cb(&user->handle, &event, 
					user->handle.pw);
			if (error != NSERROR_OK)
				break;
		}
	}

	return error;
}


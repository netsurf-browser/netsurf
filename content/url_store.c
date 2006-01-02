/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Central repository for URL data (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "netsurf/content/url_store.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/desktop/options.h"
#ifdef riscos
#include "netsurf/riscos/bitmap.h"
#endif
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"


#define ITERATIONS_BEFORE_TEST 32
#define MAXIMUM_URL_LENGTH 1024

struct hostname_data *url_store_hostnames = NULL;

static struct hostname_data *url_store_find_hostname(const char *url);
static struct hostname_data *url_store_match_hostname(const char *url,
		struct hostname_data *previous);
static char *url_store_match_scheme = NULL;

static struct hostname_data *last_hostname_found = NULL;

/**
 * Returns the hostname data for the specified URL. If no hostname
 * data is currently available then it is created.
 *
 * \param  url  the url to find hostname data for
 * \return  the current hostname data, or NULL if memory exhausted
 */
struct hostname_data *url_store_find_hostname(const char *url)
{
	struct hostname_data *search;
	struct hostname_data *result;
	url_func_result res;
	char *hostname = NULL;
	int hostname_length;
	int compare;
	int fast_exit_counter = ITERATIONS_BEFORE_TEST;

	assert(url);

	/* try to match the last hostname for http:// */
	if ((last_hostname_found) &&
			(!strncmp("http://", url, 7)) &&
			(!strncmp(last_hostname_found->hostname, url + 7,
				last_hostname_found->hostname_length))) {
		return last_hostname_found;
	}
	
	/* no match found, fallback */
	res = url_host(url, &hostname);
	switch (res) {
		case URL_FUNC_OK:
			break;
		case URL_FUNC_NOMEM:
			return NULL;
		case URL_FUNC_FAILED:
			hostname = strdup("file:/");	/* for 'file:/' */
			if (!hostname)
				return NULL;
			break;
		default:
			assert(0);
	}
	hostname_length = strlen(hostname);

	/* try to find a matching hostname fairly quickly */
	for (search = url_store_hostnames; search; search = search->next) {
		if ((fast_exit_counter <= 0) ||
				(search->hostname_length == hostname_length)) {
			compare = strcmp(hostname, search->hostname);
			if (compare == 0) {
				free(hostname);
				last_hostname_found = search;
				return search;
			} else if (compare < 0)
				break;
			fast_exit_counter = ITERATIONS_BEFORE_TEST;
		} else {
			fast_exit_counter--;
		}
	}

	/* no hostname is available: create a new one */
	result = malloc(sizeof *result);
	if (!result) {
		free(hostname);
		return NULL;
	}
	result->hostname = hostname;
	result->hostname_length = hostname_length;
	result->url = 0;
	result->previous = 0;
	result->next = 0;
	last_hostname_found = result;

	/* simple case: no current hostnames */
	if (!url_store_hostnames) {
		url_store_hostnames = result;
		return result;
	}

	/* worst case scenario: the place we need to link is within the last
	 * section of the hostname list so we have no reference to work back
	 * from. rather than slowing with the very common case of searching,
	 * we take a speed hit for this case and simply move to the very end
	 * of the hostname list ready to work backwards. */
	if (!search)
		for (search = url_store_hostnames; search->next;
				search = search->next)
			;

	/* we can now simply scan backwards as we know roughly where we need
	 * to link to (we either had an early exit from the searching so we
	 * know we're in the block following where we need to link, or we're
	 * at the very end of the list as we were in the last block.) */
	while ((search) && (strcmp(hostname, search->hostname) < 0))
		search = search->previous;

	/* simple case: our new hostname is the first in the list */
	if (!search) {
		result->next = url_store_hostnames;
		url_store_hostnames->previous = result;
		url_store_hostnames = result;
		return result;
	}

	/* general case: link in after the found hostname */
	result->previous = search;
	result->next = search->next;
	if (search->next)
		search->next->previous = result;
	search->next = result;
	return result;
}


/**
 * Returns the url data for the specified URL. If no url
 * data is currently available then it is created.
 *
 * \param  url  a normalized url to find hostname data for
 * \return  the current hostname data, or NULL if memory exhausted
 */
struct url_content *url_store_find(const char *url) {
	struct hostname_data *hostname_data;
	struct url_data *search;
	struct url_data *result;
	size_t url_length;
	int compare;
	int fast_exit_counter = ITERATIONS_BEFORE_TEST;

	assert(url);

	/* find the corresponding hostname data */
	hostname_data = url_store_find_hostname(url);
	if (!hostname_data)
		return NULL;

	/* move to the start of the leafname */
	url_length = strlen(url);

	/* try to find a matching url fairly quickly */
	for (search = hostname_data->url; search; search = search->next) {
		if ((fast_exit_counter <= 0) ||
				(search->data.url_length == url_length)) {
			compare = strcmp(url, search->data.url);
			if (compare == 0)
				return &search->data;
			else if (compare < 0)
				break;
			fast_exit_counter = ITERATIONS_BEFORE_TEST;
		} else {
			fast_exit_counter--;
		}
	}

	/* no URL is available: create a new one */
	result = calloc(1, sizeof(struct url_data));
	if (!result)
		return NULL;
	result->data.url = malloc(url_length + 1);
	if (!result->data.url) {
		free(result);
		return NULL;
	}
	strcpy(result->data.url, url);
	result->data.url_length = url_length;
	result->parent = hostname_data;

	/* simple case: no current URLs */
	if (!hostname_data->url) {
		hostname_data->url = result;
		return &result->data;
	}

	/* worst case scenario: the place we need to link is within the last
	 * section of the URL list so we have no reference to work back
	 * from. rather than slowing with the very common case of searching,
	 * we take a speed hit for this case and simply move to the very end
	 * of the URL list ready to work backwards. */
	if (!search)
		for (search = hostname_data->url; search->next;
				search = search->next)
			;

	/* we can now simply scan backwards as we know roughly where we need
	 * to link to (we either had an early exit from the searching so we
	 * know we're in the block following where we need to link, or we're
	 * at the very end of the list as we were in the last block.) */
	while ((search) && (strcmp(url, search->data.url) < 0))
		search = search->previous;

	/* simple case: our new hostname is the first in the list */
	if (!search) {
		result->next = hostname_data->url;
		hostname_data->url->previous = result;
		hostname_data->url = result;
		return &result->data;
	}

	/* general case: link in after the found hostname */
	result->previous = search;
	result->next = search->next;
	if (search->next)
		search->next->previous = result;
	search->next = result;
	return &result->data;
}


/**
 * Returns the next hostname that matches a part of the specified URL.
 *
 * \param url	   a normalized url to find the next match for
 * \param current  the current hostname to search forward from, or NULL
 * \return the next matching hostname, or NULL
 *
 * \todo  distinguish between out-of-memory and no more results in return
 */
struct hostname_data *url_store_match_hostname(const char *url,
		struct hostname_data *current) {
	url_func_result res;
	char *hostname;
	int hostname_length;
	int compare;
	bool www_test;

	assert(url);

	res = url_host(url, &hostname);
	switch (res) {
	case URL_FUNC_OK:
		break;
	case URL_FUNC_NOMEM:
		return NULL;
	case URL_FUNC_FAILED:
		hostname = strdup("file:/");		/* for 'file:/' */
		if (!hostname)
			return NULL;
		break;
	default:
		assert(0);
	}
	hostname_length = strlen(hostname);
	www_test = strncmp(hostname, "www.", 4);

	/* advance to the next hostname */
	if (!current)
		current = url_store_hostnames;
	else
		current = current->next;

	/* skip past hostname data without URLs */
	for (; current && (!current->url); current = current->next)
		;

	while (current) {
		if (current->hostname_length >= hostname_length) {
			compare = strncmp(hostname, current->hostname,
					hostname_length);
			if (compare == 0) {
				free(hostname);
				return current;
			} else if ((compare < 0) && !www_test)
				break;
		}
		/* special case: if hostname is not www then try it */
		if (www_test && ((current->hostname_length - 4) >=
				hostname_length) &&
				(!strncmp(current->hostname, "www.", 4)) &&
				(!strncmp(hostname, current->hostname + 4,
					hostname_length))) {
			free(hostname);
			return current;
		}

		/* move to next hostname with URLs */
		current = current->next;
		for (; current && (!current->url); current = current->next)
			;
	}

	free(hostname);
	return NULL;
}



/**
 * Returns the complete URL for the next matched stored URL.
 *
 * \param url	     a normalized url to find the next match for
 * \param reference  internal reference (NULL for first call)
 * \return the next URL that matches
 */
char *url_store_match(const char *url, struct url_data **reference) {
	struct hostname_data *hostname;
	struct url_data *search = NULL;
	int scheme_length;
	size_t url_length;
	url_func_result res;
	bool www_test;

	assert(url);

	if (!url_store_hostnames)
		return NULL;

	/* find the scheme and first URL, not necessarily matching */
	if (!*reference) {
		hostname = url_store_match_hostname(url, NULL);
		if (!hostname)
			return NULL;
		if (url_store_match_scheme) {
		  	free(url_store_match_scheme);
		  	url_store_match_scheme = NULL;
		}
		res = url_scheme(url, &url_store_match_scheme);
		if (res != URL_FUNC_OK)
			return NULL;
	} else {
		search = *reference;
		hostname = search->parent;
	}

	scheme_length = strlen(url_store_match_scheme);
	url_length = strlen(url);
	www_test = (!strcmp(url_store_match_scheme, "http") &&
			strncmp(url + 4 + 3, "www.", 4)); /* 'http' + '://' */

	/* work through all our strings, ignoring the scheme and 'www.' */
	while (hostname) {

		/* get the next URL to test */
		if (!search)
			search = hostname->url;
		else
			search = search->next;

		/* loop past end of list, or search */
		if (!search) {
			hostname = url_store_match_hostname(url, hostname);
			if (!hostname)
				return NULL;
		} else if (search->data.visits > 0) {
			/* straight match */
			if ((search->data.url_length >= url_length) &&
					(!strncmp(search->data.url, url,
							url_length))) {
				*reference = search;
				return search->data.url;
			}
			/* try with 'www.' inserted after the scheme */
			if (www_test && ((search->data.url_length - 4) >=
					url_length) &&
				(!strncmp(search->data.url,
						url_store_match_scheme,
						scheme_length)) &&
				(!strncmp(search->data.url + scheme_length + 3,
						"www.", 4)) &&
				(!strncmp(search->data.url + scheme_length + 7,
						url + scheme_length + 3,
						url_length - scheme_length - 3))) {
				*reference = search;
				return search->data.url;
			}
		}
	}
	return NULL;
}


/**
 * Converts a text string into one suitable for URL matching.
 *
 * \param text	     the text to search with
 * \return URL matching string allocated on heap, or NULL on error
 */
char *url_store_match_string(const char *text) {
	url_func_result res;
	char *url;

	assert(text);

	res = url_normalize(text, &url);
	if (res != URL_FUNC_OK)
		return NULL;

	/* drop the '/' from the end if it was added when normalizing */
	if ((url[strlen(url) - 1] == '/') && (text[strlen(text) - 1] != '/'))
		url[strlen(url) - 1] = '\0';
	return url;
}


/**
 * Loads the current contents of the URL store from disk
 *
 * \param file  the file to load options from
 */
void url_store_load(const char *file) {
	char s[MAXIMUM_URL_LENGTH];
	struct hostname_data *hostname;
	struct url_data *result;
	int urls;
	int i;
	int version;
	int length;
	FILE *fp;

	LOG(("Loading URL file"));

	fp = fopen(file, "r");
	if (!fp) {
		LOG(("Failed to open file '%s' for reading", file));
		return;
	}

	if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
		return;
	version = atoi(s);
	if (version < 102) {
	  	LOG(("Unsupported URL file version."));
	  	return;
	}
	if (version > 104) {
	 	LOG(("Unknown URL file version."));
		return;
	}

	while (fgets(s, MAXIMUM_URL_LENGTH, fp)) {
		if (s[strlen(s) - 1] == '\n')
			s[strlen(s) - 1] = '\0';
		hostname = url_store_find_hostname(s);
		if (!hostname)
			break;
		if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
			break;
		urls = atoi(s);
		for (i = 0; i < urls; i++) {
			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
			for (length = 0; s[length] > 32; length++);
			s[length] = 0x00;
			result = calloc(1, sizeof(struct url_data));
			if (!result)
				break;
			result->data.url_length = length;
			result->data.url = strdup(s);
			if (!result->data.url)
				die("Insufficient memory");
			result->parent = hostname;
			result->next = hostname->url;
			if (hostname->url)
				hostname->url->previous = result;
			hostname->url = result;
			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
			result->data.visits = atoi(s);
			if (version == 102) {
			  	/* ignore requests */
				if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
					break;
				/* ignore thumbnail size */
				if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
					break;
				/* set last visit as today to retain */
				result->data.last_visit = time(NULL);
			} else {
				if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
					break;
				result->data.last_visit = atoi(s);
				if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
					break;
				result->data.type = atoi(s);
			}
			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
#ifdef riscos
			for (length = 0; s[length] > 32; length++);
			s[length] = 0x00;
			if (length > 11)
				break;
			result->data.thumbnail =
					bitmap_create_file(s);
#endif
			if (version == 104) {
				if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
					break;
				for (length = 0; s[length] >= 32; length++);
				s[length] = 0x00;
				if (length > 0)
					result->data.title = strdup(s);
			}
		}
	}
	fclose(fp);
	LOG(("Successfully loaded URL file"));
}


/**
 * Saves the current contents of the URL store to disk
 *
 * \param file  the file to load options from
 */
void url_store_save(const char *file) {
	struct hostname_data *search;
	struct url_data *url;
	int url_count;
	char *normal = NULL;
	const char *thumb_file;
	char *s;
	int i;
	FILE *fp;
#ifdef riscos
	struct bitmap *bitmap;
#endif
	time_t min_date;
	char *title;

	fp = fopen(file, "w");
	if (!fp) {
		LOG(("Failed to open file '%s' for writing", file));
		return;
	}

	/* get the minimum date for expiry */
	min_date = time(NULL) - (60 * 60 * 24) * option_expire_url;
	LOG(("%d", (int) min_date));

	/* file format version number */
	fprintf(fp, "104\n");
	for (search = url_store_hostnames; search && search->next;
			search = search->next);
	for (; search; search = search->previous) {
		url_count = 0;
		for (url = search->url; url; url = url->next)
			if ((url->data.last_visit > min_date) &&
					(url->data.visits > 0) &&
					(url->data.url_length <
						MAXIMUM_URL_LENGTH)) {
				url_count++;
			}
		free(normal);
		normal = url_store_match_string(search->hostname);
		if ((url_count > 0) && (normal)) {
			fprintf(fp, "%s\n%i\n", normal, url_count);
			for (url = search->url; url && url->next;
					url = url->next);
			for (; url; url = url->previous)
				if ((url->data.last_visit > min_date) &&
						(url->data.visits > 0) &&
						(url->data.url_length <
							MAXIMUM_URL_LENGTH)) {
					thumb_file = "";
#ifdef riscos
					bitmap = url->data.thumbnail;
					if (bitmap)
						thumb_file = bitmap->filename;
#endif

					if (url->data.title) {
						s = url->data.title;
						for (i = 0; s[i] != '\0';
								i++)
							if (s[i] < 32)
								s[i] = ' ';
						for (--i;
							((i > 0) &&
							(s[i] == ' '));
								i--)
							s[i] = '\0';

						title = url->data.title;
					}
					else
						title = "";
					fprintf(fp, "%s\n%i\n%i\n%i\n%s\n%s\n",
							url->data.url,
							url->data.visits,
							(int) url->data.
								last_visit,
							url->data.type,
							thumb_file,
							title);
				}
		}
	}
	fclose(fp);
}


/**
 * Associates a thumbnail with a specified URL.
 */
void url_store_add_thumbnail(const char *url, struct bitmap *bitmap) {
	struct url_content *content;

	content = url_store_find(url);
	if (content) {
	  	if (content->thumbnail)
	  		bitmap_destroy(content->thumbnail);
	  	content->thumbnail = bitmap;
	}
}


/**
 * Gets the thumbnail associated with a given URL.
 */
struct bitmap *url_store_get_thumbnail(const char *url) {
	struct url_content *content;

	content = url_store_find(url);
	if (content)
		return content->thumbnail;
	return NULL;
}


int url_store_compare_last_visit(const void *a, const void *b) {
  	struct url_content * const *url_a = (struct url_content * const *)a;
  	struct url_content * const *url_b = (struct url_content * const *)b;
	return ((*url_a)->last_visit - (*url_b)->last_visit);
}

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
#include "netsurf/content/url_store.h"
#ifdef riscos
#include "netsurf/riscos/bitmap.h"
#endif
#include "netsurf/utils/url.h"
#include "netsurf/utils/log.h"


#define ITERATIONS_BEFORE_TEST 32
#define MAXIMUM_URL_LENGTH 1024

struct hostname_data {
	char *hostname;			/** Hostname (lowercase) */
	int hostname_length;		/** Length of hostname */
	struct url_data *url;		/** URLs for this host */
	struct hostname_data *previous;	/** Previous hostname */
	struct hostname_data *next;	/** Next hostname */
};

static struct hostname_data *url_store_hostnames = NULL;

static struct hostname_data *url_store_find_hostname(const char *url);
static struct hostname_data *url_store_match_hostname(const char *url,
		struct hostname_data *previous);
static char *url_store_match_scheme = NULL;


/**
 * Returns the hostname data for the specified URL. If no hostname
 * data is currently available then it is created.
 *
 * \param url  the url to find hostname data for
 * \return the current hostname data, or NULL on error
 */
static struct hostname_data *url_store_find_hostname(const char *url) {
	struct hostname_data *search;
	struct hostname_data *result;
	url_func_result res;
	char *hostname;
	int hostname_length;
	int compare;
	int fast_exit_counter = ITERATIONS_BEFORE_TEST;

	assert(url);

	res = url_host(url, &hostname);
	if (res != URL_FUNC_OK) {
		hostname = strdup("file:/");		/* for 'file:/' */
		if (!hostname)
			return NULL;
	}
	hostname_length = strlen(hostname);

	/* try to find a matching hostname fairly quickly */
	for (search = url_store_hostnames; search; search = search->next) {
		if ((fast_exit_counter <= 0) ||
				(search->hostname_length == hostname_length)) {
			compare = strcmp(hostname, search->hostname);
			if (compare == 0) {
				free(hostname);
				return search;
			} else if (compare < 0)
				break;
			fast_exit_counter = ITERATIONS_BEFORE_TEST;
		} else {
			fast_exit_counter--;
		}
	}

	/* no hostname is available: create a new one */
	result = calloc(sizeof(struct hostname_data), 1);
	if (!result)
		return NULL;
	result->hostname = hostname;
	result->hostname_length = hostname_length;

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
				search = search->next);

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
 * \param url  a normalized url to find hostname data for
 * \return the current hostname data, or NULL on error
 */
struct url_content *url_store_find(const char *url) {
	struct hostname_data *hostname_data;
	struct url_data *search;
	struct url_data *result;
	int url_length;
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
	result = calloc(sizeof(struct url_data), 1);
	if (!result)
		return NULL;
	result->data.url = malloc(url_length + 1);
	if (!result->data.url) {
		free(result);
		return NULL;
	}
	strcpy(result->data.url, url);
	result->data.url_length = url_length;
	result->data.requests = 0;
	result->data.visits = 0;
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
				search = search->next);

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
 */
static struct hostname_data *url_store_match_hostname(const char *url,
		struct hostname_data *current) {
	url_func_result res;
	char *hostname;
	int hostname_length;
	int compare;
	bool www_test;

	assert(url);

	res = url_host(url, &hostname);
	if (res != URL_FUNC_OK) {
		hostname = strdup("file:/");		/* for 'file:/' */
		if (!hostname)
			return NULL;
	}
	hostname_length = strlen(hostname);
	www_test = strncmp(hostname, "www.", 4);

	/* advance to the next hostname */
	if (!current)
		current = url_store_hostnames;
	else
		current = current->next;

	/* skip past hostname data without URLs */
	for (; current && (!current->url); current = current->next);

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
		for (; current && (!current->url); current = current->next);
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
	int url_length;
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
		} else if ((search->data.visits > 0) &&
				(search->data.requests > 0)){
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
 * Loads the current contents of the URL store to disk
 *
 * \param file  the file to load options from
 */
void url_store_load(const char *file) {
	struct url_content *url;
	char s[MAXIMUM_URL_LENGTH];
	struct hostname_data *hostname;
	struct url_data *result;
	int urls;
	int i;
	int version;
	int width, height, size;
	FILE *fp;

	fp = fopen(file, "r");
	if (!fp) {
		LOG(("Failed to open file '%s' for reading", file));
		return;
	}

	if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
		return;
	version = atoi(s);
	if ((version < 100) || (version > 102))
		return;


	/* version 100 file is sequences of <url><visits><requests> */
	if (version == 100) {
		while (fgets(s, MAXIMUM_URL_LENGTH, fp)) {
			if (s[strlen(s) - 1] == '\n')
				s[strlen(s) - 1] = '\0';
			url = url_store_find(s);
			if (!url)
				break;
			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
			url->visits = atoi(s);
			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
			url->requests = atoi(s);
		}
	} else if (version >= 101) {
	  	/* version 102 is as 101, but with thumbnail information */
		/* version 101 is as 100, but in hostname chunks, pre-sorted
		 * in reverse alphabetical order */
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
				if (s[strlen(s) - 1] == '\n')
					s[strlen(s) - 1] = '\0';
				result = calloc(sizeof(struct url_data), 1);
				if (!result)
					break;
				result->data.url_length = strlen(s);
				result->data.url = malloc(result->
						data.url_length + 1);
				if (!result->data.url) {
					free(result);
					break;
				}
				strcpy(result->data.url, s);
				result->parent = hostname;
				result->next = hostname->url;
				if (hostname->url)
					hostname->url->previous = result;
				hostname->url = result;
				if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
					break;
				result->data.visits = atoi(s);
				if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
					break;
				result->data.requests = atoi(s);
				if (version == 102) {
					if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
						break;
					size = atoi(s);
					width = size & 65535;
					height = size >> 16;
					if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
						break;
#ifdef riscos
					if (s[strlen(s) - 1] == '\n')
						s[strlen(s) - 1] = '\0';
					result->data.thumbnail =
							bitmap_create_file(
							width, height, s);
#endif
				}
			}
		}
	}
	fclose(fp);
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
	const char *thumb_file = "";
	int thumb_size = 0;
	FILE *fp;
	struct bitmap *bitmap;

	fp = fopen(file, "w");
	if (!fp) {
		LOG(("Failed to open file '%s' for writing", file));
		return;
	}

	/* file format version number */
	fprintf(fp, "102\n");
	for (search = url_store_hostnames; search && search->next;
			search = search->next);
	for (; search; search = search->previous) {
		url_count = 0;
		for (url = search->url; url; url = url->next)
			if ((url->data.visits > 0) &&
					(strlen(url->data.url) <
					MAXIMUM_URL_LENGTH))
				url_count++;
		free(normal);
		normal = url_store_match_string(search->hostname);
		if ((url_count > 0) && (normal)) {
			fprintf(fp, "%s\n%i\n", normal, url_count);
			for (url = search->url; url->next; url = url->next);
			for (; url; url = url->previous)
				if ((url->data.visits > 0) &&
						(strlen(url->data.url) <
						MAXIMUM_URL_LENGTH)) {
#ifdef riscos
					bitmap = url->data.thumbnail;
					if (bitmap) {
						thumb_size = bitmap->width |
							(bitmap->height << 16);
						thumb_file = bitmap->filename;
					} else {
					  	thumb_size = 0;
						thumb_file = "";
					}
#endif
					fprintf(fp, "%s\n%i\n%i\n%i\n%s\n",
							url->data.url,
							url->data.visits,
							url->data.requests,
							thumb_size,
							thumb_file);
				}
		}
	}
	fclose(fp);
}


/**
 * Dumps the currently stored URLs and hostnames to stderr.
 */
void url_store_dump(void) {
	struct hostname_data *search;
	struct url_data *url;

	fprintf(stderr, "\nDumping hostname data:\n");
	for (search = url_store_hostnames; search; search = search->next) {
		fprintf(stderr, "\n");
		fprintf(stderr, "%s", search->hostname);
		fprintf(stderr, ":\n");
		for (url = search->url; url; url = url->next) {
			fprintf(stderr, " - ");
			fprintf(stderr, "%s", url->data.url);
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "\nEnd of hostname data.\n\n");
}


void url_store_add_thumbnail(const char *url, struct bitmap *bitmap) {
	struct url_content *content;
	
	content = url_store_find(url);
	if (content) {
	  	if (content->thumbnail)
	  		bitmap_destroy(content->thumbnail);
	  	content->thumbnail = bitmap;
	}
}

struct bitmap *url_store_get_thumbnail(const char *url) {
	struct url_content *content;
	
	content = url_store_find(url);
	if (content)
		return content->thumbnail;
	return NULL;
}

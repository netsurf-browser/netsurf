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
	if (res != URL_FUNC_OK)
		return NULL;
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
				(search->url_length == url_length)) {
			compare = strcmp(url, search->url);
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
	result->url = malloc(url_length + 1);
	if (!result->url) {
		free(result);
		return NULL;
	}
	strcpy(result->url, url);
	result->url_length = url_length;
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
	while ((search) && (strcmp(url, search->url) < 0))
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
	if (res != URL_FUNC_OK)
		return NULL;
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
		if (www_test && ((current->hostname_length - 4) >= hostname_length) &&
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
	char *scheme;
	int scheme_length;
	int url_length;
	url_func_result res;
	bool www_test;

	assert(url);

	if (!url_store_hostnames)
		return NULL;

	/* find the first URL, not necessarily matching */
	if (!*reference) {
		hostname = url_store_match_hostname(url, NULL);
		if (!hostname)
			return NULL;
	} else {
		search = *reference;
		hostname = search->parent;
	}
 
	res = url_scheme(url, &scheme);
	if (res != URL_FUNC_OK)
		return NULL;
	scheme_length = strlen(scheme);
	url_length = strlen(url);
	www_test = (!strcmp(scheme, "http") &&
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
		} else if ((search->data.visits > 0) && (search->data.requests > 0)){
			/* straight match */
			if ((search->url_length >= url_length) &&
					(!strncmp(search->url, url, url_length))) {
				free(scheme);
				*reference = search;
				return search->url;
			}
			/* try with 'www.' inserted after the scheme */
			if (www_test && ((search->url_length - 4) >= url_length) &&
				(!strncmp(search->url, scheme, scheme_length)) &&
				(!strncmp(search->url + scheme_length + 3, "www.", 4)) &&
				(!strncmp(search->url + scheme_length + 7,
						url + scheme_length + 3,
						url_length - scheme_length - 3))) {
				free(scheme);
				*reference = search;
				return search->url;
			}
		}
	}
  	
	free(scheme);
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
	FILE *fp;

	fp = fopen(file, "r");
	if (!fp) {
		LOG(("Failed to open file '%s' for reading", file));
		return;
	}

	if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
		return;
	if (strncmp(s, "100", 3)) {
		LOG(("Invalid header"));
		return;
	}
	
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
	FILE *fp;

	fp = fopen(file, "w");
	if (!fp) {
		LOG(("Failed to open file '%s' for writing", file));
		return;
	}

	/* file format version number */
	fprintf(fp, "100\n");
	for (search = url_store_hostnames; search; search = search->next) {
		for (url = search->url; url; url = url->next) {
		  	if (strlen(url->url) < 1024) {
				fprintf(fp, "%s\n%i\n%i\n", url->url,
						url->data.visits, url->data.requests);
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
		fprintf(stderr, search->hostname);
		fprintf(stderr, ":\n");
		for (url = search->url; url; url = url->next) {
			fprintf(stderr, " - ");
			fprintf(stderr, url->url);
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "\nEnd of hostname data.\n\n");
}

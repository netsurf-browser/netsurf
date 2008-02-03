/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * URL parsing and joining (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <regex.h>
#include "curl/curl.h"
#include "utils/log.h"
#include "utils/url.h"
#include "utils/utils.h"

struct url_components_internal {
	char *buffer;	/* buffer used for all the following data */
	char *scheme;
	char *authority;
	char *path;
	char *query;
	char *fragment;
};


regex_t url_re, url_up_re;

/**
 * Initialise URL routines.
 *
 * Compiles regular expressions required by the url_ functions.
 */

void url_init(void)
{
	/* regex from RFC 2396 */
	regcomp_wrapper(&url_re, "^[[:space:]]*"
#define URL_RE_SCHEME 2
			"(([a-zA-Z][-a-zA-Z0-9+.]*):)?"
#define URL_RE_AUTHORITY 4
			"(//([^/?#[:space:]]*))?"
#define URL_RE_PATH 5
			"([^?#[:space:]]*)"
#define URL_RE_QUERY 7
			"(\\?([^#[:space:]]*))?"
#define URL_RE_FRAGMENT 9
			"(#([^[:space:]]*))?"
			"[[:space:]]*$", REG_EXTENDED);
	regcomp_wrapper(&url_up_re,
			"/([^/]?|[.][^./]|[^./][.]|[^./][^./]|[^/][^/][^/]+)"
			"/[.][.](/|$)",
			REG_EXTENDED);
}


/**
 * Check whether a host is an IP address
 *
 * \param  host    a hostname terminated by '\0' or '/'
 * \return true if the hostname is an IP address, false otherwise
 */
bool url_host_is_ip_address(const char *host) {
  	int b;
  	bool n;

	assert(host);

	/* an IP address is of the format XXX.XXX.XXX.XXX, ie totally
	 * numeric with 3 full stops between the numbers */
	b = 0; // number of breaks
	n = false; // number present
	do {
		if (*host == '.') {
		  	if (!n)
		  		return false;
			b++;
			n = false;
		} else if ((*host == '\0') || (*host == '/')) {
		  	if (!n)
		  		return false;
		  	/* todo: check the values are in 0-255 range */
		  	return (b == 3);
		} else if (*host < '0' || *host > '9') {
		  	return false;
                } else {
                	n = true;
                }
                host++;
        } while (1);
}

/**
 * Normalize a URL.
 *
 * \param  url	   an absolute URL
 * \param  result  pointer to pointer to buffer to hold cleaned up url
 * \return  URL_FUNC_OK on success
 *
 * If there is no scheme, http:// is added. The scheme and host are
 * lower-cased. Default ports are removed (http only). An empty path is
 * replaced with "/". Characters are unescaped if safe.
 */

url_func_result url_normalize(const char *url, char **result)
{
	char c;
	int m;
	int i;
	size_t len;
	bool http = false;
	regmatch_t match[10];

	*result = NULL;

	if ((m = regexec(&url_re, url, 10, match, 0))) {
		LOG(("url '%s' failed to match regex", url));
		return URL_FUNC_FAILED;
	}

	len = strlen(url);

	if (match[URL_RE_SCHEME].rm_so == -1) {
		/* scheme missing: add http:// and reparse */
/*		LOG(("scheme missing: using http"));*/
		if ((*result = malloc(len + 13)) == NULL) {
			LOG(("malloc failed"));
			return URL_FUNC_NOMEM;
		}
		strcpy(*result, "http://");
		strcpy(*result + sizeof("http://")-1, url);
		if ((m = regexec(&url_re, *result, 10, match, 0))) {
			LOG(("url '%s' failed to match regex", (*result)));
			free(*result);
			return URL_FUNC_FAILED;
		}
		len += sizeof("http://")-1;
	} else {
		if ((*result = malloc(len + 6)) == NULL) {
			LOG(("malloc failed"));
			return URL_FUNC_NOMEM;
		}
		strcpy(*result, url);
	}

	/*for (unsigned int i = 0; i != 10; i++) {
		if (match[i].rm_so == -1)
			continue;
		fprintf(stderr, "%i: '%.*s'\n", i,
				match[i].rm_eo - match[i].rm_so,
				res + match[i].rm_so);
	}*/

	/* see RFC 2616 section 3.2.3 */
	/* make scheme lower-case */
	if (match[URL_RE_SCHEME].rm_so != -1) {
		for (i = match[URL_RE_SCHEME].rm_so;
				i != match[URL_RE_SCHEME].rm_eo; i++)
			(*result)[i] = tolower((*result)[i]);
		if (match[URL_RE_SCHEME].rm_eo == 4
				&& (*result)[0] == 'h'
				&& (*result)[1] == 't'
				&& (*result)[2] == 't'
				&& (*result)[3] == 'p')
			http = true;
	}

	/* make empty path into "/" */
	if (match[URL_RE_PATH].rm_so != -1 &&
			match[URL_RE_PATH].rm_so == match[URL_RE_PATH].rm_eo) {
		memmove((*result) + match[URL_RE_PATH].rm_so + 1,
				(*result) + match[URL_RE_PATH].rm_so,
				len - match[URL_RE_PATH].rm_so + 1);
		(*result)[match[URL_RE_PATH].rm_so] = '/';
		len++;
	}

	/* make host lower-case */
	if (match[URL_RE_AUTHORITY].rm_so != -1) {
		for (i = match[URL_RE_AUTHORITY].rm_so;
				i != match[URL_RE_AUTHORITY].rm_eo; i++) {
			if ((*result)[i] == ':') {
				if (http && (*result)[i + 1] == '8' &&
						(*result)[i + 2] == '0' &&
						i + 3 ==
						match[URL_RE_AUTHORITY].rm_eo) {
					memmove((*result) + i,
							(*result) + i + 3,
							len -
							match[URL_RE_AUTHORITY].
							rm_eo);
					len -= 3;
					(*result)[len] = '\0';
				} else if (i + 1 == match[4].rm_eo) {
					memmove((*result) + i,
							(*result) + i + 1,
							len -
							match[URL_RE_AUTHORITY].
							rm_eo);
					len--;
					(*result)[len] = '\0';
				}
				break;
			}
			(*result)[i] = tolower((*result)[i]);
		}
	}

	/* unescape non-"reserved" escaped characters */
	for (i = 0; (unsigned)i != len; i++) {
		if ((*result)[i] != '%')
			continue;
		c = tolower((*result)[i + 1]);
		if ('0' <= c && c <= '9')
			m = 16 * (c - '0');
		else if ('a' <= c && c <= 'f')
			m = 16 * (c - 'a' + 10);
		else
			continue;
		c = tolower((*result)[i + 2]);
		if ('0' <= c && c <= '9')
			m += c - '0';
		else if ('a' <= c && c <= 'f')
			m += c - 'a' + 10;
		else
			continue;

		if (m <= 0x20 || strchr(";/?:@&=+$," "<>#%\"{}|\\^[]`", m) ||
				m >= 0x7f) {
			i += 2;
			continue;
		}

		(*result)[i] = m;
		memmove((*result) + i + 1, (*result) + i + 3, len - i - 2);
		len -= 2;
	}

	return URL_FUNC_OK;
}


/**
 * Resolve a relative URL to absolute form.
 *
 * \param  rel	   relative URL
 * \param  base	   base URL, must be absolute and cleaned as by url_normalize()
 * \param  result  pointer to pointer to buffer to hold absolute url
 * \return  URL_FUNC_OK on success
 */

url_func_result url_join(const char *rel, const char *base, char **result)
{
	url_func_result status = URL_FUNC_NOMEM;
	struct url_components_internal base_components = {0,0,0,0,0,0};
	struct url_components_internal rel_components = {0,0,0,0,0,0};
	struct url_components_internal merged_components = {0,0,0,0,0,0};
	char *merge_path = NULL, *split_point;
	char *input, *output, *start = NULL;
	int len, buf_len;

	(*result) = 0;

	assert(base);
	assert(rel);


	/* break down the relative URL (not cached, corruptable) */
	status = url_get_components(rel,
			(struct url_components *)&rel_components);
	if (status != URL_FUNC_OK) {
		LOG(("relative url '%s' failed to get components", rel));
		return URL_FUNC_FAILED;
	}

	/* [1] relative URL is absolute, use it entirely */
	merged_components = rel_components;
	if (rel_components.scheme)
		goto url_join_reform_url;

	/* break down the base URL (possibly cached, not corruptable) */
	status = url_get_components(base,
			(struct url_components *)&base_components);
	if (status != URL_FUNC_OK) {
		url_destroy_components(
				(struct url_components *)&rel_components);
		LOG(("base url '%s' failed to get components", base));
		return URL_FUNC_FAILED;
	}

	/* [2] relative authority takes presidence */
	merged_components.scheme = base_components.scheme;
	if (rel_components.authority)
		goto url_join_reform_url;

	/* [3] handle empty paths */
	merged_components.authority = base_components.authority;
	if (!rel_components.path) {
	  	merged_components.path = base_components.path;
		if (!rel_components.query)
			merged_components.query = base_components.query;
		goto url_join_reform_url;
	}

	/* [4] handle valid paths */
	if (rel_components.path[0] == '/')
		merged_components.path = rel_components.path;
	else {
		/* 5.2.3 */
		if ((base_components.authority) && (!base_components.path)) {
			merge_path = malloc(strlen(rel_components.path) + 2);
			if (!merge_path) {
				LOG(("malloc failed"));
				goto url_join_no_mem;
			}
			sprintf(merge_path, "/%s", rel_components.path);
			merged_components.path = merge_path;
		} else {
			split_point = base_components.path ?
					strrchr(base_components.path, '/') :
					NULL;
			if (!split_point) {
				merged_components.path = rel_components.path;
			} else {
				len = ++split_point - base_components.path;
				buf_len = len + 1 + strlen(rel_components.path);
				merge_path = malloc(buf_len);
				if (!merge_path) {
					LOG(("malloc failed"));
					goto url_join_no_mem;
				}
				memcpy(merge_path, base_components.path, len);
				memcpy(merge_path + len, rel_components.path,
						strlen(rel_components.path));
				merge_path[buf_len - 1] = '\0';
				merged_components.path = merge_path;
			}
		}
	}

url_join_reform_url:
	/* 5.2.4 */
	input = merged_components.path;
	if ((input) && (strchr(input, '.'))) {
	  	/* [1] remove all dot references */
	  	output = start = malloc(strlen(input) + 1);
	  	if (!output) {
			LOG(("malloc failed"));
			goto url_join_no_mem;
		}
		merged_components.path = output;
		*output = '\0';

		while (*input != '\0') {
		  	/* [2A] */
		  	if (input[0] == '.') {
		  		if (input[1] == '/') {
		  			input = input + 2;
		  			continue;
		  		} else if ((input[1] == '.') &&
		  				(input[2] == '/')) {
		  			input = input + 3;
		  			continue;
		  		}
		  	}

		  	/* [2B] */
		  	if ((input[0] == '/') && (input[1] == '.')) {
		  		if (input[2] == '/') {
		  		  	input = input + 2;
		  		  	continue;
		  		} else if (input[2] == '\0') {
		  		  	input = input + 1;
		  		  	*input = '/';
		  		  	continue;
		  		}

		  		/* [2C] */
		  		if ((input[2] == '.') && ((input[3] == '/') ||
		  				(input[3] == '\0'))) {
			  		if (input[3] == '/') {
			  		  	input = input + 3;
			  		} else {
		  				input = input + 2;
		  			  	*input = '/';
		  			}

		  			if ((output > start) &&
		  					(output[-1] == '/'))
		  				*--output = '\0';
		  			split_point = strrchr(start, '/');
		  			if (!split_point)
		  				output = start;
		  			else
		  				output = split_point;
		  			*output = '\0';
		  			continue;
		  		}
		  	}


		  	/* [2D] */
		  	if (input[0] == '.') {
		  		if (input[1] == '\0') {
		  			input = input + 1;
		  			continue;
		  		} else if ((input[1] == '.') &&
		  				(input[2] == '\0')) {
		  			input = input + 2;
		  			continue;
		  		}
		  	}

		  	/* [2E] */
		  	if (*input == '/')
		  		*output++ = *input++;
		  	while ((*input != '/') && (*input != '\0'))
		  		*output++ = *input++;
		  	*output = '\0';
                }
                /* [3] */
      		merged_components.path = start;
	}

	/* 5.3 */
	*result = url_reform_components(
			(struct url_components *)&merged_components);
  	if (!(*result))
		goto url_join_no_mem;

	/* return success */
	status = URL_FUNC_OK;

url_join_no_mem:
	free(start);
	free(merge_path);
	url_destroy_components((struct url_components *)&base_components);
	url_destroy_components((struct url_components *)&rel_components);
	return status;
}


/**
 * Return the host name from an URL.
 *
 * \param  url	   an absolute URL
 * \param  result  pointer to pointer to buffer to hold host name
 * \return  URL_FUNC_OK on success
 */

url_func_result url_host(const char *url, char **result)
{
	url_func_result status;
	struct url_components components;
	const char *host_start, *host_end;

	assert(url);

	status = url_get_components(url, &components);
	if (status == URL_FUNC_OK) {
		if (!components.authority) {
			url_destroy_components(&components);
			return URL_FUNC_FAILED;
		}
		host_start = strchr(components.authority, '@');
		host_start = host_start ? host_start + 1 : components.authority;
		host_end = strchr(host_start, ':');
		if (!host_end)
			host_end = components.authority +
					strlen(components.authority);

		*result = malloc(host_end - host_start + 1);
		if (!(*result)) {
			url_destroy_components(&components);
			return URL_FUNC_FAILED;
		}
		memcpy((*result), host_start, host_end - host_start);
		(*result)[host_end - host_start] = '\0';
	}
	url_destroy_components(&components);
	return status;
}


/**
 * Return the scheme name from an URL.
 *
 * See RFC 3986, 3.1 for reference.
 *
 * \param  url	   an absolute URL
 * \param  result  pointer to pointer to buffer to hold scheme name
 * \return  URL_FUNC_OK on success
 */

url_func_result url_scheme(const char *url, char **result)
{
	url_func_result status;
	struct url_components components;

	assert(url);

	status = url_get_components(url, &components);
	if (status == URL_FUNC_OK) {
		if (!components.scheme) {
			status = URL_FUNC_FAILED;
		} else {
			*result = strdup(components.scheme);
			if (!(*result))
				status = URL_FUNC_NOMEM;
		}
	}
	url_destroy_components(&components);
	return status;
}


/**
 * Return the canonical root of an URL
 *
 * \param url	  an absolute URL
 * \param result  pointer to pointer to buffer to hold canonical rool URL
 * \return  URL_FUNC_OK on success
 */

url_func_result url_canonical_root(const char *url, char **result)
{
	url_func_result status;
	struct url_components components;

	assert(url);

	status = url_get_components(url, &components);
	if (status == URL_FUNC_OK) {
		if ((!components.scheme) || (!components.authority)) {
			status = URL_FUNC_FAILED;
		} else {
			*result = malloc(strlen(components.scheme) +
					strlen(components.authority) + 4);
			if (!(*result))
				status = URL_FUNC_NOMEM;
			else
				sprintf((*result), "%s://%s", components.scheme,
						components.authority);
		}
	}
	url_destroy_components(&components);
	return status;
}


/**
 * Strip the topmost segment of the path
 *
 * \param url	  an absolute URL
 * \param result  pointer to pointer to buffer to hold result
 * \return URL_FUNC_OK on success
 */

url_func_result url_parent(const char *url, char **result)
{
	url_func_result status;
	struct url_components components;
	int len, path_len;

	assert(url);

	status = url_get_components(url, &components);
	if (status == URL_FUNC_OK) {
		if ((!components.scheme) || (!components.authority) ||
				(!components.path)) {
			status = URL_FUNC_FAILED;
		} else {
			if (strcmp(components.path, "/")) {
				path_len = strlen(components.path);
				if (components.path[path_len - 1] == '/')
					path_len--;
				while (components.path[path_len - 1] != '/')
					path_len--;
			} else {
				path_len = 1;
			}
			len = strlen(components.scheme) +
					strlen(components.authority) +
					path_len + 4;
			*result = malloc(len);
			if (!(*result))
				status = URL_FUNC_NOMEM;
			else
				snprintf((*result), len, "%s://%s%s",
						components.scheme,
						components.authority,
						components.path);
		}
	}
	url_destroy_components(&components);
	return status;
}


/**
 * Extract path, leafname and query segments from an URL
 *
 * \param url	  an absolute URL
 * \param result  pointer to pointer to buffer to hold result
 * \return URL_FUNC_OK on success
 */

url_func_result url_plq(const char *url, char **result)
{
	url_func_result status;
	struct url_components components;

	assert(url);

	status = url_get_components(url, &components);
	if (status == URL_FUNC_OK) {
		if (!components.path) {
			status = URL_FUNC_FAILED;
		} else if ((components.query) &&
				(strlen(components.query) > 0)) {
			*result = malloc(strlen(components.path) +
					strlen(components.query) + 2);
			if (!(*result))
				status = URL_FUNC_NOMEM;
			else
				sprintf((*result), "%s?%s", components.path,
						components.query);
		} else {
			*result = strdup(components.path);
			if (!(*result))
				status = URL_FUNC_NOMEM;
		}
	}
	url_destroy_components(&components);
	return status;
}


/**
 * Extract path segment from an URL
 *
 * \param url	  an absolute URL
 * \param result  pointer to pointer to buffer to hold result
 * \return URL_FUNC_OK on success
 */

url_func_result url_path(const char *url, char **result)
{
	url_func_result status;
	struct url_components components;

	assert(url);

	status = url_get_components(url, &components);
	if (status == URL_FUNC_OK) {
		if (!components.path) {
			status = URL_FUNC_FAILED;
		} else {
			*result = strdup(components.path);
			if (!(*result))
				status = URL_FUNC_NOMEM;
		}
	}
	url_destroy_components(&components);
	return status;
}

/**
 * Extract leafname from an URL
 *
 * \param url	  an absolute URL
 * \param result  pointer to pointer to buffer to hold result
 * \return URL_FUNC_OK on success
 */

url_func_result url_leafname(const char *url, char **result)
{
	url_func_result status;
	struct url_components components;

	assert(url);

	status = url_get_components(url, &components);
	if (status == URL_FUNC_OK) {
		if (!components.path) {
			status = URL_FUNC_FAILED;
		} else {
			char *slash = strrchr(components.path, '/');

			assert (slash != NULL);

			*result = strdup(slash + 1);
			if (!(*result))
				status = URL_FUNC_NOMEM;
		}
	}
	url_destroy_components(&components);
	return status;
}

/**
 * Extract fragment from an URL
 * This will unescape any %xx entities in the fragment
 *
 * \param url     an absolute URL
 * \param result  pointer to pointer to buffer to hold result
 * \return URL_FUNC_OK on success
 */

url_func_result url_fragment(const char *url, char **result)
{
	url_func_result status;
	struct url_components components;

	assert(url);

	status = url_get_components(url, &components);
	if (status == URL_FUNC_OK) {
		if (!components.fragment) {
			status = URL_FUNC_FAILED;
		} else {
			char *frag = curl_unescape(components.fragment,
					strlen(components.fragment));
			if (!frag) {
				status = URL_FUNC_NOMEM;
			} else {
				*result = strdup(frag);
				if (!(*result))
					status = URL_FUNC_NOMEM;
				curl_free(frag);
			}
		}
	}

	url_destroy_components(&components);
	return status;
}

/**
 * Attempt to find a nice filename for a URL.
 *
 * \param  url	   an absolute URL
 * \param  result  pointer to pointer to buffer to hold filename
 * \param  remove_extensions  remove any extensions from the filename
 * \return  URL_FUNC_OK on success
 */

url_func_result url_nice(const char *url, char **result,
		bool remove_extensions)
{
	int m;
	regmatch_t match[10];
	regoff_t start, end;
	size_t i;
	char *dot;

	*result = 0;

	m = regexec(&url_re, url, 10, match, 0);
	if (m) {
		LOG(("url '%s' failed to match regex", url));
		return URL_FUNC_FAILED;
	}

	/* extract the last component of the path, if possible */
	if (match[URL_RE_PATH].rm_so == -1 || match[URL_RE_PATH].rm_so ==
			match[URL_RE_PATH].rm_eo)
		goto no_path;  /* no path, or empty */
	for (end = match[URL_RE_PATH].rm_eo - 1;
			end != match[URL_RE_PATH].rm_so && url[end] == '/';
			end--)
		;
	if (end == match[URL_RE_PATH].rm_so)
		goto no_path;  /* path is a string of '/' */
	end++;
	for (start = end - 1;
			start != match[URL_RE_PATH].rm_so && url[start] != '/';
			start--)
		;
	if (url[start] == '/')
		start++;

	if (!strncasecmp(url + start, "index.", 6) ||
			!strncasecmp(url + start, "default.", 8)) {
		/* try again */
		if (start == match[URL_RE_PATH].rm_so)
			goto no_path;
		for (end = start - 1;
				end != match[URL_RE_PATH].rm_so &&
				url[end] == '/';
				end--)
			;
		if (end == match[URL_RE_PATH].rm_so)
			goto no_path;
		end++;
		for (start = end - 1;
				start != match[URL_RE_PATH].rm_so &&
				url[start] != '/';
				start--)
		;
		if (url[start] == '/')
			start++;
	}

	*result = malloc(end - start + 1);
	if (!*result) {
		LOG(("malloc failed"));
		return URL_FUNC_NOMEM;
	}
	strncpy(*result, url + start, end - start);
	(*result)[end - start] = 0;

	if (remove_extensions) {
		dot = strchr(*result, '.');
		if (dot && dot != *result)
			*dot = 0;
	}

	return URL_FUNC_OK;

no_path:

	/* otherwise, use the host name, with '.' replaced by '_' */
	if (match[URL_RE_AUTHORITY].rm_so != -1 &&
			match[URL_RE_AUTHORITY].rm_so !=
			match[URL_RE_AUTHORITY].rm_eo) {
		*result = malloc(match[URL_RE_AUTHORITY].rm_eo -
				match[URL_RE_AUTHORITY].rm_so + 1);
		if (!*result) {
			LOG(("malloc failed"));
			return URL_FUNC_NOMEM;
		}
		strncpy(*result, url + match[URL_RE_AUTHORITY].rm_so,
				match[URL_RE_AUTHORITY].rm_eo -
				match[URL_RE_AUTHORITY].rm_so);
		(*result)[match[URL_RE_AUTHORITY].rm_eo -
				match[URL_RE_AUTHORITY].rm_so] = 0;

		for (i = 0; (*result)[i]; i++)
			if ((*result)[i] == '.')
				(*result)[i] = '_';

		return URL_FUNC_OK;
	}

	return URL_FUNC_FAILED;
}


/**
 * Escape a string suitable for inclusion in an URL.
 *
 * \param  unescaped  the unescaped string
 * \param  sptoplus   true iff spaces should be converted to +
 * \param  result     pointer to pointer to buffer to hold escaped string
 * \return  URL_FUNC_OK on success
 */

url_func_result url_escape(const char *unescaped, bool sptoplus,
		char **result)
{
	int len;
	char *escaped, *d;
	const char *c;

	if (!unescaped || !result)
		return URL_FUNC_FAILED;

	*result = NULL;

	len = strlen(unescaped);

	escaped = malloc(len * 3 + 1);
	if (!escaped)
		return URL_FUNC_NOMEM;

	for (c = unescaped, d = escaped; *c; c++) {
		/* Check if we should escape this byte.
		 * '~' is unreserved and should not be percent encoded, if
		 * you believe the spec; however, leaving it unescaped
		 * breaks a bunch of websites, so we escape it anyway. */
		if (!isascii(*c) || strchr(":/?#[]@" /* gen-delims */
					"!$&'()*+,;=" /* sub-delims */
					"<>%\"{}|\\^`~", /* others */
					*c) ||
				*c <= 0x20 || *c == 0x7f) {
			if (*c == 0x20 && sptoplus) {
				*d++ = '+';
			} else {
				*d++ = '%';
				*d++ = "0123456789ABCDEF"[((*c >> 4) & 0xf)];
				*d++ = "0123456789ABCDEF"[(*c & 0xf)];
			}
		} else {
			/* unreserved characters: [a-zA-Z0-9-._] */
			*d++ = *c;
		}
	}

	*d++ = '\0';

	(*result) = malloc(d - escaped);
	if (!(*result)) {
		free(escaped);
		return URL_FUNC_NOMEM;
	}

	memcpy((*result), escaped, d - escaped);

	free(escaped);

	return URL_FUNC_OK;
}

/**
 * Compare two absolute, normalized URLs
 *
 * \param url1     URL 1
 * \param url2     URL 2
 * \param nofrag   Ignore fragment part in comparison
 * \param result   Pointer to location to store result (true if URLs match)
 * \return URL_FUNC_OK on success
 */
url_func_result url_compare(const char *url1, const char *url2,
		bool nofrag, bool *result)
{
	url_func_result status;
	struct url_components c1, c2;
	bool res = true;

	assert(url1 && url2 && result);

	/* Decompose URLs */
	status = url_get_components(url1, &c1);
	if (status != URL_FUNC_OK) {
		url_destroy_components(&c1);
		return status;
	}

	status = url_get_components(url2, &c2);
	if (status != URL_FUNC_OK) {
		url_destroy_components(&c2);
		url_destroy_components(&c1);
		return status;
	}

	if (((c1.scheme && c2.scheme) || (!c1.scheme && !c2.scheme )) &&
			((c1.authority && c2.authority) ||
					(!c1.authority && !c2.authority)) &&
			((c1.path && c2.path) || (!c1.path && !c2.path)) &&
			((c1.query && c2.query) ||
					(!c1.query && !c2.query)) &&
			(nofrag || (c1.fragment && c2.fragment) ||
					(!c1.fragment && !c2.fragment))) {

		if (c1.scheme)
			res &= strcasecmp(c1.scheme, c2.scheme) == 0;

		/** \todo consider each part of the authority separately */
		if (c1.authority)
			res &= strcasecmp(c1.authority, c2.authority) == 0;

		if (c1.path)
			res &= strcmp(c1.path, c2.path) == 0;

		if (c1.query)
			res &= strcmp(c1.query, c2.query) == 0;

		if (!nofrag && c1.fragment)
			res &= strcmp(c1.fragment, c2.fragment) == 0;
	} else {
		/* Can't match */
		res = false;
	}

	*result = res;

	url_destroy_components(&c2);
	url_destroy_components(&c1);

	return URL_FUNC_OK;
}

/**
 * Split a URL into separate components
 *
 * URLs passed to this function are assumed to be valid and no error checking
 * or recovery is attempted.
 *
 * See RFC 3986 for reference.
 *
 * \param  url	     a valid absolute or relative URL
 * \param  result    pointer to buffer to hold components
 * \return  URL_FUNC_OK on success
 */

url_func_result url_get_components(const char *url,
		struct url_components *result)
{
  	int storage_length;
	char *storage_end;
	const char *scheme;
	const char *authority;
	const char *path;
	const char *query;
	const char *fragment;
	struct url_components_internal *internal;

	assert(url);

	/* clear our return value */
	internal = (struct url_components_internal *)result;
	memset(result, 0x00, sizeof(struct url_components));

	/* get enough storage space for a URL with termination at each node */
	storage_length = strlen(url) + 8;
	internal->buffer = malloc(storage_length);
	if (!internal->buffer)
		return URL_FUNC_NOMEM;
	storage_end = internal->buffer;

	/* look for a valid scheme */
	scheme = url;
	if (isalpha(*scheme)) {
		for (scheme = url + 1;
				((*scheme != ':') && (*scheme != '\0'));
				*scheme++)
			if (!isalnum(*scheme) && (*scheme != '+') &&
					(*scheme != '-') && (*scheme != '.'))
				break;
		if (*scheme == ':') {
			memcpy(storage_end, url, scheme - url);
			storage_end[scheme - url] = '\0';
			result->scheme = storage_end;
			storage_end += scheme - url + 1;
			scheme++;
		} else {
			scheme = url;
		}
	}


	/* look for an authority */
	authority = scheme;
	if ((authority[0] == '/') && (authority[1] == '/')) {
		authority = strpbrk(scheme + 2, "/?#");
		if (!authority)
			authority = scheme + strlen(scheme);
		memcpy(storage_end, scheme + 2, authority - scheme - 2);
		storage_end[authority - scheme - 2] = '\0';
		result->authority = storage_end;
		storage_end += authority - scheme - 1;
	}


	/* look for a path */
	path = authority;
	if ((*path != '?') && (*path != '#') && (*path != '\0')) {
		path = strpbrk(path, "?#");
		if (!path)
			path = authority + strlen(authority);
		memcpy(storage_end, authority, path - authority);
		storage_end[path - authority] = '\0';
		result->path = storage_end;
		storage_end += path - authority + 1;
	}


	/* look for a query */
	query = path;
	if (*query == '?') {
		query = strchr(query, '#');
		if (!query)
			query = path + strlen(path);
		memcpy(storage_end, path + 1, query - path - 1);
		storage_end[query - path - 1] = '\0';
		result->query = storage_end;
		storage_end += query - path;
	}


	/* look for a fragment */
	fragment = query;
	if (*fragment == '#') {
		fragment = query + strlen(query);

		/* make a copy of the result for the caller */
		memcpy(storage_end, query + 1, fragment - query - 1);
		storage_end[fragment - query - 1] = '\0';
		result->fragment = storage_end;
		storage_end += fragment - query;
	}

	assert((result->buffer + storage_length) >= storage_end);
	return URL_FUNC_OK;
}


/**
 * Reform a URL from separate components
 *
 * See RFC 3986 for reference.
 *
 * \param  components  the components to reform into a URL
 * \return  a new URL allocated on the stack, or NULL on failure
 */

char *url_reform_components(const struct url_components *components)
{
	int scheme_len = 0, authority_len = 0, path_len = 0, query_len = 0,
			fragment_len = 0;
	char *result, *url;

	/* 5.3 */
	if (components->scheme)
		scheme_len = strlen(components->scheme) + 1;
	if (components->authority)
		authority_len = strlen(components->authority) + 2;
	if (components->path)
		path_len = strlen(components->path);
	if (components->query)
		query_len = strlen(components->query) + 1;
	if (components->fragment)
		fragment_len = strlen(components->fragment) + 1;

	/* claim memory */
	url = result = malloc(scheme_len + authority_len + path_len +
			query_len + fragment_len + 1);
	if (!url) {
		LOG(("malloc failed"));
		return NULL;
	}

	/* rebuild URL */
	if (components->scheme) {
	  	sprintf(url, "%s:", components->scheme);
		url += scheme_len;
	}
	if (components->authority) {
	  	sprintf(url, "//%s", components->authority);
		url += authority_len;
	}
	if (components->path) {
	  	sprintf(url, "%s", components->path);
		url += path_len;
	}
	if (components->query) {
	  	sprintf(url, "?%s", components->query);
		url += query_len;
	}
	if (components->fragment)
	  	sprintf(url, "#%s", components->fragment);
	return result;
}


/**
 * Release some url components from memory
 *
 * \param  result  pointer to buffer containing components
 */
void url_destroy_components(const struct url_components *components)
{
	const struct url_components_internal *internal;

	assert(components);

	internal = (const struct url_components_internal *)components;
	if (internal->buffer)
		free(internal->buffer);
}


#ifdef TEST

int main(int argc, char *argv[])
{
	int i;
	url_func_result res;
	char *s;
	url_init();
	for (i = 1; i != argc; i++) {
/*		printf("==> '%s'\n", argv[i]);
		res = url_normalize(argv[i], &s);
		if (res == URL_FUNC_OK) {
			printf("<== '%s'\n", s);
			free(s);
		}*/
/*		printf("==> '%s'\n", argv[i]);
		res = url_host(argv[i], &s);
		if (res == URL_FUNC_OK) {
			printf("<== '%s'\n", s);
			free(s);
		}*/
		if (1 != i) {
			res = url_join(argv[i], argv[1], &s);
			if (res == URL_FUNC_OK) {
				printf("'%s' + '%s' \t= '%s'\n", argv[1],
						argv[i], s);
				free(s);
			}
		}
/*		printf("'%s' => ", argv[i]);
		res = url_nice(argv[i], &s, true);
		if (res == URL_FUNC_OK) {
			printf("'%s', ", s);
			free(s);
		} else {
			printf("failed %u, ", res);
		}
		res = url_nice(argv[i], &s, false);
		if (res == URL_FUNC_OK) {
			printf("'%s', ", s);
			free(s);
		} else {
			printf("failed %u, ", res);
		}
		printf("\n");*/
	}
	return 0;
}

void regcomp_wrapper(regex_t *preg, const char *regex, int cflags)
{
	char errbuf[200];
	int r;
	r = regcomp(preg, regex, cflags);
	if (r) {
		regerror(r, preg, errbuf, sizeof errbuf);
		fprintf(stderr, "Failed to compile regexp '%s'\n", regex);
		fprintf(stderr, "error: %s\n", errbuf);
		exit(1);
	}
}

#endif

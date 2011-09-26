/*
 * Copyright 2011 Michael Drake <tlsa@netsurf-browser.org>
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
 * NetSurf URL handling (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <libwapcaplet/libwapcaplet.h>
#include <stdlib.h>
#include <string.h>

#include "utils/errors.h"
#include "utils/log.h"
#include "utils/nsurl.h"
#include "utils/utils.h"


/* Define to enable NSURL debugging */
#undef NSURL_DEBUG


/**
 * NetSurf URL object
 *
 * [scheme]://[username][:password]@[host]:[port][/path][?query][#fragment]
 */
struct nsurl {
	lwc_string *scheme;
	lwc_string *username;
	lwc_string *password;
	lwc_string *host;
	lwc_string *port;
	lwc_string *path;
	lwc_string *query;
	lwc_string *fragment;

	int count;	/* Number of references to NetSurf URL object */

	char *string;	/* Full URL as a string */
	size_t length;	/* Length of string */
};


/** Marker set, indicating positions of sections within a URL string */
struct url_markers {
	size_t start; /** start of URL */
	size_t scheme_end;
	size_t authority;

	size_t colon_first;
	size_t at;
	size_t colon_last;

	size_t path;
	size_t query;
	size_t fragment;

	size_t end; /** end of URL */
};


/** Sections of a URL */
enum url_sections {
	URL_SCHEME,
	URL_CREDENTIALS,
	URL_HOST,
	URL_PATH,
	URL_QUERY,
	URL_FRAGMENT
};


#define nsurl__component_copy(c) (c == NULL) ? NULL : lwc_string_ref(c)

#define nsurl__component_compare(c1, c2, match)		\
	if (c1 && c2)					\
		lwc_string_isequal(c1, c2, match);	\
	else if (c1 || c2)				\
		*match = false;


/**
 * Obtains a set of markers delimiting sections in a URL string
 *
 * \param url_s		URL string
 * \param markers	Updated to mark sections in the URL string
 * \param joining	True iff URL string is a relative URL for joining
 */
static void nsurl__get_string_markers(const char const *url_s,
		struct url_markers *markers, bool joining)
{
	const char *pos = url_s; /** current position in url_s */
	bool is_http = false;

	/* Initialise marker set */
	struct url_markers marker = { 0, 0, 0,   0, 0, 0,   0, 0, 0,   0 };

	/* Skip any leading whitespace in url_s */
	while (isspace(*pos))
		pos++;

	/* Record start point */
	marker.start = pos - url_s;

	marker.authority = marker.colon_first = marker.at =
			marker.colon_last = marker.path = marker.start;

	/* Get scheme */
	if (isalpha(*pos)) {
		pos++;

		while (*pos != ':' && *pos != '\0') {

			if (!isalnum(*pos) && *pos != '+' &&
					*pos != '-' && *pos != '.') {
				/* This character is not valid in the
				 * scheme */
				break;
			}
			pos++;
		}

		if (*pos == ':') {
			/* This delimits the end of the scheme */
			size_t off;

			marker.scheme_end = pos - url_s;

			off = marker.scheme_end - marker.start;

			/* Detect http(s) for scheme specifc normalisation */
			if (off == SLEN("http") &&
					(((*(pos - off + 0) == 'h') ||
					  (*(pos - off + 0) == 'H')) &&
					 ((*(pos - off + 1) == 't') ||
					  (*(pos - off + 1) == 'T')) &&
					 ((*(pos - off + 2) == 't') ||
					  (*(pos - off + 2) == 'T')) &&
					 ((*(pos - off + 3) == 'p') ||
					  (*(pos - off + 3) == 'P')))) {
				is_http = true;
			} else if (off == SLEN("https") &&
					(((*(pos - off + 0) == 'h') ||
					  (*(pos - off + 0) == 'H')) &&
					 ((*(pos - off + 1) == 't') ||
					  (*(pos - off + 1) == 'T')) &&
					 ((*(pos - off + 2) == 't') ||
					  (*(pos - off + 2) == 'T')) &&
					 ((*(pos - off + 3) == 'p') ||
					  (*(pos - off + 3) == 'P')) &&
					 ((*(pos - off + 3) == 's') ||
					  (*(pos - off + 3) == 'S')))) {
				is_http = true;
			}

			/* Skip over colon */
			pos++;

			/* Mark place as start of authority */
			marker.authority = marker.colon_first = marker.at =
					marker.colon_last = marker.path =
					pos - url_s;

		} else {
			/* Not found a scheme  */
			if (joining == false) {
				/* Assuming no scheme == http */
				is_http = true;
			}
		}
	}

	/* Get authority
	 *
	 * If this is a relative url that is to be joined onto a base URL, we
	 * require two slashes to be certain we correctly handle a missing
	 * authority.
	 *
	 * If this URL is not getting joined, we are less strict in the case of
	 * http(s) and will accept any number of slashes, including 0.
	 */
	if (*pos != '\0' && ((joining == false && is_http == true) ||
			(*pos == '/' && *(pos + 1) == '/'))) {
		/* Skip over leading slashes */
		if (is_http == false) {
			if (*pos == '/') pos++;
			if (*pos == '/') pos++;
		} else {
			while (*pos == '/')
				pos++;
		}

		marker.authority = marker.colon_first = marker.at =
				marker.colon_last = marker.path = pos - url_s;

		/* Need to get (or complete) the authority */
		do {
			if (*pos == '/' || *pos == '?' || *pos == '#') {
				/* End of the authority */
				break;

			} else if (*pos == ':' && marker.colon_first ==
					marker.authority) {
				/* could be username:password or host:port
				 * separator */
				marker.colon_first = pos - url_s;

			} else if (*pos == ':' && marker.colon_first !=
					marker.authority) {
				/* could be host:port separator */
				marker.colon_last = pos - url_s;

			} else if (*pos == '@' && marker.at ==
					marker.authority) {
				/* Credentials @ host separator */
				marker.at = pos - url_s;
			}
		} while (*(++pos) != '\0');

		marker.path = pos - url_s;
	}

	/* Get path
	 *
	 * Needs to start with '/' if there's no authority
	 */
	if (*pos == '/' || ((marker.path == marker.authority) &&
			(*pos != '?') && (*pos != '#') && (*pos != '\0'))) {
		while (*(++pos) != '\0') {
			if (*pos == '?' || *pos == '#') {
				/* End of the path */
				break;
			}
		}
	}

	marker.query = pos - url_s;

	/* Get query */
	if (*pos == '?') {
		while (*(++pos) != '\0') {
			if (*pos == '#') {
				/* End of the query */
				break;
			}
		}
	}

	marker.fragment = pos - url_s;

	/* Get fragment */
	if (*pos == '#') {
		while (*(++pos) != '\0')
			;
	}

	/* We got to the end of url_s.
	 * Need to skip back over trailing whitespace to find end of URL */
	pos--;
	while (isspace(*pos))
		pos--;
	marker.end = pos + 1 - url_s;

	/* Got all the URL components pegged out now */
	*markers = marker;
}


/**
 * Remove dot segments from a path, as per rfc 3986, 5.2.4
 *
 * \param path		path to remove dot segments from ('\0' terminated)
 * \param output	path with dot segments removed
 * \return size of output
 */
static size_t nsurl__remove_dot_segments(char *path, char *output)
{
	char *path_pos = path;
	char *output_pos = output;

	while (*path_pos != '\0') {
#ifdef NSURL_DEBUG
		LOG((" in:%s", path_pos));
		LOG(("out:%.*s", output_pos - output, output));
#endif
		if (*path_pos == '.') {
			if (*(path_pos + 1) == '.' &&
					*(path_pos + 2) == '/') {
				/* Found prefix of "../" */
				path_pos += SLEN("../");
				continue;

			} else if (*(path_pos + 1) == '/') {
				/* Found prefix of "./" */
				path_pos += SLEN("./");
				continue;
			}
		} else if (*path_pos == '/' && *(path_pos + 1) == '.') {
			if (*(path_pos + 2) == '/') {
				/* Found prefix of "/./" */
				path_pos += SLEN("/.");
				continue;

			} else if (*(path_pos + 2) == '\0') {
				/* Found "/." at end of path */
				*(output_pos++) = '/';

				/* End of input path */
				break;

			} else if (*(path_pos + 2) == '.') {
				if (*(path_pos + 3) == '/') {
					/* Found prefix of "/../" */
					path_pos += SLEN("/..");

					if (output_pos > output)
						output_pos--;
					while (output_pos > output &&
							*output_pos != '/')
						output_pos--;

					continue;

				} else if (*(path_pos + 3) == '\0') {
					/* Found "/.." at end of path */

					while (output_pos > output &&
							*(output_pos -1 ) !='/')
						output_pos--;

					/* End of input path */
					break;
				}
			}
		} else if (*path_pos == '.') {
			if (*(path_pos + 1) == '\0') {
				/* Found "." at end of path */

				/* End of input path */
				break;

			} else if (*(path_pos + 1) == '.' &&
					*(path_pos + 2) == '\0') {
				/* Found ".." at end of path */

				/* End of input path */
				break;
			}
		}
		/* Copy first character into output path */
		*output_pos++ = *path_pos++;

		/* Copy up to but not including next '/' */
		  while ((*path_pos != '/') && (*path_pos != '\0'))
		  	*output_pos++ = *path_pos++;
	}

	return output_pos - output;
}


/**
 * Get the length of the longest section
 *
 * \param m	markers delimiting url sections in a string
 * \return the length of the longest section
 */
static size_t nsurl__get_longest_section(struct url_markers *m)
{
	size_t length = m->scheme_end - m->start;	/* scheme */

	if (length < m->at - m->authority)		/* credentials */
		length = m->at - m->authority;

	if (length < m->path - m->at)			/* host */
		length = m->path - m->at;

	if (length < m->query - m->path)		/* path */
		length = m->query - m->path;

	if (length < m->fragment - m->query)		/* query */
		length = m->fragment - m->query;

	if (length < m->end - m->fragment)		/* fragment */
		length = m->end - m->fragment;

	return length;
}


/**
 * Converts two hexadecimal digits to a single number
 *
 * \param c1	most significant hex digit
 * \param c2	least significant hex digit
 * \return the total value of the two digit hex number
 *
 * For unescaping url encoded characters.
 */
static inline int nsurl__get_ascii_offset(char c1, char c2)
{
	int offset;

	/* Use 1st char as most significant hex digit */
	if (isdigit(c1))
		offset = 16 * (c1 - '0');
	else if (c1 >= 'a' && c1 <= 'f')
		offset = 16 * (c1 - 'a' + 10);
	else
		/* TODO: return something special to indicate error? */
		return 0;

	/* Use 2nd char as least significant hex digit and sum */
	if (isdigit(c2))
		offset += c2 - '0';
	else if (c2 >= 'a' && c2 <= 'f')
		offset += c2 - 'a' + 10;
	else
		/* TODO: return something special to indicate error? */
		return 0;

	return offset;
}


/**
 * Create the components of a NetSurf URL object for a section of a URL string
 *
 * \param url_s		URL string
 * \param section	Sets which section of URL string is to be normalised
 * \param pegs		Set of markers delimiting the URL string's sections
 * \param pos_norm	A buffer large enough for the normalised string (*3 + 1)
 * \param url		A NetSurf URL object, to which components may be added
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * The section of url_s is normalised appropriately.
 */
static nserror nsurl__create_from_section(const char const *url_s,
		const enum url_sections section,
		const struct url_markers *pegs,
		char *pos_norm,
		nsurl *url)
{
	int ascii_offset;
	int start;
	int end;
	const char *pos;
	const char *pos_url_s;
	char *norm_start = pos_norm;
	size_t copy_len;
	size_t length;
	enum {
		NSURL_F_IS_HTTP		= (1 << 0),
		NSURL_F_IS_HTTPS	= (1 << 1),
		NSURL_F_NO_PORT		= (1 << 2)
	} flags = 0;

	switch (section) {
	case URL_SCHEME:
		start = pegs->start;
		end = pegs->scheme_end;
		break;

	case URL_CREDENTIALS:
		start = pegs->authority;
		end = pegs->at;
		break;

	case URL_HOST:
		start = (pegs->at == pegs->authority &&
				*(url_s + pegs->at) != '@') ?
				pegs->at :
				pegs->at + 1;
		end = pegs->path;
		break;

	case URL_PATH:
		start = pegs->path;
		end = pegs->query;
		break;

	case URL_QUERY:
		start = pegs->query;
		end = pegs->fragment;
		break;

	case URL_FRAGMENT:
		start = pegs->fragment;
		end = pegs->end;
		break;
	}

	length = end - start;

	/* Stage 1: Normalise the required section */

	pos = pos_url_s = url_s + start;
	copy_len = 0;
	for (; pos < url_s + end; pos++) {
		if (*pos == '%' && (pos + 2 < url_s + end)) {
			/* Might be an escaped character needing unescaped */

			/* Find which character which was escaped */
			ascii_offset = nsurl__get_ascii_offset(*(pos + 1),
					*(pos + 2));

			if (ascii_offset <= 0x20 ||
					strchr(";/?:@&=+$,<>#%\"{}|\\^[]`",
							ascii_offset) ||
					ascii_offset >= 0x7f) {
				/* This character should be escaped after all,
				 * just let it get copied */
				copy_len += 3;
				pos += 2;
				continue;
			}

			if (copy_len > 0) {
				/* Copy up to here */
				memcpy(pos_norm, pos_url_s, copy_len);
				pos_norm += copy_len;
				copy_len = 0;
			}

			/* Put the unescaped character in the normalised URL */
			*(pos_norm++) = (char)ascii_offset;
			pos += 2;
			pos_url_s = pos + 1;

			length -= 2;

		} else if (isspace(*pos)) {
			/* This whitespace needs to be escaped */

			if (copy_len > 0) {
				/* Copy up to here */
				memcpy(pos_norm, pos_url_s, copy_len);
				pos_norm += copy_len;
				copy_len = 0;
			}
			/* escape */

			*(pos_norm++) = '%';
			*(pos_norm++) = digit2lowcase_hex(*pos >> 4);
			*(pos_norm++) = digit2lowcase_hex(*pos & 0xf);
			pos_url_s = pos + 1;

			length += 2;

		} else if ((section == URL_SCHEME || section == URL_HOST) &&
				isupper(*pos)) {
			/* Lower case this letter */

			if (copy_len > 0) {
				/* Copy up to here */
				memcpy(pos_norm, pos_url_s, copy_len);
				pos_norm += copy_len;
				copy_len = 0;
			}
			/* Copy lower cased letter into normalised URL */
			*(pos_norm++) = tolower(*pos);
			pos_url_s = pos + 1;

		} else {
			/* This character is safe in normalised URL */
			copy_len++;
		}
	}

	if (copy_len > 0) {
		/* Copy up to here */
		memcpy(pos_norm, pos_url_s, copy_len);
		pos_norm += copy_len;
	}

	/* Mark end of section */
	(*pos_norm) = '\0';

	/* Stage 2: Create the URL components for the required section */
	switch (section) {
	case URL_SCHEME:
		if (length == 0 || (length == SLEN("http") &&
				strncmp(norm_start, "http",
						SLEN("http")) == 0)) {
			flags |= NSURL_F_IS_HTTP;
		} else if (length == SLEN("https") &&
				strncmp(norm_start, "https",
						SLEN("https")) == 0) {
			flags |= NSURL_F_IS_HTTPS;
		}

		if (length == 0) {
			/* No scheme, assuming http, and add to URL */
			if (lwc_intern_string("http", SLEN("http"),
					&url->scheme) != lwc_error_ok) {
				return NSERROR_NOMEM;
			}
		} else {
			/* Add scheme to URL */
			if (lwc_intern_string(norm_start, length,
					&url->scheme) != lwc_error_ok) {
				return NSERROR_NOMEM;
			}
		}

		break;

	case URL_CREDENTIALS:
		if (length != 0 && *norm_start != ':') {
			char *sec_start = norm_start;
			if (pegs->colon_first != pegs->authority &&
					pegs->at > pegs->colon_first + 1) {
				/* there's a password */
				sec_start += pegs->colon_first -
						pegs->authority + 1;
				if (lwc_intern_string(sec_start,
						pegs->at - pegs->colon_first -1,
						&url->password) !=
						lwc_error_ok) {
					return NSERROR_NOMEM;
				}

				/* update start pos and length for username */
				sec_start = norm_start;
				length -= pegs->at - pegs->colon_first;
			} else if (pegs->colon_first != pegs->authority &&
					pegs->at == pegs->colon_first + 1) {
				/* strip username colon */
				length--;
			}

			/* Username */
			if (lwc_intern_string(sec_start, length,
					&url->username) != lwc_error_ok) {
				return NSERROR_NOMEM;
			}
		}

		break;

	case URL_HOST:
		if (length != 0) {
			size_t colon;
			char *sec_start = norm_start;
			if (pegs->at < pegs->colon_first &&
					pegs->colon_last == pegs->authority) {
				/* There's one colon and it's after @ marker */
				colon = pegs->colon_first;
			} else if (pegs->colon_last != pegs->authority) {
				/* There's more than one colon */
				colon = pegs->colon_last;
			} else {
				/* There's no colon that could be a port
				 * separator */
				flags |= NSURL_F_NO_PORT;
			}

			if (!(flags & NSURL_F_NO_PORT)) {
				/* Determine whether colon is a port separator
				 */
				sec_start += colon - pegs->at;
				while (++sec_start < norm_start + length) {
					if (!isdigit(*sec_start)) {
						/* Character after port isn't a
						 * digit; not a port separator
						 */
						flags |= NSURL_F_NO_PORT;
						break;
					}
				}
			}

			if (!(flags & NSURL_F_NO_PORT)) {
				/* There's a port */
				sec_start = norm_start + colon - pegs->at + 1;
				if (flags & NSURL_F_IS_HTTP &&
						length -
						(colon - pegs->at + 1) == 2 &&
						*sec_start == '8' &&
						*(sec_start + 1) == '0') {
					/* Scheme is http, and port is default
					 * (80) */
					flags |= NSURL_F_NO_PORT;
				}

				if (length - (colon - pegs->at + 1) <= 0) {
					/* No space for a port after the colon
					 */
					flags |= NSURL_F_NO_PORT;
				}

				/* Add non-redundant ports to NetSurf URL */
				sec_start = norm_start + colon - pegs->at + 1;
				if (!(flags & NSURL_F_NO_PORT) &&
						lwc_intern_string(sec_start,
						length - (colon - pegs->at + 1),
						&url->port) != lwc_error_ok) {
					return NSERROR_NOMEM;
				}

				/* update length for host */
				length = colon - pegs->at;
			}

			/* host */
			if (lwc_intern_string(norm_start, length,
					&url->host) != lwc_error_ok) {
				return NSERROR_NOMEM;
			}
		}

		break;

	case URL_PATH:
		if (length != 0) {
			if (lwc_intern_string(norm_start, length,
					&url->path) != lwc_error_ok) {
				return NSERROR_NOMEM;
			}
		} else if (url->host != NULL) {
			/* Set empty path to "/", if there's a host */
			if (lwc_intern_string("/", SLEN("/"),
					&url->path) != lwc_error_ok) {
				return NSERROR_NOMEM;
			}
		}

		break;

	case URL_QUERY:
		if (length != 0) {
			if (lwc_intern_string(norm_start, length,
					&url->query) != lwc_error_ok) {
				return NSERROR_NOMEM;
			}
		}

		break;

	case URL_FRAGMENT:
		if (length != 0) {
			if (lwc_intern_string(norm_start, length,
					&url->fragment) != lwc_error_ok) {
				return NSERROR_NOMEM;
			}
		}

		break;
	}

	return NSERROR_OK;
}


#ifdef NSURL_DEBUG
/**
 * Dump a NetSurf URL's internal components
 *
 * \param url	The NetSurf URL to dump components of
 */
static void nsurl__dump(const nsurl *url)
{
	if (url->scheme)
		LOG(("  Scheme: %s", lwc_string_data(url->scheme)));

	if (url->username)
		LOG(("Username: %s", lwc_string_data(url->username)));

	if (url->password)
		LOG(("Password: %s", lwc_string_data(url->password)));

	if (url->host)
		LOG(("    Host: %s", lwc_string_data(url->host)));

	if (url->port)
		LOG(("    Port: %s", lwc_string_data(url->port)));

	if (url->path)
		LOG(("    Path: %s", lwc_string_data(url->path)));

	if (url->query)
		LOG(("   Query: %s", lwc_string_data(url->query)));

	if (url->fragment)
		LOG(("Fragment: %s", lwc_string_data(url->fragment)));
}
#endif


/******************************************************************************
 * NetSurf URL Public API                                                     *
 ******************************************************************************/

/* exported interface, documented in nsurl.h */
nserror nsurl_create(const char const *url_s, nsurl **url)
{
	struct url_markers m;
	size_t length;
	char *buff;
	nserror e = NSERROR_OK;

	assert(url_s != NULL);

	/* Peg out the URL sections */
	nsurl__get_string_markers(url_s, &m, false);

	/* Get the length of the longest section */
	length = nsurl__get_longest_section(&m);

	/* Create NetSurf URL object */
	*url = calloc(1, sizeof(nsurl));
	if (*url == NULL) {
		return NSERROR_NOMEM;
	}

	/* Allocate enough memory to url escape the longest section */
	buff = malloc(length * 3 + 1);
	if (buff == NULL) {
		free(*url);
		return NSERROR_NOMEM;
	}

	/* Build NetSurf URL object from sections */
	e |= nsurl__create_from_section(url_s, URL_SCHEME, &m, buff, *url);
	e |= nsurl__create_from_section(url_s, URL_CREDENTIALS, &m, buff, *url);
	e |= nsurl__create_from_section(url_s, URL_HOST, &m, buff, *url);
	e |= nsurl__create_from_section(url_s, URL_PATH, &m, buff, *url);
	e |= nsurl__create_from_section(url_s, URL_QUERY, &m, buff, *url);
	e |= nsurl__create_from_section(url_s, URL_FRAGMENT, &m, buff, *url);

	/* Finished with buffer */
	free(buff);

	if (e != NSERROR_OK) {
		free(*url);
		return NSERROR_NOMEM;
	}

	/* Get the complete URL string */
	if (nsurl_get(*url, NSURL_WITH_FRAGMENT, &((*url)->string),
			&((*url)->length)) != NSERROR_OK) {
		free(*url);
		return NSERROR_NOMEM;
	}

	/* Give the URL a reference */
	(*url)->count = 1;

	return NSERROR_OK;
}


/* exported interface, documented in nsurl.h */
nsurl *nsurl_ref(nsurl *url)
{
	assert(url != NULL);

	url->count++;

	return url;
}


/* exported interface, documented in nsurl.h */
void nsurl_unref(nsurl *url)
{
	assert(url != NULL);

	if (--url->count > 0)
		return;

#ifdef NSURL_DEBUG
	nsurl__dump(url);
#endif

	/* Release lwc strings */
	if (url->scheme)
		lwc_string_unref(url->scheme);

	if (url->username)
		lwc_string_unref(url->username);

	if (url->password)
		lwc_string_unref(url->password);

	if (url->host)
		lwc_string_unref(url->host);

	if (url->port)
		lwc_string_unref(url->port);

	if (url->path)
		lwc_string_unref(url->path);

	if (url->query)
		lwc_string_unref(url->query);

	if (url->fragment)
		lwc_string_unref(url->fragment);

	free(url->string);

	/* Free the NetSurf URL */
	free(url);
}


/* exported interface, documented in nsurl.h */
nserror nsurl_compare(const nsurl *url1, const nsurl *url2,
		nsurl_component parts, bool *match)
{
	assert(url1 != NULL);
	assert(url2 != NULL);

	*match = true;

	/* Compare URL components */

	/* Path, host and query first, since they're most likely to differ */

	if (parts & NSURL_PATH) {
		nsurl__component_compare(url1->path, url2->path, match);

		if (*match == false)
			return NSERROR_OK;
	}

	if (parts & NSURL_HOST) {
		nsurl__component_compare(url1->host, url2->host, match);

		if (*match == false)
			return NSERROR_OK;
	}

	if (parts & NSURL_QUERY) {
		nsurl__component_compare(url1->query, url2->query, match);

		if (*match == false)
			return NSERROR_OK;
	}

	if (parts & NSURL_SCHEME) {
		nsurl__component_compare(url1->scheme, url2->scheme, match);

		if (*match == false)
			return NSERROR_OK;
	}

	if (parts & NSURL_USERNAME) {
		nsurl__component_compare(url1->username, url2->username, match);

		if (*match == false)
			return NSERROR_OK;
	}

	if (parts & NSURL_PASSWORD) {
		nsurl__component_compare(url1->password, url2->password, match);

		if (*match == false)
			return NSERROR_OK;
	}

	if (parts & NSURL_PORT) {
		nsurl__component_compare(url1->port, url2->port, match);

		if (*match == false)
			return NSERROR_OK;
	}

	if (parts & NSURL_FRAGMENT) {
		nsurl__component_compare(url1->fragment, url2->fragment, match);

		if (*match == false)
			return NSERROR_OK;
	}

	*match = true;
	return NSERROR_OK;
}


/* exported interface, documented in nsurl.h */
nserror nsurl_get(const nsurl *url, nsurl_component parts,
		char **url_s, size_t *url_l)
{
	size_t scheme;
	size_t username;
	size_t password;
	size_t host;
	size_t port;
	size_t path;
	size_t query;
	size_t fragment;
	char *pos;
	enum {
		NSURL_F_SCHEME			= (1 << 0),
		NSURL_F_SCHEME_PUNCTUATION	= (1 << 1),
		NSURL_F_AUTHORITY_PUNCTUATION	= (1 << 2),
		NSURL_F_USERNAME		= (1 << 3),
		NSURL_F_PASSWORD		= (1 << 4),
		NSURL_F_CREDENTIALS_PUNCTUATION	= (1 << 5),
		NSURL_F_HOST			= (1 << 6),
		NSURL_F_PORT			= (1 << 7),
		NSURL_F_AUTHORITY		= (NSURL_F_USERNAME |
							NSURL_F_PASSWORD |
							NSURL_F_HOST |
							NSURL_F_PORT),
		NSURL_F_PATH			= (1 << 8),
		NSURL_F_QUERY			= (1 << 9),
		NSURL_F_FRAGMENT		= (1 << 10)
	} flags = 0;

	/* Intersection of required parts and available parts gives
	 * the output parts */
	if (url->scheme && parts & NSURL_SCHEME)
		flags |= NSURL_F_SCHEME;
	if (url->username && parts & NSURL_USERNAME)
		flags |= NSURL_F_USERNAME;
	if (url->password && parts & NSURL_PASSWORD)
		flags |= NSURL_F_PASSWORD;
	if (url->host && parts & NSURL_HOST)
		flags |= NSURL_F_HOST;
	if (url->port && parts & NSURL_PORT)
		flags |= NSURL_F_PORT;
	if (url->path && parts & NSURL_PATH)
		flags |= NSURL_F_PATH;
	if (url->query && parts & NSURL_QUERY)
		flags |= NSURL_F_QUERY;
	if (url->fragment && parts & NSURL_FRAGMENT)
		flags |= NSURL_F_FRAGMENT;

	/* Turn on any spanned punctuation */
	if ((flags & NSURL_F_SCHEME) && (parts > NSURL_SCHEME))
		flags |= NSURL_F_SCHEME_PUNCTUATION;
	if ((flags & NSURL_F_SCHEME) && (flags > NSURL_F_SCHEME) &&
			url->path && lwc_string_data(url->path)[0] == '/')
		flags |= NSURL_F_AUTHORITY_PUNCTUATION;
	if ((flags & (NSURL_F_USERNAME | NSURL_F_PASSWORD)) &&
				flags & NSURL_F_HOST)
		flags |= NSURL_F_CREDENTIALS_PUNCTUATION;

	/* Get total output length */
	*url_l = 0;

	if (flags & NSURL_F_SCHEME) {
		scheme = lwc_string_length(url->scheme);
		*url_l += scheme;
	}

	if (flags & NSURL_F_SCHEME_PUNCTUATION)
		*url_l += SLEN(":");

	if (flags & NSURL_F_AUTHORITY_PUNCTUATION)
		*url_l += SLEN("//");

	if (flags & NSURL_F_USERNAME) {
		username = lwc_string_length(url->username);
		*url_l += username;
	}

	if (flags & NSURL_F_PASSWORD) {
		password = lwc_string_length(url->password);
		*url_l += SLEN(":") + password;
	}

	if (flags & NSURL_F_CREDENTIALS_PUNCTUATION)
		*url_l += SLEN("@");

	if (flags & NSURL_F_HOST) {
		host = lwc_string_length(url->host);
		*url_l += host;
	}

	if (flags & NSURL_F_PORT) {
		port = lwc_string_length(url->port);
		*url_l += SLEN(":") + port;
	}

	if (flags & NSURL_F_PATH) {
		path = lwc_string_length(url->path);
		*url_l += path;
	}

	if (flags & NSURL_F_QUERY) {
		query = lwc_string_length(url->query);
		*url_l += query;
	}

	if (flags & NSURL_F_FRAGMENT) {
		fragment = lwc_string_length(url->fragment);
		*url_l += fragment;
	}

	if (*url_l == 0)
		return NSERROR_BAD_URL;

	/* Allocate memory for url string */
	*url_s = malloc(*url_l + 1); /* adding 1 for '\0' */
	if (*url_s == NULL) {
		return NSERROR_NOMEM;
	}

	/* Copy the required parts into the url string */
	pos = *url_s;

	if (flags & NSURL_F_SCHEME) {
		memcpy(pos, lwc_string_data(url->scheme), scheme);
		pos += scheme;
	}

	if (flags & NSURL_F_SCHEME_PUNCTUATION) {
		*(pos++) = ':';
	}

	if (flags & NSURL_F_AUTHORITY_PUNCTUATION) {
		*(pos++) = '/';
		*(pos++) = '/';
	}

	if (flags & NSURL_F_USERNAME) {
		memcpy(pos, lwc_string_data(url->username), username);
		pos += username;
	}

	if (flags & NSURL_F_PASSWORD) {
		*(pos++) = ':';
		memcpy(pos, lwc_string_data(url->password), password);
		pos += password;
	}

	if (flags & NSURL_F_CREDENTIALS_PUNCTUATION) {
		*(pos++) = '@';
	}

	if (flags & NSURL_F_HOST) {
		memcpy(pos, lwc_string_data(url->host), host);
		pos += host;
	}

	if (flags & NSURL_F_PORT) {
		*(pos++) = ':';
		memcpy(pos, lwc_string_data(url->port), port);
		pos += port;
	}

	if (flags & NSURL_F_PATH) {
		memcpy(pos, lwc_string_data(url->path), path);
		pos += path;
	}

	if (flags & NSURL_F_QUERY) {
		memcpy(pos, lwc_string_data(url->query), query);
		pos += query;
	}

	if (flags & NSURL_F_FRAGMENT) {
		memcpy(pos, lwc_string_data(url->fragment), fragment);
		pos += fragment;
	}

	*pos = '\0';

	return NSERROR_OK;
}


/* exported interface, documented in nsurl.h */
const char *nsurl_access(const nsurl *url)
{
	assert(url != NULL);

	return url->string;
}


/* exported interface, documented in nsurl.h */
nserror nsurl_join(const nsurl *base, const char *rel, nsurl **joined)
{
	struct url_markers m;
	size_t length;
	char *buff;
	char *buff_pos;
	char *buff_start;
	nserror error = 0;
	enum {
		NSURL_F_REL		=  0,
		NSURL_F_BASE_SCHEME	= (1 << 0),
		NSURL_F_BASE_AUTHORITY	= (1 << 1),
		NSURL_F_BASE_PATH	= (1 << 2),
		NSURL_F_MERGED_PATH	= (1 << 3),
		NSURL_F_BASE_QUERY	= (1 << 4)
	} joined_parts;

	assert(base != NULL);
	assert(rel != NULL);

	/* Peg out the URL sections */
	nsurl__get_string_markers(rel, &m, true);

	/* Get the length of the longest section */
	length = nsurl__get_longest_section(&m);

	/* Initially assume that the joined URL can be formed entierly from
	 * the relative URL. */
	joined_parts = NSURL_F_REL;

	/* Update joined_compnents to indicate any required parts from the
	 * base URL. */
	if (m.scheme_end - m.start <= 0) {
		/* The relative url has no scheme.
		 * Use base URL's scheme. */
		joined_parts |= NSURL_F_BASE_SCHEME;

		if (m.path - m.authority <= 0) {
			/* The relative URL has no authority.
			 * Use base URL's authority. */
			joined_parts |= NSURL_F_BASE_AUTHORITY;

			if (m.query - m.path <= 0) {
				/* The relative URL has no path.
				 * Use base URL's path. */
				joined_parts |= NSURL_F_BASE_PATH;

				if (m.fragment - m.query <= 0) {
					/* The relative URL has no query.
					 * Use base URL's query. */
					joined_parts |= NSURL_F_BASE_QUERY;
				}

			} else if (*(rel + m.path) != '/') {
				/* Relative URL has relative path */
				joined_parts |= NSURL_F_MERGED_PATH;
			}
		}
	}

	/* Create NetSurf URL object */
	*joined = calloc(1, sizeof(nsurl));
	if (*joined == NULL) {
		return NSERROR_NOMEM;
	}

	/* Allocate enough memory to url escape the longest section, plus
	 * space for path merging (if required). */
	if (joined_parts & NSURL_F_MERGED_PATH) {
		/* Need to merge paths */
		length += lwc_string_length(base->path);
	}
	length *= 4;
	/* Plus space for removing dots from path */
	length += (m.query - m.path) + lwc_string_length(base->path);
	buff = malloc(length + 5);
	if (buff == NULL) {
		free(*joined);
		return NSERROR_NOMEM;
	}

	buff_start = buff_pos = buff;

	/* Form joined URL from base or rel components, as appropriate */

	if (joined_parts & NSURL_F_BASE_SCHEME)
		(*joined)->scheme = nsurl__component_copy(base->scheme);
	else
		error |= nsurl__create_from_section(rel, URL_SCHEME, &m,
				buff, *joined);

	if (joined_parts & NSURL_F_BASE_AUTHORITY) {
		(*joined)->username = nsurl__component_copy(base->username);
		(*joined)->password = nsurl__component_copy(base->password);
		(*joined)->host = nsurl__component_copy(base->host);
		(*joined)->port = nsurl__component_copy(base->port);
	} else {
		error |= nsurl__create_from_section(rel, URL_CREDENTIALS, &m,
				buff, *joined);
		error |= nsurl__create_from_section(rel, URL_HOST, &m,
				buff, *joined);
	}

	if (joined_parts & NSURL_F_BASE_PATH) {
		(*joined)->path = nsurl__component_copy(base->path);

	} else if (joined_parts & NSURL_F_MERGED_PATH) {
		struct url_markers m_path;
		size_t path_len;
		size_t new_length;

		if (base->host != NULL && base->path == NULL) {
			/* Append relative path to "/". */
			*(buff_pos++) = '/';
			memcpy(buff_pos, rel + m.path, m.query - m.path);
			buff_pos += m.query - m.path;

			path_len = 1 + m.query - m.path;

		} else {
			/* Append relative path to all but last segment of
			 * base path. */
			size_t path_end = lwc_string_length(base->path);
			const char *path = lwc_string_data(base->path);

			while (*(path + path_end) != '/' &&
					path_end != 0) {
				path_end--;
			}
			if (*(path + path_end) == '/')
				path_end++;

			/* Copy the base part */
			memcpy(buff_pos, path, path_end);
			buff_pos += path_end;

			/* Copy the relative part */
			memcpy(buff_pos, rel + m.path, m.query - m.path);
			buff_pos += m.query - m.path;

			path_len = path_end + m.query - m.path;
		}

		/* add termination to string */
		*buff_pos++ = '\0';

		new_length = nsurl__remove_dot_segments(buff, buff_pos);

		m_path.path = 0;
		m_path.query = new_length;

		buff_start = buff_pos + new_length;
		error |= nsurl__create_from_section(buff_pos, URL_PATH, &m_path,
				buff_start, *joined);

	} else {
		struct url_markers m_path;
		size_t new_length;

		memcpy(buff_pos, rel + m.path, m.query - m.path);
		buff_pos += m.query - m.path;
		*(buff_pos++) = '\0';

		new_length = nsurl__remove_dot_segments(buff, buff_pos);

		m_path.path = 0;
		m_path.query = new_length;

		buff_start = buff_pos + new_length;
		error |= nsurl__create_from_section(buff_pos, URL_PATH, &m_path,
				buff_start, *joined);
	}

	if (joined_parts & NSURL_F_BASE_QUERY)
		(*joined)->query = nsurl__component_copy(base->query);
	else
		error |= nsurl__create_from_section(rel, URL_QUERY, &m,
				buff, *joined);

	error |= nsurl__create_from_section(rel, URL_FRAGMENT, &m,
			buff, *joined);

	/* Free temporary buffer */
	free(buff);

	if (error != NSERROR_OK) {
		free(*joined);
		return NSERROR_NOMEM;
	}

	/* Get the complete URL string */
	if (nsurl_get(*joined, NSURL_WITH_FRAGMENT, &((*joined)->string),
			&((*joined)->length)) != NSERROR_OK) {
		free(*joined);
		return NSERROR_NOMEM;
	}

	/* Give the URL a reference */
	(*joined)->count = 1;

	return NSERROR_OK;
}


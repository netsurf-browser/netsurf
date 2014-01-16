/*
 * Copyright 2011 Daniel Silverstone <dsilvers@digital-scurf.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/config.h"
#include "utils/utils.h"
#include "utils/url.h"
#include "utils/utf8.h"

char *path_to_url(const char *path)
{
	int urllen;
	char *url;

	if (path == NULL) {
		return NULL;
	}
		
	urllen = strlen(path) + FILE_SCHEME_PREFIX_LEN + 1;

	url = malloc(urllen);
	if (url == NULL) {
		return NULL;
	}

	if (*path == '/') {
		path++; /* file: paths are already absolute */
	} 

	snprintf(url, urllen, "%s%s", FILE_SCHEME_PREFIX, path);

	return url;
}

char *url_to_path(const char *url)
{
	char *path;
	char *respath;
	url_func_result res; /* result from url routines */

	res = url_path(url, &path);
	if (res != URL_FUNC_OK) {
		return NULL;
	}

	res = url_unescape(path, &respath);
	free(path);
	if (res != URL_FUNC_OK) {
		return NULL;
	}

	return respath;
}


void warn_user(const char *warning, const char *detail)
{
  fprintf(stderr, "WARN %s %s\n", warning, detail);
}

void die(const char * const error)
{
  fprintf(stderr, "DIE %s\n", error);
  exit(EXIT_FAILURE);
}

utf8_convert_ret
utf8_to_local_encoding(const char *string, size_t len,
                       char **result)
{
  *result = strndup(string, len);
  return (*result == NULL) ? UTF8_CONVERT_NOMEM : UTF8_CONVERT_OK;
}

utf8_convert_ret
utf8_from_local_encoding(const char *string, size_t len,
                         char **result)
{
  *result = strndup(string, len);
  return (*result == NULL) ? UTF8_CONVERT_NOMEM : UTF8_CONVERT_OK;
}



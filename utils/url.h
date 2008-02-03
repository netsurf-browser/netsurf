/*
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
 * URL parsing and joining (interface).
 */

#ifndef _NETSURF_UTILS_URL_H_
#define _NETSURF_UTILS_URL_H_

typedef enum {
	URL_FUNC_OK,     /**< No error */
	URL_FUNC_NOMEM,  /**< Insufficient memory */
	URL_FUNC_FAILED  /**< Non fatal error (eg failed to match regex) */
} url_func_result;

struct url_components {
  	const char *buffer;
	const char *scheme;
	const char *authority;
	const char *path;
	const char *query;
	const char *fragment;
};

void url_init(void);
bool url_host_is_ip_address(const char *host);
url_func_result url_normalize(const char *url, char **result);
url_func_result url_join(const char *rel, const char *base, char **result);
url_func_result url_host(const char *url, char **result);
url_func_result url_scheme(const char *url, char **result);
url_func_result url_nice(const char *url, char **result,
		bool remove_extensions);
url_func_result url_escape(const char *unescaped, bool sptoplus,
		char **result);
url_func_result url_canonical_root(const char *url, char **result);
url_func_result url_parent(const char *url, char **result);
url_func_result url_plq(const char *url, char **result);
url_func_result url_path(const char *url, char **result);
url_func_result url_leafname(const char *url, char **result);
url_func_result url_fragment(const char *url, char **result);
url_func_result url_compare(const char *url1, const char *url2,
		bool nofrag, bool *result);

url_func_result url_get_components(const char *url,
		struct url_components *result);
char *url_reform_components(const struct url_components *components);
void url_destroy_components(const struct url_components *components);

char *path_to_url(const char *path);
char *url_to_path(const char *url);

#endif

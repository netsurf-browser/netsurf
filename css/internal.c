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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include "css/internal.h"

#include "utils/url.h"

/**
 * URL resolution callback for libcss
 *
 * \param pw    Resolution context
 * \param base  Base URI
 * \param rel   Relative URL
 * \param abs   Pointer to location to receive resolved URL
 * \return CSS_OK       on success,
 *         CSS_NOMEM    on memory exhaustion,
 *         CSS_INVALID  if resolution failed.
 */
css_error nscss_resolve_url(void *pw, const char *base, 
		lwc_string *rel, lwc_string **abs)
{
	lwc_error lerror;
	char *abs_url, *norm_url;
	url_func_result res;

	/* Resolve URI */
	res = url_join(lwc_string_data(rel), base, &abs_url);
	if (res != URL_FUNC_OK) {
		return res == URL_FUNC_NOMEM ? CSS_NOMEM : CSS_INVALID;
	}

	/* Normalise it */
	res = url_normalize(abs_url, &norm_url);
	if (res != URL_FUNC_OK) {
		free(abs_url);
		return res == URL_FUNC_NOMEM ? CSS_NOMEM : CSS_INVALID;
	}

	free(abs_url);

	/* Intern it */
	lerror = lwc_intern_string(norm_url, strlen(norm_url), abs);
	if (lerror != lwc_error_ok) {
		*abs = NULL;
		free(norm_url);
		return lerror == lwc_error_oom ? CSS_NOMEM : CSS_INVALID;
	}

	free(norm_url);

	return CSS_OK;
}


/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
#include <string.h>
#include <sys/types.h>
//#include "utils/url.h"
#include "desktop/cookies.h"

void warn_user(const char *warning, const char *detail)
{
	printf("WARNING: %s %s\n", warning, detail);
}

void die(const char *error)
{
	printf("die: %s\n", error);
	exit(1);
}

bool cookies_update(const char *domain, const struct cookie_data *data)
{ return true; }

char *url_to_path(const char *url)
{
	return strdup(url + 5);
}

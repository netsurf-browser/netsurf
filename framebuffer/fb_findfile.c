/*
 * Copyright 2008 Daniel Silverstone <dsilvers@netsurf-browser.org>
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

#include <limits.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/log.h"

#include "fb_findfile.h"

char *path_to_url(const char *path)
{
	char *r = malloc(strlen(path) + 7 + 1);

	strcpy(r, "file://");
	strcat(r, path);

	return r;
}

/**
 * Locate a shared resource file by searching known places in order.
 *
 * \param  buf      buffer to write to.  must be at least PATH_MAX chars
 * \param  filename file to look for
 * \param  def      default to return if file not found
 * \return buf
 *
 * Search order is: ~/.netsurf/, $NETSURFRES/ (where NETSURFRES is an
 * environment variable), and finally the path specified by NETSURF_FB_RESPATH
 * from the Makefile
 */

char *fb_find_resource(char *buf, const char *filename, const char *def)
{
	char *cdir = getenv("HOME");
	char t[PATH_MAX];

	if (cdir != NULL) {
		strcpy(t, cdir);
		strcat(t, "/.netsurf/");
		strcat(t, filename);
		if (realpath(t, buf) != NULL) {
                        if (access(buf, R_OK) == 0)
                                return buf;
                }
	}

	cdir = getenv("NETSURFRES");

	if (cdir != NULL) {
		if (realpath(cdir, buf) != NULL) {
                        strcat(buf, "/");
                        strcat(buf, filename);
                        if (access(buf, R_OK) == 0)
                                return buf;
                }
	}

	strcpy(t, NETSURF_FB_RESPATH);
	strcat(t, filename);
	if (realpath(t, buf) != NULL) {
                if (access(buf, R_OK) == 0)
                        return buf;
        }

	if (def[0] == '~') {
		snprintf(t, PATH_MAX, "%s%s", getenv("HOME"), def + 1);
		if (realpath(t, buf) == NULL) {
                        strcpy(buf, t);
                }
	} else {
		if (realpath(def, buf) == NULL) {
                        strcpy(buf, def);
                }
	}

	return buf;
}


/*
 * Local Variables:
 * c-basic-offset: 8
 * End:
 */


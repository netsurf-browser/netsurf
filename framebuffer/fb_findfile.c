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

#include "fb_findfile.h"

static bool
fb_findfile_exists(char *buffer, const char *base, const char *filename)
{
        if (base == NULL)
                return false;
        
        if (*base == '~') {
                snprintf(buffer, PATH_MAX, "%s/%s/%s", 
                         getenv("HOME") ? getenv("HOME") : "",
                         base + 1, filename);
        } else {
                snprintf(buffer, PATH_MAX, "%s/%s", base, filename);
        }
        
        return (access(buffer, R_OK) == 0);
}

char *
fb_findfile(const char *filename)
{
        static char buffer[PATH_MAX];

        /* Search sequence is:
         * home/filename
         * res-env/filename
         * resources/filename
         */
        
        if (fb_findfile_exists(buffer, NETSURF_FB_HOMEPATH, filename))
                return buffer;
        if (fb_findfile_exists(buffer, getenv("NETSURF_RES"), filename))
                return buffer;
        if (fb_findfile_exists(buffer, NETSURF_FB_RESPATH, filename))
                return buffer;
        
        return NULL;
}

char *
fb_findfile_asurl(const char *filename)
{
        static char buffer[PATH_MAX];
        char *f = fb_findfile(filename);
        
        if (f == NULL)
                return NULL;
        
        snprintf(buffer, PATH_MAX, "file://%s", f);
        
        return strdup(buffer);
}

/*
 * Local Variables:
 * c-basic-offset: 8
 * End:
 */


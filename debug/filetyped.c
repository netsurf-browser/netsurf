/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#include <stdlib.h>
#include "netsurf/content/fetch.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

/**
 * filetype -- determine the MIME type of a local file
 */

const char *fetch_filetype(const char *unix_path)
{
	int l;
	LOG(("unix path %s", unix_path));
	l = strlen(unix_path);
	if (2 < l && strcasecmp(unix_path + l - 3, "css") == 0)
		return "text/css";
	if (2 < l && strcasecmp(unix_path + l - 3, "jpg") == 0)
		return "image/jpeg";
	if (3 < l && strcasecmp(unix_path + l - 4, "jpeg") == 0)
		return "image/jpeg";
	return "text/html";
}


char *fetch_mimetype(const char *ro_path)
{
	return strdup("text/plain");
}

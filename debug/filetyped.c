/**
 * $Id: filetyped.c,v 1.1 2003/06/21 13:18:00 bursa Exp $
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
	LOG(("unix path %s", unix_path));
	if (strcasecmp(unix_path, "home/james/Projects/netsurf/CSS") == 0)
		return "text/css";
	return "text/html";
}


/*
 * Copyright 2008, 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
#include "desktop/cookies.h"
#include <proto/dos.h>
#include "utils/messages.h"
#include <stdlib.h>
#include <curl/curl.h>

void warn_user(const char *warning, const char *detail)
{
	TimedDosRequesterTags(TDR_ImageType,TDRIMAGE_WARNING,
							TDR_TitleString,messages_get("NetSurf"),
							TDR_GadgetString,messages_get("OK"),
//							TDR_CharSet,106,
							TDR_FormatString,"%s\n%s",
							TDR_Arg1,messages_get(warning),
							TDR_Arg2,detail,
							TAG_DONE);
}

void die(const char *error)
{
	TimedDosRequesterTags(TDR_ImageType,TDRIMAGE_ERROR,
							TDR_TitleString,messages_get("NetSurf"),
							TDR_GadgetString,messages_get("OK"),
//							TDR_CharSet,106,
							TDR_FormatString,"%s",
							TDR_Arg1,error,
							TAG_DONE);
	exit(1);
}

char *url_to_path(const char *url)
{
	char *tmps, *unesc;
	CURL *curl;

	tmps = strstr(url, "///localhost/") + 13;

	if(tmps < url) tmps = strstr(url,"///") + 3;

	if(tmps >= url)
	{
		if(curl = curl_easy_init())
		{
			unesc = curl_easy_unescape(curl,tmps,0,NULL);
			tmps = strdup(unesc);
			curl_free(unesc);
			curl_easy_cleanup(curl);
			return tmps;
		}
	}

	return strdup((char *)url);
}

char *path_to_url(const char *path)
{
	char *r = malloc(strlen(path) + 8 + 1);

	strcpy(r, "file:///");
	strcat(r, path);

	return r;
}

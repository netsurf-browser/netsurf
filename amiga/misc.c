/*
 * Copyright 2008-2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
#include "utils/utils.h"

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

/**
 * Return the filename part of a full path
 *
 * \param path full path and filename
 * \return filename (will be freed with free())
 */

char *filename_from_path(char *path)
{
	return strdup(FilePart(path));
}

/**
 * returns a string without escape chars or |M chars.
 * (based on remove_underscores from utils.c)
 * \param translate true to insert a linebreak where there was |M,
 *        and capitalise initial characters after escape chars.
 */

char *remove_escape_chars(const char *s, bool translate)
{
	size_t i, ii, len;
	char *ret;
	bool nextcharupper = false;
	len = strlen(s);
	ret = malloc(len + 1);
	if (ret == NULL)
		return NULL;
	for (i = 0, ii = 0; i < len; i++) {
		if ((s[i] != '\\') && (s[i] != '|')) {
			if(nextcharupper) {
				ret[ii++] = toupper(s[i]);
				nextcharupper = false;
			}
			else ret[ii++] = s[i];
		}
		else if ((translate) && (s[i] == '|') && (s[i+1] == 'M')) {
			ret[ii++] = '\n';
			i++;
		}
		else {
			if(translate) nextcharupper = true;
			i++;
		}
	}
	ret[ii] = '\0';
	return ret;
}

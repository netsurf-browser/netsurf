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
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include <proto/dos.h>

#include "amiga/utf8.h"
#include "desktop/cookies.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"

void warn_user(const char *warning, const char *detail)
{
	char *utf8warning = ami_utf8_easy(messages_get(warning));

	LOG(("%s %s", warning, detail));

	TimedDosRequesterTags(TDR_ImageType,TDRIMAGE_WARNING,
							TDR_TitleString,messages_get("NetSurf"),
							TDR_GadgetString,messages_get("OK"),
//							TDR_CharSet,106,
							TDR_FormatString,"%s\n%s",
							TDR_Arg1,utf8warning != NULL ? utf8warning : warning,
							TDR_Arg2,detail,
							TAG_DONE);

	if(utf8warning) free(utf8warning);
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
	char *tmps, *unesc, *slash, *colon, *url2;

	if (strncmp(url, "file://", SLEN("file://")) != 0)
		return NULL;

	url += SLEN("file://");

	if (strncmp(url, "localhost", SLEN("localhost")) == 0)
		url += SLEN("localhost");

	if (strncmp(url, "/", SLEN("/")) == 0)
		url += SLEN("/");

	if(*url == '\0')
		return NULL; /* file:/// is not a valid path */

	url2 = malloc(strlen(url) + 2);
	strcpy(url2, url);

	colon = strchr(url2, ':');
	if(colon == NULL)
	{
		if(slash = strchr(url2, '/'))
		{
			*slash = ':';
		}
		else
		{
			int len = strlen(url2);
			url2[len] = ':';
			url2[len + 1] = '\0';
		}
	}

	if(url_unescape(url2,&unesc) == URL_FUNC_OK)
		return unesc;

	return (char *)url2;
}

char *path_to_url(const char *path)
{
	char *colon = NULL;
	char *r = NULL;
	char newpath[1024 + strlen(path)];
	BPTR lock = 0;

	if(lock = Lock(path, MODE_OLDFILE))
	{
		DevNameFromLock(lock, newpath, sizeof newpath, DN_FULLPATH);
		UnLock(lock);
	}
	else strncpy(newpath, path, sizeof newpath);

	r = malloc(strlen(newpath) + SLEN("file:///") + 1);

	if(colon = strchr(newpath, ':')) *colon = '/';

	strcpy(r, "file:///");
	strcat(r, newpath);

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
 * Add a path component/filename to an existing path
 *
 * \param path buffer containing path + free space
 * \param length length of buffer "path"
 * \param newpart string containing path component to add to path
 * \return true on success
 */

bool path_add_part(char *path, int length, const char *newpart)
{
	if(AddPart(path, newpart, length)) return true;
		else return false;
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

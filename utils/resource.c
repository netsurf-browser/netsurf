/*
 * Copyright 2010 Vincent Sanders <vince@kyllikki.org>
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
 * Provides utility functions for finding readable files.
 *
 * These functions are intended to make finding resource files more straightforward.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "utils/config.h"
#include "utils/resource.h"

/** maximum number of elements in the resource vector */
#define MAX_RESPATH 128 

/* exported interface documented in findresource.h */
char *resource_vsfindfile(char *str, const char *format, va_list ap)
{
	char *realpathname;
	char *pathname;
	int len;

	pathname = malloc(PATH_MAX);
	if (pathname == NULL)
		return NULL; /* unable to allocate memory */

	len = vsnprintf(pathname, PATH_MAX, format, ap);

	if ((len < 0) || (len >= PATH_MAX)) {
		/* error or output exceeded PATH_MAX length so
		 * operation is doomed to fail.
		 */
		free(pathname);
		return NULL;
	}

	realpathname = realpath(pathname, str);
	
	free(pathname);
	
	if (realpathname != NULL) {
		/* sucessfully expanded pathname */
		if (access(realpathname, R_OK) != 0) {
			/* unable to read the file */
			return NULL;
		}	
	}

	return realpathname;
}

/* exported interface documented in findresource.h */
char *resource_sfindfile(char *str, const char *format, ...)
{
	va_list ap;
	char *ret;

	va_start(ap, format);
	ret = resource_vsfindfile(str, format, ap);
	va_end(ap);

	return ret;
}

/* exported interface documented in findresource.h */
char *resource_findfile(const char *format, ...)
{
	char *str;
	char *ret;
	va_list ap;

	str = malloc(PATH_MAX);
	if (str == NULL)
		return NULL; /* unable to allocate memory */

	va_start(ap, format);
	ret = resource_vsfindfile(str, format, ap);
	va_end(ap);

	if (ret == NULL)
		free(str);

	return ret;
}

/* exported interface documented in findresource.h */
char *resource_sfind(char **respathv, char *filepath, const char *filename)
{
	int respathc = 0;

	if ((respathv == NULL) || (respathv[0] == NULL) || (filepath == NULL))
		return NULL;

	while (respathv[respathc] != NULL) {
		if (resource_sfindfile(filepath, "%s/%s", respathv[respathc], filename) != NULL)
			return filepath;

		respathc++;
	}

	return NULL;
}

/* exported interface documented in findresource.h */
char *resource_find(char **respathv, const char *filename)
{
	char *ret;
	char *filepath;

	if ((respathv == NULL) || (respathv[0] == NULL))
		return NULL;

	filepath = malloc(PATH_MAX);
	if (filepath == NULL)
		return NULL;

	ret = resource_sfind(respathv, filepath, filename);

	if (ret == NULL)
		free(filepath);

	return ret;
}

/* exported interface documented in findresource.h */
char *resource_sfinddef(char **respathv, char *filepath, const char *filename, const char *def)
{
	char t[PATH_MAX];
	char *ret;

	if ((respathv == NULL) || (respathv[0] == NULL) || (filepath == NULL))
		return NULL;

	ret = resource_sfind(respathv, filepath, filename);

	if ((ret == NULL) && (def != NULL)) {
		/* search failed, return the path specified */
		ret = filepath;
		if (def[0] == '~') {
			snprintf(t, PATH_MAX, "%s/%s/%s", getenv("HOME"), def + 1, filename);
		} else {
			snprintf(t, PATH_MAX, "%s/%s", def, filename);
		}		
		if (realpath(t, ret) == NULL) {
			strcpy(ret, t);
		}

	}
	return ret;
}


/* exported interface documented in resource.h */
char **
resource_generate(char * const *pathv, const char * const *langv)
{
	char **respath; /* resource paths vector */
	int pathc = 0;
	int langc = 0;
	int respathc = 0;
	struct stat dstat;
	char tmppath[PATH_MAX];

	respath = calloc(MAX_RESPATH, sizeof(char *));

	while (pathv[pathc] != NULL) {
		if ((stat(pathv[pathc], &dstat) == 0) && 
		    S_ISDIR(dstat.st_mode)) {
			/* path element exists and is a directory */
			langc = 0;
			while (langv[langc] != NULL) {
				snprintf(tmppath, sizeof tmppath, "%s/%s", pathv[pathc],langv[langc]);
				if ((stat(tmppath, &dstat) == 0) && 
				    S_ISDIR(dstat.st_mode)) {
					/* path element exists and is a directory */
					respath[respathc++] = strdup(tmppath);
				}
				langc++;
			}
			respath[respathc++] = strdup(pathv[pathc]);
		}
		pathc++;
	}

	return respath;
}

/* expand ${} in a string into environment variables */
static char *
expand_path(const char *path)
{
	char *exp = strdup(path);
	int explen;
	int cstart = -1;
	int cloop = 0;
	char *envv;
	int envlen;
	int replen; /* length of replacement */

	if (exp == NULL)
		return NULL;

	explen = strlen(exp) + 1;

	while (exp[cloop] != 0) {
		if ((exp[cloop] == '$') && 
		    (exp[cloop + 1] == '{')) {
			cstart = cloop;
			cloop++;
		} 
		
		if ((cstart != -1) &&
		    (exp[cloop] == '}')) {
			replen = cloop - cstart;
			exp[cloop] = 0;
			envv = getenv(exp + cstart + 2);
			if (envv == NULL) {
				memmove(exp + cstart, 
					exp + cloop + 1, 
					explen - cloop - 1);
				explen -= replen;
			} else {
				envlen = strlen(envv);
				exp = realloc(exp, explen + envlen - replen);
				memmove(exp + cstart + envlen, 
					exp + cloop + 1, 
					explen - cloop - 1);
				memmove(exp + cstart, envv, envlen);
				explen += envlen - replen;
			}
			cloop -= replen;
			cstart = -1;
		}

		cloop++;
	}

	return exp;
}

/* exported interface documented in resource.h */
char **
resource_path_to_strvec(const char *path)
{
	char **strvec;
	int strc = 0;

	strvec = calloc(MAX_RESPATH, sizeof(char *));
	if (strvec == NULL)
		return NULL;

	strvec[strc] = expand_path(path);
	if (strvec[strc] == NULL) {
		free(strvec);
		return NULL;
	}
	strc++;

	strvec[strc] = strchr(strvec[0], ':');
	while ((strc < (MAX_RESPATH - 2)) && 
	       (strvec[strc] != NULL)) {
		/* null terminate previous entry */
		*strvec[strc] = 0; 
		strvec[strc]++;

		/* skip colons */
		while (*strvec[strc] == ':')
			strvec[strc]++;

		if (*strvec[strc] == 0)
			break; /* string is terminated */

		strc++;

		strvec[strc] = strchr(strvec[strc - 1], ':');
	}

	return strvec;
}

/* exported interface documented in resource.h */
void resource_free_strvec(char **pathv)
{
	free(pathv[0]);
	free(pathv);
}

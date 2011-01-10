/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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

#include <curl/curl.h>

#include "utils/log.h"
#include "utils/url.h"

#include "atari/findfile.h"

char *path_to_url(const char *path)
{
	int urllen = strlen(path) + FILE_SCHEME_PREFIX_LEN + 1;
	char *url = malloc(urllen);

	if (*path == '/') {
		path++; /* file: paths are already absolute */
	} 

	snprintf(url, urllen, "%s%s", FILE_SCHEME_PREFIX, path);

	return url;
}


char *url_to_path(const char *url)
{
	char *url_path = curl_unescape(url, 0);
	char *path;

	/* return the absolute path including leading / */
	path = strdup(url_path + (FILE_SCHEME_PREFIX_LEN - 1));
	curl_free(url_path);

	return path;
}


#ifdef PLAIN_TOS
#undef realpath
#undef access
#define access(f,m) (0)
#define PATH_SEP '/'
char * realpath(const char * path, char * rpath)
{
	size_t l;
	size_t i;
	char old;
	char new = PATH_SEP;

	if( rpath == NULL ){
		return( NULL );
	}

	if( PATH_SEP == '/') {
		/* replace '\' with / */
		old = 0x2F; /* / */
	} else {
		/* replace '/' with \ */
		old = '/';
	}

	if( path[0] == 0x2F || path[0] == '/' ){
		strcpy(rpath, "U:");
		strcat(rpath, path);
	}
	else if( path[0] == '.') {
		char cwd[PATH_MAX];
		getcwd((char*)&cwd, PATH_MAX);
		strcpy(rpath, (char*)&cwd);
		if( (path[1] == '/' || path[1] == 0x27 ) ) {
			strcat(rpath, &path[1]);
		} else {
			strcat(rpath, "/");
			strcat(rpath, path);
		}
	} else {
		strcpy(rpath, path);
	}
	/* convert path seperator to configured value: */
	l = strlen(rpath);
	for( i = 0; i<l-1; i++){
		if( rpath[i] == old ){
			rpath[i] = new;
		}
	}
	return( rpath );	
}
#endif

/**
 * Locate a shared resource file by searching known places in order.
 *
 * \param  buf      buffer to write to.  must be at least PATH_MAX chars
 * \param  filename file to look for
 * \param  def      default to return if file not found
 * \return buf
 *
 * Search order is: ./, NETSURF_GEM_RESPATH, ./$HOME/.netsurf/, $NETSURFRES/ (where NETSURFRES is an
 * environment variable), 
 */
#ifndef NETSURF_GEM_RESPATH
#define NETSURF_GEM_RESPATH "./res/"
#endif
char * atari_find_resource(char *buf, const char *filename, const char *def)
{
	char *cdir = NULL;
	char t[PATH_MAX];
	LOG(("%s (def: %s)", filename, def ));

	strcpy(t, NETSURF_GEM_RESPATH);
	strcat(t, filename);
	LOG(("checking %s", (char*)&t));
	if (realpath(t, buf) != NULL) {
		if (access(buf, R_OK) == 0) {
			return buf;
		}
	}

	strcpy(t, "./");
	strcat(t, filename);
	LOG(("checking %s", (char*)&t));
	if (realpath(t, buf) != NULL) {
		if (access(buf, R_OK) == 0) {
			return buf;
		}
	}

	cdir = getenv("HOME");
	if (cdir != NULL) {
		strcpy(t, cdir);
		strcat(t, "/.netsurf/");
		strcat(t, filename);
		LOG(("checking %s", (char*)&t));
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
			LOG(("checking %s", (char*)&t));
			if (access(buf, R_OK) == 0)
				return buf;
		}
	}

	if (def[0] == '~') {
		snprintf(t, PATH_MAX, "%s%s", getenv("HOME"), def + 1);
		LOG(("checking %s", (char*)&t));
		if (realpath(t, buf) == NULL) {
			strcpy(buf, t);
		}
	} else {
		LOG(("checking %s", (char*)def));
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


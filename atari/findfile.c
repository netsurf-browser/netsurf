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
#include <windom.h>

#include "utils/log.h"
#include "utils/url.h"
#include "atari/findfile.h"
#include "atari/gui.h"
#include "atari/misc.h"

extern unsigned short gdosversion;

static void fix_path(char * path);

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
	/* TODO: fix path seperator */
	if(gdosversion > TOS4VER ) {
		path = strdup(url_path + (FILE_SCHEME_PREFIX_LEN - 1));
	} else {
		/* do not include / within ulr_path */
		path = strdup(url_path + (FILE_SCHEME_PREFIX_LEN));
		int l = strlen(path);
		int i;
		for( i = 0; i<l-1; i++){
			if( path[i] == '/' ){
				path[i] = 0x5C;
			}
		} 
	}
	curl_free(url_path);
	LOG(("%s", path));
	return path;
}




/* convert nonsense getcwd path (returned by mintlib getcwd on plain TOS) */
static void fix_path(char * path)
{
	char npath[PATH_MAX];
	/* only apply fix to paths that contain /dev/ */
	if( strlen(path) < 6 ){
		return;
	}
	if( strncmp(path, "/dev/", 5) != 0 ) {
		return;
	}
	strncpy((char*)&npath, path, PATH_MAX);
	npath[0] = path[5];
	npath[1] = ':';
	npath[2] = 0;
	strcat((char*)&npath, &path[6]);
	strcpy(path, (char*)&npath);
}

/* 
 a fixed version of realpath() which returns valid 
 paths for TOS which have no root fs. (/ , U: )
*/
char * gdos_realpath(const char * path, char * rpath)
{
	size_t l;
	size_t i;
	char old;
	char fsep = 0x5C;
	if( rpath == NULL ){
		return( NULL );
	}
	if( gdosversion > TOS4VER ){
		return( realpath(path, rpath) );
	}

	if( fsep == '/') {
		/* replace '\' with / */
		old = 0x5C; /* / */
	} else {
		/* replace '/' with \ */
		old = '/';
	}

	if( path[0] != '/' && path[0] != 0x5c && path[1] != ':') {
		/* it is not an absolute path */
		char cwd[PATH_MAX];
		getcwd((char*)&cwd, PATH_MAX);
		fix_path((char*)&cwd);
		strcpy(rpath, (char*)&cwd);
		l = strlen(rpath);
		if(rpath[l-1] != 0x5C && rpath[l-1] != '/') {
			rpath[l] = fsep;
			rpath[l+1] = 0;
		}
		if( (path[1] == '/' || path[1] == 0x5C ) ) {
			strcat(rpath, &path[2]);
		} else {
			strcat(rpath, path);
		}
	} else {
		strcpy(rpath, path);
	}
	/* convert path seperator to configured value: */
	l = strlen(rpath);
	for( i = 0; i<l-1; i++){
		if( rpath[i] == old ){
			rpath[i] = fsep;
		}
	}
	return( rpath );	
}


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
	if (gdos_realpath(t, buf) != NULL) {
		if (access(buf, R_OK) == 0) {
			return buf;
		}
	}
	strcpy(t, "./");
	strcat(t, filename);
	LOG(("checking %s", (char*)&t));
	if (gdos_realpath(t, buf) != NULL) {
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
		if (gdos_realpath(t, buf) != NULL) {
			if (access(buf, R_OK) == 0)
				return buf;
		}
	}

	cdir = getenv("NETSURFRES");
	if (cdir != NULL) {
		if (gdos_realpath(cdir, buf) != NULL) {
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
		if (gdos_realpath(t, buf) == NULL) {
			strcpy(buf, t);
		}
	} else {
		LOG(("checking %s", (char*)def));
		if (gdos_realpath(def, buf) == NULL) {
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

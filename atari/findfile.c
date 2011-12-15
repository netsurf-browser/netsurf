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
#include <assert.h>
#include <curl/curl.h>
#include <windom.h>

#include "utils/log.h"
#include "utils/url.h"
#include "atari/findfile.h"
#include "atari/gui.h"
#include "atari/misc.h"
#include "atari/osspec.h"

char * local_file_to_url( const char * filename )
{
	#define BACKSLASH	0x5C
	char * url;
	if( strlen(filename) <= 2){
		return( NULL );
	}

	char * fname_local = alloca( strlen(filename)+1 );
	char * start = (char*)fname_local;
	strcpy( start, filename );

	/* if path points to unified filesystem, skip that info: */
	if( fname_local[1] == ':' && fname_local[0] == 'U' ){
		start = &fname_local[2];
	}

	/* if we got something like "C:\folder\file.txt", handle that: */
	if( start[1] == ':' ){
		start[1] = (char)tolower(start[0]);
		start++;
	}

	/* skip leading slash, already included in file scheme: */
	if( start[0] == (char)BACKSLASH || start[0] == '/' ){
		start++;
	}

	/* convert backslashes: */
	for( unsigned int i=0; i<strlen(start); i++ ){
		if( start[i] == BACKSLASH ){
			start[i] = '/';
		}
	}

	// TODO: make file path absolute if it isn't yet.
	url = malloc( strlen(start) + FILE_SCHEME_PREFIX_LEN + 1);
	strcpy( url, FILE_SCHEME_PREFIX );
	strcat( url, start );
	return( url );
	#undef BACKSLASH
}

/* convert an local path to an URL, memory for URL is allocated. */
char *path_to_url(const char *path_in)
{
	#define BACKSLASH	0x5C
	char * path_ptr=NULL;
	char * path;
	LOG(("path2url in: %s\n", path_in));

	if (*path_in == '/') {
		path_in++; /* file: path is are already absolute */
		path = (char*)path_in;
	} else {
		path = path_ptr = (char*)malloc(PATH_MAX+1);
		gemdos_realpath(path_in, path);

		if( *path == '/' || *path == BACKSLASH ) {
			path++;
		}
		if( sys_type() != SYS_MINT ){
			if( path[1] == ':' ) {
				path[1] = path[0];
				path++;
			}
		}
	}

	int urllen = strlen(path) + FILE_SCHEME_PREFIX_LEN + 1;
	char *url = malloc(urllen);

	snprintf(url, urllen, "%s%s", FILE_SCHEME_PREFIX, path);

	int i=0;
	while( url[i] != 0 ){
		if( url[i] == BACKSLASH ){
			url[i] = '/';
		}
		i++;
	}
	if( path_ptr )
		free( path_ptr );
	LOG(("path2url out: %s\n", url));
	return url;
	#undef BACKSLASH
}


char *url_to_path(const char *url)
{
	char *url_path = curl_unescape(url, 0);
	char *path;
	char abspath[PATH_MAX+1];
	LOG(( "url2path in: %s\n", url ));
	/* printf( "url2path in: %s\n", url_path ); */
	/* return the absolute path including leading / */
	/* todo: better check for filesystem? */
	if( sys_type() & SYS_MINT ) {
		/* it's ok to have relative paths with mint, just strip proto: */
		path = strdup(url_path + (FILE_SCHEME_PREFIX_LEN -1));
	} else {
		/* do not include / within url_path */
		char * tmp = url_path + (FILE_SCHEME_PREFIX_LEN-1);
		gemdos_realpath( tmp, (char*)&abspath );
		path = strdup( (char*)&abspath );
	}
	curl_free(url_path);
	LOG(( "url2path out: %s\n", path ));
	return path;
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
	if (gemdos_realpath(t, buf) != NULL) {
		if (access(buf, R_OK) == 0) {
			return buf;
		}
	}
	strcpy(t, "./");
	strcat(t, filename);
	LOG(("checking %s", (char*)&t));
	if (gemdos_realpath(t, buf) != NULL) {
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
		if (gemdos_realpath(t, buf) != NULL) {
			if (access(buf, R_OK) == 0)
				return buf;
		}
	}

	cdir = getenv("NETSURFRES");
	if (cdir != NULL) {
		if (gemdos_realpath(cdir, buf) != NULL) {
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
		if (gemdos_realpath(t, buf) == NULL) {
			strcpy(buf, t);
		}
	} else {
		LOG(("checking %s", (char*)def));
		if (gemdos_realpath(def, buf) == NULL) {
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

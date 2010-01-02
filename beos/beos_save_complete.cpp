/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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

#define _GNU_SOURCE

#define __STDBOOL_H__	1
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <libxml/HTMLtree.h>
extern "C" {
#include "content/content.h"
#include "desktop/save_complete.h"
#include "utils/utils.h"
}

/**
* conducts the filesystem save appropriate to the gui
* \param path save path
* \param filename name of file to save
* \param len data length
* \param sourcedata pointer to data to save
* \param type content type
* \return true for success
*/

bool save_complete_gui_save(const char *path, const char *filename,
		size_t len, const char *sourcedata, content_type type)
{
	int res;
	int namelen;
	namelen = strlen(path) + strlen(filename) + 2; /* '/', '\0' */
	char *fullpath = (char *)malloc(namelen);
	if (!fullpath) {
		warn_user("NoMemory", 0);
		return false;
	}
	snprintf(fullpath, namelen, "%s/%s", path, filename);
	FILE *f;
	f = fopen(fullpath, "wb");
	free(fullpath);
	if (f == NULL)
		return false;
	res = fwrite(sourcedata, len, 1, f);
	fclose(f);
	if (res != 1)
		return false;
	return true;
}

/**
* wrapper for lib function htmlSaveFileFormat; front sets path from path
* + filename in a filesystem-specific way
*/

int save_complete_htmlSaveFileFormat(const char *path, const char *filename, 
		xmlDocPtr cur, const char *encoding, int format)
{
	int ret;
	int len = strlen(path) + strlen(filename) + 2;
	char *fullpath = (char *)malloc(len);
	if (fullpath == NULL) {
		warn_user("NoMemory", 0);
		return -1;
	}
	snprintf(fullpath, len, "%s/%s", path, filename);
	ret = htmlSaveFileFormat(fullpath, cur, encoding, format);
	free(fullpath);
	return ret;
}


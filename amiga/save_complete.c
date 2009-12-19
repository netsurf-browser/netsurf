/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004-2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <libxml/HTMLtree.h>
#include "desktop/save_complete.h"
#include "utils/utils.h"
#include <proto/icon.h>
#include <workbench/icon.h>
#include "content/content.h"

/**
* conducts the filesystem save appropriate to the gui
* \param path save path
* \param filename name of file to save
* \param len data length
* \param sourcedata pointer to data to save, NULL when all data in c
* \param type content type
* \return true for success
*/

bool save_complete_gui_save(const char *path, const char *filename, size_t len,
		const char *sourcedata, content_type type)
{
	int res;
	int namelen;
	char deftype[5];
	struct DiskObject *dobj = NULL;
	namelen = strlen(path) + strlen(filename) + 2;
	char *fullpath = malloc(namelen);
	if (!fullpath) {
		warn_user("NoMemory", 0);
		return false;
	}
	snprintf(fullpath, namelen, "%s/%s", path, filename);
	FILE *f = fopen(fullpath, "w");
	if (f == NULL)
		return false;
	res = fwrite(sourcedata, len, 1, f);
	fclose(f);
	switch(type)
	{
		case CONTENT_HTML:
			strcpy(deftype,"html");
			break;
		case CONTENT_CSS:
			strcpy(deftype,"css");
			break;
		default:
			free(fullpath);
			return true;
			break;
	}
	
	dobj = GetIconTags(NULL,ICONGETA_GetDefaultName,deftype,
			    ICONGETA_GetDefaultType,WBPROJECT,
			    TAG_DONE);		
			    
	PutIconTags(fullpath, dobj,
			 ICONPUTA_NotifyWorkbench, TRUE, TAG_DONE);
	free(fullpath);
	if (res != 1)
		return false;
	return true;
}

/**
* wrapper for lib function htmlSaveFileFormat; front sets path from 
* path + filename in a filesystem-specific way
*/

int save_complete_htmlSaveFileFormat(const char *path, const char *filename, 
		xmlDocPtr cur, const char *encoding, int format)
{
	int ret;
	int len = strlen(path) + strlen(filename) + 2;
	struct DiskObject *dobj = NULL;
	char *fullpath = malloc(len);
	if (!fullpath){
		warn_user("NoMemory", 0);
		return -1;
	}
	snprintf(fullpath, len, "%s/%s", path, filename);
	ret = htmlSaveFileFormat(fullpath, cur, encoding, format);
	dobj = GetIconTags(NULL,ICONGETA_GetDefaultName, "html",
			    ICONGETA_GetDefaultType,WBPROJECT,
			    TAG_DONE);		
			    
	PutIconTags(fullpath, dobj,
			ICONPUTA_NotifyWorkbench, TRUE, TAG_DONE);
	 
	free(fullpath);
	return ret;
}


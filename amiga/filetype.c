/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <stdlib.h>
#include <string.h>
#include "content/fetch.h"
#include "utils/log.h"
#include "utils/utils.h"
#include <proto/icon.h>
#include <proto/dos.h>
#include <proto/datatypes.h>
#include <workbench/icon.h>

/**
 * filetype -- determine the MIME type of a local file
 */

const char *fetch_filetype(const char *unix_path)
{
	static char mimetype[50];
	STRPTR ttype = NULL;
	struct DiskObject *dobj = NULL;
	BPTR lock = 0;
    struct DataTypeHeader *dth = NULL;
    struct DataType *dtn;
	BOOL found = FALSE;

	/* First try getting a tooltype "MIMETYPE" and use that as the MIME type.  Will fail over
		to default icons if the file doesn't have a real icon. */

	if(dobj = GetIconTags(unix_path,ICONGETA_FailIfUnavailable,FALSE,
						TAG_DONE))
	{
		ttype = FindToolType(dobj->do_ToolTypes, "MIMETYPE");
		if(ttype)
		{
			strcpy(mimetype,ttype);
			found = TRUE;
		}

		FreeDiskObject(dobj);
	}

	/* If that didn't work, have a go at guessing it using datatypes.library.  This isn't
		accurate - the base names differ from those used by MIME and it relies on the
		user having a datatype installed which can handle the file. */

	if(!found)
	{
		if (lock = Lock (unix_path, ACCESS_READ))
		{
			if (dtn = ObtainDataTypeA (DTST_FILE, (APTR)lock, NULL))
			{
				dth = dtn->dtn_Header;

				switch(dth->dth_GroupID)
				{
					case GID_SYSTEM:
						sprintf(mimetype,"application/%s",dth->dth_BaseName);
					break;
					case GID_TEXT:
					case GID_DOCUMENT:
						if(strcmp("ascii",dth->dth_BaseName)==0)
						{
							sprintf(mimetype,"text/plain",dth->dth_BaseName);
						}
						else if(strcmp("simplehtml",dth->dth_BaseName)==0)
						{
							sprintf(mimetype,"text/html",dth->dth_BaseName);
						}
						else
						{
							sprintf(mimetype,"text/%s",dth->dth_BaseName);
						}
					break;
					case GID_SOUND:
					case GID_INSTRUMENT:
					case GID_MUSIC:
						sprintf(mimetype,"audio/%s",dth->dth_BaseName);
					break;
					case GID_PICTURE:
						sprintf(mimetype,"image/%s",dth->dth_BaseName);
					break;
					case GID_ANIMATION:
					case GID_MOVIE:
						sprintf(mimetype,"video/%s",dth->dth_BaseName);
					break;
				}
				found = TRUE;
				ReleaseDataType(dtn);
			}
			UnLock(lock);
		}
	}

	if(!found) strcpy(mimetype,"text/html"); /* If all else fails */

	//printf("%s: %s\n",unix_path,mimetype);

	return mimetype;
}


char *fetch_mimetype(const char *ro_path)
{
	return strdup(fetch_filetype(ro_path));
}

/*
 * Copyright 2008, 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
#include "amiga/filetype.h"
#include "amiga/object.h"
#include "content/fetch.h"
#include "content/content.h"
#include "utils/log.h"
#include "utils/utils.h"
#include <proto/icon.h>
#include <proto/dos.h>
#include <proto/datatypes.h>
#include <proto/exec.h>
#include <workbench/icon.h>

/**
 * filetype -- determine the MIME type of a local file
 */

struct MinList *ami_mime_list;

struct ami_mime_entry
{
	lwc_string *mimetype;
	lwc_string *datatype;
	lwc_string *filetype;
	lwc_string *plugincmd;
};

const char *fetch_filetype(const char *unix_path)
{
	static char mimetype[50];
	STRPTR ttype = NULL;
	struct DiskObject *dobj = NULL;
	BPTR lock = 0;
	struct DataType *dtn;
	BOOL found = FALSE;

	/* First, check if we appear to have an icon.
	   We'll just do a filename check here for quickness, although the
	   first word ought to be checked against WB_DISKMAGIC really. */

	if(strncmp(unix_path + strlen(unix_path) - 5, ".info", 5) == 0)
	{
		strcpy(mimetype,"image/x-amiga-icon");
		found = TRUE;
	}


	/* Secondly try getting a tooltype "MIMETYPE" and use that as the MIME type.
	    Will fail over to default icons if the file doesn't have a real icon. */

	if(!found)
	{
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
				ami_datatype_to_mimetype(dtn, &mimetype);
				found = TRUE;
				ReleaseDataType(dtn);
			}
			UnLock(lock);
		}
	}

	/* Have a quick check for file extensions (inc RISC OS filetype).
	 * Makes detection a little more robust, and some of the redirects
	 * caused by links in the SVN tree prevent NetSurf from reading the
	 * MIME type from the icon (step two, above).
	 */

	if((!found) || (strcmp("text/plain", mimetype) == 0))
	{
		if((strncmp(unix_path + strlen(unix_path) - 4, ".css", 4) == 0) ||
			(strncmp(unix_path + strlen(unix_path) - 4, ",f79", 4) == 0))
		{
			strcpy(mimetype,"text/css");
			found = TRUE;
		}

		if((strncmp(unix_path + strlen(unix_path) - 4, ".htm", 4) == 0) ||
			(strncmp(unix_path + strlen(unix_path) - 5, ".html", 5) == 0) ||
			(strncmp(unix_path + strlen(unix_path) - 4, ",faf", 4) == 0))
		{
			strcpy(mimetype,"text/html");
			found = TRUE;
		}
	}

	if(!found) strcpy(mimetype,"text/plain"); /* If all else fails */

	return mimetype;
}


char *fetch_mimetype(const char *ro_path)
{
	return strdup(fetch_filetype(ro_path));
}

const char *ami_content_type_to_file_type(content_type type)
{
	switch(type)
	{
		case CONTENT_HTML:
			return "html";
		break;

		case CONTENT_TEXTPLAIN:
			return "ascii";
		break;

		case CONTENT_CSS:
			return "css";
		break;

		default:
			return "project";	
		break;
	}
}

void ami_datatype_to_mimetype(struct DataType *dtn, char *mimetype)
{
	struct DataTypeHeader *dth = dtn->dtn_Header;

	switch(dth->dth_GroupID)
	{
		case GID_TEXT:
		case GID_DOCUMENT:
			if(strcmp("ascii",dth->dth_BaseName)==0)
			{
				strcpy(mimetype,"text/plain");
			}
			else if(strcmp("simplehtml",dth->dth_BaseName)==0)
			{
				strcpy(mimetype,"text/html");
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
			if(strcmp("sprite",dth->dth_BaseName)==0)
			{
				strcpy(mimetype,"image/x-riscos-sprite");
			}
			if(strcmp("mng",dth->dth_BaseName)==0)
			{
				strcpy(mimetype,"video/mng");
			}
		break;
		case GID_ANIMATION:
		case GID_MOVIE:
			sprintf(mimetype,"video/%s",dth->dth_BaseName);
		break;
		case GID_SYSTEM:
		default:
			if(strcmp("directory",dth->dth_BaseName)==0)
			{
				strcpy(mimetype,"application/x-netsurf-directory");
			}
			else if(strcmp("binary",dth->dth_BaseName)==0)
			{
				strcpy(mimetype,"application/octet-stream");
			}
			else sprintf(mimetype,"application/%s",dth->dth_BaseName);
		break;
	}
}

bool ami_mime_compare(struct hlcache_handle *c, const char *type)
{
	lwc_string *mime = content_get_mime_type(c);
	const char *mime_string = lwc_string_data(mime);
	size_t mime_length = lwc_string_length(mime);

	if(!strncmp("svg", type, 3))
	{
		if(!strncmp(mime_string, "image/svg", mime_length)) return true;
		if(!strncmp(mime_string, "image/svg+xml", mime_length)) return true;
	}
	else return false;
}

nserror ami_mime_init(const char *mimefile)
{
	lwc_string *type;
	lwc_error lerror;
	nserror error;
	char buffer[256];
	BPTR fh = 0;
	struct RDArgs *rargs = NULL;
	STRPTR template = "MIMETYPE/A,DT=DATATYPE/K,TYPE=DEFICON/K,CMD=PLUGINCMD/K";
	long rarray[] = {0,0,0,0};
	struct nsObject *node;
	struct ami_mime_entry *mimeentry;

	enum
	{
		A_MIMETYPE,
		A_DATATYPE,
		A_FILETYPE,
		A_PLUGINCMD
	};

	ami_mime_list = NewObjList();

	rargs = AllocDosObjectTags(DOS_RDARGS,TAG_DONE);

	if(fh = FOpen(mimefile, MODE_OLDFILE, 0))
	{
		while(FGets(fh, (UBYTE *)&buffer, 256) != 0)
		{
			rargs->RDA_Source.CS_Buffer = (char *)&buffer;
			rargs->RDA_Source.CS_Length = 256;
			rargs->RDA_Source.CS_CurChr = 0;

			rargs->RDA_DAList = NULL;
			rargs->RDA_Buffer = NULL;
			rargs->RDA_BufSiz = 0;
			rargs->RDA_ExtHelp = NULL;
			rargs->RDA_Flags = 0;

			rarray[A_MIMETYPE] = 0;
			rarray[A_DATATYPE] = 0;
			rarray[A_FILETYPE] = 0;
			rarray[A_PLUGINCMD] = 0;

			if(ReadArgs(template, rarray, rargs))
			{
				node = AddObject(ami_mime_list, AMINS_MIME);
				mimeentry = AllocVec(sizeof(struct ami_mime_entry), MEMF_PRIVATE | MEMF_CLEAR);
				node->objstruct = mimeentry;

				if(rarray[A_MIMETYPE])
				{
					lerror = lwc_intern_string((char *)rarray[A_MIMETYPE],
								strlen((char *)rarray[A_MIMETYPE]), &mimeentry->mimetype);
					if (lerror != lwc_error_ok)
						return NSERROR_NOMEM;
				}

				if(rarray[A_DATATYPE])
				{
					lerror = lwc_intern_string((char *)rarray[A_DATATYPE],
								strlen((char *)rarray[A_DATATYPE]), &mimeentry->datatype);
					if (lerror != lwc_error_ok)
						return NSERROR_NOMEM;
				}

				if(rarray[A_FILETYPE])
				{
					lerror = lwc_intern_string((char *)rarray[A_FILETYPE],
								strlen((char *)rarray[A_FILETYPE]), &mimeentry->filetype);
					if (lerror != lwc_error_ok)
						return NSERROR_NOMEM;
				}

				if(rarray[A_PLUGINCMD])
				{
					lerror = lwc_intern_string((char *)rarray[A_PLUGINCMD],
								strlen((char *)rarray[A_PLUGINCMD]), &mimeentry->plugincmd);
					if (lerror != lwc_error_ok)
						return NSERROR_NOMEM;
				}
			}
		}
		FClose(fh);
	}
}

void ami_mime_free(void)
{
	FreeObjList(ami_mime_list);
}

void ami_mime_entry_free(struct ami_mime_entry *mimeentry)
{
	if(mimeentry->mimetype) lwc_string_unref(mimeentry->mimetype);
	if(mimeentry->datatype) lwc_string_unref(mimeentry->datatype);
	if(mimeentry->filetype) lwc_string_unref(mimeentry->filetype);
	if(mimeentry->plugincmd) lwc_string_unref(mimeentry->plugincmd);
}

/**
 * Return a MIME Type matching a DataType
 *
 * \param dt a DataType structure
 * \param mimetype lwc_string to hold the MIME type
 * \param start_node node to feed back in to continue search
 * \return node or NULL if no match
 */

struct Node *ami_mime_from_datatype(struct DataType *dt,
		lwc_string **mimetype, struct Node *start_node)
{
	struct DataTypeHeader *dth = dt->dtn_Header;
	struct nsObject *node;
	struct nsObject *nnode;
	struct ami_mime_entry *mimeentry;
	lwc_string *dt_name;
	lwc_error lerror;
	bool ret = false;

	if(IsMinListEmpty(ami_mime_list)) return NULL;

	lerror = lwc_intern_string(dth->dth_Name, strlen(dth->dth_Name), &dt_name);
	if (lerror != lwc_error_ok)
		return NULL;

	if(start_node)
	{
		node = (struct nsObject *)GetSucc(start_node);
		if(node == NULL) return NULL;
	}
	else
	{
		node = (struct nsObject *)GetHead((struct List *)ami_mime_list);
	}

	do
	{
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		mimeentry = node->objstruct;
		lerror = lwc_string_isequal(mimeentry->datatype, dt_name, &ret);

		if((lerror == lwc_error_ok) && (ret == true))
		{
			*mimetype = mimeentry->mimetype;
			break;
		}

	}while(node=nnode);

	lwc_string_unref(dt_name);

	if(ret == true) return (struct Node *)node;
		else return NULL;
}

/*
 * Copyright 2008,2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "desktop/gui.h"
#include <parserutils/charset/mibenum.h>
#include "amiga/iff_cset.h"
#include <proto/iffparse.h>
#include <datatypes/textclass.h>
#include "amiga/options.h"
#include "amiga/gui.h"
#include <proto/exec.h>
#include "amiga/utf8.h"
#include "utils/utf8.h"
#include "desktop/selection.h"
#include <datatypes/pictureclass.h>
#include <proto/datatypes.h>
#include "amiga/bitmap.h"

struct IFFHandle *iffh = NULL;

void ami_clipboard_init(void)
{
	if(iffh = AllocIFF())
	{
		if(iffh->iff_Stream = (ULONG)OpenClipboard(0))
		{
			InitIFFasClip(iffh);
		}
	}
}

void ami_clipboard_free(void)
{
	if(iffh->iff_Stream) CloseClipboard((struct ClipboardHandle *)iffh->iff_Stream);
	if(iffh) FreeIFF(iffh);
}

void gui_start_selection(struct gui_window *g)
{
}

void gui_paste_from_clipboard(struct gui_window *g, int x, int y)
{
	/* This and the other clipboard code is heavily based on the RKRM examples */
	struct ContextNode *cn;
	ULONG rlen=0,error;
	struct CSet cset;
	char *clip;
	STRPTR readbuf = AllocVec(1024,MEMF_PRIVATE | MEMF_CLEAR);

	cset.CodeSet = 0;

	if(OpenIFF(iffh,IFFF_READ)) return;
	if(StopChunk(iffh,ID_FTXT,ID_CHRS)) return;
	if(StopChunk(iffh,ID_FTXT,ID_CSET)) return;

	while(1)
	{
		error = ParseIFF(iffh,IFFPARSE_SCAN);
		if(error == IFFERR_EOC) continue;
		else if(error) break;

		cn = CurrentChunk(iffh);

		if((cn)&&(cn->cn_Type == ID_FTXT)&&(cn->cn_ID == ID_CSET))
		{
			rlen = ReadChunkBytes(iffh,&cset,24);
		}

		if((cn)&&(cn->cn_Type == ID_FTXT)&&(cn->cn_ID == ID_CHRS))
		{
			while((rlen = ReadChunkBytes(iffh,readbuf,1024)) > 0)
			{
				if(cset.CodeSet == 0)
				{
					utf8_from_local_encoding(readbuf,rlen,&clip);
				}
				else
				{
					utf8_from_enc(readbuf,parserutils_charset_mibenum_to_name(cset.CodeSet),rlen,&clip);
				}

				browser_window_paste_text(g->shared->bw,clip,rlen,true);
			}
			if(rlen < 0) error = rlen;
		}
	}
	CloseIFF(iffh);
}

bool gui_empty_clipboard(void)
{
}

bool gui_add_to_clipboard(const char *text, size_t length, bool space)
{
	char *buffer;
	if(option_utf8_clipboard)
	{
		WriteChunkBytes(iffh,text,length);
	}
	else
	{
		utf8_to_local_encoding(text,length,&buffer);
		if(buffer) WriteChunkBytes(iffh,buffer,strlen(buffer));
		ami_utf8_free(buffer);
	}

	if(space) WriteChunkBytes(iffh," ",1);

	return true;
}

bool gui_commit_clipboard(void)
{
	if(iffh) CloseIFF(iffh);

	return true;
}

bool ami_clipboard_copy(const char *text, size_t length, struct box *box,
	void *handle, const char *whitespace_text,size_t whitespace_length)
{
	if(!(PushChunk(iffh,0,ID_CHRS,IFFSIZE_UNKNOWN)))
	{
		if (whitespace_text)
		{
			if(!gui_add_to_clipboard(whitespace_text,whitespace_length, false)) return false;
		}

		if(text)
		{
			if (!gui_add_to_clipboard(text, length, box->space)) return false;
		}

		PopChunk(iffh);
	}
	else
	{
		PopChunk(iffh);
		return false;
	}

	return true;
}

bool gui_copy_to_clipboard(struct selection *s)
{
	struct CSet cset = {0};

	if(!(OpenIFF(iffh,IFFF_WRITE)))
	{
		if(!(PushChunk(iffh,ID_FTXT,ID_FORM,IFFSIZE_UNKNOWN)))
		{
			if(option_utf8_clipboard)
			{
				if(!(PushChunk(iffh,0,ID_CSET,24)))
				{
					cset.CodeSet = 106; // UTF-8
					WriteChunkBytes(iffh,&cset,24);
					PopChunk(iffh);
				}
			}

			if (s->defined && selection_traverse(s, ami_clipboard_copy, NULL))
			{
				gui_commit_clipboard();
				return true;
			}
		}
		else
		{
			PopChunk(iffh);
		}
		CloseIFF(iffh);
	}

	return false;
}

bool ami_easy_clipboard(char *text)
{
	struct CSet cset = {0};

	if(!(OpenIFF(iffh,IFFF_WRITE)))
	{
		if(!(PushChunk(iffh,ID_FTXT,ID_FORM,IFFSIZE_UNKNOWN)))
		{
			if(option_utf8_clipboard)
			{
				if(!(PushChunk(iffh,0,ID_CSET,24)))
				{
					cset.CodeSet = 106; // UTF-8
					WriteChunkBytes(iffh,&cset,24);
					PopChunk(iffh);
				}
			}

			if(!(PushChunk(iffh,0,ID_CHRS,IFFSIZE_UNKNOWN)))
			{
				if(gui_add_to_clipboard(text,strlen(text),false))
				{
					PopChunk(iffh);
					gui_commit_clipboard();
					return true;
				}
			}
			else
			{
				PopChunk(iffh);
			}
		}
		else
		{
			PopChunk(iffh);
		}
		CloseIFF(iffh);
	}

	return false;
}

bool ami_easy_clipboard_bitmap(struct bitmap *bitmap)
{
	Object *dto = NULL;

	if(dto = ami_datatype_object_from_bitmap(bitmap))
	{
		DoDTMethod(dto,NULL,NULL,DTM_COPY,NULL);
		DisposeDTObject(dto);
	}
}

/*
 * Copyright 2008-2012 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
#include "utils/utf8.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"

#include "amiga/bitmap.h"
#include "amiga/clipboard.h"
#include "amiga/drag.h"
#include "amiga/filetype.h"
#include "amiga/gui.h"
#include "amiga/iff_cset.h"
#include "amiga/iff_dr2d.h"
#include "amiga/menu.h"
#include "amiga/options.h"
#include "amiga/utf8.h"

#include <proto/iffparse.h>
#include <proto/intuition.h>
#include <proto/exec.h>
#include <proto/datatypes.h>
#include <proto/diskfont.h>

#include <diskfont/diskfonttag.h>
#include <datatypes/textclass.h>
#include <datatypes/pictureclass.h>

struct IFFHandle *iffh = NULL;
bool ami_utf8_clipboard = false; // force UTF-8 in clipboard

bool ami_add_to_clipboard(const char *text, size_t length, bool space);
static bool ami_copy_selection(const char *text, size_t length,
	struct box *box, void *handle, const char *whitespace_text,
	size_t whitespace_length);

struct IFFHandle *ami_clipboard_init_internal(int unit)
{
	struct IFFHandle *iffhandle = NULL;

	if(iffhandle = AllocIFF())
	{
		if(iffhandle->iff_Stream = (ULONG)OpenClipboard(unit))
		{
			InitIFFasClip(iffhandle);
		}
	}

	return iffhandle;
}

void ami_clipboard_init(void)
{
	iffh = ami_clipboard_init_internal(0);
}

void ami_clipboard_free_internal(struct IFFHandle *iffhandle)
{
	if(iffhandle->iff_Stream) CloseClipboard((struct ClipboardHandle *)iffhandle->iff_Stream);
	if(iffhandle) FreeIFF(iffhandle);
}

void ami_clipboard_free(void)
{
	ami_clipboard_free_internal(iffh);
}

void gui_start_selection(struct gui_window *g)
{
	if(!g) return;
	if(!g->shared->win) return;
	if(option_kiosk_mode == true) return;

	OnMenu(g->shared->win, AMI_MENU_CLEAR);
	OnMenu(g->shared->win, AMI_MENU_COPY);

	if(selection_read_only(browser_window_get_selection(g->shared->bw)) == false)
		OnMenu(g->shared->win, AMI_MENU_CUT);
}

void gui_clear_selection(struct gui_window *g)
{
	if(!g) return;
	if(!g->shared->win) return;
	if(option_kiosk_mode == true) return;

	OffMenu(g->shared->win, AMI_MENU_CLEAR);
	OffMenu(g->shared->win, AMI_MENU_CUT);
	OffMenu(g->shared->win, AMI_MENU_COPY);
}

void gui_paste_from_clipboard(struct gui_window *g, int x, int y)
{
	/* This and the other clipboard code is heavily based on the RKRM examples */
	struct ContextNode *cn;
	ULONG rlen=0,error;
	struct CSet cset;
	LONG codeset = 0;
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
			rlen = ReadChunkBytes(iffh,&cset,32);
			if(cset.CodeSet == 1) codeset = 106;
				else codeset = cset.CodeSet;
		}

		if((cn)&&(cn->cn_Type == ID_FTXT)&&(cn->cn_ID == ID_CHRS))
		{
			while((rlen = ReadChunkBytes(iffh,readbuf,1024)) > 0)
			{
				if(codeset == 0)
				{
					utf8_from_local_encoding(readbuf,rlen,&clip);
				}
				else
				{
					utf8_from_enc(readbuf,
						(const char *)ObtainCharsetInfo(DFCS_NUMBER,
										codeset, DFCS_MIMENAME),
						rlen, &clip);
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
	/* Put a half-completed FTXT on the clipboard and leave it open for more additions */

	struct CSet cset = {0};

	if(!(OpenIFF(iffh,IFFF_WRITE)))
	{
		if(!(PushChunk(iffh,ID_FTXT,ID_FORM,IFFSIZE_UNKNOWN)))
		{
			if(option_utf8_clipboard || ami_utf8_clipboard)
			{
				if(!(PushChunk(iffh,0,ID_CSET,32)))
				{
					cset.CodeSet = 106; // UTF-8
					WriteChunkBytes(iffh,&cset,32);
					PopChunk(iffh);
				}
			}
		}
		else
		{
			PopChunk(iffh);
			return false;
		}
		return true;
	}
	return false;
}

bool gui_add_to_clipboard(const char *text, size_t length, bool space)
{
	/* This might crash or at least not work if gui_empty_clipboard isn't called first,
	   and gui_commit_clipboard after.
	   These only seem to be called from desktop/textinput.c in this specific order, if they
	   are added elsewhere this might need a rewrite. */

	if(!(PushChunk(iffh,0,ID_CHRS,IFFSIZE_UNKNOWN)))
	{
		if(text)
		{
			if(!ami_add_to_clipboard(text, length, space)) return false;
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

bool ami_add_to_clipboard(const char *text, size_t length, bool space)
{
	char *buffer;

	if(option_utf8_clipboard || ami_utf8_clipboard)
	{
		WriteChunkBytes(iffh,text,length);
	}
	else
	{
		buffer = ami_utf8_easy(text);

		if(buffer)
		{
			char *p;

			p = buffer;

			while(*p != '\0')
			{
				if(*p == 0xa0) *p = 0x20;
				p++;
			}
			WriteChunkBytes(iffh, buffer, strlen(buffer));

			ami_utf8_free(buffer);
		}
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
			if(!ami_add_to_clipboard(whitespace_text,whitespace_length, false)) return false;
		}

		if(text)
		{
			bool add_space = box != NULL ? box->space != 0 : false;

			if (!ami_add_to_clipboard(text, length, add_space)) return false;
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
	bool success;

	if(s->defined == false) return false;
	if(!gui_empty_clipboard()) return false;

	success = selection_traverse(s, ami_clipboard_copy, NULL);

	/* commit regardless, otherwise we leave the clipboard in an unusable state */
	gui_commit_clipboard();

	return success;
}

struct ami_text_selection *ami_selection_to_text(struct gui_window_2 *gwin)
{
	struct ami_text_selection *sel;

	sel = AllocVec(sizeof(struct ami_text_selection),
			MEMF_PRIVATE | MEMF_CLEAR);

	if(sel) selection_traverse(browser_window_get_selection(gwin->bw), ami_copy_selection, sel);

	return sel;
}

static bool ami_copy_selection(const char *text, size_t length,
	struct box *box, void *handle, const char *whitespace_text,
	size_t whitespace_length)
{
	struct ami_text_selection *sel = handle;
	int len = length;

	if((length + (sel->length)) > (sizeof(sel->text)))
		len = sizeof(sel->text) - (sel->length);

	if(len <= 0) return false;

	memcpy((sel->text) + (sel->length), text, len);
	sel->length += len;

	sel->text[sel->length] = '\0';

	return true;
}

void ami_drag_selection(struct selection *s)
{
	struct box *text_box;
	int x;
	int y;
	char *utf8text;
	struct ami_text_selection *sel;
	struct IFFHandle *old_iffh = iffh;
	struct gui_window_2 *gwin = ami_window_at_pointer(AMINS_WINDOW);

	if(!gwin)
	{
		DisplayBeep(scrn);
		return;
	}

	x = gwin->win->MouseX;
	y = gwin->win->MouseY;

	if(text_box = ami_text_box_at_point(gwin, (ULONG *)&x, (ULONG *)&y))
	{
		ami_utf8_clipboard = true;

		iffh = ami_clipboard_init_internal(1);

		if(gui_copy_to_clipboard(s))
		{
			browser_window_mouse_click(gwin->bw, BROWSER_MOUSE_PRESS_1, x, y);
			browser_window_key_press(gwin->bw, KEY_PASTE);
		}

		ami_clipboard_free_internal(iffh);
		iffh = old_iffh;
		ami_utf8_clipboard = false;
	}
	else
	{
		x = gwin->win->MouseX;
		y = gwin->win->MouseY;

		if(ami_gadget_hit(gwin->objects[GID_URL], x, y))
		{
			if(sel = ami_selection_to_text(gwin))
			{
				utf8text = ami_utf8_easy(sel->text);
				RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_URL],
					gwin->win, NULL, STRINGA_TextVal, utf8text, TAG_DONE);
				FreeVec(sel);
				ami_utf8_free(utf8text);
			}
		}
		else if(ami_gadget_hit(gwin->objects[GID_SEARCHSTRING], x, y))
		{
			if(sel = ami_selection_to_text(gwin))
			{
				utf8text = ami_utf8_easy(sel->text);
				RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_SEARCHSTRING],
					gwin->win, NULL, STRINGA_TextVal, utf8text, TAG_DONE);
				FreeVec(sel);
				ami_utf8_free(utf8text);
			}
		}
		else
		{
			DisplayBeep(scrn);
		}
	}
}

bool ami_easy_clipboard(char *text)
{
	if(!gui_empty_clipboard()) return false;
	if(!gui_add_to_clipboard(text,strlen(text),false)) return false;
	if(!gui_commit_clipboard()) return false;

	return true;
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

#ifdef WITH_NS_SVG
bool ami_easy_clipboard_svg(struct hlcache_handle *c)
{
	const char *source_data;
	ULONG source_size;

	if(ami_mime_compare(c, "svg") == false) return false;
	if((source_data = content_get_source_data(c, &source_size)) == NULL) return false;

	if(!(OpenIFF(iffh,IFFF_WRITE)))
	{
		ami_svg_to_dr2d(iffh, source_data, source_size, nsurl_access(hlcache_handle_get_url(c)));
		CloseIFF(iffh);
	}

	return true;
}
#endif

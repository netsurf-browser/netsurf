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
#include "desktop/plotters.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "utils/utf8.h"

#include "amiga/bitmap.h"
#include "amiga/clipboard.h"
#include "amiga/drag.h"
#include "amiga/filetype.h"
#include "amiga/gui.h"
#include "amiga/iff_cset.h"
#include "amiga/iff_dr2d.h"
#include "amiga/menu.h"
#include "desktop/options.h"
#include "amiga/utf8.h"

#include <proto/iffparse.h>
#include <proto/intuition.h>
#include <proto/exec.h>
#include <proto/datatypes.h>
#include <proto/diskfont.h>

#include <diskfont/diskfonttag.h>
#include <datatypes/textclass.h>
#include <datatypes/pictureclass.h>

#define ID_UTF8  MAKE_ID('U','T','F','8')

struct IFFHandle *iffh = NULL;

static LONG ami_clipboard_iffp_do_nothing(struct Hook *hook, void *object, LONG *cmd)
{
	return 0;
}

static void ami_clipboard_iffp_clear_stopchunk(struct IFFHandle *iffh, ULONG iff_type, ULONG iff_chunk)
{
	static struct Hook entry_hook;

	entry_hook.h_Entry = (void *)ami_clipboard_iffp_do_nothing;
	entry_hook.h_Data = 0;

	EntryHandler(iffh, iff_type, iff_chunk, IFFSLI_TOP, &entry_hook, NULL);
}

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
	if(nsoption_bool(kiosk_mode) == true) return;

	OnMenu(g->shared->win, AMI_MENU_CLEAR);
	OnMenu(g->shared->win, AMI_MENU_COPY);

	if(selection_read_only(browser_window_get_selection(g->shared->bw)) == false)
		OnMenu(g->shared->win, AMI_MENU_CUT);
}

void gui_clear_selection(struct gui_window *g)
{
	if(!g) return;
	if(!g->shared->win) return;
	if(nsoption_bool(kiosk_mode) == true) return;

	OffMenu(g->shared->win, AMI_MENU_CLEAR);
	OffMenu(g->shared->win, AMI_MENU_CUT);
	OffMenu(g->shared->win, AMI_MENU_COPY);
}

bool ami_clipboard_check_for_utf8(struct IFFHandle *iffh) {
	struct ContextNode *cn;
	ULONG error;
	bool utf8_chunk = false;
	
	if(OpenIFF(iffh, IFFF_READ)) return false;
	
	ami_clipboard_iffp_clear_stopchunk(iffh, ID_FTXT, ID_CSET);
	ami_clipboard_iffp_clear_stopchunk(iffh, ID_FTXT, ID_CHRS);
	
	if(!StopChunk(iffh, ID_FTXT, ID_UTF8)) {
		error = ParseIFF(iffh, IFFPARSE_SCAN);
		if(error != IFFERR_EOF)
			utf8_chunk = true; /* or a real error, but that'll get caught later */
	}
	CloseIFF(iffh);

	return utf8_chunk;
}

void gui_get_clipboard(char **buffer, size_t *length)
{
	/* This and the other clipboard code is heavily based on the RKRM examples */
	struct ContextNode *cn;
	ULONG rlen=0,error;
	struct CSet cset;
	LONG codeset = 0;
	char *clip;
	bool utf8_chunks = false;
	STRPTR readbuf = AllocVec(1024,MEMF_PRIVATE | MEMF_CLEAR);

	cset.CodeSet = 0;

	if(ami_clipboard_check_for_utf8(iffh))
		utf8_chunks = true;
	
	if(OpenIFF(iffh,IFFF_READ)) return;
	
	if(utf8_chunks == false) {
		if(StopChunk(iffh,ID_FTXT,ID_CHRS)) return;
		if(StopChunk(iffh,ID_FTXT,ID_CSET)) return;
	} else {
		if(StopChunk(iffh,ID_FTXT,ID_UTF8)) return;
	}
	
	while(1)
	{
		error = ParseIFF(iffh,IFFPARSE_SCAN);
		if(error == IFFERR_EOC) continue;
		else if(error) break;

		cn = CurrentChunk(iffh);

		if((cn)&&(cn->cn_Type == ID_FTXT)&&(cn->cn_ID == ID_CSET)&&(utf8_chunks == false))
		{
			rlen = ReadChunkBytes(iffh,&cset,32);
			if(cset.CodeSet == 1) codeset = 106;
				else codeset = cset.CodeSet;
		}

		if((cn)&&(cn->cn_Type == ID_FTXT)&&(cn->cn_ID == ID_CHRS)&&(utf8_chunks == false))
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

				//browser_window_paste_text(g->shared->bw,clip,rlen,true);
			}
			if(rlen < 0) error = rlen;
		}

		if((cn)&&(cn->cn_Type == ID_FTXT)&&(cn->cn_ID == ID_UTF8)&&(utf8_chunks == true))
		{
			while((rlen = ReadChunkBytes(iffh, readbuf, 1024)) > 0)
			{
				//browser_window_paste_text(g->shared->bw, readbuf, rlen, true);
			}
			if(rlen < 0) error = rlen;
		}
	}
	CloseIFF(iffh);
}

void gui_set_clipboard(const char *buffer, size_t length,
	nsclipboard_styles styles[], int n_styles)
{
	char *text;
	struct CSet cset = {0};

	if(buffer == NULL) return;

	if(!(OpenIFF(iffh, IFFF_WRITE)))
	{
		if(!(PushChunk(iffh, ID_FTXT, ID_FORM, IFFSIZE_UNKNOWN)))
		{
			if(nsoption_bool(utf8_clipboard))
			{
				if(!(PushChunk(iffh, 0, ID_CSET, 32)))
				{
					cset.CodeSet = 106; // UTF-8
					WriteChunkBytes(iffh, &cset, 32);
					PopChunk(iffh);
				}
			}
		}
		else
		{
			PopChunk(iffh);
		}

		if(!(PushChunk(iffh, 0, ID_CHRS, IFFSIZE_UNKNOWN))) {
			if(nsoption_bool(utf8_clipboard)) {
				WriteChunkBytes(iffh, buffer, length);
			} else {
				if(utf8_to_local_encoding(buffer, length, &text) == UTF8_CONVERT_OK) {
					char *p;

					p = text;

					while(*p != '\0') {
						if(*p == 0xa0) *p = 0x20;
						p++;
					}
					WriteChunkBytes(iffh, text, strlen(text));
					ami_utf8_free(text);
				}
			}

			PopChunk(iffh);
		} else {
			PopChunk(iffh);
		}

		if(!(PushChunk(iffh, 0, ID_UTF8, IFFSIZE_UNKNOWN))) {
			WriteChunkBytes(iffh, buffer, length);
			PopChunk(iffh);
		} else {
			PopChunk(iffh);
		}
		CloseIFF(iffh);
	}
}

struct ami_text_selection *ami_selection_to_text(struct gui_window_2 *gwin)
{
	struct ami_text_selection *sel;
	int len;
	char *ss;

	sel = AllocVec(sizeof(struct ami_text_selection),
			MEMF_PRIVATE | MEMF_CLEAR);

	if (sel) {
		/* Get selection string */
		ss = selection_get_copy(browser_window_get_selection(gwin->bw));
		if (ss == NULL)
			return sel;

		len = strlen(ss);

		if (len > sizeof(sel->text))
			len = sizeof(sel->text) - 1;

		memcpy(sel->text, ss, len);
		sel->length = len;
		sel->text[sel->length] = '\0';

		free(ss);
	}
	return sel;
}

#if 0
void ami_drag_selection(struct selection *s)
{
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

	if(ami_text_box_at_point(gwin, (ULONG *)&x, (ULONG *)&y))
	{
		iffh = ami_clipboard_init_internal(1);

		if(gui_copy_to_clipboard(s))
		{
			browser_window_mouse_click(gwin->bw, BROWSER_MOUSE_PRESS_1, x, y);
			browser_window_key_press(gwin->bw, KEY_PASTE);
		}

		ami_clipboard_free_internal(iffh);
		iffh = old_iffh;
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
#endif

bool ami_easy_clipboard(char *text)
{
	gui_set_clipboard(text, strlen(text), NULL, 0);
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

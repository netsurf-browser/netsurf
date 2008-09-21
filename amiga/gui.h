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

#ifndef AMIGA_GUI_H
#define AMIGA_GUI_H
#include <graphics/rastport.h>
#include "amiga/object.h"
#include <intuition/classusr.h>
#include "desktop/browser.h"
#include <dos/dos.h>

void ami_get_msg(void);

enum
{
    GID_MAIN=0,
	GID_BROWSER,
	GID_STATUS,
	GID_URL,
	GID_STOP,
	GID_RELOAD,
	GID_HOME,
	GID_BACK,
	GID_FORWARD,
	GID_THROBBER,
	GID_USER,
	GID_PASS,
	GID_LOGIN,
	GID_CANCEL,
    GID_LAST
};

enum
{
    OID_MAIN=0,
	OID_VSCROLL,
	OID_HSCROLL,
	OID_MENU,
    OID_LAST
};

struct gui_download_window {
	struct Window *win;
	Object *objects[OID_LAST];
	struct Gadget *gadgets[GID_LAST];
	struct nsObject *node;
	ULONG pad[4];
	BPTR fh;
	uint32 size;
	uint32 downloaded;
};

struct gui_window {
	struct Window *win;
	Object *objects[OID_LAST];
	struct Gadget *gadgets[GID_LAST];
	struct nsObject *node;
	struct browser_window *bw;
	bool redraw_required;
	int throbber_frame;
	struct List *tab_bw_list;
	struct BitMap *bm;
	struct RastPort rp;
	struct Layer_Info *layerinfo;
	APTR areabuf;
	APTR tmprasbuf;
	struct Hook scrollerhook;
	struct Hook popuphook;
	struct form_control *control;
	union content_msg_data *redraw_data;
	browser_mouse_state mouse_state;
	browser_mouse_state key_state;
	ULONG throbber_update_count;
	int c_x;
	int c_y;
	int c_h;
};

struct RastPort *currp;
struct TextFont *origrpfont;
struct MinList *window_list;
struct Screen *scrn;
STRPTR nsscreentitle;
struct FileRequester *filereq;
struct MsgPort *sport;
#endif

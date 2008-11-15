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
#include "desktop/gui.h"

/* temp icon.library stuff */
#define ICONCTRLA_SetImageDataFormat        (ICONA_Dummy + 0x67) /*103*/
#define ICONCTRLA_GetImageDataFormat        (ICONA_Dummy + 0x68) /*104*/

/* Values for ICONCTRLA_Set/GetImageDataFormat */
#define IDFMT_BITMAPPED     (0)  /* Bitmapped icon (planar, legacy) */
#define IDFMT_PALETTEMAPPED (1)  /* Palette mapped icon (chunky, V44+) */
#define IDFMT_DIRECTMAPPED  (2)  /* Direct mapped icon (truecolor 0xAARRGGBB, V51+) */ 
/* temp icon.library stuff */

enum
{
    GID_MAIN=0,
	GID_TABLAYOUT,
	GID_BROWSER,
	GID_STATUS,
	GID_URL,
	GID_STOP,
	GID_RELOAD,
	GID_HOME,
	GID_BACK,
	GID_FORWARD,
	GID_THROBBER,
	GID_CLOSETAB,
	GID_TABS,
	GID_USER,
	GID_PASS,
	GID_LOGIN,
	GID_CANCEL,
	GID_TREEBROWSER,
	GID_OPEN,
	GID_LEFT,
	GID_UP,
	GID_DOWN,
	GID_NEWF,
	GID_NEWB,
	GID_DEL,
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
	ULONG pad[5];
	BPTR fh;
	uint32 size;
	uint32 downloaded;
};

struct gui_window_2 {
	struct Window *win;
	Object *objects[OID_LAST];
	struct Gadget *gadgets[GID_LAST];
	struct nsObject *node;
	struct browser_window *bw;
	bool redraw_required;
	int throbber_frame;
	struct List tab_list;
	ULONG tabs;
	ULONG next_tab;
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
};

struct gui_window
{
	struct gui_window_2 *shared;
	int tab;
	struct Node *tab_node;
	int c_x;
	int c_y;
	int c_h;
	int scrollx;
	int scrolly;
	struct browser_window *bw; // not used
};

void ami_get_msg(void);
void ami_update_pointer(struct Window *win, gui_pointer_shape shape);
void ami_close_all_tabs(struct gui_window_2 *gwin);
void ami_quit_netsurf(void);
void ami_get_theme_filename(char *filename,char *themestring);

struct RastPort *currp;
struct TextFont *origrpfont;
struct MinList *window_list;
struct Screen *scrn;
STRPTR nsscreentitle;
struct FileRequester *filereq;
struct MsgPort *sport;
bool win_destroyed;
struct browser_window *curbw;
#endif

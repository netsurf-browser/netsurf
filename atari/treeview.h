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

#ifndef NS_ATARI_TREEVIEW_H
#define NS_ATARI_TREEVIEW_H

#include <stdbool.h>
#include "desktop/tree.h"
#include "atari/gui.h"
#include "atari/gemtk/gemtk.h"

#define ATARI_TREEVIEW_WIDGETS (CLOSER | MOVER | SIZER| NAME | FULLER | SMALLER | VSLIDE | HSLIDE | UPARROW | DNARROW | LFARROW | RTARROW)

struct atari_treeview
{
	struct tree * tree;
	GUIWIN * window;
	bool disposing;
	bool redraw;
	GRECT rdw_area;
	POINT extent;
	POINT click;
	POINT startdrag;
	gemtk_wm_event_handler_f user_func;
};

typedef struct atari_treeview * NSTREEVIEW;

NSTREEVIEW atari_treeview_create( uint32_t flags, GUIWIN *win,
								gemtk_wm_event_handler_f user_func);
void atari_treeview_destroy( NSTREEVIEW tv );
void atari_treeview_open( NSTREEVIEW tv );
void atari_treeview_close( NSTREEVIEW tv );
void atari_treeview_request_redraw(int x, int y, int w, int h, void *pw);
void atari_treeview_redraw( NSTREEVIEW tv );
bool atari_treeview_mevent( NSTREEVIEW tv, browser_mouse_state bms, int x, int y);



#endif

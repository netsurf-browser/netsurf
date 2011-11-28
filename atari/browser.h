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

#ifndef NS_ATARI_BROWSER_H
#define NS_ATARI_BROWSER_H

#include "atari/redrawslots.h"

/*
 Each browser_window in the Atari Port is represented by an  struct s_browser,
 which consist mainly of an WinDom COMPONENT.
*/

/*
	BROWSER_SCROLL_SVAL
	The small scroll inc. value (used by scroll-wheel, arrow click):
*/
#define BROWSER_SCROLL_SVAL 64


enum browser_rect
{
	BR_CONTENT = 1,
	BR_FULL = 2,
	BR_HSLIDER = 3,
	BR_VSLIDER = 4
};


/*
  This struct contains info of current browser viewport scroll
  and the scroll which is requested. If a scroll is requested,
  the field required is set to true.
*/
struct s_scroll_info
{
	POINT requested;
	POINT current;
	bool required;
};

/*
	This struct holds information of the cursor within the browser
	viewport.
*/
struct s_caret
{
	LGRECT requested;
	LGRECT current;
	bool redraw;
	MFDB background;
};

/*
	This is the browser content area (viewport).
	It is redrawable and scrollable. It is based on the WinDOM
	Component window (undocumented feature).

	It's an windom component containing it's own Window controls,
	like scrollbars, resizer, etc.

	Now that the NetSurf core handles frames, the advantages of this
	choice have probably vanished.
*/
struct s_browser
{
	int type;
	COMPONENT * comp;
	struct browser_window * bw;
	struct s_scroll_info scroll;
	struct s_redrw_slots redraw;
	struct s_caret caret;
	bool attached;
	bool reformat_pending;
};

struct s_browser * browser_create( struct gui_window * gw, struct browser_window * clone, struct browser_window *bw, int lt,  int w, int flex );
bool browser_destroy( struct s_browser * b );
void browser_get_rect( struct gui_window * gw, enum browser_rect type, LGRECT * out);
bool browser_input( struct gui_window * gw, unsigned short nkc ) ;
void browser_redraw( struct gui_window * gw );
void browser_set_content_size(struct gui_window * gw, int w, int h);
void browser_scroll( struct gui_window * gw, short MODE, int value, bool abs );
struct gui_window * browser_find_root( struct gui_window * gw );
bool browser_redraw_required( struct gui_window * gw);
void browser_redraw_caret( struct gui_window * gw, LGRECT * area);
void browser_restore_caret_background(struct gui_window * gw, LGRECT * area);
/* update loc / size of the browser widgets: */
void browser_update_rects(struct gui_window * gw );
/*
	This queues an redraw to one of the slots.
	The following strategy is used:
	1. It checks if the rectangle to be scheduled is within one of the
		already queued bboxes. If yes, it will return.
	2. It checks for an intersection, and it will merge the rectangle to
		already queued rectangle where it fits best.
	3. it tries to put the rectangle into one available slot.
	4. if no slot is available, it will simply merge the new rectangle with
   	the last available slot.
*/
void browser_schedule_redraw_rect(struct gui_window * gw, short x, short y, short w, short h);
void browser_schedule_redraw(struct gui_window * gw, short x, short y, short w, short h );

#endif

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

/* 
 Each browser_window in the Atari Port is represented by an  struct s_browser,
 which cosnist mainly of an WinDom COMPONENT.
*/ 

#define BROWSER_SCROLL_SVAL 64  /* The small scroll inc. value */

enum browser_type 
{
	BT_ROOT=1,
	BT_FRAME=2,
	BT_FRAMESET=3,
	BT_IFRAME=4
};

enum browser_rect
{
	BR_CONTENT = 1,
	BR_FULL = 2,
	BR_HSLIDER = 3,
	BR_VSLIDER = 4
};

struct s_browser * browser_create( struct gui_window * gw, struct browser_window * clone, struct browser_window *bw, enum browser_type, int lt,  int w, int flex );
bool browser_destroy( struct s_browser * b );
void browser_get_rect( struct gui_window * gw, enum browser_rect type, LGRECT * out);
bool browser_input( struct gui_window * gw, unsigned short nkc ) ;
void browser_redraw( struct gui_window * gw );
void browser_set_content_size(struct gui_window * gw, int w, int h);
void browser_scroll( struct gui_window * gw, short MODE, int value, bool abs );
bool browser_attach_frame( struct gui_window * container, struct gui_window * frame );
struct gui_window * browser_find_root( struct gui_window * gw );
static void browser_process_scroll( struct gui_window * gw, LGRECT bwrect );
bool browser_redraw_required( struct gui_window * gw);
void browser_redraw_caret( struct gui_window * gw, GRECT * area );
static void browser_redraw_content( struct gui_window * gw, int xoff, int yoff );

/* update loc / size of the browser widgets: */
void browser_update_rects(struct gui_window * gw );
void browser_schedule_redraw(struct gui_window * gw, short x, short y, short w, short h );
static void __CDECL browser_evnt_resize( COMPONENT * c, long buff[8], void * data);
static void __CDECL browser_evnt_destroy( COMPONENT * c, long buff[8], void * data);
static void __CDECL browser_evnt_mbutton( WINDOW * c, short buff[8], void * data);
static void __CDECL browser_evnt_arrowed( WINDOW *win, short buff[8], void * data);
static void __CDECL browser_evnt_slider( WINDOW *win, short buff[8], void * data);
static void __CDECL browser_evnt_redraw( COMPONENT * c, long buff[8], void * data);
static void __CDECL browser_evnt_redraw_x( WINDOW * c, short buff[8], void * data);

#endif

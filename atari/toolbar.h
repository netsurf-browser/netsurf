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

#ifndef NS_ATARI_TOOLBAR_H
#define NS_ATARI_TOOLBAR_H

#define THROBBER_MIN_INDEX 1
#define THROBBER_MAX_INDEX 12
#define THROBBER_INACTIVE_INDEX 13


CMP_TOOLBAR tb_create( struct gui_window * gw );
void tb_destroy( CMP_TOOLBAR tb );


static void __CDECL evnt_toolbar_redraw( COMPONENT *c, long buff[8], void *data );
//static void __CDECL evnt_toolbar_mbutton( COMPONENT *c, long buff[8], void *data );
static void __CDECL evnt_toolbar_resize( COMPONENT *c, long buff[8], void *data );

/* report click to toolbar, relative coords : */
void tb_click( struct gui_window * gw, short mx, short my, short mb, short kstat );
void tb_back_click( struct gui_window * gw );
void tb_reload_click( struct gui_window * gw );
void tb_forward_click( struct gui_window * gw );
void tb_home_click( struct gui_window * gw );
void tb_stop_click( struct gui_window * gw );

/* enable / disable buttons etc. */
void tb_update_buttons( struct gui_window * gw );
/* handles clicks on url widget: */
void tb_url_click( struct gui_window * gw, short mx, short my, short mb, short kstat );
/* handle keybd event while url widget has focus:*/
bool tb_url_input( struct gui_window * gw, short keycode );
/* place the caret and adjust scrolling position: */
void tb_url_place_caret( struct gui_window * gw, int steps, bool abs);
/* set the url: */
void tb_url_set( struct gui_window * gw, char * text );

struct gui_window * tb_gui_window( CMP_TOOLBAR tb );

#endif

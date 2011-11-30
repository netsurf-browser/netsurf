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

#ifndef NS_ATARI_BROWSER_WIN_H
#define NS_ATARI_BROWSER_WIN_H

#define GUIWIN_VISIBLE(gw) (gw->root->handle->status & WS_OPEN)
#define GEMWIN_VISIBLE(win) (win->status & WS_OPEN)

#define WIDGET_STATUSBAR  0x1
#define WIDGET_TOOLBAR    0x2
#define WIDGET_SCROLL	  0x4
#define WIDGET_RESIZE	  0x8

/* WinDom & Custom bindings for gui window */

/* -------------------------------------------------------------------------- */
/* Public module functions:                                                   */
/* -------------------------------------------------------------------------- */

/*	Creates an normal Browser window with [toolbar], [statusbar] */
int window_create( struct gui_window * gw,
				struct browser_window * bw, unsigned long flags );
/* Destroys WinDom part of gui_window */
int window_destroy( struct gui_window * gw );

/* show the window */
void window_open( struct gui_window * gw, GRECT pos);

void window_snd_redraw(struct gui_window * gw, short x, short y, short w, short h );
/* Update Shade / Unshade state of the fwd/back buttons*/
void window_update_back_forward(struct gui_window * gw);
/* set root browser component: */
void window_attach_browser( struct gui_window * gw, CMP_BROWSER b);

/* set focus element */
void window_set_focus( struct gui_window * gw, enum focus_element_type type, void * element );
/* adjust scroll settings */
void window_set_scroll_info(struct gui_window *gw, int content_h, int content_w);
/* Shade / Unshade the forward/back bt. of toolbar, depending on history.*/
bool window_widget_has_focus( struct gui_window * gw, enum focus_element_type t, void * element);
bool window_url_widget_has_focus( struct gui_window * gw );
void window_set_url( struct gui_window * gw, const char * text);
void window_set_stauts( struct gui_window * gw , char * text );
void window_set_icon(struct gui_window * gw, struct bitmap * bmp );


/* -------------------------------------------------------------------------- */
/* Public event handlers:                                                     */
/* -------------------------------------------------------------------------- */

#endif

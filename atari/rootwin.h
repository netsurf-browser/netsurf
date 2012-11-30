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

#include <atari/gui.h>

#define GUIWIN_VISIBLE(gw) (gw->root->handle->status & WS_OPEN)
#define GEMWIN_VISIBLE(win) (win->status & WS_OPEN)

#define WIDGET_STATUSBAR  	0x1
#define WIDGET_TOOLBAR    	0x2
#define WIDGET_SCROLL	  	0x4
#define WIDGET_RESIZE	  	0x8

#define WIN_TOP				0x100

/* WinDom & Custom bindings for gui window */

/* -------------------------------------------------------------------------- */
/* Public module functions:                                                   */
/* -------------------------------------------------------------------------- */

/*	Creates an normal Browser window with [toolbar], [statusbar] */
int window_create(struct gui_window * gw,
				struct browser_window * bw, unsigned long flags );
/* Destroys WinDom part of gui_window */
int window_destroy(struct s_gui_win_root * rootwin);

/* show the window */
void window_open(struct s_gui_win_root * rootwin, GRECT pos);

void window_snd_redraw(struct s_gui_win_root * rootwin, short x, short y,
                       short w, short h );
/* Update Shade / Unshade state of the fwd/back buttons*/
void window_update_back_forward(struct s_gui_win_root * rootwin);
/* set root browser component: */
void window_attach_browser(struct s_gui_win_root * rootwin, CMP_BROWSER b);

/* set focus element */
void window_set_focus(struct s_gui_win_root * rootwin,
                      enum focus_element_type type, void * element );
/* adjust scroll settings */
void window_set_scroll_info(struct s_gui_win_root * rootwin, int content_h,
                            int content_w);
/* Shade / Unshade the forward/back bt. of toolbar, depending on history.*/
bool window_widget_has_focus(struct s_gui_win_root * rootwin,
                             enum focus_element_type t, void * element);
bool window_url_widget_has_focus(struct s_gui_win_root * rootwin);
void window_set_url(struct s_gui_win_root * rootwin, const char * text);
void window_set_stauts(struct s_gui_win_root * rootwin, char * text);
void window_set_title(struct s_gui_win_root * rootwin, char * text);
void window_set_content_size(struct s_gui_win_root * rootwin, int w, int h);
void window_set_icon(struct s_gui_win_root * rootwin, struct bitmap * bmp );
void window_set_active_gui_window(ROOTWIN *rootwin, struct gui_window *gw);
void window_schedule_redraw_grect(ROOTWIN *rootwin, GRECT *area);
void window_process_redraws(ROOTWIN * rootwin);
struct gui_window * window_get_active_gui_window(ROOTWIN * rootwin);
void window_redraw_favicon(struct s_gui_win_root * rootwin, GRECT *clip);
void window_unref_gui_window(ROOTWIN *rootwin, struct gui_window *gw);
bool window_key_input(unsigned short kcode, unsigned short kstate,
						unsigned short nkc);


/* -------------------------------------------------------------------------- */
/* Public event handlers:                                                     */
/* -------------------------------------------------------------------------- */

#endif

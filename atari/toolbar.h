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

#include "desktop/textarea.h"
#include "desktop/textinput.h"
#include "content/hlcache.h"
#include "atari/browser.h"

#define TB_BUTTON_WIDTH 32
#define THROBBER_WIDTH 32
#define THROBBER_MIN_INDEX 1
#define THROBBER_MAX_INDEX 12
#define THROBBER_INACTIVE_INDEX 13

#define TOOLBAR_URL_MARGIN_LEFT 	2
#define TOOLBAR_URL_MARGIN_RIGHT 	2
#define TOOLBAR_URL_MARGIN_TOP		2
#define TOOLBAR_URL_MARGIN_BOTTOM	2

enum e_toolbar_button_states {
        button_on = 0,
        button_off = 1
};
#define TOOLBAR_BUTTON_NUM_STATES   2

struct s_tb_button
{
	short rsc_id;
	void (*cb_click)(struct gui_window * gw);
	COMPONENT * comp;
	hlcache_handle * icon[TOOLBAR_BUTTON_NUM_STATES];
	struct gui_window * gw;
    short state;
    short index;
};


struct s_url_widget
{
	bool redraw;		 	/* widget is only redrawn when this flag is set */
	struct textarea *textarea;
	COMPONENT * comp;
	GRECT rdw_area;
};

struct s_throbber_widget
{
	COMPONENT * comp;
	short index;
	short max_index;
	bool running;
};

struct s_toolbar
{
	COMPONENT * comp;
	struct gui_window * owner;
	struct s_url_widget url;
	struct s_throbber_widget throbber;
	GRECT btdim;
	/* size & location of buttons: */
	struct s_tb_button * buttons;
	bool hidden;
	int btcnt;
	int style;
	bool redraw;
};

/* interface to the toolbar */

/* Must be called before any other toolbar function is called: */
void toolbar_init( void );
/*Must be called when netsurf exits to free toolbar resources: */
void toolbar_exit( void );
CMP_TOOLBAR tb_create( struct gui_window * gw );
void tb_destroy( CMP_TOOLBAR tb );
/* recalculate size/position of nested controls within the toolbar: */
void tb_adjust_size( struct gui_window * gw );
/* report click to toolbar, relative coords : */
void tb_click( struct gui_window * gw, short mx, short my, short mb, short kstat );
void tb_back_click( struct gui_window * gw );
void tb_reload_click( struct gui_window * gw );
void tb_forward_click( struct gui_window * gw );
void tb_home_click( struct gui_window * gw );
void tb_stop_click( struct gui_window * gw );
/* enable / disable buttons etc. */
void tb_update_buttons( struct gui_window * gw, short buttonid );
/* handles clicks on url widget: */
void tb_url_click( struct gui_window * gw, short mx, short my, short mb, short kstat );
/* handle keybd event while url widget has focus:*/
bool tb_url_input( struct gui_window * gw, short keycode );
/* set the url: */
void tb_url_set( struct gui_window * gw, char * text );
/* perform redraw of invalidated url textinput areas: */
void tb_url_redraw( struct gui_window * gw );
struct gui_window * tb_gui_window( CMP_TOOLBAR tb );
/* hide toolbar, mode = 1: hide, mode = 0: show */
void tb_hide( struct gui_window * gw, short mode );

#endif

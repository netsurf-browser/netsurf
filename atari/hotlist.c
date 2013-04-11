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

#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "desktop/browser.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/options.h"
#include "desktop/hotlist.h"
#include "desktop/tree.h"
#include "desktop/tree_url_node.h"
#include "desktop/gui.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/url.h"
#include "atari/gui.h"
#include "atari/misc.h"
#include "atari/treeview.h"
#include "atari/hotlist.h"
#include "atari/findfile.h"
#include "atari/gemtk/gemtk.h"
#include "atari/res/netsurf.rsh"

extern GRECT desk_area;

struct atari_hotlist hl;

static short handle_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8])
{
	NSTREEVIEW tv=NULL;
	GRECT tb_area;

	if(ev_out->emo_events & MU_MESAG){
		switch (msg[0]) {

			case WM_TOOLBAR:

				tv = (NSTREEVIEW) gemtk_wm_get_user_data(win);

				switch	(msg[4]) {
					case TOOLBAR_HOTLIST_CREATE_FOLDER:
						hotlist_add_folder(true);
						break;

					case TOOLBAR_HOTLIST_ADD:
						atari_hotlist_add_page("http://www.de", "");
						break;

					case TOOLBAR_HOTLIST_DELETE:
						hotlist_delete_selected();
						gemtk_wm_exec_redraw(tv->window, NULL);
						break;

					case TOOLBAR_HOTLIST_EDIT:
						hotlist_edit_selected();
						break;
				}

				gemtk_obj_get_tree(TOOLBAR_HOTLIST)[msg[4]].ob_state &= ~OS_SELECTED;
				gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_TOOLBAR, &tb_area);
				evnt_timer(150);
				gemtk_wm_exec_redraw(tv->window, &tb_area);
			break;

			case WM_CLOSED:
				hotlist_close();
			break;

			default: break;
		}
	}

	// TODO: implement selectable objects in toolbar API:
	// ObjcChange( OC_TOOLBAR, win, buff[4], ~SELECTED, OC_MSG );
}



void hotlist_init(void)
{
	if (hl.init == false) {
		if( strcmp(nsoption_charp(hotlist_file), "") == 0 ){
			atari_find_resource( (char*)&hl.path, "hotlist", "hotlist" );
		} else {
			strncpy( (char*)&hl.path, nsoption_charp(hotlist_file), PATH_MAX-1 );
		}

		LOG(("Hotlist: %s",  (char*)&hl.path ));

		if( hl.window == NULL ){
			int flags = ATARI_TREEVIEW_WIDGETS;
			short handle = -1;
			GRECT desk;
			OBJECT * tree = gemtk_obj_get_tree(TOOLBAR_HOTLIST);
			assert( tree );
			hl.open = false;

			handle = wind_create(flags, 0, 0, desk_area.g_w, desk_area.g_h);
			hl.window = gemtk_wm_add(handle, GEMTK_WM_FLAG_DEFAULTS, NULL);
			if( hl.window == NULL ) {
				gemtk_msg_box_show(GEMTK_MSG_BOX_ALERT,
									"Failed to allocate Hotlist");
				return;
			}
			wind_set_str(handle, WF_NAME, (char*)messages_get("Hotlist"));
			gemtk_wm_set_toolbar(hl.window, tree, 0, 0);
			gemtk_wm_unlink(hl.window);
			hl.tv = atari_treeview_create(
				hotlist_get_tree_flags(),
				hl.window,
				handle_event
			);
			if (hl.tv == NULL) {
				/* handle it properly, clean up previous allocs */
				LOG(("Failed to allocate treeview"));
				return;
			}

			hotlist_initialise(
				hl.tv->tree,
				(char*)&hl.path,
				"dir.png"
			);

		} else {

		}
	}
	hl.init = true;
}


void hotlist_open(void)
{
	if( hl.init == false ) {
		return;
	}

	if( hl.open == false ) {

	    GRECT pos;
	    pos.g_x = desk_area.g_w - desk_area.g_w / 4;
	    pos.g_y = desk_area.g_y;
	    pos.g_w = desk_area.g_w / 4;
	    pos.g_h = desk_area.g_h;

		wind_open_grect(gemtk_wm_get_handle(hl.window), &pos);
		hl.open = true;
		atari_treeview_open( hl.tv );
	} else {
		wind_set(gemtk_wm_get_handle(hl.window), WF_TOP, 1, 0, 0, 0);
	}
}

void hotlist_close(void)
{
	wind_close(gemtk_wm_get_handle(hl.window));
	hl.open = false;
	atari_treeview_close(hl.tv);
}

void hotlist_destroy(void)
{

	if( hl.init == false) {
		return;
	}
	if( hl.window != NULL ) {
		hotlist_cleanup( (char*)&hl.path );
		if (hl.open)
			hotlist_close();
		wind_delete(gemtk_wm_get_handle(hl.window));
		gemtk_wm_remove(hl.window);
		hl.window = NULL;
		atari_treeview_destroy(hl.tv);
		hl.init = false;
	}
	LOG(("done"));
}

void hotlist_redraw(void)
{
	int i = 01;
	atari_treeview_redraw(hl.tv);
}

struct node;

void atari_hotlist_add_page( const char * url, const char * title )
{
	struct node * root;
	struct node * selected = NULL;
	struct node * folder = NULL;
	NSTREEVIEW tv = hl.tv;
	if(hl.tv == NULL )
		return;

	hotlist_open();

	if( hl.tv->click.x >= 0 && hl.tv->click.y >= 0 ){
		hotlist_add_page_xy( url, hl.tv->click.x, hl.tv->click.y );
	} else {
		hotlist_add_page( url );
	}
}

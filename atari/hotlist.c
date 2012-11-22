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

struct atari_hotlist hl;

static short handle_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8])
{
	NSTREEVIEW tv=NULL;
	GRECT tb_area;

	if(ev_out->emo_events & MU_MESAG){
		switch (msg[0]) {

			case WM_TOOLBAR:

				tv = (NSTREEVIEW) guiwin_get_user_data(win);

				switch	(msg[4]) {
					case TOOLBAR_HOTLIST_CREATE_FOLDER:
						hotlist_add_folder(true);
						break;

					case TOOLBAR_HOTLIST_ADD:
						atari_hotlist_add_page("http://www.de", "");
						break;

					case TOOLBAR_HOTLIST_DELETE:
						hotlist_delete_selected();
						guiwin_send_redraw(tv->window, NULL);
						break;

					case TOOLBAR_HOTLIST_EDIT:
						hotlist_edit_selected();
						break;
				}

				get_tree(TOOLBAR_HOTLIST)[msg[4]].ob_state &= ~OS_SELECTED;
				guiwin_get_grect(tv->window, GUIWIN_AREA_TOOLBAR, &tb_area);
				evnt_timer(150);
				guiwin_send_redraw(tv->window, &tb_area);
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
		OBJECT * tree = get_tree(TOOLBAR_HOTLIST);
		assert( tree );
		hl.open = false;

		wind_get_grect(0, WF_FULLXYWH, &desk);

		handle = wind_create(flags, 0, 0, desk.g_w, desk.g_h);
		hl.window = guiwin_add(handle, GW_FLAG_DEFAULTS, NULL);
		if( hl.window == NULL ) {
			LOG(("Failed to allocate Hotlist"));
			return;
		}
		wind_set_str(handle, WF_NAME, (char*)messages_get("Hotlist"));
		guiwin_set_toolbar(hl.window, tree, 0, 0);
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
	hl.init = true;
}


void hotlist_open(void)
{
	if( hl.init == false ) {
		return;
	}

	if( hl.open == false ) {

	    GRECT pos;
	    wind_get_grect(0, WF_FULLXYWH, &pos);
	    pos.g_x = pos.g_w - pos.g_w / 4;
	    pos.g_y = pos.g_y;
	    pos.g_w = pos.g_w / 4;
	    pos.g_h = pos.g_h;

		wind_open_grect(guiwin_get_handle(hl.window), &pos);
		hl.open = true;
		atari_treeview_open( hl.tv );
	} else {
		wind_set(guiwin_get_handle(hl.window), WF_TOP, 1, 0, 0, 0);
	}
}

void hotlist_close(void)
{
	wind_close(guiwin_get_handle(hl.window));
	hl.open = false;
	atari_treeview_close( hl.tv );
}

void hotlist_destroy(void)
{
	if( hl.init == false ) {
		return;
	}
	if( hl.window != NULL ) {
		hotlist_cleanup( (char*)&hl.path );
		if( hl.open )
			hotlist_close();
		wind_delete(guiwin_get_handle(hl.window));
		guiwin_remove(hl.window);
		hl.window = NULL;
		atari_treeview_destroy( hl.tv  );
		hl.init = false;
	}
	LOG(("done"));
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
	if( hl.tv->click.x >= 0 && hl.tv->click.y >= 0 ){
		hotlist_add_page_xy( url, hl.tv->click.x, hl.tv->click.y );
	} else {
		hotlist_add_page( url );
	}
}

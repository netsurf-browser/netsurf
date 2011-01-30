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
#include "atari/res/netsurf.rsh"

static struct atari_hotlist {
	WINDOW * window;
	NSTREEVIEW tv;		/*< The hotlist treeview handle.  */
	bool open;
} hl;


static void evnt_hl_toolbar( WINDOW *win, short buff[8]) {
	int obj = buff[4];	/* Selected object */
	
	LOG(("item: %d clicked", obj ));

	switch( obj) {
	case TOOLBAR_HOTLIST_CREATE_FOLDER:
		break;
	case TOOLBAR_HOTLIST_ADD:
		break;
	case TOOLBAR_HOTLIST_DELETE:
		break;
	}
	
	/* object state to normal and redraw object */
/*
	ObjcChange( TOOLBAR_HOTLIST, hl.window, obj, SELECTED, 1);
	ObjcChange( TOOLBAR_HOTLIST, hl.window, obj, 0 , 1);
*/
}

static void __CDECL evnt_hl_close( WINDOW *win, short buff[8] )
{
	hotlist_close();
}

void hotlist_init(void)
{
	char hlfilepath[PATH_MAX];

	atari_find_resource( 
		(char*)&hlfilepath, "hotlist", "res/Hotlist"
	);

	if( hl.window == NULL ){
		int flags = CLOSER | MOVER | SIZER| NAME | FULLER | SMALLER ;
		OBJECT * tree = get_tree(TOOLBAR_HOTLIST);
		assert( tree );
		hl.open = false;
		hl.window = WindCreate( flags,40, 40, app.w, app.h );
		if( hl.window == NULL ) {
			LOG(("Failed to allocate Hotlist"));
			return;
		}
		/* TODO: load hotlist strings from messages */
		WindSetStr( hl.window, WF_NAME, (char*)"Hotlist" );
		WindSetPtr( hl.window, WF_TOOLBAR, 
			tree, 
			evnt_hl_toolbar
		);
		EvntAttach( hl.window, WM_CLOSED, evnt_hl_close );
		hl.tv = atari_treeview_create( 
			hotlist_get_tree_flags(),
			hl.window
		);
		if (hl.tv == NULL) {
			LOG(("Failed to allocate treeview"));
			return;
		}
		hotlist_initialise(
				atari_treeview_get_tree(hl.tv),
				/* TODO: use option_hotlist_file or slt*/
				(char*)&hlfilepath,
				"dir.png"
		);	
	} else {

	}
}


void hotlist_open(void) 
{
	hotlist_init();
	if( hl.open == false ) {
		WindOpen( hl.window, -1, -1, app.w/3, app.h/2);
		hl.open = true;
	}
	tree_set_redraw(atari_treeview_get_tree(hl.tv), true);
}

void hotlist_close(void)
{
	WindClose(hl.window);
	hl.open = false;
}

void hotlist_destroy(void)
{
	if( hl.window != NULL ) {
		if( hl.open ) 
			hotlist_close();
		WindDelete( hl.window );
		printf("delete hl tree");
		atari_treeview_destroy( hl.tv  );
		hl.window = NULL;
	}
}




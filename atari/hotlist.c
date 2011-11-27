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
#include "atari/res/netsurf.rsh"
#include "atari/options.h"

struct atari_hotlist hl;

static void evnt_hl_toolbar( WINDOW *win, short buff[8]) {
	/* handle toolbar object (index in buff[4] ) */	
	switch( buff[4] ) { 
	case TOOLBAR_HOTLIST_CREATE_FOLDER:
		hotlist_add_folder(true);
		break;

	case TOOLBAR_HOTLIST_ADD:
		atari_hotlist_add_page("http://www.de", "");
		break;

	case TOOLBAR_HOTLIST_DELETE:
		hotlist_delete_selected();
		break;
	
	case TOOLBAR_HOTLIST_EDIT:
		hotlist_edit_selected();
		break;
	}
	ObjcChange( OC_TOOLBAR, win, buff[4], ~SELECTED, OC_MSG ); 
}


static void __CDECL evnt_hl_close( WINDOW *win, short buff[8] )
{
	hotlist_close();
}


static void __CDECL evnt_hl_mbutton( WINDOW *win, short buff[8] )
{
	/* todo: implement popup? 
	if(evnt.mbut & 2) {
		
	}
	*/
}


void hotlist_init(void)
{
	if( strcmp(option_hotlist_file, "") == 0 ){
		atari_find_resource( (char*)&hl.path, "hotlist", "hotlist" );
	} else {
		strncpy( (char*)&hl.path, option_hotlist_file, PATH_MAX-1 );
	}

	LOG(("Hotlist: %s",  (char*)&hl.path ));

	if( hl.window == NULL ){
		int flags = ATARI_TREEVIEW_WIDGETS;
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
		WindSetPtr( hl.window, WF_TOOLBAR, tree, evnt_hl_toolbar );
		EvntAttach( hl.window, WM_CLOSED, evnt_hl_close );
		EvntAttach( hl.window, WM_XBUTTON,evnt_hl_mbutton );
		hl.tv = atari_treeview_create( 
			hotlist_get_tree_flags(),
			hl.window
		);
		if (hl.tv == NULL) {
			/* handle it properly, clean up previous allocs */
			LOG(("Failed to allocate treeview"));
			return;
		}
		
		hotlist_initialise(
			hl.tv->tree,
			/* TODO: use option_hotlist_file or slt*/
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
		WindOpen( hl.window, -1, -1, app.w/3, app.h/2);
		hl.open = true;
		atari_treeview_open( hl.tv );
	} else {
		WindTop( hl.window );	
	}
}

void hotlist_close(void)
{
	WindClose(hl.window);
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
		WindDelete( hl.window );
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

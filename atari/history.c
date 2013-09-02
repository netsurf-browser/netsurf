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

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "desktop/browser.h"
#include "utils/nsoption.h"
#include "desktop/tree.h"
#include "desktop/gui.h"
#include "desktop/history_global_core.h"
#include "desktop/browser.h"
#include "utils/messages.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "utils/log.h"
#include "atari/treeview.h"
#include "atari/findfile.h"
#include "atari/res/netsurf.rsh"
#include "atari/history.h"


//TODO: remove/add guiwin handle on close / open - so that the list
// is kept tiny.

extern char * tree_directory_icon_name;
extern GRECT desk_area;

struct s_atari_global_history gl_history;


void atari_global_history_open( void )
{
	atari_global_history_init();
	if (gl_history.init == false ) {
		return;
	}
	if( gl_history.open == false ) {

	    GRECT pos;
	    wind_get_grect(0, WF_WORKXYWH, &pos);
	    pos.g_x = desk_area.g_w - desk_area.g_w / 4;
	    pos.g_y = desk_area.g_y;
	    pos.g_w = desk_area.g_w / 4;
	    pos.g_h = desk_area.g_h;

		wind_open(gemtk_wm_get_handle(gl_history.window), pos.g_x, pos.g_y,
            pos.g_w, pos.g_h);
		gl_history.open = true;
		atari_treeview_open(gl_history.tv);
	} else {
		wind_set(gemtk_wm_get_handle(gl_history.window), WF_TOP, 1, 0, 0, 0);
	}
}

void atari_global_history_close( void )
{
    wind_close(gemtk_wm_get_handle(gl_history.window));
	gl_history.open = false;
	atari_treeview_close(gl_history.tv);
}

static short handle_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8])
{
	NSTREEVIEW tv=NULL;

    //printf("Hotlist event %d, open: %d\n", ev_out->emo_events, gl_history.open);

	if(ev_out->emo_events & MU_MESAG){
		switch (msg[0]) {

			case WM_CLOSED:
				atari_global_history_close();
			break;

			default: break;
		}
	}

	// TODO: implement selectable objects in toolbar API:
	// ObjcChange( OC_TOOLBAR, win, buff[4], ~SELECTED, OC_MSG );
}

bool atari_global_history_init( void )
{

    if( gl_history.init == false ) {

        short handle;
        GRECT desk;
        int flags = ATARI_TREEVIEW_WIDGETS;

        gl_history.open = false;
        handle = wind_create(flags, 40, 40, desk_area.g_w, desk_area.g_h);
        gl_history.window = gemtk_wm_add(handle, GEMTK_WM_FLAG_DEFAULTS, NULL);
        if( gl_history.window == NULL ) {
			LOG(("Failed to allocate history window"));
			return( false );
		}
		wind_set_str(handle, WF_NAME, (char*)messages_get("GlobalHistory"));

		gl_history.tv = atari_treeview_create(history_global_get_tree_flags(),
                                        gl_history.window, handle_event);

		gemtk_wm_unlink(gl_history.window);

        if (gl_history.tv == NULL) {
            /* TODO: handle it properly, clean up previous allocs */
            LOG(("Failed to allocate history treeview"));
            return( false );
        }

        history_global_initialise(gl_history.tv->tree, "dir.png");
        gl_history.init = true;
    }
	return( true );
}


void atari_global_history_destroy( void )
{

	if( gl_history.init == false ) {
		return;
	}
	if( gl_history.window != NULL ) {
		history_global_cleanup();
		if( gl_history.open )
			atari_global_history_close();
		wind_delete(gemtk_wm_get_handle(gl_history.window));
		gemtk_wm_remove(gl_history.window);
		gl_history.window = NULL;
		atari_treeview_destroy(gl_history.tv);
		gl_history.init = false;
	}
	LOG(("done"));
}

void atari_global_history_redraw( void )
{
	atari_treeview_redraw( gl_history.tv );
}



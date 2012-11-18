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

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <windom.h>

#include "desktop/gui.h"
#include "utils/log.h"

#include "atari/misc.h"
#include "atari/gui.h"
#include "atari/rootwin.h"
#include "atari/toolbar.h"
#include "atari/browser.h"
#include "atari/global_evnt.h"
#include "atari/res/netsurf.rsh"
#include "atari/deskmenu.h"
#include "cflib.h"

extern struct gui_window *input_window;
extern int mouse_click_time[3];
extern int mouse_hold_start[3];
extern browser_mouse_state bmstate;
extern short last_drag_x;
extern short last_drag_y;

/* Global event handlers: */
static void __CDECL global_evnt_apterm( WINDOW * win, short buff[8] );
static void __CDECL global_evnt_m1( WINDOW * win, short buff[8] );
static void __CDECL global_evnt_keybd( WINDOW * win, short buff[8],void * data);

void __CDECL global_evnt_apterm(WINDOW * win, short buff[8])
{
	int i = 0;
	LOG((""));
	netsurf_quit = true;
}


static void __CDECL global_evnt_m1(WINDOW * win, short buff[8])
{
	struct gui_window * gw = input_window;
	static bool prev_url = false;
	static short prev_x=0;
	static short prev_y=0;
	bool within = false;
	LGRECT urlbox, bwbox, sbbox;
	int nx, ny;

	if (gw == NULL)
		return;

	if (prev_x == evnt.mx && prev_y == evnt.my) {
		return;
	}

	short ghandle = wind_find( evnt.mx, evnt.my );
	if (input_window->root->handle->handle == ghandle) {

		// The window found at x,y is an gui_window
		// and it's the input window.

		browser_get_rect( gw, BR_CONTENT, &bwbox );

		if (evnt.mx > bwbox.g_x && evnt.mx < bwbox.g_x + bwbox.g_w &&
			evnt.my > bwbox.g_y &&  evnt.my < bwbox.g_y + bwbox.g_h) {
			within = true;
			browser_window_mouse_track(
							input_window->browser->bw,
							0,
							evnt.mx - bwbox.g_x + gw->browser->scroll.current.x,
							evnt.my - bwbox.g_y + gw->browser->scroll.current.y
						);
		}

		if (gw->root->toolbar && within == false) {
			mt_CompGetLGrect(&app, gw->root->toolbar->url.comp, WF_WORKXYWH, &urlbox);
			if( (evnt.mx > urlbox.g_x && evnt.mx < urlbox.g_x + urlbox.g_w ) &&
				(evnt.my > urlbox.g_y && evnt.my < + urlbox.g_y + urlbox.g_h )) {
				gem_set_cursor( &gem_cursors.ibeam );
				prev_url = true;
			} else {
				if( prev_url ) {
					gem_set_cursor( &gem_cursors.arrow );
					prev_url = false;
				}
			}
		}
	} else {
		gem_set_cursor( &gem_cursors.arrow );
		prev_url = false;
	}

	prev_x = evnt.mx;
	prev_y = evnt.my;
}

void __CDECL global_evnt_keybd( WINDOW * win, short buff[8], void * data)
{
	long kstate = 0;
	long kcode = 0;
	unsigned short nkc = 0;
	unsigned short nks = 0;

	int i=0;
	bool done = false;
	struct gui_window * gw = input_window;
	struct gui_window * gw_tmp;
	if( gw == NULL )
		return;
	kstate = evnt.mkstate;
	kcode = evnt.keybd;
	nkc= gem_to_norm( (short)kstate, (short)kcode );
	nks = (nkc & 0xFF00);
	if( kstate & (K_LSHIFT|K_RSHIFT))
		kstate |= K_LSHIFT|K_RSHIFT;
	if( window_url_widget_has_focus( gw ) ) {
		/* make sure we report for the root window and report...: */
		done = tb_url_input( gw, nkc );
	}  else  {
		gw_tmp = window_list;
		/* search for active browser component: */
		while( gw_tmp != NULL && done == false ) {
			/* todo: only handle when input_window == ontop */
			if( window_widget_has_focus( (struct gui_window *)input_window,
										 BROWSER,(void*)gw_tmp->browser)) {
				done = browser_input( gw_tmp, nkc );
				break;
			} else {
				gw_tmp = gw_tmp->next;
			}
		}
	}
	if(!done)
		deskmenu_dispatch_keypress(evnt.keybd, kstate, nkc);
}


/* Bind global and menu events to event handler functions, create accelerators */
void bind_global_events( void )
{
	memset( (void*)&evnt_data, 0, sizeof(struct s_evnt_data) );
	EvntDataAttach( NULL, WM_XKEYBD, global_evnt_keybd, (void*)&evnt_data );
	EvntAttach( NULL, AP_TERM, global_evnt_apterm );
	EvntAttach( NULL, WM_XM1, global_evnt_m1 );
}

void unbind_global_events( void )
{

}


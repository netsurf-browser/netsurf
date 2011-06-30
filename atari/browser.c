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
#include "desktop/netsurf.h"
#include "desktop/browser.h"
#include "desktop/mouse.h"
#include "desktop/textinput.h"
#include "utils/log.h"
#include "utils/messages.h"

#include "atari/gui.h"
#include "atari/browser_win.h"
#include "atari/misc.h"
#include "atari/global_evnt.h"
#include "atari/browser_win.h"
#include "atari/res/netsurf.rsh"
#include "atari/browser.h"
#include "atari/plot/plotter.h"
#include "atari/plot.h"
#include "atari/font.h"
#include "cflib.h"

extern browser_mouse_state bmstate;
extern int mouse_click_time[3];
extern int mouse_hold_start[3];
extern GEM_PLOTTER plotter;
extern struct gui_window *input_window;
extern short last_drag_x;
extern short last_drag_y;



static void __CDECL browser_evnt_wdestroy( WINDOW * c, short buff[8], void * data);
COMPONENT *comp_widget_create( APPvar *app, WINDOW *win, int size, int flex );

static bool frameinit = true;


/* create an browser component */
struct s_browser * browser_create(  struct gui_window * gw, 
									struct browser_window *bw, 
									struct browser_window * clone, 
									enum browser_type type, 
									int lt, int w, int flex )
{
	LGRECT cwork;
	COMPONENT * scrollv, * scrollh, * drawable;

	if( frameinit ) {
		mt_FrameInit( &app );
		frameinit = false;
	}

	CMP_BROWSER bnew = (CMP_BROWSER)malloc( sizeof(struct s_browser) );
	if( bnew )
	{
		memset(bnew, 0, sizeof(struct s_browser) );
		bnew->type = type;
		bnew->bw = bw;
		bnew->attached = false;
		if(clone)
			bw->scale = clone->scale;
		else
			bw->scale = 1;
		bnew->redraw.areas_used = 0;
		bnew->compwin = mt_WindCreate( &app, VSLIDE|HSLIDE, 1, 1, app.w, app.h);
		bnew->compwin->w_u = 1;
		bnew->compwin->h_u = 1;
		/* needs to be adjusted when content width is known: */
		bnew->compwin->ypos_max = w; 
		bnew->compwin->xpos_max = w;
		mt_WindSlider( &app, bnew->compwin, HSLIDER|VSLIDER);
		bnew->comp = (COMPONENT*)comp_widget_create( (void*)&app, (WINDOW*)bnew->compwin, 1, 1 ); 
		if( bnew->comp == NULL ) {
			free(bnew);
			return(NULL);
		}
		mt_EvntDataAdd( &app, bnew->compwin, WM_XBUTTON, 
							browser_evnt_mbutton, (void*)gw, EV_BOT );
		mt_CompEvntDataAttach( &app, bnew->comp, WM_REDRAW, 
								browser_evnt_redraw, (void*)gw );
		mt_EvntDataAttach( &app, bnew->compwin , WM_REDRAW, browser_evnt_redraw_x, NULL );
		mt_EvntDataAttach( &app, bnew->compwin, WM_SLIDEXY, 
								browser_evnt_slider, gw );
		mt_EvntDataAttach( &app, bnew->compwin, WM_ARROWED, 
								browser_evnt_arrowed, gw );	
		mt_CompEvntDataAttach( &app, bnew->comp, WM_DESTROY, 
								browser_evnt_destroy, (void*)bnew );
		/* just stub, as an reminder: */
		mt_EvntDataAttach( &app, bnew->compwin, WM_DESTROY, 
								browser_evnt_wdestroy, (void*)bnew );

		mt_CompDataAttach( &app, bnew->comp, CDT_OWNER, gw ); 
		bnew->scroll.requested.y = 0;	
		bnew->scroll.requested.x = 0;
		bnew->scroll.current.x = 0;
		bnew->scroll.current.y = 0;
	}
	return( bnew );
}

bool browser_destroy( struct s_browser * b )
{
	short type = BT_ROOT;
	LGRECT restore;
	/* only free the components if it is an root browser,
		this should delete all attached COMPONENTS... 
	*/

	LOG(("%s (%s)\n", b->bw->name,  (b->type == BT_ROOT) ? "ROOT" : "FRAME"));
	assert( b != NULL );
	assert( b->comp != NULL );
	assert( b->bw != NULL );
	struct gui_window * rootgw = browser_find_root( b->bw->window );

	if( b->type == BT_ROOT ) {
		
	} else if( BT_FRAME ){
		type = BT_FRAME;
	} else {
		assert( 1 == 0 ); 
	}

	if( b->compwin != NULL ){
		COMPONENT * old = b->comp;
		WINDOW * oldwin = b->compwin;
		b->comp = NULL;
		b->compwin = NULL;
		/* Im not sure if this is the right thing to do (after compdelete above,... */
		/* listRemove should be used when we just remove an frame... */
		/* (I encountered spurious events when not doing that...) */
		/* listRemove( (LINKABLE*) oldwin ); */
		/* listRemove( (LINKABLE*) old ); */
		WindDelete( oldwin );
		mt_CompDelete(&app,  old );	
	}

	if( type == BT_FRAME ) {
		/* TODO: restore the remaining frameset (rebuild???) */
	}
	return( true );
}

bool browser_attach_frame( struct gui_window * container, struct gui_window * frame )
{
	struct browser_window * cbw = container->browser->bw;
	int lt = CLT_STACK;
	if (cbw->rows >= cbw->cols)
		lt = CLT_VERTICAL;
	else 
		lt = CLT_HORIZONTAL;

	printf("attaching frame as: %s\n", (lt == CLT_VERTICAL) ? "CLT_VERTICAL" : "CLT_HORIZONTAL" );
	/* todo: if first frame, remove compwin, or something like that, because it is still occupiying space */
	container->browser->compwin->h_max = 0;
	container->browser->compwin->w_max = 0;
	container->browser->comp->flex = 0;
	container->browser->comp->size = 300;
	container->browser->comp->type = lt;
	mt_CompAttach( &app,  container->browser->comp, frame->browser->comp );
	browser_update_rects( container );
	frame->browser->attached = true;	
}

/* find the root of an frame ( or just return gw if is already the root) */
struct gui_window * browser_find_root( struct gui_window * gw )
{
	if( gw->parent == NULL )
		return( gw ); 
	struct gui_window * g;
	for( g=window_list; g; g=g->next){
		if( g->root == gw->root && g->parent == NULL )
			return( g );
	}
	return( NULL );
}

void browser_get_rect( struct gui_window * gw, enum browser_rect type, LGRECT * out)
{
	GRECT work;
	assert( out != NULL);
	int slider_v_w = 20;
	int slider_v_h = 22;
	int slider_h_w = 20;
	int slider_h_h = 22;

	WindGetGrect( gw->browser->compwin, WF_WORKXYWH, &work);
	if( type == BR_CONTENT ){
		out->g_w = work.g_w;
		out->g_h = work.g_h;
		out->g_x = work.g_x;
		out->g_y = work.g_y;
		return;
	}
	
	LGRECT cur;
	mt_CompGetLGrect(&app, gw->browser->comp, WF_WORKXYWH, &cur);
	if( type == BR_HSLIDER ){
		out->g_x = cur.g_x;
		out->g_y = cur.g_y + work.g_h;
		out->g_h = cur.g_h - work.g_h;
		out->g_w = cur.g_w;
	}

	if( type == BR_VSLIDER ){
		out->g_x = cur.g_x + work.g_w;
		out->g_y = cur.g_y;
		out->g_w = cur.g_w - work.g_w;
		out->g_h = work.g_h;
	}

}

/* Report an resize to the COMPONENT interface */
void browser_update_rects(struct gui_window * gw )
{
	short buff[8];
	LGRECT cmprect;
	mt_WindGetGrect( &app, gw->root->handle, WF_CURRXYWH, (GRECT*)&buff[4]);
	buff[0] = CM_REFLOW;
	buff[1] = _AESapid;
	buff[2] = 0;
	EvntExec(gw->root->handle, buff);
}

void browser_set_content_size(struct gui_window * gw, int w, int h)
{
	CMP_BROWSER b = gw->browser;
	LGRECT work;
	browser_get_rect( gw, BR_CONTENT, &work );
	b->compwin->ypos_max = h;
	b->compwin->xpos_max = w;
	if( w < work.g_w + b->scroll.current.x || w < work.g_h + b->scroll.current.y ) {
		/* let the scroll routine detect invalid scroll values... */
		browser_scroll(gw, WA_LFLINE, b->scroll.current.x, true );
		browser_scroll(gw, WA_UPLINE, b->scroll.current.y, true );
		/* force update of scrollbars: */
		b->scroll.required = true;
	}
}

static void __CDECL browser_evnt_wdestroy( WINDOW * c, short buff[8], void * data)
{
	struct s_browser * b = (struct s_browser*)data;
	LOG((""));
}

static void __CDECL browser_evnt_destroy( COMPONENT * c, long buff[8], void * data)
{
	struct s_browser * b = (struct s_browser*)data;
	struct gui_window * gw = b->bw->window;
	LOG(("%s: %s (%s)\n", __FUNCTION__ ,gw->browser->bw->name, ( gw->browser->type ==BT_FRAME) ? "FRAME" : "NOFRAME"));
	
	assert( b != NULL );
	assert( gw != NULL );
	/* TODO: inspect why this assert fails with frames / iframes
	/* this should have been happened alrdy */
	/* assert( b->comp == NULL ); */

	free( b );
	gw->browser = NULL;
	LOG(("evnt_destroy done!"));
}


static void __CDECL browser_evnt_arrowed( WINDOW *win, short buff[8], void * data) 
{
	bool abs = false;
	int value = BROWSER_SCROLL_SVAL;
	struct gui_window * gw = data;
	LGRECT cwork;
	browser_get_rect( gw, BR_CONTENT, &cwork );

	switch( buff[4] ) {
		case WA_UPPAGE:
		case WA_DNPAGE:
				value = cwork.g_h;
			break;

		case WA_LFPAGE:
		case WA_RTPAGE:
				value = cwork.g_w;
			break;
		
		default: 
			break;
	}
	browser_scroll( gw, buff[4], value, abs );
}

void __CDECL browser_evnt_slider( WINDOW *win, short buff[8], void * data)
{
	int dx = buff[4];
	int dy = buff[5];
	struct gui_window * gw = data;
	GRECT work, screen;

	if (!dx && !dy) return;

	/* update the sliders _before_ we call redraw (which might depend on the slider possitions) */
	mt_WindSlider( &app, win, (dx?HSLIDER:0) | (dy?VSLIDER:0) );

	if( dy > 0 )
		browser_scroll( gw, WA_DNPAGE, abs(dy), false );
	else if ( dy < 0)
		browser_scroll( gw, WA_UPPAGE, abs(dy), false );
	if( dx > 0 )
		browser_scroll( gw, WA_RTPAGE, abs(dx), false );
	else if( dx < 0 )
		browser_scroll( gw, WA_LFPAGE, abs(dx), false );
}

static void __CDECL browser_evnt_mbutton( WINDOW * c, short buff[8], void * data)
{
	long lbuff[8];
	short i;
	short mx, my, dummy, mbut;
	uint32_t tnow = clock()*1000 / CLOCKS_PER_SEC;
	LGRECT cwork;
	struct gui_window * gw = data;
	input_window = gw;
	window_set_focus( gw, BROWSER, (void*)gw->browser );
	browser_get_rect( gw, BR_CONTENT, &cwork );
	mx = evnt.mx - cwork.g_x; 
	my = evnt.my - cwork.g_y;
	LOG(("mevent (%d) within %s at %d / %d\n", evnt.nb_click, gw->browser->bw->name, mx, my ));

	if( evnt.mkstate & (K_RSHIFT | K_LSHIFT) ){
		bmstate |= BROWSER_MOUSE_MOD_1;
	} else {
		bmstate &= ~(BROWSER_MOUSE_MOD_1);
	}
	if( (evnt.mkstate & K_CTRL) ){
		bmstate |= BROWSER_MOUSE_MOD_2;
	} else {
		bmstate &= ~(BROWSER_MOUSE_MOD_2);
	}
	if( (evnt.mkstate & K_ALT) ){
		bmstate |= BROWSER_MOUSE_MOD_3;
	} else {
		bmstate &= ~(BROWSER_MOUSE_MOD_3);
	}
	int sx = (mx + gw->browser->scroll.current.x);
	int sy = (my + gw->browser->scroll.current.y);

	graf_mkstate(&dummy, &dummy, &mbut, &dummy);
	/* todo: if we need right button click, increase loop count */
	for( i = 1; i<2; i++) {
		if( (mbut & i)  ) {
			if( mouse_hold_start[i-1] == 0 ) {
				mouse_hold_start[i-1] = tnow;
				LOG(("Drag %d starts at %d,%d\n", i, sx, sy));
				if( i == 1 ) {
					browser_window_mouse_click(gw->browser->bw,BROWSER_MOUSE_PRESS_1,sx,sy);
					bmstate |= BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_DRAG_ON;
				}
				if( i == 2 ) {
					browser_window_mouse_click(gw->browser->bw,BROWSER_MOUSE_PRESS_2,sx,sy);
					bmstate |= BROWSER_MOUSE_HOLDING_2 | BROWSER_MOUSE_DRAG_ON;
				}				
			} else {
				if( i == 1 ) {
					bmstate |= BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_DRAG_ON;
				}
				if( i == 2 ) {
					bmstate |= BROWSER_MOUSE_DRAG_2 | BROWSER_MOUSE_DRAG_ON;
				}
			}

			if( i != 0 ){
				if( (abs(mx-last_drag_x)>5) || (abs(mx-last_drag_y)>5) ){	
					browser_window_mouse_track( 
						gw->browser->bw,
						bmstate, 
						sx, sy
					);
					last_drag_x = mx;
					last_drag_y = my;
				}
			}

		} else {
			mouse_click_time[i-1] = tnow; /* clock in ms */
			/* check if this event was during an drag op: */
			if( mouse_hold_start[i-1] == 0 ) {
				if( i == 1) {
					LOG(("Click within %s at %d / %d\n", gw->browser->bw->name, sx, sy ));
					browser_window_mouse_click(gw->browser->bw,BROWSER_MOUSE_PRESS_1,sx,sy);
					browser_window_mouse_click(gw->browser->bw,BROWSER_MOUSE_CLICK_1,sx,sy);
					bmstate &= ~( BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_CLICK_1);
				}
					
				if( i == 2 ) {
					LOG(("Click within %s at %d / %d", gw->browser->bw->name, mx, my ));
					browser_window_mouse_click(gw->browser->bw,BROWSER_MOUSE_PRESS_1,sx,sy);
					browser_window_mouse_click(gw->browser->bw,BROWSER_MOUSE_CLICK_2,sx,sy);
					bmstate &= ~( BROWSER_MOUSE_HOLDING_2 | BROWSER_MOUSE_DRAG_2 | BROWSER_MOUSE_CLICK_2);
				}
			}
			mouse_hold_start[i-1] = 0;
		}
	}
}

void browser_scroll( struct gui_window * gw, short mode, int value, bool abs )
{
	LGRECT work;
	int max_y_scroll;
	int max_x_scroll;
	int oldx = gw->browser->scroll.current.x;
	int oldy = gw->browser->scroll.current.y;
	struct s_browser * b = gw->browser;
	LOG((""));

	if( b->bw->current_content != NULL ) {
		browser_get_rect( gw, BR_CONTENT, &work);
		max_y_scroll = (content_get_height( b->bw->current_content ) - work.g_h );
		max_x_scroll = (content_get_width( b->bw->current_content ) - work.g_w);
	} else {
		return;
	}


	switch( mode ) {

		case WA_UPPAGE:
		case WA_UPLINE: 
			if( max_y_scroll < 1 )
				return;
			if( abs == false )
				b->scroll.requested.y -= value;
			else 
				b->scroll.requested.y = value - b->scroll.current.y;
		break;

		case WA_DNPAGE:
		case WA_DNLINE:
			if( max_y_scroll < 1 )
				return;
			if( abs == false )
				b->scroll.requested.y += value;
			else
				b->scroll.requested.y = value - b->scroll.current.y;
		break;

		case WA_LFPAGE:
		case WA_LFLINE: 
			if( max_x_scroll < 1 )
				return;
			if( abs == false )
				b->scroll.requested.x -= value;
			else
				b->scroll.requested.x = value - b->scroll.current.x;
		break;

		case WA_RTPAGE:
		case WA_RTLINE:
			if( max_x_scroll < 1 )
				return;
			if( abs == false )
				b->scroll.requested.x += value;
			else
				b->scroll.requested.x = value - b->scroll.current.x;
		break;

		default: break;				
	}

	if( b->scroll.current.y + b->scroll.requested.y < 0 ) {
			b->scroll.requested.y = -b->scroll.current.y;
	}

	if( b->scroll.current.y + b->scroll.requested.y > max_y_scroll  ) {
			b->scroll.requested.y = max_y_scroll - b->scroll.current.y;
	}

	if( b->scroll.current.x + b->scroll.requested.x < 0 ) {
			b->scroll.requested.x = -b->scroll.current.x;
	}

	if( b->scroll.current.x + b->scroll.requested.x > max_x_scroll ) {
			b->scroll.requested.x = max_x_scroll - b->scroll.current.x;
	}

	if( oldy != b->scroll.current.y + b->scroll.requested.y ||
		oldx != b->scroll.current.x + b->scroll.requested.x ) {
		b->scroll.required = true;
	}
}

/*
	perform the requested scrolling.
	gw -> the browser window to act upon.
	bwrect -> the dimensions of the browser, so that this function
			  doesn't need to get it. 
*/
static void browser_process_scroll( struct gui_window * gw, LGRECT bwrect )
{
	struct s_browser * b = gw->browser;
	GRECT src;
	GRECT dst;
	short h,w;
	
	if( gw->browser->bw->current_content == NULL )
		return;

	h = (short) abs( b->scroll.requested.y );
	w = (short) abs( b->scroll.requested.x ); 

	/* if the request exceeds the browser size, redraw the whole area */
	if ( b->scroll.requested.y > bwrect.g_h || b->scroll.requested.y < -bwrect.g_h ||
	    b->scroll.requested.x > bwrect.g_w || b->scroll.requested.x < -bwrect.g_w ) {
		b->scroll.current.y += b->scroll.requested.y;
		b->scroll.current.x += b->scroll.requested.x;
		browser_schedule_redraw( gw, 0, 0, bwrect.g_w, bwrect.g_h);
		/* don't scroll again: */
		b->scroll.requested.y = 0;
		b->scroll.requested.x = 0;
	}
	if( b->scroll.requested.y < 0 ) {
		/* scroll up */
		src.g_x = 0;
		src.g_y = 0;
		src.g_w = bwrect.g_w;
		src.g_h = bwrect.g_h - h;
		dst.g_x = 0;
		dst.g_y = h;	
		dst.g_w = src.g_w;
		dst.g_h = src.g_h;
		plotter->copy_rect( plotter, src, dst );
		b->scroll.current.y += b->scroll.requested.y;
		browser_schedule_redraw( gw, 0, 0, bwrect.g_w, h ) ; 
	}

	if( b->scroll.requested.y > 0 ) {
		/* scroll down */ 
		src.g_x = 0;
		src.g_y = h;
		src.g_w = bwrect.g_w;
		src.g_h = bwrect.g_h - h;
		dst.g_x = 0;
		dst.g_y = 0;
		dst.g_w = bwrect.g_w;
		dst.g_h = bwrect.g_h - h;
		plotter->copy_rect( plotter, src, dst );
		b->scroll.current.y += b->scroll.requested.y;
		browser_schedule_redraw( gw, 0, bwrect.g_h - h, bwrect.g_w, bwrect.g_h );
	} 

	if( b->scroll.requested.x < 0 ) {
		/* scroll to the left */
		src.g_x = 0;
		src.g_y = 0;
		src.g_w = bwrect.g_w - w;
		src.g_h = bwrect.g_h;
		dst.g_x = w;
		dst.g_y = 0;
		dst.g_w = bwrect.g_w - w;
		dst.g_h = bwrect.g_h;
		plotter->copy_rect( plotter, src, dst );
		b->scroll.current.x += b->scroll.requested.x;
		browser_schedule_redraw( gw, 0, 0, w, bwrect.g_h );
	}

	if( b->scroll.requested.x > 0 ) {
		/* scroll to the right */
		src.g_x = w;
		src.g_y = 0;
		src.g_w = bwrect.g_w - w;
		src.g_h = bwrect.g_h;
		dst.g_x = 0;
		dst.g_y = 0;
		dst.g_w = bwrect.g_w - w;
		dst.g_h = bwrect.g_h;
		plotter->copy_rect( plotter, src, dst );
		b->scroll.current.x += b->scroll.requested.x;
		browser_schedule_redraw( gw, bwrect.g_w - w, 0, bwrect.g_w, bwrect.g_h );
	} 
	b->scroll.requested.y = 0;
	b->scroll.requested.x = 0;
	gw->browser->compwin->xpos = b->scroll.current.x;
	gw->browser->compwin->ypos = b->scroll.current.y;

	mt_WindSlider( &app, gw->browser->compwin, HSLIDER|VSLIDER);
}


bool browser_input( struct gui_window * gw, unsigned short nkc ) 
{
	LGRECT work;
	bool r = false;
	unsigned char ascii = (nkc & 0xFF);
	nkc = (nkc & (NKF_CTRL|NKF_SHIFT|0xFF));
	browser_get_rect(gw, BR_CONTENT, &work);

	if( (nkc & NKF_CTRL) != 0  ) {
		switch ( ascii ) {
			case 'A':
				r = browser_window_key_press(gw->browser->bw, KEY_SELECT_ALL);
			break;
	
			case 'C':
				r = browser_window_key_press(gw->browser->bw, KEY_COPY_SELECTION);
			break;

			case 'X':
				r = browser_window_key_press(gw->browser->bw, KEY_CUT_SELECTION);
			break;
	
			case 'V':
				r = browser_window_key_press(gw->browser->bw, KEY_PASTE);
			break;

			default:
			break;		
		}		
	}
	if( (nkc & NKF_SHIFT) != 0 ) {
		switch( ascii ) {

			case NK_TAB:
				r = browser_window_key_press(gw->browser->bw, KEY_SHIFT_TAB);			
			break;

			case NK_LEFT:
				if( browser_window_key_press(gw->browser->bw, KEY_LINE_START) == false) {
					browser_scroll( gw, WA_LFPAGE, work.g_w, false );
					r = true;
				}
			break;

			case NK_RIGHT:
				if( browser_window_key_press(gw->browser->bw, KEY_LINE_END) == false) {
					browser_scroll( gw, WA_RTPAGE, work.g_w, false );
					r = true;
				}
			break;

			case NK_UP:
				if ( browser_window_key_press(gw->browser->bw, KEY_PAGE_UP) ==false ){
					browser_scroll( gw, WA_UPPAGE, work.g_h, false );
					r = true;
				}
			break;

			case NK_DOWN:
				if (browser_window_key_press(gw->browser->bw, KEY_PAGE_DOWN) == false) {
					browser_scroll( gw, WA_DNPAGE, work.g_h, false );
					r = true;
				}
			break;

			default:
			break;
		}
	}
	if( (nkc & (NKF_SHIFT|NKF_CTRL) ) == 0 ) {
		switch( ascii ) {
			case NK_BS:
				r = browser_window_key_press(gw->browser->bw, KEY_DELETE_LEFT);
			break;

			case NK_DEL:
				r = browser_window_key_press(gw->browser->bw, KEY_DELETE_RIGHT);
			break;

			case NK_TAB:
				r = browser_window_key_press(gw->browser->bw, KEY_TAB);
			break;


			case NK_ENTER:
				r = browser_window_key_press(gw->browser->bw, KEY_NL);
			break;

			case NK_RET:
				r = browser_window_key_press(gw->browser->bw, KEY_CR);
			break;

			case NK_ESC:
				r = browser_window_key_press(gw->browser->bw, KEY_ESCAPE);		
			break;

			case NK_CLRHOME:
				r = browser_window_key_press(gw->browser->bw, KEY_TEXT_START);
			break;

			case NK_RIGHT:
				if (browser_window_key_press(gw->browser->bw, KEY_RIGHT) == false){
					browser_scroll( gw, WA_RTLINE, 16, false );
					r = true;
				}
			break;	

			case NK_LEFT:
				if (browser_window_key_press(gw->browser->bw, KEY_LEFT) == false) {
					browser_scroll( gw, WA_LFLINE, 16, false );
					r = true;
				}
			break;

			case NK_UP:
				if (browser_window_key_press(gw->browser->bw, KEY_UP) == false) {
					browser_scroll( gw, WA_UPLINE, 16, false);
					r = true;
				}
			break;

			case NK_DOWN:
				if (browser_window_key_press(gw->browser->bw, KEY_DOWN) == false) {
					browser_scroll( gw, WA_DNLINE, 16, false);
					r = true;
				}
			break;

			case NK_M_PGUP:
				if ( browser_window_key_press(gw->browser->bw, KEY_PAGE_UP) ==false ) {
					browser_scroll( gw, WA_UPPAGE, work.g_h, false );
					r = true;
				}
			break;

			case NK_M_PGDOWN: 
				if (browser_window_key_press(gw->browser->bw, KEY_PAGE_DOWN) == false) {
					browser_scroll( gw, WA_DNPAGE, work.g_h, false );
					r = true;
				}
			break;

			default:
			break;
		}
	}

	if( r == false && ( (nkc & NKF_CTRL)==0)  ) {
		if (ascii >= 9 ) {
			int ucs4 = atari_to_ucs4(ascii);
            r = browser_window_key_press(gw->browser->bw, ucs4 );
		}
	}
	return( r );	
}

static void __CDECL browser_evnt_redraw_x( WINDOW * c, short buf[8], void * data)
{	
	/* just an stub to prevent wndclear */ 
	/* Probably the browser redraw is better placed here? dunno! */ 
	return;
}

/* determines if a browser window needs redraw */
bool browser_redraw_required( struct gui_window * gw) 
{
	bool ret = true;
	int frames = 0;
	CMP_BROWSER b = gw->browser;

	if( b->bw->current_content == NULL )
		return ( false );

	{	
		/* don't do redraws if we have subframes */
		/* iframes will be an special case and must be handled special... */
		struct gui_window * g;
		for( g=window_list; g; g=g->next ) {
			if ( g != gw && g->parent == gw ) {
				if( g->browser->type == BT_FRAME ) {
					frames++;
				}
			}
		}
	}
	ret = ( ((b->redraw.areas_used > 0) && frames == 0) 
			|| b->scroll.required 
			|| b->caret.redraw );
	return( ret );
}


/* schedule a redraw of content */
/* coords are relative to the framebuffer */
void browser_schedule_redraw_rect(struct gui_window * gw, short x, short y, short w, short h)
{
	if( x < 0  ){
		w += x;
		x = 0;
	}

	if( y < 0 ) {
		h += y;
		y = 0;	
	}
	browser_schedule_redraw( gw, x, y, x+w, y+h );
}

static inline bool bbox_intersect(BBOX * box1, BBOX * box2)
{
    if (box2->x1 < box1->x0)
        return false;

    if (box2->y1 < box1->y0)
        return false;

    if (box2->x0 > box1->x1)
        return false;

    if (box2->y0 > box1->y1)
        return false;

    return true;
}

/* 
	schedule a redraw of content, coords are relative to the framebuffer
*/
void browser_schedule_redraw(struct gui_window * gw, short x0, short y0, short x1, short y1)
{
	assert( gw != NULL );
	CMP_BROWSER b = gw->browser;
	int i;
	LGRECT work;

	if( y1 < 0 || x1 < 0 )
		return;

	browser_get_rect( gw, BR_CONTENT, &work);
	if( x0 > work.g_w )
		return;
	if( y0 > work.g_h )
		return;

	for( i=0; i<b->redraw.areas_used; i++) {
		if(    b->redraw.areas[i].x0 <= x0 
			&& b->redraw.areas[i].x1 >= x1
			&& b->redraw.areas[i].y0 <= y0 
			&& b->redraw.areas[i].y1 >= y1 ){
			/* the area is already queued for redraw */
			return;
		} else {
			BBOX area;
			area.x0 = x0;
			area.y0 = y0;
			area.x1 = x1;
			area.y1 = y1;
			if( bbox_intersect(&b->redraw.areas[i], &area ) ){
				b->redraw.areas[i].x0 = MIN(b->redraw.areas[i].x0, x0);
				b->redraw.areas[i].y0 = MIN(b->redraw.areas[i].y0, y0);
				b->redraw.areas[i].x1 = MAX(b->redraw.areas[i].x1, x1);
				b->redraw.areas[i].y1 = MAX(b->redraw.areas[i].y1, y1);
				return;
			}
		}
	} 

	if( b->redraw.areas_used < MAX_REDRW_SLOTS ) {
		b->redraw.areas[b->redraw.areas_used].x0 = x0;
		b->redraw.areas[b->redraw.areas_used].x1 = x1;
		b->redraw.areas[b->redraw.areas_used].y0 = y0;
		b->redraw.areas[b->redraw.areas_used].y1 = y1;
		b->redraw.areas_used++;
	} else {
		/* 	
			we are out of available slots, merge box with last slot
			this is dumb... but also a very rare case.
		*/
		b->redraw.areas[MAX_REDRW_SLOTS-1].x0 = MIN(b->redraw.areas[i].x0, x0);
		b->redraw.areas[MAX_REDRW_SLOTS-1].y0 = MIN(b->redraw.areas[i].y0, y0);
		b->redraw.areas[MAX_REDRW_SLOTS-1].x1 = MAX(b->redraw.areas[i].x1, x1);
		b->redraw.areas[MAX_REDRW_SLOTS-1].y1 = MAX(b->redraw.areas[i].y1, y1);
	}
done: 
	return;
}

static void browser_redraw_content( struct gui_window * gw, int xoff, int yoff )
{
	LGRECT work;
	CMP_BROWSER b = gw->browser;
	struct rect clip;

	struct redraw_context ctx = {
		.interactive = true,
		.plot = &atari_plotters
	};

	LOG(("%s : %d,%d - %d,%d\n", b->bw->name, b->redraw.area.x0, 
		b->redraw.area.y0, b->redraw.area.x1, b->redraw.area.y1
	));

	current_redraw_browser = b->bw;
   struct rect a;
   a.x0 = b->redraw.area.x0;
   a.y0 = b->redraw.area.y0;
   a.x1 = b->redraw.area.x1;
   a.y1 = b->redraw.area.y1;
   
	browser_window_redraw( b->bw, -b->scroll.current.x,
			-b->scroll.current.y, &a, &ctx );

	current_redraw_browser = NULL;
}


void browser_redraw_caret( struct gui_window * gw, GRECT * area )
{
	GRECT caret;
	struct s_browser * b = gw->browser;
	if( b->caret.redraw == true ){
		struct rect old_clip;
		struct rect clip;

		caret = b->caret.requested;
		caret.g_x -= gw->browser->scroll.current.x;
		caret.g_y -= gw->browser->scroll.current.y;
		clip.x0 = caret.g_x - 1;
		clip.y0 = caret.g_y - 1;
		clip.x1 = caret.g_x + caret.g_w + 1;
		clip.y1 = caret.g_y + caret.g_h + 1;
		/* store old clip before adjusting it: */
		plot_get_clip( &old_clip );
		/* clip to cursor: */
		plot_clip( &clip );
		plot_rectangle( caret.g_x, caret.g_y, 
			caret.g_x+caret.g_w, caret.g_y+caret.g_h,
			plot_style_caret );
		/* restore old clip area: */
		plot_clip( &old_clip );
		b->caret.current.g_x = caret.g_x + gw->browser->scroll.current.x;
		b->caret.current.g_y = caret.g_y + gw->browser->scroll.current.y;
		b->caret.current.g_w = caret.g_w;
		b->caret.current.g_h = caret.g_h;		
	}
}

void browser_redraw( struct gui_window * gw ) 
{	
	LGRECT bwrect;
	struct s_browser * b = gw->browser;
	short todo[4];
	struct rect clip;

	if( b->attached == false || b->bw->current_content == NULL ) {
		return;
	}

	browser_get_rect(gw, BR_CONTENT, &bwrect);

	plotter->resize(plotter, bwrect.g_w, bwrect.g_h);
	plotter->move(plotter, bwrect.g_x, bwrect.g_y );
	clip.x0 = 0;
	clip.y0 = 0;
	clip.x1 = bwrect.g_w;
	clip.y1 = bwrect.g_h;
	plotter->clip( plotter, &clip );
	plotter->lock(plotter);

	if( b->scroll.required == true && b->bw->current_content != NULL) {
		browser_process_scroll( gw, bwrect );
		b->scroll.required = false;
	}

	if ((b->redraw.areas_used > 0) && b->bw->current_content != NULL ) {
		if( (plotter->flags & PLOT_FLAG_OFFSCREEN) == 0 ) {
			int i;
			GRECT area;
			GRECT fbwork;
			BBOX cliporg;
			todo[0] = bwrect.g_x;
			todo[1] = bwrect.g_y;
			todo[2] = todo[0] + bwrect.g_w-1;
			todo[3] = todo[1] + bwrect.g_h-1;
			vs_clip(plotter->vdi_handle, 1, (short*)&todo[0]);
			if( wind_get(gw->root->handle->handle, WF_FIRSTXYWH, 
							&todo[0], &todo[1], &todo[2], &todo[3] )!=0 ) {
				while (todo[2] && todo[3]) {
					/* convert screen to framebuffer coords: */
					fbwork.g_x = todo[0] - bwrect.g_x;					
					fbwork.g_y = todo[1] - bwrect.g_y;
					if( fbwork.g_x < 0 ){
						fbwork.g_w = todo[2] + todo[0];
						fbwork.g_x = 0;
					} else {
						fbwork.g_w = todo[2];
					}
					if( fbwork.g_y < 0 ){
						fbwork.g_h = todo[3] + todo[1];
						fbwork.g_y = 0;
					} else {
						fbwork.g_h = todo[3];
					}
					/* walk the redraw requests: */
					for( i=0; i<b->redraw.areas_used; i++ ){
						area.g_x = b->redraw.areas[i].x0;
						area.g_y = b->redraw.areas[i].y0;
						area.g_w = b->redraw.areas[i].x1 - b->redraw.areas[i].x0;
						area.g_h = b->redraw.areas[i].y1 - b->redraw.areas[i].y0; 
						if (rc_intersect((GRECT *)&fbwork,(GRECT *)&area)) {
							
							b->redraw.area.x0 = area.g_x; //todo[0];
							b->redraw.area.y0 = area.g_y; //todo[1];
							b->redraw.area.x1 = area.g_x + area.g_w; //todo[0] + todo[2];
							b->redraw.area.y1 = area.g_y + area.g_h; //todo[1] + todo[3];
							browser_redraw_content( gw, 0, 0 );
						} else {
							/* 
								the area should be kept scheduled for later redraw, but because this
								is onscreen plotter, it doesn't make much sense anyway... 
							*/ 
						}

					}
					if (wind_get(gw->root->handle->handle, WF_NEXTXYWH, 
							&todo[0], &todo[1], &todo[2], &todo[3])==0) {
						break;
					}
				}
			}
			vs_clip(plotter->vdi_handle, 0, (short*)&todo);
		} else {
			/* its save to do a complete redraw :) */
		}
		b->redraw.areas_used = 0;
	}
	if( b->caret.redraw == true && 	b->bw->current_content != NULL ) {
		GRECT area;
		todo[0] = bwrect.g_x;
		todo[1] = bwrect.g_y;
		todo[2] = todo[0] + bwrect.g_w;
		todo[3] = todo[1] + bwrect.g_h;
		area.g_x = bwrect.g_x;
		area.g_y = bwrect.g_y;
		area.g_w = bwrect.g_w;
		area.g_h = bwrect.g_h;
		vs_clip(plotter->vdi_handle, 1, (short*)&todo[0]);
		browser_redraw_caret( gw, &area );
		vs_clip(plotter->vdi_handle, 0, (short*)&todo[0]);
		b->caret.redraw = false;
	}
	plotter->unlock(plotter);
	/* TODO: if we use offscreen bitmap, trigger content redraw here */
}

static void __CDECL browser_evnt_redraw( COMPONENT * c, long buff[8], void * data)
{	
	short pxy[8];
	WINDOW * w;
	struct gui_window * gw = (struct gui_window *) data;
	CMP_BROWSER b = gw->browser;
	struct gui_window * rgw = browser_find_root( gw );
	LGRECT work, lclip, rwork;
	int xoff,yoff,width,heigth;
	short cw, ch, cellw, cellh;
	/* use that instead of browser_find_root() ? */
	w = (WINDOW*)mt_CompGetPtr( &app, c, CF_WINDOW );
	browser_get_rect( gw, BR_CONTENT, &work );
	browser_get_rect( rgw, BR_CONTENT, &rwork );
	lclip = work;
	if ( !rc_lintersect( (LGRECT*)&buff[4], &lclip ) ) return;
	 
	if( b->bw->current_content == NULL )
		return;
	/* convert redraw coords to framebuffer coords: */
	lclip.g_x -= work.g_x;
	lclip.g_y -= work.g_y;
	if( lclip.g_x < 0 ) {
		lclip.g_w = work.g_w + lclip.g_x;
		lclip.g_x = 0;
	}
	if( lclip.g_y < 0 ) {
		lclip.g_h = work.g_h + lclip.g_y;
		lclip.g_y = 0;
	}
	if( lclip.g_h > 0 && lclip.g_w > 0 ) { 
		browser_schedule_redraw( gw, lclip.g_x, lclip.g_y, 
			lclip.g_x + lclip.g_w, lclip.g_y + lclip.g_h 
		);	
	}

	return;
}

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
 *
 * Module Description:
 *
 * This WinDom compo
 *
 *
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
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "css/css.h"
#include "render/box.h"
#include "render/form.h"
#include "utils/log.h"
#include "utils/messages.h"

#include "atari/gui.h"
#include "atari/browser_win.h"
#include "atari/misc.h"
#include "atari/global_evnt.h"
#include "atari/res/netsurf.rsh"
#include "atari/redrawslots.h"
#include "atari/browser.h"
#include "atari/plot/plotter.h"
#include "atari/plot.h"
#include "atari/encoding.h"
#include "atari/ctxmenu.h"
#include "cflib.h"

extern GEM_PLOTTER plotter;
extern struct gui_window *input_window;

static void browser_process_scroll( struct gui_window * gw, LGRECT bwrect );
static void browser_redraw_content( struct gui_window * gw, int xoff, int yoff,
								struct rect * area );
static void __CDECL browser_evnt_destroy( COMPONENT * c, long buff[8],
										void * data);
static void __CDECL browser_evnt_redraw( COMPONENT * c, long buff[8],
										void * data);
static void __CDECL browser_evnt_mbutton( COMPONENT * c, long buff[8],
										void * data);


/*
	Create an browser component.
	Currently, this area is the area which is used to display HTML content.
	However, it could also contains other areas, these need to be handled within
	"browser_get_rect" function.
*/
struct s_browser * browser_create
(
	struct gui_window * gw,
	struct browser_window *bw,
	struct browser_window * clone,
	int lt, int w, int flex
)
{
	CMP_BROWSER bnew = (CMP_BROWSER)malloc( sizeof(struct s_browser) );
	if( bnew )
	{
		memset(bnew, 0, sizeof(struct s_browser) );
		bnew->bw = bw;
		bnew->attached = false;
		if(clone)
			bw->scale = clone->scale;
		else
			bw->scale = 1;
		redraw_slots_init( &bnew->redraw, MAX_REDRW_SLOTS );
		bnew->comp = (COMPONENT*)mt_CompCreate(&app, CLT_HORIZONTAL, 100, 1);
		if( bnew->comp == NULL ) {
			free(bnew);
			return(NULL);
		}

		/* Attach events to the component: */
		mt_CompEvntDataAdd( &app, bnew->comp, WM_XBUTTON,
						browser_evnt_mbutton, (void*)gw, EV_BOT
		);
		mt_CompEvntDataAttach( &app, bnew->comp, WM_REDRAW,
								browser_evnt_redraw, (void*)gw
		);
		mt_CompEvntDataAttach( &app, bnew->comp, WM_DESTROY,
								browser_evnt_destroy, (void*)bnew
		);

		/* Set the gui_window owner. */
		/* it is an link to the netsurf window system */
		mt_CompDataAttach( &app, bnew->comp, CDT_OWNER, gw );

		bnew->scroll.requested.y = 0;
		bnew->scroll.requested.x = 0;
		bnew->scroll.current.x = 0;
		bnew->scroll.current.y = 0;
		bnew->reformat_pending = false;

	}
	return( bnew );
}

bool browser_destroy( struct s_browser * b )
{

	LOG(("%s\n", b->bw->name ));

	assert( b != NULL );
	assert( b->comp != NULL );
	assert( b->bw != NULL );

	if( b->comp != NULL ){
		mt_CompDelete(&app,  b->comp );
	}
	return( true );
}

/*
	Query the browser component for widget rectangles.
*/
void browser_get_rect( struct gui_window * gw, enum browser_rect type, LGRECT * out)
{
	LGRECT cur;

	/* Query component for it's current size: */
	mt_CompGetLGrect(&app, gw->browser->comp, WF_WORKXYWH, &cur);

	/* And extract the different widget dimensions: */
	if( type == BR_CONTENT ){
		out->g_w = cur.g_w;
		out->g_h = cur.g_h;
		out->g_x = cur.g_x;
		out->g_y = cur.g_y;
	}

	return;
}

/* Report an resize to the COMPONENT interface */
void browser_update_rects(struct gui_window * gw )
{
	short buff[8];
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

	gw->root->handle->xpos_max = w;
	gw->root->handle->ypos_max = h;

	if( w < work.g_w + b->scroll.current.x || w < work.g_h + b->scroll.current.y ) {
		/* let the scroll routine detect invalid scroll values... */
		browser_scroll(gw, WA_LFLINE, b->scroll.current.x, true );
		browser_scroll(gw, WA_UPLINE, b->scroll.current.y, true );
		/* force update of scrollbars: */
		b->scroll.required = true;
	}
}


static void __CDECL browser_evnt_destroy( COMPONENT * c, long buff[8], void * data)
{
	struct s_browser * b = (struct s_browser*)data;
	struct gui_window * gw = b->bw->window;
	LOG(("%s\n",gw->browser->bw->name));

	assert( b != NULL );
	assert( gw != NULL );
	free( b );
	gw->browser = NULL;
	LOG(("evnt_destroy done!"));
}

/*
	Mouse Button handler for browser component.
*/

static void __CDECL browser_evnt_mbutton( COMPONENT * c, long buff[8], void * data)
{
	short mx, my, dummy, mbut;
	LGRECT cwork;
	browser_mouse_state bmstate = 0;
	struct gui_window * gw = data;

	if( input_window != gw ){
		input_window = gw;
	}

	window_set_focus( gw, BROWSER, (void*)gw->browser );
	browser_get_rect( gw, BR_CONTENT, &cwork );

	/* convert screen coords to component coords: */
	mx = evnt.mx - cwork.g_x;
	my = evnt.my - cwork.g_y;

	/* Translate GEM key state to netsurf mouse modifier */
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

	/* convert component coords to scrolled content coords: */
	int sx_origin = (mx + gw->browser->scroll.current.x);
	int sy_origin = (my + gw->browser->scroll.current.y);

	short rel_cur_x, rel_cur_y;
	short prev_x=sx_origin, prev_y=sy_origin;
	bool dragmode = 0;

	/* Detect left mouse button state and compare with event state: */
	graf_mkstate(&rel_cur_x, &rel_cur_y, &mbut, &dummy);
	if( (mbut & 1) && (evnt.mbut & 1) ){
		/* Mouse still pressed, report drag */
		rel_cur_x = (rel_cur_x - cwork.g_x) + gw->browser->scroll.current.x;
		rel_cur_y = (rel_cur_y - cwork.g_y) + gw->browser->scroll.current.y;
		browser_window_mouse_click( gw->browser->bw,
									BROWSER_MOUSE_DRAG_ON|BROWSER_MOUSE_DRAG_1,
									sx_origin, sy_origin);
		do{
			if( abs(prev_x-rel_cur_x) > 5 || abs(prev_y-rel_cur_y) > 5 ){
				browser_window_mouse_track( gw->browser->bw,
									BROWSER_MOUSE_DRAG_ON|BROWSER_MOUSE_DRAG_1,
									rel_cur_x, rel_cur_y);
				prev_x = rel_cur_x;
				prev_y = rel_cur_y;
				dragmode = true;
			} else {
				if( dragmode == false ){
					browser_window_mouse_track( gw->browser->bw,BROWSER_MOUSE_PRESS_1,
									rel_cur_x, rel_cur_y);
				}
			}
			if( browser_redraw_required( gw ) ){
				browser_redraw( gw );
			}
			graf_mkstate(&rel_cur_x, &rel_cur_y, &mbut, &dummy);
			rel_cur_x = (rel_cur_x - cwork.g_x) + gw->browser->scroll.current.x;
			rel_cur_y = (rel_cur_y - cwork.g_y) + gw->browser->scroll.current.y;
		} while( mbut & 1 );
		browser_window_mouse_track(gw->browser->bw, 0, rel_cur_x,rel_cur_y);
	} else {
		/* Right button pressed? */
		if( (evnt.mbut & 2 ) ) {
			context_popup( gw, evnt.mx, evnt.my );
		} else {
			browser_window_mouse_click(gw->browser->bw,
										bmstate|BROWSER_MOUSE_PRESS_1,
										sx_origin,sy_origin);
			browser_window_mouse_click(gw->browser->bw,
										bmstate|BROWSER_MOUSE_CLICK_1,
										sx_origin,sy_origin);
		}
	}


}


/*
	Report scroll event to the browser component.
*/
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
		browser_schedule_redraw( gw, 0, 0, bwrect.g_w, h );
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
	if( b->caret.requested.g_w > 0 ){
		b->caret.redraw = true;
	}

	gw->root->handle->xpos = b->scroll.current.x;
	gw->root->handle->ypos = b->scroll.current.y;

	mt_WindSlider( &app, gw->root->handle, HSLIDER|VSLIDER );
}

/*
	Report keypress to browser component.
	The browser component doesn't listen for keyinput by itself.
	parameter:
		- gui_window ( compocnent owner ).
		- unsigned short nkc ( CFLIB normalised key code )
*/
bool browser_input( struct gui_window * gw, unsigned short nkc )
{
	LGRECT work;
	bool r = false;
	unsigned char ascii = (nkc & 0xFF);
	long ucs4;
	long ik = nkc_to_input_key( nkc, &ucs4 );

	// pass event to specific control?

	if( ik == 0 ){
		if (ascii >= 9 ) {
            r = browser_window_key_press(gw->browser->bw, ucs4 );
		}
	} else {
		r = browser_window_key_press(gw->browser->bw, ik );
		if( r == false ){
			browser_get_rect(gw, BR_CONTENT, &work);
			switch( ik ){
				case KEY_LINE_START:
					browser_scroll( gw, WA_LFPAGE, work.g_w, false );
				break;

				case KEY_LINE_END:
					browser_scroll( gw, WA_RTPAGE, work.g_w, false );
				break;

				case KEY_PAGE_UP:
					browser_scroll( gw, WA_UPPAGE, work.g_h, false );
				break;

				case KEY_PAGE_DOWN:
					browser_scroll( gw, WA_DNPAGE, work.g_h, false );
				break;

				case KEY_RIGHT:
					browser_scroll( gw, WA_RTLINE, 16, false );
				break;

				case KEY_LEFT:
					browser_scroll( gw, WA_LFLINE, 16, false );
				break;

				case KEY_UP:
					browser_scroll( gw, WA_UPLINE, 16, false);
				break;

				case KEY_DOWN:
					browser_scroll( gw, WA_DNLINE, 16, false);
				break;

				default:
				break;
			}
		}
	}

	return( r );
}

/* determines if a browser window needs redraw */
bool browser_redraw_required( struct gui_window * gw)
{
	bool ret = true;
	CMP_BROWSER b = gw->browser;

	if( b->bw->current_content == NULL )
		return ( false );

	/* disable redraws when the browser awaits WM_REDRAW caused by resize */
	if( b->reformat_pending )
		return( false );

	ret = ( ((b->redraw.areas_used > 0) )
			|| b->scroll.required
			|| b->caret.redraw);
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


/*
	schedule a redraw of content, coords are relative to the framebuffer
*/
void browser_schedule_redraw(struct gui_window * gw, short x0, short y0, short x1, short y1)
{
	assert( gw != NULL );
	CMP_BROWSER b = gw->browser;
	LGRECT work;

	if( y1 < 0 || x1 < 0 )
		return;

	browser_get_rect( gw, BR_CONTENT, &work);
	if( x0 > work.g_w )
		return;
	if( y0 > work.g_h )
		return;

	redraw_slot_schedule( &b->redraw, x0, y0, x1, y1 );

	return;
}

static void browser_redraw_content( struct gui_window * gw, int xoff, int yoff,
								struct rect * area )
{
	CMP_BROWSER b = gw->browser;

	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &atari_plotters
	};

	LOG(("%s : %d,%d - %d,%d\n", b->bw->name, area->x0,
		area->y0, area->x1, area->y1
	));


	browser_window_redraw( b->bw, -b->scroll.current.x,
			-b->scroll.current.y, area, &ctx );

}

/*
	area: the browser canvas
*/
void browser_restore_caret_background( struct gui_window * gw, LGRECT * area)
{
	CMP_BROWSER b = gw->browser;
	LGRECT rect;
	if( area == NULL ){
		browser_get_rect( gw, BR_CONTENT, &rect );
		area = &rect;
	}
	/* This call restores the background and releases the memory: */
	// TODO: only release memory/clear flag when the caret is not clipped.
	// TODO: apply clipping.
	w_put_bkgr( &app,
			area->g_x-b->scroll.current.x+b->caret.current.g_x,
			area->g_y-b->scroll.current.y+b->caret.current.g_y,
			gw->browser->caret.current.g_w,
			gw->browser->caret.current.g_h,
			&gw->browser->caret.background
	);
	gw->browser->caret.background.fd_addr = NULL;
}

/*
	area: the browser canvas
*/
void browser_redraw_caret( struct gui_window * gw, LGRECT * area )
{
	// TODO: only redraw caret when window is topped.
	if( gw->browser->caret.redraw && gw->browser->caret.requested.g_w > 0 ){
		LGRECT caret;
		struct s_browser * b = gw->browser;
		struct rect old_clip;
		struct rect clip;

		if( b->caret.current.g_w > 0 && b->caret.background.fd_addr != NULL ){
			browser_restore_caret_background( gw, area );
		}

		caret = b->caret.requested;
		caret.g_x -= b->scroll.current.x - area->g_x;
		caret.g_y -= b->scroll.current.y - area->g_y;

		if( !rc_lintersect( area, &caret ) ) {
			return;
		}

		MFDB screen;
		short pxy[8];

		/* save background: */
		//assert( b->caret.background.fd_addr == NULL );
		init_mfdb( app.nplanes, caret.g_w, caret.g_h, 0,
					&b->caret.background );
		init_mfdb( 0, caret.g_w, caret.g_h, 0, &screen );
		pxy[0] = caret.g_x;
		pxy[1] = caret.g_y;
		pxy[2] = caret.g_x + caret.g_w - 1;
		pxy[3] = caret.g_y + caret.g_h - 1;
		pxy[4] = 0;
		pxy[5] = 0;
		pxy[6] = caret.g_w - 1;
		pxy[7] = caret.g_h - 1;
		/* hide the mouse */
		v_hide_c ( app.graf.handle);
		/* copy screen image */
		vro_cpyfm ( app.graf.handle, S_ONLY, pxy, &screen, &b->caret.background);
		/* restore the mouse */
		v_show_c ( app.graf.handle, 1);
		/* draw caret: */
		caret.g_x -= area->g_x;
		caret.g_y -= area->g_y;
		clip.x0 = caret.g_x;
		clip.y0 = caret.g_y;
		clip.x1 = caret.g_x + caret.g_w-1;
		clip.y1 = caret.g_y + caret.g_h-1;
		/* store old clip before adjusting it: */
		plot_get_clip( &old_clip );
		/* clip to cursor: */
		plot_clip( &clip );
		plot_line( caret.g_x, caret.g_y, caret.g_x, caret.g_y + caret.g_h,
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
	/* used for clipping of content redraw: */
	struct rect redraw_area;

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
	if( plotter->lock(plotter) == 0 )
		return;

	if( b->scroll.required == true && b->bw->current_content != NULL) {
		browser_process_scroll( gw, bwrect );
		b->scroll.required = false;
	}

	if ((b->redraw.areas_used > 0) && b->bw->current_content != NULL ) {
		if( (plotter->flags & PLOT_FLAG_OFFSCREEN) == 0 ) {
			int i;
			GRECT area;
			GRECT fbwork;
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
							redraw_area.x0 = area.g_x;
							redraw_area.y0 = area.g_y;
							redraw_area.x1 = area.g_x + area.g_w;
							redraw_area.y1 = area.g_y + area.g_h;
							browser_redraw_content( gw, 0, 0, &redraw_area );
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
		LGRECT area;
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
	struct gui_window * gw = (struct gui_window *) data;
	CMP_BROWSER b = gw->browser;
	LGRECT work, lclip;

	browser_get_rect( gw, BR_CONTENT, &work );
	lclip = work;
	if ( !rc_lintersect( (LGRECT*)&buff[4], &lclip ) ) return;

	if( b->bw->current_content == NULL ){
		short pxy[4];
		pxy[0] = lclip.g_x;
		pxy[1] = lclip.g_y;
		pxy[2] = lclip.g_x + lclip.g_w - 1;
		pxy[3] = lclip.g_y + lclip.g_h - 1;
		vsf_color( gw->root->handle->graf->handle, WHITE );
		vsf_perimeter( gw->root->handle->graf->handle, 0);
		vsf_interior( gw->root->handle->graf->handle, FIS_SOLID );
		vsf_style( gw->root->handle->graf->handle, 1);
		v_bar( gw->root->handle->graf->handle, (short*)&pxy );
		return;
	}

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

		if( gw->browser->reformat_pending == true ){
			LGRECT newsize;
			gw->browser->reformat_pending = false;
			browser_get_rect(gw, BR_CONTENT, &newsize);
			/* this call will also schedule an redraw for the complete */
			/* area. */
			/* Resize must be handled here, because otherwise */
			/* a redraw is scheduled twice (1. by the frontend, 2. by AES) */
			browser_window_reformat(b->bw, false, newsize.g_w, newsize.g_h );
		} else {
			browser_schedule_redraw( gw, lclip.g_x, lclip.g_y,
				lclip.g_x + lclip.g_w, lclip.g_y + lclip.g_h
			);
		}
	}

	return;
}

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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <windom.h>
#include <assert.h>
#include <math.h>
#include <osbind.h>

#include "utils/log.h"
#include "desktop/gui.h"
#include "desktop/history_core.h"
#include "desktop/netsurf.h"
#include "desktop/browser.h"
#include "desktop/mouse.h"
#include "desktop/plotters.h"
#include "desktop/textinput.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "css/css.h"
#include "render/box.h"
#include "render/form.h"
#include "atari/gui.h"
#include "atari/browser_win.h"
#include "atari/browser.h"
#include "atari/misc.h"
#include "atari/global_evnt.h"
#include "atari/res/netsurf.rsh"
#include "atari/browser.h"
#include "atari/toolbar.h"
#include "atari/statusbar.h"
#include "atari/plot/plotter.h"
#include "atari/dragdrop.h"
#include "atari/search.h"
#include "atari/osspec.h"
#include "atari/encoding.h"
#include "atari/toolbar.h"

extern void * h_gem_rsrc;
extern struct gui_window *input_window;
extern GEM_PLOTTER plotter;

void __CDECL std_szd( WINDOW * win, short buff[8], void * );
void __CDECL std_mvd( WINDOW * win, short buff[8], void * );


/* -------------------------------------------------------------------------- */
/* Static module methods follow here:                                         */
/* -------------------------------------------------------------------------- */
static void __CDECL evnt_window_icondraw( WINDOW *win, short buff[8], void *data );
static void __CDECL evnt_window_newtop( WINDOW *win, short buff[8], void *data );
void __CDECL evnt_window_resize( WINDOW *win, short buff[8], void * data );
static void __CDECL evnt_window_move( WINDOW *win, short buff[8], void * data );
static void __CDECL evnt_window_rt_resize( WINDOW *win, short buff[8], void * date );
static void __CDECL evnt_window_close( WINDOW *win, short buff[8], void *data );
static void __CDECL evnt_window_dd( WINDOW *win, short wbuff[8], void * data ) ;
static void __CDECL evnt_window_destroy( WINDOW *win, short buff[8], void *data );
static void __CDECL evnt_window_slider( WINDOW * win, short buff[8], void * data);
static void __CDECL evnt_window_arrowed( WINDOW *win, short buff[8], void *data );
static void __CDECL evnt_window_uniconify( WINDOW *win, short buff[8], void * data );
static void __CDECL evnt_window_iconify( WINDOW *win, short buff[8], void * data );

/* -------------------------------------------------------------------------- */
/* Module public functions:                                                   */
/* -------------------------------------------------------------------------- */


int window_create( struct gui_window * gw,
				struct browser_window * bw,
				unsigned long inflags )
{
	int err = 0;
	bool tb, sb;
	int flags;

	tb = (inflags & WIDGET_TOOLBAR );
	sb = (inflags & WIDGET_STATUSBAR );

	flags = CLOSER | MOVER | NAME | FULLER | SMALLER;
	if( inflags & WIDGET_SCROLL ){
		flags |= (UPARROW | DNARROW | LFARROW | RTARROW | VSLIDE | HSLIDE);
	}
	if( inflags & WIDGET_RESIZE ){
		flags |= ( SIZER );
	}

	gw->root = malloc( sizeof(struct s_gui_win_root) );
	if( gw->root == NULL )
		return( -1 );
	memset( gw->root, 0, sizeof(struct s_gui_win_root) );
	gw->root->title = malloc(atari_sysinfo.aes_max_win_title_len+1);
	gw->root->handle = WindCreate( flags,40, 40, app.w, app.h );
	if( gw->root->handle == NULL ) {
		free( gw->root->title );
		free( gw->root );
		return( -1 );
	}

	/* set scroll / content granularity ( 1 unit ) */
	gw->root->handle->w_u = 1;
	gw->root->handle->h_u = 1;

	/* Create Root component: */
	gw->root->cmproot = mt_CompCreate(&app, CLT_VERTICAL, 1, 1);
	WindSetPtr( gw->root->handle, WF_COMPONENT, gw->root->cmproot, NULL);

	/* create toolbar component: */
	if( tb ) {
		gw->root->toolbar = tb_create( gw );
		assert( gw->root->toolbar );
		mt_CompAttach( &app, gw->root->cmproot, gw->root->toolbar->comp );

	} else {
		gw->root->toolbar = NULL;
	}

	/* create browser component: */
	gw->browser = browser_create( gw, bw, NULL, CLT_HORIZONTAL, 1, 1 );
	mt_CompAttach( &app, gw->root->cmproot,  gw->browser->comp );

	/* create statusbar component: */
	if( sb ) {
		gw->root->statusbar = sb_create( gw );
		mt_CompAttach( &app, gw->root->cmproot, gw->root->statusbar->comp );
	} else {
		gw->root->statusbar = NULL;
	}

	WindSetStr(gw->root->handle, WF_ICONTITLE, (char*)"NetSurf");

	/* Event Handlers: */
	EvntDataAttach( gw->root->handle, WM_CLOSED, evnt_window_close, gw );
	/* capture resize/move events so we can handle that manually */
	EvntDataAdd( gw->root->handle, WM_SIZED, evnt_window_rt_resize, gw, EV_BOT );
	EvntDataAdd( gw->root->handle, WM_MOVED, evnt_window_rt_resize, gw, EV_BOT );
	EvntDataAdd( gw->root->handle, WM_FULLED, evnt_window_rt_resize, gw, EV_BOT );
	EvntDataAdd( gw->root->handle, WM_DESTROY,evnt_window_destroy, gw, EV_TOP );
	EvntDataAdd( gw->root->handle, WM_ARROWED,evnt_window_arrowed, gw, EV_TOP );
	EvntDataAdd( gw->root->handle, WM_NEWTOP, evnt_window_newtop, gw, EV_BOT);
	EvntDataAdd( gw->root->handle, WM_TOPPED, evnt_window_newtop, gw, EV_BOT);
	EvntDataAdd( gw->root->handle, WM_ICONIFY, evnt_window_iconify, gw, EV_BOT);
	EvntDataAdd( gw->root->handle, WM_UNICONIFY, evnt_window_uniconify, gw, EV_BOT);
	EvntDataAttach( gw->root->handle, AP_DRAGDROP, evnt_window_dd, gw );
	EvntDataAttach( gw->root->handle, WM_ICONDRAW, evnt_window_icondraw, gw);
	EvntDataAttach( gw->root->handle, WM_SLIDEXY, evnt_window_slider, gw );

	/* TODO: check if window is openend as "foreground" window... */
	window_set_focus( gw, BROWSER, gw->browser);

	return (err);
}

int window_destroy( struct gui_window * gw)
{
	int err = 0;

	search_destroy( gw );
	if( input_window == gw )
		input_window = NULL;

	if( gw->root ) {
		if( gw->root->toolbar )
			tb_destroy( gw->root->toolbar );

		if( gw->root->statusbar )
			sb_destroy( gw->root->statusbar );
	}

	search_destroy( gw );

	if( gw->browser )
		browser_destroy( gw->browser );

	/* needed? */ /*listRemove( (LINKABLE*)gw->root->cmproot ); */
	if( gw->root ) {
		/* TODO: check if no other browser is bound to this root window! */
		/* only needed for tabs */
		if( gw->root->title )
			free( gw->root->title );
		if( gw->root->cmproot )
			mt_CompDelete( &app, gw->root->cmproot );
		ApplWrite( _AESapid, WM_DESTROY, gw->root->handle->handle, 0, 0, 0, 0);
		EvntWindom( MU_MESAG );
		gw->root->handle = NULL;
		free( gw->root );
		gw->root = NULL;
	}
	return( err );
}



void window_open( struct gui_window * gw, GRECT pos )
{
	LGRECT br;

	WindOpen(gw->root->handle, pos.g_x, pos.g_y, pos.g_w, pos.g_h );
	WindClear( gw->root->handle );
	WindSetStr( gw->root->handle, WF_NAME, (char *)"" );

	/* apply focus to the root frame: */
	long lfbuff[8] = { CM_GETFOCUS };
	mt_CompEvntExec( gl_appvar, gw->browser->comp, lfbuff );

	/* recompute the nested component sizes and positions: */
	browser_update_rects( gw );
	mt_WindGetGrect( &app, gw->root->handle, WF_CURRXYWH, (GRECT*)&gw->root->loc);
	browser_get_rect( gw, BR_CONTENT, &br );
	plotter->move( plotter, br.g_x, br.g_y );
	plotter->resize( plotter, br.g_w, br.g_h );
	gw->browser->attached = true;
	if( gw->root->statusbar != NULL ){
		gw->root->statusbar->attached = true;
	}
	tb_adjust_size( gw );
	/*TBD: get already present content and set size? */
	input_window = gw;
	window_set_focus( gw, BROWSER, gw->browser );
}



/* update back forward buttons (see tb_update_buttons (bug) ) */
void window_update_back_forward( struct gui_window * gw)
{
	tb_update_buttons( gw );
}

void window_set_stauts( struct gui_window * gw , char * text )
{
	if( gw->root == NULL )
		return;

	CMP_STATUSBAR sb = gw->root->statusbar;

	if( sb == NULL || gw->browser->attached == false )
		return;

	sb_set_text( sb, text );
}

/* set focus to an arbitary element */
void window_set_focus( struct gui_window * gw, enum focus_element_type type, void * element )
{
	if( gw->root->focus.type != type || gw->root->focus.element != element ) {
		LOG(("Set focus: %p (%d)\n", element, type));
		gw->root->focus.type = type;
		gw->root->focus.element = element;
		if( element != NULL ){
			switch( type ){

				case URL_WIDGET:
					textarea_keypress(((struct s_url_widget*)(element))->textarea,
										KEY_SELECT_ALL );
					break;

				default: break;

			}
		}
	}
}

/* check if the url widget has focus */
bool window_url_widget_has_focus( struct gui_window * gw )
{
	assert( gw );
	assert( gw->root );
	if( gw->root->focus.type == URL_WIDGET && gw->root->focus.element != NULL ) {
		assert( ( &gw->root->toolbar->url == (struct s_url_widget*)gw->root->focus.element ) );
		assert( GUIWIN_VISIBLE(gw) );
		return true;
	}
	return false;
}

/* check if an arbitary window widget / or frame has the focus */
bool window_widget_has_focus( struct gui_window * gw, enum focus_element_type t, void * element )
{
	if( gw == NULL )
		return( false );
	if( element == NULL  ){
		assert( 1 != 0 );
		return( (gw->root->focus.type == t ) );
	}
	assert( gw->root != NULL );
	return( ( element == gw->root->focus.element && t == gw->root->focus.type) );
}


/* -------------------------------------------------------------------------- */
/* Event Handlers:                                                            */
/* -------------------------------------------------------------------------- */

static void __CDECL evnt_window_arrowed( WINDOW *win, short buff[8], void *data )
{
	bool abs = false;
	LGRECT cwork;
	struct gui_window * gw = data;
	int value = BROWSER_SCROLL_SVAL;

	assert( gw != NULL );

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


static void __CDECL evnt_window_dd( WINDOW *win, short wbuff[8], void * data )
{
	struct gui_window * gw = (struct gui_window *)data;
	char file[DD_NAMEMAX];
	char name[DD_NAMEMAX];
	char *buff=NULL;
	int dd_hdl;
	int dd_msg; /* pipe-handle */
	long size;
	char ext[32];
	short mx,my,bmstat,mkstat;
	graf_mkstate(&mx, &my, &bmstat, &mkstat);

	if( gw == NULL )
		return;
	if( (win->status & WS_ICONIFY))
		return;

	dd_hdl = ddopen( wbuff[7], DD_OK);
	if( dd_hdl<0)
		return;	/* pipe not open */
	memset( ext, 0, 32);
	strcpy( ext, "ARGS");
	dd_msg = ddsexts( dd_hdl, ext);
	if( dd_msg<0)
		goto error;
	dd_msg = ddrtry( dd_hdl, (char*)&name[0], (char*)&file[0], (char*)&ext[0], &size);
	if( size+1 >= PATH_MAX )
		goto error;
	if( !strncmp( ext, "ARGS", 4) && dd_msg > 0)
	{
		ddreply(dd_hdl, DD_OK);
		buff = (char*)alloca(sizeof(char)*(size+1));
		if( buff != NULL )
		{
			if( Fread(dd_hdl, size, buff ) == size)
			{
				buff[size] = 0;
			}
			LOG(("file: %s, ext: %s, size: %d dropped at: %d,%d\n",
				(char*)buff, (char*)&ext,
				size, mx, my
			));
			{
				LGRECT bwrect;
				struct browser_window * bw = gw->browser->bw;
				browser_get_rect( gw, BR_CONTENT, &bwrect );
				mx = mx - bwrect.g_x;
				my = my - bwrect.g_y;
				if( (mx < 0 || mx > bwrect.g_w) || (my < 0 || my > bwrect.g_h) )
					return;

				utf8_convert_ret ret;
				char *utf8_fn;

				ret = utf8_from_local_encoding( buff, 0, &utf8_fn);
				if (ret != UTF8_CONVERT_OK) {
						/* A bad encoding should never happen */
						LOG(("utf8_from_local_encoding failed"));
						assert(ret != UTF8_CONVERT_BADENC);
						/* no memory */
						return;
				}
				browser_window_drop_file_at_point( gw->browser->bw,
											mx+gw->browser->scroll.current.x,
											my+gw->browser->scroll.current.y,
											utf8_fn );
				free( utf8_fn );
			}
		}
	}
error:
	ddclose( dd_hdl);
}

static void __CDECL evnt_window_destroy( WINDOW *win, short buff[8], void *data )
{
	LOG(("%s\n", __FUNCTION__ ));
}

static void __CDECL evnt_window_close( WINDOW *win, short buff[8], void *data )
{
	struct gui_window * gw = (struct gui_window *) data ;
	if( gw != NULL ) {
		browser_window_destroy( gw->browser->bw );
	}
}


static void __CDECL evnt_window_newtop( WINDOW *win, short buff[8], void *data )
{
	input_window = (struct gui_window *) data;
	window_set_focus( input_window, BROWSER, input_window->browser );
	LOG(("newtop gui window: %p, WINDOW: %p", input_window, win ));
	assert( input_window != NULL );
}

static void __CDECL evnt_window_slider( WINDOW * win, short buff[8], void * data)
{
	int dx = buff[4];
	int dy = buff[5];
	struct gui_window * gw = data;

	if (!dx && !dy) return;

	if( input_window == NULL || input_window != gw ) {
		return;
	}

	/* 	update the sliders _before_ we call redraw
		(which might depend on the slider possitions) */
	WindSlider( win, (dx?HSLIDER:0) | (dy?VSLIDER:0) );

	if( dy > 0 )
		browser_scroll( gw, WA_DNPAGE, abs(dy), false );
	else if ( dy < 0)
		browser_scroll( gw, WA_UPPAGE, abs(dy), false );
	if( dx > 0 )
		browser_scroll( gw, WA_RTPAGE, abs(dx), false );
	else if( dx < 0 )
		browser_scroll( gw, WA_LFPAGE, abs(dx), false );
}

static void __CDECL evnt_window_uniconify( WINDOW *win, short buff[8], void * data )
{
	struct gui_window * gw = (struct gui_window *)data;

	input_window = gw;
	WindTop( gw->root->handle );
	window_set_focus( gw, BROWSER, gw->browser );
}

static void __CDECL evnt_window_iconify( WINDOW *win, short buff[8], void * data )
{
	struct gui_window * gw = (struct gui_window *)data;
	if( input_window == gw){
		input_window = NULL;
	}
}

static void __CDECL evnt_window_icondraw( WINDOW *win, short buff[8], void * data )
{
	short x,y,w,h;
	struct gui_window * gw = (struct gui_window*)data;

	WindClear( win);
	WindGet( win, WF_WORKXYWH, &x, &y, &w, &h);
	if( gw->icon == NULL ) {
		OBJECT * tree;
		RsrcGaddr( h_gem_rsrc, R_TREE, ICONIFY , &tree );
		tree->ob_x = x;
		tree->ob_y = y;
		tree->ob_width = w;
		tree->ob_height = h;
		mt_objc_draw( tree, 0, 8, buff[4], buff[5], buff[6], buff[7], app.aes_global );
	} else {
	    struct rect clip = { 0,0,w,h };
        plotter->move( plotter, x, y );
        plotter->resize( plotter, w, h );
        plotter->clip(plotter, &clip );
        plotter->bitmap_resize( plotter,  gw->icon, w, h  );
        plotter->bitmap(
			plotter,
			( gw->icon->resized ) ? gw->icon->resized : gw->icon,
			0, 0, 0xffffff, BITMAPF_NONE
		);
	}
}

/* perform the actual resize */
static void __CDECL evnt_window_rt_resize( WINDOW *win, short buff[8], void * data )
{
	short x,y,w,h;
	struct gui_window * gw;

	wind_get( win->handle, WF_CURRXYWH, &x, &y, &w, &h );
	gw = (struct gui_window *)data;

	assert( gw != NULL );

	if(gw->root->loc.g_w != w || gw->root->loc.g_h != h ){
		/* report resize to component interface: */
		browser_update_rects( gw );
		tb_adjust_size( gw );
		if( gw->browser->bw->current_content != NULL ){
			/* Reformat will happen when next redraw message arrives: */
			gw->browser->reformat_pending = true;
			if( sys_XAAES() ){
				if( gw->root->loc.g_w > w || gw->root->loc.g_h > h ){
					ApplWrite( _AESapid, WM_REDRAW, gw->root->handle->handle,
								gw->root->loc.g_x, gw->root->loc.g_y,
								gw->root->loc.g_w, gw->root->loc.g_h );
				}
			}
			mt_WindGetGrect( &app, gw->root->handle, WF_CURRXYWH,
							(GRECT*)&gw->root->loc);
		}
		else {
			WindClear( gw->root->handle );
		}
	} else {
		if(gw->root->loc.g_x != x || gw->root->loc.g_y != y ){
			mt_WindGetGrect( &app, gw->root->handle, WF_CURRXYWH, (GRECT*)&gw->root->loc);
			browser_update_rects( gw );
		}
	}
}

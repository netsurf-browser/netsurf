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


bool cfg_rt_resize = false;
bool cfg_rt_move = false;
extern void * h_gem_rsrc;
extern struct gui_window *input_window;
extern GEM_PLOTTER plotter;

void __CDECL std_szd( WINDOW * win, short buff[8], void * );
void __CDECL std_mvd( WINDOW * win, short buff[8], void * );

/* -------------------------------------------------------------------------- */
/* Module public functions:                                                   */
/* -------------------------------------------------------------------------- */


static void __CDECL evnt_window_arrowed( WINDOW *win, short buff[8], void *data )
{
	bool abs = false;
	LGRECT cwork;
	int value = BROWSER_SCROLL_SVAL;
	
	if( input_window == NULL ) {
		return;
	}
	
	browser_get_rect( input_window, BR_CONTENT, &cwork );

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
	browser_scroll( input_window, buff[4], value, abs );
}

int window_create( struct gui_window * gw, struct browser_window * bw, unsigned long inflags)
{
	short buff[8];
	OBJECT * tbtree;
	int err = 0;
	bool tb, sb;
	tb = (inflags & WIDGET_TOOLBAR );
	sb = (inflags & WIDGET_STATUSBAR); 
	short w,h, wx, wy, wh, ww;
	int flags = CLOSER | MOVER | NAME | FULLER | SMALLER ;
	gw->parent = NULL;
	gw->root = malloc( sizeof(struct s_gui_win_root) );
	if( gw->root == NULL )
		return( -1 );
	memset( gw->root, 0, sizeof(struct s_gui_win_root) );
	gw->root->title = malloc(atari_sysinfo.aes_max_win_title_len+1);
	gw->root->handle = WindCreate( flags,40, 40, app.w, app.h );
	gw->root->cmproot = mt_CompCreate(&app, CLT_VERTICAL, 1, 1);
	WindSetPtr( gw->root->handle, WF_COMPONENT, gw->root->cmproot, NULL);

	if( tb ) {
		gw->root->toolbar = tb_create( gw );
		assert( gw->root->toolbar );
		mt_CompAttach( &app, gw->root->cmproot, gw->root->toolbar->comp );
		
	} else {
		gw->root->toolbar = NULL;
	}

	gw->browser = browser_create( gw, bw, NULL, BT_ROOT, CLT_HORIZONTAL, 1, 1 );
	mt_CompAttach( &app, gw->root->cmproot,  gw->browser->comp );

	if( sb ) {
		gw->root->statusbar = sb_create( gw );
		mt_CompAttach( &app, gw->root->cmproot, gw->root->statusbar->comp );
	} else {
		gw->root->statusbar = NULL;
	}

	WindSetStr(gw->root->handle, WF_ICONTITLE, (char*)"NetSurf");
	
	/* Event Handlers: */
	EvntDataAttach( gw->root->handle, WM_CLOSED, evnt_window_close, NULL );
	/* capture resize/move events so we can handle that manually */
	if( !cfg_rt_resize ) {
		EvntAttach( gw->root->handle, WM_SIZED, evnt_window_resize);
	} else {
		EvntAdd( gw->root->handle, WM_SIZED, evnt_window_rt_resize, EV_BOT );
	}
	if( !cfg_rt_move ) {
		EvntAttach( gw->root->handle, WM_MOVED, evnt_window_move );
	} else {
		EvntAdd( gw->root->handle, WM_MOVED, evnt_window_rt_resize, EV_BOT );
	}
	EvntAttach( gw->root->handle, WM_FORCE_MOVE, evnt_window_rt_resize );
	EvntDataAttach( gw->root->handle, AP_DRAGDROP, evnt_window_dd, gw );
	EvntDataAdd( gw->root->handle, WM_DESTROY,evnt_window_destroy, NULL, EV_TOP );
	EvntDataAdd( gw->root->handle, WM_ARROWED,evnt_window_arrowed, NULL, EV_TOP );
	EvntDataAdd( gw->root->handle, WM_NEWTOP, evnt_window_newtop, &evnt_data, EV_BOT);
	EvntDataAdd( gw->root->handle, WM_TOPPED, evnt_window_newtop, &evnt_data, EV_BOT);
	EvntDataAttach( gw->root->handle, WM_ICONDRAW, evnt_window_icondraw, gw);

	/*
	OBJECT * tbut;
	RsrcGaddr( h_gem_rsrc, R_TREE, FAVICO , &tbut );
	window_set_icon(gw, &tbut[]);
	*/
	/* TODO: check if window is openend as "foreground" window... */
	window_set_focus( gw, BROWSER, gw->browser);
	return (err);
}

int window_destroy( struct gui_window * gw)
{
	short buff[8];
	int err = 0;

	if( gw->browser->type != BT_ROOT ) {
		return(0);
	}
	
	/* test this with frames: */
	/* assert( gw->parent == NULL); */

	search_destroy( gw );

	if( input_window == gw )
		input_window = NULL;

	if( gw->root ) {
		if( gw->root->toolbar )
			tb_destroy( gw->root->toolbar );

		if( gw->root->statusbar )
			sb_destroy( gw->root->statusbar );
	}

	LOG(("Freeing browser window"));
	if( gw->browser )
		browser_destroy( gw->browser );

	/* destroy the icon: */
	/*window_set_icon(gw, NULL, false );*/

	/* needed? */ /*listRemove( (LINKABLE*)gw->root->cmproot ); */
	LOG(("Freeing root window"));
	if( gw->root ) {
		/* TODO: check if no other browser is bound to this root window! */
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

void window_open( struct gui_window * gw)
{
	LGRECT br;
	WindOpen(gw->root->handle, 20, 20, app.w/2, app.h/2 );
	WindSetStr( gw->root->handle, WF_NAME, (char *)"" );
	/* apply focus to the root frame: */
	long lfbuff[8] = { CM_GETFOCUS };
	mt_CompEvntExec( gl_appvar, gw->browser->comp, lfbuff );
	/* recompute the nested component sizes and positions: */
	browser_update_rects( gw );
	browser_get_rect( gw, BR_CONTENT, &br );
	plotter->move( plotter, br.g_x, br.g_y );
	plotter->resize( plotter, br.g_w, br.g_h );
	gw->browser->attached = true;
	if( gw->root->statusbar != NULL ){
		gw->root->statusbar->attached = true;
	}
	snd_rdw( gw->root->handle );
}

/*
TODO
void window_set_icon(struct gui_window * gw, void * data, bool is_rsc )
{
	#define CDT_ICON_TYPE_OBJECT 1UL
	#define CDT_ICON_TYPE_BITMAP 2UL
	void * prev_type;
	void * ico = DataSearch(&app, gw->root->handle, CDT_ICON );
	if(ico != NULL) {
		prev_type = DataSearch(&app, gw->root->handle, CDT_ICON_TYPE );
		if( prev_type == (void*)CDT_ICON_TYPE_OBJECT ){
			mt_ObjcFree( &app, (OBJECT*)ico );
		}
		if( prev_type == (void*)CDT_ICON_TYPE_BITMAP ){
			bitmap_destroy(ico);
		}
	}
	if( data != NULL ) {
		DataAttach( &app, gw->root->handle, CDT_ICON, data);
		if(is_rsc) {
			DataAttach( &app, gw->root->handle, CDT_ICON_TYPE, CDT_ICON_TYPE_OBJECT);
		} else {
			DataAttach( &app, gw->root->handle, CDT_ICON_TYPE, CDT_ICON_TYPE_BITMAP);
		}
	}
	#undef CDT_ICON_TYPE_OBJECT
	#undef CDT_ICON_TYPE_BITMAP
}
*/


/* update back forward buttons (see tb_update_buttons (bug) ) */
void window_update_back_forward( struct gui_window * gw)
{
	tb_update_buttons( gw );
}

static void window_redraw_controls(struct gui_window *gw, uint32_t flags)
{
	LGRECT rect;
	/* redraw sliders manually, dunno why this is needed (mt_WindSlider should do the job anytime)!*/

	browser_get_rect( gw, BR_VSLIDER, &rect);
	ApplWrite( _AESapid, WM_REDRAW,  gw->root->handle->handle,
		rect.g_x, rect.g_y, rect.g_w, rect.g_h );
	
	browser_get_rect( gw, BR_HSLIDER, &rect);
	ApplWrite( _AESapid, WM_REDRAW,  gw->root->handle->handle,
		rect.g_x, rect.g_y, rect.g_w, rect.g_h );

	/* send redraw to toolbar & statusbar & scrollbars: */
	mt_CompGetLGrect(&app, gw->root->toolbar->comp, WF_WORKXYWH, &rect);
	ApplWrite( _AESapid, WM_REDRAW,  gw->root->handle->handle,
		rect.g_x, rect.g_y, rect.g_w, rect.g_h );
	mt_CompGetLGrect(&app, gw->root->statusbar->comp, WF_WORKXYWH, &rect);	
	ApplWrite( _AESapid, WM_REDRAW,  gw->root->handle->handle,
		rect.g_x, rect.g_y, rect.g_w, rect.g_h );
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
				int posx, posy;
				struct box *box;
				struct box *file_box = 0;
				hlcache_handle *h;
				int box_x, box_y;
				LGRECT bwrect;
				struct browser_window * bw = gw->browser->bw;
				h = bw->current_content;
				if (!bw->current_content || content_get_type(h) != CONTENT_HTML)
					return;
				browser_get_rect( gw, BR_CONTENT, &bwrect );
				mx = mx - bwrect.g_x;
				my = my - bwrect.g_y;
				if( (mx < 0 || mx > bwrect.g_w) || (my < 0 || my > bwrect.g_h) )
					return;
				box = html_get_box_tree(h);
				box_x = box->margin[LEFT];
				box_y = box->margin[TOP];

				while ((box = box_at_point(box, mx+gw->browser->scroll.current.x, my+gw->browser->scroll.current.y, &box_x, &box_y, &h))) 
				{
					if (box->style && css_computed_visibility(box->style) == CSS_VISIBILITY_HIDDEN)
						continue;
					if (box->gadget) 
					{
						switch (box->gadget->type) 
						{
							case GADGET_FILE:
								file_box = box;
							break;
							/*
							TODO: handle these
							case GADGET_TEXTBOX:
							case GADGET_TEXTAREA:
							case GADGET_PASSWORD:
							text_box = box;
							break;
							*/
							default:
								break;
						}
					}
				} /* end While */
				if ( !file_box )
					return;
				if (file_box) {
					utf8_convert_ret ret;
					char *utf8_fn;

					ret = local_encoding_to_utf8( buff, 0, &utf8_fn);
					if (ret != UTF8_CONVERT_OK) {
						/* A bad encoding should never happen */
						LOG(("utf8_from_local_encoding failed"));
						assert(ret != UTF8_CONVERT_BADENC);
						/* Load was for us - just no memory */
						return;
					}
					/* Found: update form input. */
					free(file_box->gadget->value);
					file_box->gadget->value = utf8_fn;
					/* Redraw box. */
					box_coords(file_box, &posx, &posy);
					browser_schedule_redraw(bw->window, 
						posx - gw->browser->scroll.current.x, 
						posy - gw->browser->scroll.current.y,
						posx - gw->browser->scroll.current.x + file_box->width,
						posy - gw->browser->scroll.current.y + file_box->height);
				}
			}
		}
	}
error:	
	ddclose( dd_hdl);	
}

/* -------------------------------------------------------------------------- */
/* Non Public Modul event handlers:                                           */
/* -------------------------------------------------------------------------- */
static void __CDECL evnt_window_destroy( WINDOW *win, short buff[8], void *data )
{
	LOG(("%s\n", __FUNCTION__ ));

	if( data )
		free( data  );
}

static void __CDECL evnt_window_close( WINDOW *win, short buff[8], void *data )
{
	struct gui_window * gw = find_root_gui_window( win );
	if( gw != NULL ) {
		browser_window_destroy( gw->browser->bw );
	}
}


static void __CDECL evnt_window_newtop( WINDOW *win, short buff[8], void *data )
{
	input_window = find_root_gui_window( win );
	LOG(("newtop: iw: %p, win: %p", input_window, win ));
	assert( input_window != NULL );

	window_redraw_controls(input_window, 0);
}

static void __CDECL evnt_window_shaded( WINDOW *win, short buff[8], void *data )
{
	if(buff[0] == WM_SHADED){
		LOG(("WM_SHADED, vis: %d, state: %d", GEMWIN_VISIBLE(win), win->status ));
	}
	if(buff[0] == WM_UNSHADED){

	}
}

static void __CDECL evnt_window_icondraw( WINDOW *win, short buff[8], void * data )
{
	short x,y,w,h;
	struct gui_window * gw = (struct gui_window*)data;
	bool has_favicon = false;

	WindClear( win);
	WindGet( win, WF_WORKXYWH, &x, &y, &w, &h);
	
	if( has_favicon == false ) {
		OBJECT * tree;
		RsrcGaddr( h_gem_rsrc, R_TREE, ICONIFY , &tree );
		tree->ob_x = x;
		tree->ob_y = y;
		tree->ob_width = w;
		tree->ob_height = h;
		mt_objc_draw( tree, 0, 8, buff[4], buff[5], buff[6], buff[7], app.aes_global );
	}
}

static void __CDECL evnt_window_move( WINDOW *win, short buff[8] )
{
	short mx,my, mb, ks;
	short wx, wy, wh, ww, nx, ny;
	short r;
	short xoff, yoff;
	if( cfg_rt_move  ) {
		std_mvd( win, buff, &app );
		evnt_window_rt_resize( win, buff );
	} else { 
		wind_get( win->handle, WF_CURRXYWH, &wx, &wy, &ww, &wh );
		if( graf_dragbox( ww, wh, wx, wy, app.x-ww, app.y, app.w+ww, app.h+wh, &nx, &ny )){
			buff[4] = nx;
			buff[5] = ny;
			buff[6] = ww;
			buff[7] = wh;
			std_mvd( win, buff, &app );
			evnt_window_rt_resize( win, buff );
		}
	}
}

void __CDECL evnt_window_resize( WINDOW *win, short buff[8] )
{
	short mx,my, mb, ks;
	short wx, wy, wh, ww, nw, nh;
	short r;
	graf_mkstate( &mx, &my, &mb,  &ks ); 
	if( cfg_rt_resize  ) {
		std_szd( win, buff, &app );
		evnt_window_rt_resize( win, buff );
	} else { 
		wind_get( win->handle, WF_CURRXYWH, &wx, &wy, &ww, &wh );
		r = graf_rubberbox(wx, wy, 20, 20, &nw, &nh);
		if( nw < 40 && nw < 40 )
			return;
		buff[4] = wx;
		buff[5] = wy;
		buff[6] = nw;
		buff[7] = nh;
		std_szd( win, buff, &app );
		evnt_window_rt_resize( win, buff );
	}
}

/* perform the actual resize */
static void __CDECL evnt_window_rt_resize( WINDOW *win, short buff[8] )
{
	short x,y,w,h;
	struct gui_window * gw;
	LGRECT rect;
	bool resized;
	bool moved;
	
	if(buff[0] == WM_FORCE_MOVE ) {
		std_mvd(win, buff, &app);
		std_szd(win, buff, &app);
	}

	wind_get( win->handle, WF_WORKXYWH, &x, &y, &w, &h );
	gw = find_root_gui_window( win );

	assert( gw != NULL );

	if(gw->root->loc.g_x != x || gw->root->loc.g_y != y ){
		moved = true;
		gw->root->loc.g_x = x;
		gw->root->loc.g_y = y;
		browser_update_rects( gw );
	}

	if(gw->root->loc.g_w != w || gw->root->loc.g_h != h ){
		resized = true;
		/* report resize to component interface: */
		browser_update_rects( gw );
		browser_get_rect( gw, BR_CONTENT, &rect );
		if( gw->browser->bw->current_content != NULL )
			browser_window_reformat(gw->browser->bw, rect.g_w, rect.g_h );
		gw->root->toolbar->url.scrollx = 0;	
		window_redraw_controls(gw, 0);
		/* TODO: recalculate scroll position, istead of zeroing? */
	}
}

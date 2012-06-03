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

#include "utils/log.h"
#include "desktop/gui.h"
#include "desktop/history_core.h"
#include "desktop/netsurf.h"
#include "desktop/browser.h"
#include "desktop/mouse.h"
#include "desktop/plot_style.h"
#include "desktop/plotters.h"
#include "desktop/tree.h"
#include "utils/utf8.h"
#include "atari/clipboard.h"
#include "atari/gui.h"
#include "atari/toolbar.h"
#include "atari/browser_win.h"
#include "atari/browser.h"
#include "atari/clipboard.h"
#include "atari/misc.h"
#include "atari/global_evnt.h"
#include "atari/plot.h"
#include "cflib.h"
#include "atari/res/netsurf.rsh"
#include "atari/plot/plotter.h"


extern char * cfg_homepage_url;
extern short vdih;
extern void * h_gem_rsrc;
extern GEM_PLOTTER plotter;
extern struct gui_window * input_window;
static OBJECT * throbber_form = NULL;
static bool img_toolbar = false;
static char * img_toolbar_folder = (char *)"default";
static short toolbar_bg_color = LWHITE;
static hlcache_handle * toolbar_image;
static hlcache_handle * throbber_image;
static bool toolbar_image_ready = false;
static bool throbber_image_ready = false;


static plot_font_style_t font_style_url = {
    .family = PLOT_FONT_FAMILY_SANS_SERIF,
    .size = 14*FONT_SIZE_SCALE,
    .weight = 400,
    .flags = FONTF_NONE,
    .background = 0xffffff,
    .foreground = 0x0
 };

/* prototypes & order for button widgets: */

static struct s_tb_button tb_buttons[] =
{
	{
        TOOLBAR_BT_BACK,
        tb_back_click,
        "toolbar/%s/bck_%s.png",
        0, 0,
        {0,0},
        0, 0, 0
    },
	{
        TOOLBAR_BT_HOME,
        tb_home_click,
        "toolbar/%s/hme_%s.png",
        0, 0, {0,0}, 0, 0, 0
    },
	{
        TOOLBAR_BT_FORWARD,
        tb_forward_click,
        "toolbar/%s/fwd_%s.png",
        0, 0,
        {0,0},
        0, 0, 0
    },
	{
        TOOLBAR_BT_STOP,
        tb_stop_click,
        "toolbar/%s/stp_%s.png",
        0, 0,
        {0,0},
        0, 0, 0
    },
	{
        TOOLBAR_BT_RELOAD,
        tb_reload_click,
        "toolbar/%s/rld_%s.png",
        0, 0,
        {0,0},
        0, 0, 0
    },
	{ 0, 0, 0, 0, 0,  {0,0}, 0, 0, -1 }
};

struct s_toolbar_style {
	int font_height_pt;
	int height;
	int icon_width;
	int icon_height;
	int button_hmargin;
	int button_vmargin;
	short bgcolor;
	/* RRGGBBAA: */
	uint32_t icon_bgcolor;
};

static struct s_toolbar_style toolbar_styles[] =
{
	/* small (18 px height) */
	{ 9, 18, 16, 16, 0, 0, LWHITE, 0 },
	/* medium (default - 26 px height) */
	{14, 26, 24, 24, 1, 4, LWHITE, 0 },
	/* large ( 49 px height ) */
	{18, 34, 64, 64, 2, 0, LWHITE, 0 },
	/* custom style: */
	{18, 34, 64, 64, 2, 0, LWHITE, 0 }
};

static void tb_txt_request_redraw( void *data, int x, int y, int w, int h );
static nserror toolbar_icon_callback( hlcache_handle *handle,
		const hlcache_event *event, void *pw );


void toolbar_init( void )
{
	int i=0, n;
	short vdicolor[3];
	uint32_t rgbcolor;

	img_toolbar = (nsoption_int( atari_image_toolbar ) > 0 ) ? true : false;
	if( img_toolbar ){

        char imgfile[PATH_MAX];

        while( tb_buttons[i].rsc_id != 0){
			tb_buttons[i].index = i;
			i++;
		}
		toolbar_image = load_icon( "toolbar/default/main.png",
									toolbar_icon_callback, NULL );
		throbber_image = load_icon( "toolbar/default/throbber.png",
								toolbar_icon_callback, NULL );

	}
    n = (sizeof( toolbar_styles ) / sizeof( struct s_toolbar_style ));
    for( i=0; i<n; i++ ){
        toolbar_styles[i].bgcolor = toolbar_bg_color;
        if( img_toolbar ){
			vq_color( vdih, toolbar_styles[i].bgcolor, 0, vdicolor );
			vdi1000_to_rgb( vdicolor, (unsigned char*)&rgbcolor );
			toolbar_styles[i].icon_bgcolor = rgbcolor;
        }
    }
}

void toolbar_exit( void )
{
	if( toolbar_image )
		hlcache_handle_release( toolbar_image );
	if( throbber_image )
		hlcache_handle_release( throbber_image );
}

/**
 * Callback for load_icon(). Should be removed once bitmaps get loaded directly
 * from disc
 */
static nserror toolbar_icon_callback(hlcache_handle *handle,
		const hlcache_event *event, void *pw)
{
	if( event->type == CONTENT_MSG_READY ){
		if( handle == toolbar_image ){
			toolbar_image_ready = true;
			if( input_window != NULL )
				tb_update_buttons( input_window, 0 );
		}
		else if( handle == throbber_image ){
			throbber_image_ready = true;
		}
	}

	return NSERROR_OK;
}


static void __CDECL button_redraw( COMPONENT *c, long buff[8], void * data )
{

	OBJECT *tree=NULL;
	LGRECT work,clip;
	GRECT todo,crect;
	struct s_tb_button *bt = (struct s_tb_button*)data;
	struct gui_window * gw = bt->gw;
	struct s_toolbar * tb = gw->root->toolbar;

	short pxy[4];
	int  bmpx=0, bmpy=0, bmpw=0, bmph = 0, drawstate=0;
	struct rect icon_clip;
	struct bitmap * icon = NULL;
	bool draw_bitmap = false;

	mt_CompGetLGrect(&app, c, WF_WORKXYWH, &work);
	work.g_h = work.g_h - 1;
	clip = work;
	/* return if component and redraw region does not intersect: */
	if ( !rc_lintersect( (LGRECT*)&buff[4], &clip ) ) {
		return;
	}

	if( img_toolbar && toolbar_image != NULL ){
		if( toolbar_image_ready == false ){
			return;
		}
		icon = content_get_bitmap( toolbar_image );
		if( icon == NULL ){
			return;
		}
		draw_bitmap = true;
		drawstate = bt->state;

		bmpw = bitmap_get_width(icon);
		bmph = bitmap_get_height(icon);
		bmpx = 0;
		bmpy = 0;

		plot_set_dimensions(
			work.g_x-(toolbar_styles[tb->style].icon_width * bt->index)+toolbar_styles[tb->style].button_vmargin,
			work.g_y-(toolbar_styles[tb->style].icon_height * drawstate)+toolbar_styles[tb->style].button_hmargin,
			toolbar_styles[tb->style].icon_width*(bt->index+1),
			toolbar_styles[tb->style].icon_height*(drawstate+1)
		);
		icon_clip.x0 = bmpx+(toolbar_styles[tb->style].icon_width*bt->index);
		icon_clip.y0 = bmpy+(toolbar_styles[tb->style].icon_height*drawstate);
		icon_clip.x1 = icon_clip.x0+toolbar_styles[tb->style].icon_width;
		icon_clip.y1 = icon_clip.y0+toolbar_styles[tb->style].icon_height;
		plot_clip( &icon_clip  );
	}

	if( draw_bitmap == false ){
		/* Place the CICON into workarea: */
		tree = bt->aes_object;
		if( tree == NULL )
			return;
		tree->ob_x = work.g_x;
		tree->ob_y = work.g_y + (work.g_h - tree->ob_height) / 2;
	}

	/* Setup draw mode: */
	vsf_interior( vdih , 1 );
	vsf_color( vdih, toolbar_styles[tb->style].bgcolor );
	vswr_mode( vdih, MD_REPLACE);

	/* go through the rectangle list, using classic AES methods. */
	/* Windom ComGetLGrect is buggy for WF_FIRST/NEXTXYWH	     */
	crect.g_x = clip.g_x;
	crect.g_y = clip.g_y;
	crect.g_w = clip.g_w;
	crect.g_h = clip.g_h;
	wind_get(gw->root->handle->handle, WF_FIRSTXYWH,
							&todo.g_x, &todo.g_y, &todo.g_w, &todo.g_h );
	while( (todo.g_w > 0) && (todo.g_h > 0) ){

		if( rc_intersect( &crect, &todo ) ){
			pxy[0] = todo.g_x;
			pxy[1] = todo.g_y;
			pxy[2] = todo.g_w + todo.g_x-1;
			pxy[3] = todo.g_h + todo.g_y-1;
			vs_clip( vdih, 1, (short*)&pxy );
			v_bar( vdih, (short*)&pxy );

			if( draw_bitmap ){
				atari_plotters.bitmap( bmpx, bmpy, bmpw, bmph, icon,
										toolbar_styles[tb->style].icon_bgcolor,
										BITMAPF_BUFFER_NATIVE );
			} else {
				objc_draw( tree, 0, 0, todo.g_x, todo.g_y, todo.g_w, todo.g_h );
			}
			vs_clip( vdih, 0, (short*)&clip );
		}
		wind_get(gw->root->handle->handle, WF_NEXTXYWH,
							&todo.g_x, &todo.g_y, &todo.g_w, &todo.g_h );
	}
}

static void __CDECL button_enable( COMPONENT *c, long buff[8], void * data )
{
	struct s_tb_button * bt = (struct s_tb_button *)data;
	bt->aes_object->ob_state &= ~OS_DISABLED;
}

static void __CDECL button_disable( COMPONENT *c, long buff[8], void * data )
{
	struct s_tb_button * bt = (struct s_tb_button *)data;
	bt->aes_object->ob_state |= OS_DISABLED;
}

static void __CDECL button_click( COMPONENT *c, long buff[8], void * data )
{
	struct s_tb_button * bt = (struct s_tb_button *)data;
	int i = 0;
	struct gui_window * gw = bt->gw;
	assert( gw );
	gw->root->toolbar->buttons[bt->index].cb_click( gw );
}

static struct s_tb_button * find_button( struct gui_window * gw, int rsc_id )
{
	int i = 0;
	while( i < gw->root->toolbar->btcnt ) {
		if( gw->root->toolbar->buttons[i].rsc_id == rsc_id ) {
			return( &gw->root->toolbar->buttons[i] );
		}
		i++;
	}
}


static COMPONENT *button_init( CMP_TOOLBAR t, OBJECT * tree, int index,
							struct s_tb_button * instance )
{
	int comp_width;

	*instance = tb_buttons[index];
	instance->gw = t->owner;

	comp_width = toolbar_styles[t->style].icon_width + \
		( toolbar_styles[t->style].button_vmargin * 2 );

	instance->comp = mt_CompCreate( &app, CLT_VERTICAL, comp_width, 0 );
	assert( instance->comp );

	instance->comp->bounds.max_width = comp_width;

	if( img_toolbar == false ){
		// FIXME: is it really required to dup the object? Can this be moved
		// to toolbar_init() ?
		OBJECT *oc = mt_ObjcNDup( &app, &tree[instance->rsc_id], NULL, 1);
		oc->ob_next = oc->ob_head = oc->ob_tail = -1;
		instance->aes_object = oc;
	}
	mt_CompEvntDataAttach( &app, instance->comp, WM_REDRAW, button_redraw,
						instance );
	mt_CompEvntDataAttach( &app, instance->comp, WM_XBUTTON, button_click,
						instance );
	return instance->comp;
}


static
void __CDECL evnt_throbber_redraw( COMPONENT *c, long buff[8])
{
	LGRECT work, clip;
	int idx;
	short pxy[4];
	struct s_toolbar * tb;
	struct gui_window * gw = (struct gui_window *)mt_CompDataSearch(&app,
																	c,
																	CDT_OWNER );

	tb = gw->root->toolbar;
	mt_CompGetLGrect(&app, c, WF_WORKXYWH, &work);
	clip = work;
	if ( !rc_lintersect( (LGRECT*)&buff[4], &clip ) ) return;

	vsf_interior( vdih , 1 );
	if(app.nplanes > 2 )
		vsf_color( vdih, toolbar_styles[tb->style].bgcolor );
	else
		vsf_color( vdih, WHITE );
	pxy[0] = (short)buff[4];
	pxy[1] = (short)buff[5];
	pxy[2] = (short)buff[4] + buff[6]-1;
	pxy[3] = (short)buff[5] + buff[7]-2;
	v_bar( vdih, (short*)&pxy );
	vs_clip( vdih, 1, (short*)&pxy );

	if( img_toolbar && throbber_image != NULL ){

		int  bmpx=0, bmpy=0, bmpw=0, bmph = 0, drawstate=0;
		struct rect icon_clip;
		struct bitmap * icon = NULL;

		if( throbber_image_ready == false ){
			return;
		}
		icon = content_get_bitmap( throbber_image );
		if( icon == NULL ){
			return;
		}

		if( tb->throbber.running == false ) {
				idx = 0;
		}
		else {
			idx = tb->throbber.index;
			if( idx > tb->throbber.max_index ) {
				idx = tb->throbber.index = 1;
			}
		}
		bmpw = bitmap_get_width(icon);
		bmph = bitmap_get_height(icon);
		bmpx = 0;
		bmpy = 0;

		/*
			for some reason, adding
			toolbar_styles[tb->style].button_vmargin to the x pos of
			the plotter shifts the icon a bit to much.
			That shouldn't happen... maybe the URL widget
			size is a bit to large - to be inspected...
		*/
		plot_set_dimensions(
			work.g_x-(toolbar_styles[tb->style].icon_width * idx),
			work.g_y+toolbar_styles[tb->style].button_hmargin,
			toolbar_styles[tb->style].icon_width*(idx+1),
			toolbar_styles[tb->style].icon_height
		);
		icon_clip.x0 = bmpx+(toolbar_styles[tb->style].icon_width*idx);
		icon_clip.y0 = bmpy;
		icon_clip.x1 = icon_clip.x0+toolbar_styles[tb->style].icon_width;
		icon_clip.y1 = icon_clip.y0+toolbar_styles[tb->style].icon_height;
		plot_clip( &icon_clip  );
		atari_plotters.bitmap( bmpx, bmpy, bmpw, bmph, icon,
										toolbar_styles[tb->style].icon_bgcolor,
										BITMAPF_BUFFER_NATIVE );
	}
	else {
		if( throbber_form != NULL ) {
			if( gw->root->toolbar->throbber.running == false ) {
				idx = THROBBER_INACTIVE_INDEX;
			} else {
				idx = gw->root->toolbar->throbber.index;
				if( idx > THROBBER_MAX_INDEX || idx < THROBBER_MIN_INDEX ) {
					idx = THROBBER_MIN_INDEX;
				}
			}
			throbber_form[idx].ob_x = work.g_x+1;
			throbber_form[idx].ob_y = work.g_y+4;
			mt_objc_draw( throbber_form, idx, 8, clip.g_x, clip.g_y, clip.g_w, clip.g_h, app.aes_global );
		}
	}

}

static
void __CDECL evnt_url_redraw( COMPONENT *c, long buff[8] )
{
	LGRECT work, clip;
	struct gui_window * gw;
	short pxy[10];
    // FIXME: optimize this:
	gw = (struct gui_window *)mt_CompDataSearch(&app, c, CDT_OWNER);
	if( gw == NULL )
		return;

	CMP_TOOLBAR tb = gw->root->toolbar;
	mt_CompGetLGrect(&app, tb->url.comp, WF_WORKXYWH, &work);

	// this last pixel is drawn by the root component of the toolbar:
	// it's the black border, so we leave it out:
	work.g_h--;
	clip = work;
	if ( !rc_lintersect( (LGRECT*)&buff[4], &clip ) ) return;

	pxy[0] = clip.g_x;
	pxy[1] = clip.g_y;
	pxy[2] = clip.g_w + clip.g_x-1;
	pxy[3] = clip.g_h + clip.g_y-1;
	vs_clip( vdih, 1, (short*)&pxy );

	vsf_perimeter( vdih, 0 );
	vsf_interior( vdih , 1 );
	vsf_color( vdih, toolbar_styles[tb->style].bgcolor );

	//left margin:
	pxy[0] = work.g_x;
	pxy[1] = work.g_y;
	pxy[2] = work.g_x + TOOLBAR_URL_MARGIN_LEFT-1;
	pxy[3] = work.g_y + work.g_h-1;
	v_bar( vdih, pxy );

	// right margin:
	pxy[0] = work.g_x+work.g_w-TOOLBAR_URL_MARGIN_RIGHT;
	pxy[1] = work.g_y;
	pxy[2] = work.g_x+work.g_w-1;
	pxy[3] = work.g_y+work.g_h-1;
	v_bar( vdih, pxy );

	// top margin:
	pxy[0] = work.g_x;
	pxy[1] = work.g_y;
	pxy[2] = work.g_x+work.g_w-1;
	pxy[3] = work.g_y+TOOLBAR_URL_MARGIN_TOP-1;
	v_bar( vdih, pxy );

	// bottom margin:
	pxy[0] = work.g_x;
	pxy[1] = work.g_y+work.g_h-TOOLBAR_URL_MARGIN_BOTTOM;
	pxy[2] = work.g_x+work.g_w-1;
	pxy[3] = work.g_y+work.g_h-1;
	v_bar( vdih, pxy );

	vs_clip( vdih, 0, (short*)&pxy );

	// TBD: request redraw of textarea for specific region.
	clip.g_x -= work.g_x+TOOLBAR_URL_MARGIN_LEFT;
	clip.g_y -= work.g_y+TOOLBAR_URL_MARGIN_TOP;
	tb_txt_request_redraw( tb, clip.g_x, clip.g_y, clip.g_w, clip.g_h );
}

static
void __CDECL evnt_url_click( COMPONENT *c, long buff[8] )
{
	LGRECT work;
	short pxy[4];
	short mx, my, mb, kstat;
	int old;
	graf_mkstate( &mx, &my, &mb,  &kstat );
	struct gui_window * gw = (struct gui_window *)mt_CompDataSearch(&app, c, CDT_OWNER);
	assert( gw != NULL );
	CMP_TOOLBAR tb = gw->root->toolbar;
	mt_CompGetLGrect(&app, c, WF_WORKXYWH, &work);
	mx = evnt.mx - (work.g_x + TOOLBAR_URL_MARGIN_LEFT);
	my = evnt.my - (work.g_y + TOOLBAR_URL_MARGIN_TOP);

	/* TODO: reset mouse state of browser window? */
	/* select whole text when newly focused, otherwise set caret to end of text */
	if( !window_url_widget_has_focus(gw) ) {
		window_set_focus( gw, URL_WIDGET, (void*)&tb->url );
	} else {
		if( mb & 1 ) {
			textarea_mouse_action( tb->url.textarea, BROWSER_MOUSE_DRAG_1,
									mx, my );
			short prev_x = mx;
			short prev_y = my;
			do{
				if( abs(prev_x-mx) > 5 || abs(prev_y-my) > 5 ){
					textarea_mouse_action( tb->url.textarea,
										BROWSER_MOUSE_HOLDING_1, mx, my );
					prev_x = mx;
					prev_y = my;
					if( tb->url.redraw ){
						tb_url_redraw( gw );
					}
				}
				graf_mkstate( &mx, &my, &mb,  &kstat );
				mx = mx - (work.g_x + TOOLBAR_URL_MARGIN_LEFT);
				my = my - (work.g_y + TOOLBAR_URL_MARGIN_TOP);
			}while( mb & 1 );
				textarea_drag_end( tb->url.textarea, 0, mx, my );
		} else {
			/* TODO: recognize click + shift key */
			int mstate = BROWSER_MOUSE_PRESS_1;
			if( (kstat & (K_LSHIFT|K_RSHIFT)) != 0 )
				mstate = BROWSER_MOUSE_MOD_1;
			textarea_mouse_action( tb->url.textarea,
									BROWSER_MOUSE_PRESS_1, mx, my );
		}
	}
	// TODO: do not send an complete redraw!
	ApplWrite( _AESapid, WM_REDRAW,  gw->root->handle->handle,
		work.g_x, work.g_y, work.g_w, work.g_h );
}


void tb_adjust_size( struct gui_window * gw )
{
	LGRECT work;
	CMP_TOOLBAR t = gw->root->toolbar;

	mt_CompGetLGrect( &app, t->url.comp, WF_WORKXYWH, &work);
	work.g_w -= (TOOLBAR_URL_MARGIN_LEFT + TOOLBAR_URL_MARGIN_RIGHT);
	/* do not overwrite the black border, because of that, add 1 */
	work.g_h -= (TOOLBAR_URL_MARGIN_TOP + TOOLBAR_URL_MARGIN_BOTTOM+1);
	textarea_set_dimensions( t->url.textarea, work.g_w, work.g_h );
	tb_txt_request_redraw( t, 0,0, work.g_w-1, work.g_h-1);
}

static void __CDECL evnt_toolbar_redraw( COMPONENT *c, long buff[8], void *data )
{
	LGRECT work, clip;
	short pxy[4];

	mt_CompGetLGrect(&app, c, WF_WORKXYWH, &work);
	clip = work;
	if( !rc_lintersect( (LGRECT*)&buff[4], &clip ) ) return;

	if( work.g_y + work.g_h != clip.g_y + clip.g_h )	return;

	vswr_mode( vdih, MD_REPLACE );
	vsl_color( vdih, BLACK );
	vsl_type( vdih, 1 );
	vsl_width( vdih, 1 );
	pxy[0] = clip.g_x;
	pxy[1] = pxy[3] = work.g_y + work.g_h-1 ;
	pxy[2] = clip.g_x + clip.g_w;
	v_pline( vdih, 2, (short*)&pxy );
}


static void tb_txt_request_redraw(void *data, int x, int y, int w, int h)
{
	LGRECT work;
	if( data == NULL )
		return;
	CMP_TOOLBAR t = data;
	if( t->url.redraw == false ){
		t->url.redraw = true;
		//t->redraw = true;
		t->url.rdw_area.g_x = x;
		t->url.rdw_area.g_y = y;
		t->url.rdw_area.g_w = w;
		t->url.rdw_area.g_h = h;
	} else {
		/* merge the redraw area to the new area.: */
		int newx1 = x+w;
		int newy1 = y+h;
		int oldx1 = t->url.rdw_area.g_x + t->url.rdw_area.g_w;
		int oldy1 = t->url.rdw_area.g_y + t->url.rdw_area.g_h;
		t->url.rdw_area.g_x = MIN(t->url.rdw_area.g_x, x);
		t->url.rdw_area.g_y = MIN(t->url.rdw_area.g_y, y);
		t->url.rdw_area.g_w = ( oldx1 > newx1 ) ?
			oldx1 - t->url.rdw_area.g_x : newx1 - t->url.rdw_area.g_x;
		t->url.rdw_area.g_h = ( oldy1 > newy1 ) ?
			oldy1 - t->url.rdw_area.g_y : newy1 - t->url.rdw_area.g_y;
	}
}

void tb_url_redraw( struct gui_window * gw )
{
	CMP_TOOLBAR t = gw->root->toolbar;
	if (t != NULL) {
		if( t->url.redraw && ((plotter->flags & PLOT_FLAG_OFFSCREEN) == 0) ) {

			const struct redraw_context ctx = {
				.interactive = true,
				.background_images = true,
				.plot = &atari_plotters
			};
			short todo[4];
			LGRECT work;

			mt_CompGetLGrect(&app, gw->root->toolbar->url.comp, WF_WORKXYWH, &work);
			work.g_x += TOOLBAR_URL_MARGIN_RIGHT;
			work.g_y += TOOLBAR_URL_MARGIN_LEFT;
			work.g_w -= TOOLBAR_URL_MARGIN_RIGHT;
			work.g_h -= TOOLBAR_URL_MARGIN_BOTTOM;

			plot_set_dimensions( work.g_x, work.g_y, work.g_w, work.g_h );
			if( plotter->lock( plotter ) == 0 )
				return;

			todo[0] = work.g_x;
			todo[1] = work.g_y;
			todo[2] = todo[0] + work.g_w-1;
			todo[3] = todo[1] + work.g_h-1;
			vs_clip(plotter->vdi_handle, 1, (short*)&todo );

			if( wind_get(gw->root->handle->handle, WF_FIRSTXYWH,
							&todo[0], &todo[1], &todo[2], &todo[3] )!=0 ) {
				while (todo[2] && todo[3]) {

					/* convert screen to relative coords: */
					todo[0] = todo[0] - work.g_x;
					todo[1] = todo[1] - work.g_y;
					if( todo[0] < 0 ){
						todo[2] = todo[2] + todo[0];
						todo[0] = 0;
					}
					if( todo[1] < 0 ){
						todo[3] = todo[3] + todo[1];
						todo[1] = 0;
					}

					if (rc_intersect(&t->url.rdw_area,(GRECT *)&todo)) {
						struct rect clip = {
							.x0 = todo[0],
							.y0 = todo[1],
							.x1 = todo[0]+todo[2],
							.y1 = todo[1]+todo[3]
						};
						textarea_redraw( t->url.textarea, 0, 0, &clip, &ctx );
					}
					if (wind_get(gw->root->handle->handle, WF_NEXTXYWH,
							&todo[0], &todo[1], &todo[2], &todo[3])==0) {
						break;
					}
				}
			} else {
				plotter->unlock( plotter );
				return;
			}
			plotter->unlock( plotter );
			vs_clip(plotter->vdi_handle, 0, (short*)&todo);
			t->url.redraw = false;
			t->url.rdw_area.g_x = 65000;
			t->url.rdw_area.g_y = 65000;
			t->url.rdw_area.g_w = -1;
			t->url.rdw_area.g_h = -1;
		} else {
			/* just copy stuff from the offscreen buffer */
		}
	}
}

CMP_TOOLBAR tb_create( struct gui_window * gw )
{
	int i;
	OBJECT * tbut = NULL;

	CMP_TOOLBAR t = malloc( sizeof(struct s_toolbar) );
	if( t == NULL )
		return( NULL );

	t->owner = gw;
	t->style = 1;

	/* create the root component: */
	t->comp = (COMPONENT*)mt_CompCreate(&app, CLT_HORIZONTAL,
										toolbar_styles[t->style].height, 0 );
	t->comp->rect.g_h = toolbar_styles[t->style].height;
	t->comp->bounds.max_height = toolbar_styles[t->style].height;
	mt_CompEvntDataAdd(&app, t->comp, WM_REDRAW, evnt_toolbar_redraw,
						NULL, EV_BOT);

	if( img_toolbar == false ){
		RsrcGaddr( h_gem_rsrc, R_TREE, TOOLBAR, &tbut );
	}

	/* count buttons and add them as components: */
	i = 0;
	while( tb_buttons[i].rsc_id > 0 ) {
		i++;
	}
	t->btcnt = i;
	t->buttons = malloc( t->btcnt * sizeof(struct s_tb_button) );
	memset( t->buttons, 0, t->btcnt * sizeof(struct s_tb_button) );
	for( i=0; i < t->btcnt; i++ ) {
		button_init( t, tbut, i, &t->buttons[i] );
		mt_CompAttach( &app, t->comp,  t->buttons[i].comp );
	}

	/* create the url widget: */
	font_style_url.size =
		toolbar_styles[t->style].font_height_pt * FONT_SIZE_SCALE;

	int ta_height = toolbar_styles[t->style].height;
	ta_height -= (TOOLBAR_URL_MARGIN_TOP + TOOLBAR_URL_MARGIN_BOTTOM);
	t->url.textarea = textarea_create( 300,
									ta_height,
									0,
									&font_style_url, tb_txt_request_redraw,
									t );
	if( t->url.textarea != NULL ){
		textarea_set_text(t->url.textarea, "http://");
	}

	t->url.comp = (COMPONENT*)mt_CompCreate(&app, CLT_HORIZONTAL,
											toolbar_styles[t->style].height, 1);
	mt_CompEvntAttach( &app, t->url.comp, WM_REDRAW, evnt_url_redraw );
	mt_CompEvntAttach( &app, t->url.comp, WM_XBUTTON, evnt_url_click );
	mt_CompDataAttach( &app, t->url.comp, CDT_OWNER, gw );
	mt_CompAttach( &app, t->comp, t->url.comp );

	/* create the throbber widget: */
	if( throbber_form == NULL && img_toolbar == false ) {
		RsrcGaddr( h_gem_rsrc, R_TREE, THROBBER , &throbber_form );
		throbber_form->ob_x = 0;
		throbber_form->ob_y = 0;
	}
	t->throbber.comp = (COMPONENT*)mt_CompCreate(&app, CLT_HORIZONTAL,
												toolbar_styles[t->style].height, 0);
	t->throbber.comp->rect.g_h = toolbar_styles[t->style].height;
	t->throbber.comp->rect.g_w = t->throbber.comp->bounds.max_width = \
		toolbar_styles[t->style].icon_width + \
		(2*toolbar_styles[t->style].button_vmargin );
	t->throbber.comp->bounds.max_height = toolbar_styles[t->style].height;
	if( img_toolbar ){
		t->throbber.index = 0;
		t->throbber.max_index = 8;
	} else {
		t->throbber.index = THROBBER_MIN_INDEX;
		t->throbber.max_index = THROBBER_MAX_INDEX;
	}
	t->throbber.running = false;
	mt_CompEvntAttach( &app, t->throbber.comp, WM_REDRAW, evnt_throbber_redraw );
	mt_CompDataAttach( &app, t->throbber.comp, CDT_OWNER, gw );
	mt_CompAttach( &app, t->comp, t->throbber.comp );
	return( t );
}


void tb_destroy( CMP_TOOLBAR tb )
{
	int i=0;
	while( i < tb->btcnt ) {
		if( tb->buttons[i].aes_object ){
			mt_ObjcFree( &app, tb->buttons[i].aes_object );
		}
		i++;
	}
	free( tb->buttons );
	textarea_destroy( tb->url.textarea );
	mt_CompDelete( &app, tb->comp);
	free( tb );
}


struct gui_window * tb_gui_window( CMP_TOOLBAR tb )
{
	struct gui_window * gw;
	gw = window_list;
	while( gw != NULL ) {
		if( gw->root->toolbar == tb ) {
			LOG(("found tb gw: %p (tb: %p) for tb: %p", gw, gw->root->toolbar, tb ));
			return( gw );
		}
		else
			gw = gw->next;
	}
	return( NULL );
}


void tb_update_buttons( struct gui_window * gw, short button )
{

#define FIRST_BUTTON TOOLBAR_BT_BACK

	struct s_tb_button * bt;
	bool enable = false;
	if( button == TOOLBAR_BT_BACK || button <= 0 ){
		bt = &gw->root->toolbar->buttons[TOOLBAR_BT_BACK-FIRST_BUTTON];
		enable = browser_window_back_available(gw->browser->bw);
        if( enable ){
            bt->state = button_on;
        } else {
            bt->state = button_off;
        }
		if( bt->aes_object ){
			if( enable ) {
				bt->aes_object->ob_state |= OS_DISABLED;
			} else {
				bt->aes_object->ob_state &= ~OS_DISABLED;
			}
		} else {
			// TODOs
		}
        mt_CompEvntRedraw( &app, bt->comp );
	}

	if( button == TOOLBAR_BT_HOME || button <= 0 ){
		bt = &gw->root->toolbar->buttons[TOOLBAR_BT_HOME-FIRST_BUTTON];
		mt_CompEvntRedraw( &app, bt->comp );
	}

	if( button == TOOLBAR_BT_FORWARD || button <= 0 ){
		bt = &gw->root->toolbar->buttons[TOOLBAR_BT_FORWARD-FIRST_BUTTON];
		enable = browser_window_forward_available(gw->browser->bw);
        if( enable ){
            bt->state = button_on;
        } else {
            bt->state = button_off;
        }
		if( bt->aes_object ){
			if( enable ) {
				bt->aes_object->ob_state |= OS_DISABLED;
			} else {
				bt->aes_object->ob_state &= ~OS_DISABLED;
			}
		} else {
			// TODOs
		}
        mt_CompEvntRedraw( &app, bt->comp );
	}

	if( button == TOOLBAR_BT_RELOAD || button <= 0 ){
		bt = &gw->root->toolbar->buttons[TOOLBAR_BT_RELOAD-FIRST_BUTTON];
		enable = browser_window_reload_available(gw->browser->bw);
        if( enable ){
            bt->state = button_on;
        } else {
            bt->state = button_off;
        }
		if( bt->aes_object ){
			if( enable ) {
				bt->aes_object->ob_state |= OS_DISABLED;
			} else {
				bt->aes_object->ob_state &= ~OS_DISABLED;
			}
		} else {
			// TODOs
		}
        mt_CompEvntRedraw( &app, bt->comp );
	}

	if( button == TOOLBAR_BT_STOP || button <= 0 ){
		bt = &gw->root->toolbar->buttons[TOOLBAR_BT_STOP-FIRST_BUTTON];
		enable = browser_window_stop_available(gw->browser->bw);
        if( enable ){
            bt->state = button_on;
        } else {
            bt->state = button_off;
        }
		if( bt->aes_object ){
			if( enable ) {
				bt->aes_object->ob_state |= OS_DISABLED;
			} else {
				bt->aes_object->ob_state &= ~OS_DISABLED;
			}
		} else {
			// TODOs
		}
        mt_CompEvntRedraw( &app, bt->comp );
	}

#undef FIRST_BUTON
}



void tb_url_set( struct gui_window * gw, char * text )
{
	LGRECT work;
	int len = strlen(text);
	char * newtext;
	int newsize;

	if( gw->root->toolbar == NULL )
		return;

	if( gw->browser->attached == false )
		return;

	struct s_url_widget * url = &gw->root->toolbar->url;

	assert( gw != NULL );
	assert( gw->browser != NULL );
	assert( gw->root != NULL );
	assert( gw->browser->bw != NULL );

	textarea_set_text(url->textarea, text);

	mt_CompGetLGrect( &app, gw->root->toolbar->url.comp, WF_WORKXYWH, &work);
	work.g_w -= (TOOLBAR_URL_MARGIN_LEFT + TOOLBAR_URL_MARGIN_RIGHT);
	/* do not overwrite the black border, because of that, add 1 */
	work.g_h -= (TOOLBAR_URL_MARGIN_TOP + TOOLBAR_URL_MARGIN_BOTTOM+1);
	tb_txt_request_redraw( gw->root->toolbar, 0,0,work.g_w,work.g_h );
	tb_update_buttons( gw, TOOLBAR_BT_STOP );
	return;
}


/* -------------------------------------------------------------------------- */
/* Public Module event handlers:                                              */
/* -------------------------------------------------------------------------- */

bool tb_url_input( struct gui_window * gw, short nkc )
{
	CMP_TOOLBAR tb = gw->root->toolbar;
	assert(tb!=NULL);
	LGRECT work;
	bool ret = false;

	assert( gw != NULL );

	long ucs4;
	long ik = nkc_to_input_key( nkc, &ucs4 );

	if( ik == 0 ){
		if ( (nkc&0xFF) >= 9 ) {
			ret = textarea_keypress( tb->url.textarea, ucs4  );
		}
	}
	else if( ik == KEY_CR || ik == KEY_NL ){
		char tmp_url[PATH_MAX];
		if( textarea_get_text( tb->url.textarea, tmp_url, PATH_MAX) > 0 ) {
			window_set_focus( gw, BROWSER, gw->browser);
			browser_window_go(gw->browser->bw, (const char*)&tmp_url, 0, true);
			ret = true;
		}
	}
	else if( ik == KEY_COPY_SELECTION ){
		// copy whole text
		char * text;
		int len;
		len = textarea_get_text( tb->url.textarea, NULL, 0 );
		text = malloc( len+1 );
		if( text ){
			textarea_get_text( tb->url.textarea, text, len+1 );
			scrap_txt_write( &app, text );
			free( text );
		}
	}
	else if( ik == KEY_PASTE ){
		char * clip = scrap_txt_read( &app );
		if( clip != NULL ){
			int clip_length = strlen( clip );
			if ( clip_length > 0 ) {
				char *utf8;
				utf8_convert_ret res;
				/* Clipboard is in local encoding so
				 * convert to UTF8 */
				res = utf8_from_local_encoding( clip, clip_length, &utf8 );
				if ( res == UTF8_CONVERT_OK ) {
					tb_url_set( gw, utf8 );
					free(utf8);
					ret = true;
				}
				free( clip );
			}
		}
	}
	else if( ik == KEY_ESCAPE ) {
		textarea_keypress( tb->url.textarea, KEY_SELECT_ALL );
		textarea_keypress( tb->url.textarea, KEY_DELETE_LEFT );
	}
	else {
		ret = textarea_keypress( tb->url.textarea, ik );
	}

	return( ret );
}

void tb_back_click( struct gui_window * gw )
{
	struct browser_window *bw = gw->browser->bw;

	if( history_back_available(bw->history) )
		history_back(bw, bw->history);
}

void tb_reload_click( struct gui_window * gw )
{
	browser_window_reload( gw->browser->bw, true );
}

void tb_forward_click( struct gui_window * gw )
{
	struct browser_window *bw = gw->browser->bw;
	if (history_forward_available(bw->history))
		history_forward(bw, bw->history);
}

void tb_home_click( struct gui_window * gw )
{
	browser_window_go(gw->browser->bw, cfg_homepage_url, 0, true);
}


void tb_stop_click( struct gui_window * gw )
{
	browser_window_stop( gw->browser->bw );
}


void tb_hide( struct gui_window * gw, short mode )
{
	CMP_TOOLBAR tb = gw->root->toolbar;
	assert( tb != NULL );
	if( mode == 1 ){
		tb->hidden = true;
		tb->comp->rect.g_h = 0;
		tb->comp->bounds.max_height = 0;

	} else {
		tb->hidden = false;
		tb->comp->rect.g_h = toolbar_styles[tb->style].height;
		tb->comp->bounds.max_height = toolbar_styles[tb->style].height;
	}
	gw->browser->reformat_pending = true;
	browser_update_rects( gw );
	snd_rdw( gw->root->handle  );
}


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
static OBJECT * throbber_form = NULL;

static const plot_font_style_t font_style_url = {
    .family = PLOT_FONT_FAMILY_SANS_SERIF,
    .size = TOOLBAR_URL_TEXT_SIZE_PT*FONT_SIZE_SCALE,
    .weight = 400,
    .flags = FONTF_NONE,
    .background = 0xffffff,
    .foreground = 0x0
 };

/* prototypes & order for button widgets: */
static struct s_tb_button tb_buttons[] =
{
	{ TOOLBAR_BT_BACK, tb_back_click, NULL },
	{ TOOLBAR_BT_HOME, tb_home_click,  NULL },
	{ TOOLBAR_BT_FORWARD, tb_forward_click,  NULL },
	{ TOOLBAR_BT_RELOAD, tb_reload_click,  NULL },
	{ TOOLBAR_BT_STOP, tb_stop_click,  NULL },
	{ 0, NULL, NULL }
};

static void tb_txt_request_redraw(void *data, int x, int y, int w, int h);

static void __CDECL button_redraw( COMPONENT *c, long buff[8])
{
	OBJECT *tree = (OBJECT*)mt_CompDataSearch( &app, c, CDT_OBJECT );
	struct gui_window * gw = mt_CompDataSearch( &app, c, CDT_OWNER );
	LGRECT work,clip;
	GRECT todo,crect;
	short pxy[4];

	mt_CompGetLGrect(&app, c, WF_WORKXYWH, &work);
	clip = work;
	/* return if component and redraw region does not intersect: */
	if ( !rc_lintersect( (LGRECT*)&buff[4], &clip ) ) {
		return;
	}
	/* clip contains intersecting part: */
	pxy[0] = clip.g_x;
	pxy[1] = clip.g_y;
	pxy[2] = clip.g_w + clip.g_x;
	pxy[3] = clip.g_h + clip.g_y;

	vs_clip( vdih, 1, (short*)&pxy );

	tree->ob_x = work.g_x+1;
	tree->ob_y = work.g_y+2;
	tree->ob_width = work.g_w;
	tree->ob_height = work.g_h;
	vsf_interior( vdih , 1 );
	vsf_color( vdih, LWHITE );
	pxy[0] = (short)buff[4];
	pxy[1] = (short)buff[5];
	pxy[2] = (short)buff[4] + buff[6];
	pxy[3] = MIN( (short)buff[5] + buff[7], work.g_y + work.g_h - 2);
	vswr_mode( vdih, MD_REPLACE);
	v_bar( vdih, (short*)&pxy );

	/* go through the rectangle list, using classic AES methods. */
	/* Windom ComGetLGrect is buggy for WF_FIRST/NEXTXYWH	     */
	crect.g_x = clip.g_x;
	crect.g_y = clip.g_y;
	crect.g_w = clip.g_w;
	crect.g_h = clip.g_h;
	wind_get(gw->root->handle->handle, WF_FIRSTXYWH,
							&todo.g_x, &todo.g_y, &todo.g_w, &todo.g_h );
	while( (todo.g_w > 0) && (todo.g_h > 0) ){

		if( rc_intersect(&crect, &todo) ){
			objc_draw( tree, 0, 0, todo.g_x, todo.g_y, todo.g_w, todo.g_h );
		}
		wind_get(gw->root->handle->handle, WF_NEXTXYWH,
							&todo.g_x, &todo.g_y, &todo.g_w, &todo.g_h );
	}

	if( gw->root->toolbar->buttons[0].comp ==  c && work.g_x == buff[4] ){
		vsl_color( vdih, LWHITE );
		pxy[0] = (short)buff[4];
		pxy[1] = (short)buff[5];
		pxy[2] = (short)buff[4];
		pxy[3] = (short)buff[5] + buff[7];
		v_pline( vdih, 2, (short*) pxy );
	}
	vs_clip( vdih, 0, (short*)&clip );
}

static void __CDECL button_enable( COMPONENT *c, long buff[8])
{
	((OBJECT*)mt_CompDataSearch(&app, c, CDT_OBJECT))->ob_state &= ~OS_DISABLED;
}

static void __CDECL button_disable( COMPONENT *c, long buff[8])
{
	((OBJECT*)mt_CompDataSearch(&app, c, CDT_OBJECT))->ob_state |= OS_DISABLED;
}

static void __CDECL button_click( COMPONENT *c, long buff[8])
{
	int i = 0;
	struct gui_window * gw = (struct gui_window *)mt_CompDataSearch(&app, c, CDT_OWNER);
	assert( gw );
	while( i < gw->root->toolbar->btcnt ) {
		if(c == gw->root->toolbar->buttons[i].comp ) {
			gw->root->toolbar->buttons[i].cb_click( gw );
			break;
		}
		i++;
	}
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


static COMPONENT *button_create( OBJECT *o, short type, long size, short flex )
{
	COMPONENT *c = mt_CompCreate( &app, type, size, flex );
	OBJECT *oc = mt_ObjcNDup( &app, o, NULL, 1);
	oc->ob_next = oc->ob_head = oc->ob_tail = -1;
	mt_CompDataAttach( &app, c, CDT_OBJECT, oc );
	mt_CompEvntAttach( &app, c, WM_REDRAW, button_redraw );
	mt_CompEvntAttach( &app, c, WM_XBUTTON, button_click );
	mt_CompEvntAttach( &app, c, CM_GETFOCUS, button_enable );
	mt_CompEvntAttach( &app, c, CM_LOSEFOCUS, button_disable );
	return c;
}


static
void __CDECL evnt_throbber_redraw( COMPONENT *c, long buff[8])
{
	LGRECT work, clip;
	int idx;
	short pxy[4];
	struct gui_window * gw = (struct gui_window *)mt_CompDataSearch(&app, c, CDT_OWNER);
	if( gw->root->toolbar->throbber.running == false ) {
		idx = THROBBER_INACTIVE_INDEX;
	} else {
		idx = gw->root->toolbar->throbber.index;
		if( idx > THROBBER_MAX_INDEX || idx < THROBBER_MIN_INDEX ) {
			idx = THROBBER_MIN_INDEX;
		}
	}

	mt_CompGetLGrect(&app, c, WF_WORKXYWH, &work);
	clip = work;
	if ( !rc_lintersect( (LGRECT*)&buff[4], &clip ) ) return;

	vsf_interior( vdih , 1 );
	if(app.nplanes > 2 )
		vsf_color( vdih, LWHITE );
	else
		vsf_color( vdih, WHITE );
	pxy[0] = (short)buff[4];
	pxy[1] = (short)buff[5];
	pxy[2] = (short)buff[4] + buff[6]-1;
	pxy[3] = (short)buff[5] + buff[7]-2;
	v_bar( vdih, (short*)&pxy );

	if( throbber_form != NULL ) {
		throbber_form[idx].ob_x = work.g_x+1;
		throbber_form[idx].ob_y = work.g_y+4;
		mt_objc_draw( throbber_form, idx, 8, clip.g_x, clip.g_y, clip.g_w, clip.g_h, app.aes_global );
	}
}

static
void __CDECL evnt_url_redraw( COMPONENT *c, long buff[8] )
{
	LGRECT work, clip;
	struct gui_window * gw;
	short pxy[10];
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
	vsf_color( vdih, LWHITE );

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

			plotter->resize(plotter, work.g_w, work.g_h );
			plotter->move(plotter, work.g_x, work.g_y );
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
	OBJECT * tbut;

	CMP_TOOLBAR t = malloc( sizeof(struct s_toolbar) );
	if( t == NULL )
		return( NULL );

	t->owner = gw;

	/* create the root component: */
	t->comp = (COMPONENT*)mt_CompCreate(&app, CLT_HORIZONTAL, TOOLBAR_HEIGHT, 0);
	t->comp->rect.g_h = TOOLBAR_HEIGHT;
	t->comp->bounds.max_height = TOOLBAR_HEIGHT;
	mt_CompEvntDataAdd(&app, t->comp, WM_REDRAW, evnt_toolbar_redraw, NULL, EV_BOT);

	/* count buttons and add them as components: */
	RsrcGaddr( h_gem_rsrc, R_TREE, TOOLBAR , &tbut );
	t->btdim.g_x = 0;
	t->btdim.g_y = 1;
	t->btdim.g_w = 0;
	t->btdim.g_h = TB_BUTTON_HEIGHT+1;
	i = 0;
	while( tb_buttons[i].rsc_id > 0 ) {
		i++;
	}
	t->btcnt = i;
	t->buttons = malloc( sizeof(struct s_tb_button) * t->btcnt );
	for( i=0; i < t->btcnt; i++ ) {
		t->buttons[i].rsc_id = tb_buttons[i].rsc_id;
		t->buttons[i].cb_click = tb_buttons[i].cb_click;
		t->buttons[i].comp = button_create( &tbut[t->buttons[i].rsc_id] , CLT_VERTICAL, TB_BUTTON_WIDTH+1, 0);
		mt_CompDataAttach( &app, t->buttons[i].comp, CDT_OWNER, gw );
		t->buttons[i].comp->bounds.max_width = TB_BUTTON_WIDTH+1;
		mt_CompAttach( &app, t->comp,  t->buttons[i].comp );
		t->btdim.g_w += TB_BUTTON_WIDTH+1;
	}

	/* create the url widget: */
	t->url.textarea = textarea_create( 300, TOOLBAR_TEXTAREA_HEIGHT, 0,
									&font_style_url, tb_txt_request_redraw,
									t );
	if( t->url.textarea != NULL ){
		textarea_set_text(t->url.textarea, "http://");
	}

	t->url.comp = (COMPONENT*)mt_CompCreate(&app, CLT_HORIZONTAL, TOOLBAR_HEIGHT, 1);
	mt_CompEvntAttach( &app, t->url.comp, WM_REDRAW, evnt_url_redraw );
	mt_CompEvntAttach( &app, t->url.comp, WM_XBUTTON, evnt_url_click );
	mt_CompDataAttach( &app, t->url.comp, CDT_OWNER, gw );
	mt_CompAttach( &app, t->comp, t->url.comp );

	/* create the throbber widget: */
	if( throbber_form == NULL ) {
		RsrcGaddr( h_gem_rsrc, R_TREE, THROBBER , &throbber_form );
		throbber_form->ob_x = 0;
		throbber_form->ob_y = 0;
	}
	t->throbber.comp = (COMPONENT*)mt_CompCreate(&app, CLT_HORIZONTAL, TOOLBAR_HEIGHT, 0);
	t->throbber.comp->rect.g_h = TOOLBAR_HEIGHT;
	t->throbber.comp->rect.g_w = 32;
	t->throbber.comp->bounds.max_height = TOOLBAR_HEIGHT;
	t->throbber.comp->bounds.max_width = 32;
	t->throbber.index = THROBBER_MIN_INDEX;
	t->throbber.max_index = THROBBER_MAX_INDEX;
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
		mt_ObjcFree( &app, (OBJECT*)mt_CompDataSearch(&app, tb->buttons[i].comp, CDT_OBJECT) );
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


void tb_update_buttons( struct gui_window * gw )
{
	struct s_tb_button * bt;
	return; /* not working correctly, buttons disabled, even if it shouldn't */

	bt = find_button( gw, TOOLBAR_BT_BACK );
	if( browser_window_back_available(gw->browser->bw) ) {
		((OBJECT*)mt_CompDataSearch(&app, bt->comp, CDT_OBJECT))->ob_state |= OS_DISABLED;
	} else {
		((OBJECT*)mt_CompDataSearch(&app, bt->comp, CDT_OBJECT))->ob_state &= ~OS_DISABLED;
	}
	mt_CompEvntRedraw( &app, bt->comp );

	bt = find_button( gw, TOOLBAR_BT_FORWARD );
	if( browser_window_forward_available(gw->browser->bw) ) {
		((OBJECT*)mt_CompDataSearch(&app, bt->comp, CDT_OBJECT))->ob_state |= OS_DISABLED;
	} else {
		((OBJECT*)mt_CompDataSearch(&app, bt->comp, CDT_OBJECT))->ob_state &= ~OS_DISABLED;
	}
	mt_CompEvntRedraw( &app, bt->comp );

	bt = find_button( gw, TOOLBAR_BT_RELOAD );
	if( browser_window_reload_available(gw->browser->bw) ) {
		((OBJECT*)mt_CompDataSearch(&app, bt->comp, CDT_OBJECT))->ob_state |= OS_DISABLED;
	} else {
		((OBJECT*)mt_CompDataSearch(&app, bt->comp, CDT_OBJECT))->ob_state &= ~OS_DISABLED;
	}
	mt_CompEvntRedraw( &app, bt->comp );

	bt = find_button( gw, TOOLBAR_BT_STOP );
	if( browser_window_stop_available(gw->browser->bw) ) {
		((OBJECT*)mt_CompDataSearch(&app, bt->comp, CDT_OBJECT))->ob_state |= OS_DISABLED;
	} else {
		((OBJECT*)mt_CompDataSearch(&app, bt->comp, CDT_OBJECT))->ob_state &= ~OS_DISABLED;
	}
	mt_CompEvntRedraw( &app, bt->comp );
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
	tb_update_buttons(gw);
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

	tb_update_buttons(gw);
}

void tb_home_click( struct gui_window * gw )
{
	browser_window_go(gw->browser->bw, cfg_homepage_url, 0, true);
	tb_update_buttons(gw);
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
		tb->comp->rect.g_h = TOOLBAR_HEIGHT;
		tb->comp->bounds.max_height = TOOLBAR_HEIGHT;
	}
	gw->browser->reformat_pending = true;
	browser_update_rects( gw );
	snd_rdw( gw->root->handle  );
}


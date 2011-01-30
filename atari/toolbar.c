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
#include <cflib.h>
#include <assert.h>
#include <math.h>

#include "utils/log.h"
#include "desktop/gui.h"
#include "desktop/history_core.h"
#include "desktop/netsurf.h"
#include "desktop/browser.h"
#include "desktop/mouse.h"
#include "desktop/plotters.h"
#include "atari/clipboard.h"
#include "atari/gui.h"
#include "atari/toolbar.h"
#include "atari/browser_win.h"
#include "atari/clipboard.h"
#include "atari/misc.h"
#include "atari/global_evnt.h"
#include "atari/res/netsurf.rsh"
#include "atari/plot/plotter.h"

extern char * cfg_homepage_url;
extern short vdih;
extern void * h_gem_rsrc;

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

static OBJECT * throbber_form = NULL;


static void __CDECL button_redraw( COMPONENT *c, long buff[8])
{
	OBJECT *tree = (OBJECT*)mt_CompDataSearch( &app, c, CDT_OBJECT );
	struct gui_window * gw = mt_CompDataSearch( &app, c, CDT_OWNER );
	LGRECT work,clip;
	short pxy[4];

	mt_CompGetLGrect(&app, c, WF_WORKXYWH, &work);
	clip = work;
	if ( !rc_lintersect( (LGRECT*)&buff[4], &clip ) ) return;

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
	mt_objc_draw( tree, 0, 8, clip.g_x, clip.g_y, clip.g_w, clip.g_h, app.aes_global );
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
	idx = gw->root->toolbar->throbber.index+1;
	mt_CompGetLGrect(&app, c, WF_WORKXYWH, &work);
	clip = work;
	if ( !rc_lintersect( (LGRECT*)&buff[4], &clip ) ) return;

	vsf_interior( vdih , 1 );
	vsf_color( vdih, LWHITE );
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
	short pxy[10];
	short i;
	short d;
	short mchars;
	struct gui_window * gw = (struct gui_window *)mt_CompDataSearch(&app, c, CDT_OWNER);
	assert( gw != NULL );
	assert( gw->browser != NULL );
	assert( gw->root != NULL );
	assert( gw->browser->bw != NULL );
	CMP_TOOLBAR tb = gw->root->toolbar;

	mt_CompGetLGrect(&app, c, WF_WORKXYWH, &work);
	clip = work;
	if ( !rc_lintersect( (LGRECT*)&buff[4], &clip ) ) return;

	pxy[0] = clip.g_x;
	pxy[1] = clip.g_y;
	pxy[2] = clip.g_w + clip.g_x;
	pxy[3] = clip.g_h + clip.g_y;
	vs_clip( vdih, 1, (short*)&pxy );

	mchars = (work.g_w-6 / tb->url.char_size); /* subtract 6px -> 3px padding around text on each side */

	vswr_mode( vdih, MD_REPLACE);
	vsf_perimeter( vdih, 0 );
	vsf_interior( vdih , 1 );
	vsf_color( vdih, LWHITE );
	vst_point( vdih, 10, &pxy[0], &pxy[1], &pxy[2], &pxy[3] );
	vst_alignment(vdih, 0, 5, &d, &d );
	vst_effects( vdih, 0 );
	vst_color( vdih, BLACK );
	/* gray the whole component: */

	pxy[0] = work.g_x;
	pxy[1] = work.g_y;
	pxy[2] = work.g_x + work.g_w;
	pxy[3] = work.g_y + work.g_h-2;
	v_bar( vdih, (short*)&pxy );

	/* draw outer line, left top: */
	pxy[0] = work.g_x + 2;
	pxy[1] = work.g_y + ((TOOLBAR_HEIGHT - URLBOX_HEIGHT)/2);
	/* right, top: */
	pxy[2] = work.g_x + work.g_w - 4;
	pxy[3] = work.g_y + ((TOOLBAR_HEIGHT - URLBOX_HEIGHT)/2);
	/* right, bottom: */
	pxy[4] = work.g_x + work.g_w - 4;
	pxy[5] = work.g_y + ((TOOLBAR_HEIGHT - URLBOX_HEIGHT)/2) + URLBOX_HEIGHT;
	/* left, bottom: */
	pxy[6] = work.g_x + 2;
	pxy[7] = work.g_y + ((TOOLBAR_HEIGHT - URLBOX_HEIGHT)/2) + URLBOX_HEIGHT;
	/* left, top again: */
	pxy[8] = work.g_x + 2;
	pxy[9] = work.g_y + ((TOOLBAR_HEIGHT - URLBOX_HEIGHT)/2);
	vsf_interior( vdih, FIS_SOLID );
	vsf_style( vdih, 1);
	vsl_color( vdih, BLACK);
	v_pline( vdih, 5, pxy );

	/* draw white txt box: */
	pxy[0] = pxy[0] + 1;
	pxy[1] = pxy[1] + ((TOOLBAR_HEIGHT - URLBOX_HEIGHT)/2) - 1;
	pxy[2] = pxy[2] - 1; 
	pxy[3] = work.g_y + ((TOOLBAR_HEIGHT - URLBOX_HEIGHT)/2)  + URLBOX_HEIGHT ;
	vsf_color( vdih, WHITE);
	v_bar( vdih, pxy );
	if( gw->root->toolbar->url.used  > 1 ) {
		short curx;
		short vqw[4];
		char t[2];
		short cw = 0;

		t[1]=0;
		if( atari_sysinfo.sfont_monospaced ) {
			vqt_width( vdih, t[0], &vqw[0], &vqw[1], &vqw[2] );
			cw = vqw[0];
		}
		for( curx = work.g_x + 3, i=tb->url.scrollx ; (curx < clip.g_x + clip.g_w) && i < MIN(tb->url.used-1, mchars); i++ ){
			t[0] = tb->url.text[i];
			v_gtext( vdih, curx, work.g_y + ((TOOLBAR_HEIGHT - URLBOX_HEIGHT)/2) + 2, (char*)&t );
			if( !atari_sysinfo.sfont_monospaced ) {
				vqt_width( vdih, t[0], &vqw[0], &vqw[1], &vqw[2] );
				curx += vqw[0];
			} else {
				curx += cw;
			}
		}
	}
	
	if( window_url_widget_has_focus( gw ) ) {
		/* draw caret: */
		pxy[0] = 3 + work.g_x + ((tb->url.caret_pos - tb->url.scrollx) * tb->url.char_size);
		pxy[1] = pxy[1] + 1;
		pxy[2] = 3 + work.g_x + ((tb->url.caret_pos - tb->url.scrollx) * tb->url.char_size);
		pxy[3] = pxy[3] - 1 ;
		v_pline( vdih, 2, pxy );
		/* draw selection: */
		if( tb->url.selection_len != 0 ) {
			vswr_mode( vdih, MD_XOR);
			vsl_color( vdih, BLACK);
			pxy[0] = 3 + work.g_x + ((tb->url.caret_pos - tb->url.scrollx) * tb->url.char_size);
			pxy[2] = pxy[0] + ( gw->root->toolbar->url.selection_len * tb->url.char_size);
			v_bar( vdih, pxy );
			vswr_mode( vdih, MD_REPLACE );
		}
	}
	vs_clip( vdih, 0, (short*)&pxy );
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
	mx = evnt.mx - work.g_x;
	my = evnt.my - work.g_y;

	/* TODO: reset mouse state of browser window? */
	/* select whole text when newly focused, otherwise set caret to end of text */
	if( !window_url_widget_has_focus(gw) ) {
		tb_url_place_caret( gw, strlen(tb->url.text), true);
		tb->url.selection_len = -tb->url.caret_pos;
		window_set_focus( gw, URL_WIDGET, (void*)&tb->url );
	} else {
		if( mb & 1 ) {
			/* if the button is dragging, place selection: */
			old = tb->url.selection_len;
			tb->url.selection_len = (tb->url.scrollx + (mx / tb->url.char_size)) -  tb->url.caret_pos;
			if(tb->url.caret_pos + tb->url.selection_len > (int)strlen(tb->url.text) )
				tb->url.selection_len = strlen(tb->url.text) - tb->url.caret_pos;
			if( old == tb->url.selection_len )
				/* avoid redraw when nothing changed */
				return; 
		} else {
			/* TODO: recognize click + shift key */
			tb->url.selection_len = 0;
			tb_url_place_caret( gw, tb->url.scrollx + (mx / tb->url.char_size), true);
		}
	}

	ApplWrite( _AESapid, WM_REDRAW,  gw->root->handle->handle,
		work.g_x, work.g_y, work.g_w, work.g_h );

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
	t->url.char_size = 8;
	t->url.text = malloc( 2*URL_WIDGET_BSIZE );
	strcpy( t->url.text, "http://" );
	t->url.allocated = 2*URL_WIDGET_BSIZE;
	t->url.scrollx = 0;
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
	t->throbber.index = 0;
	t->throbber.max_index = 7;
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
	if( tb->url.text != NULL )
		free( tb->url.text );
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

	if( len+1 > url->allocated ) {
		newsize = (len / (URL_WIDGET_BSIZE-1))+1;
		newtext = realloc( url->text, newsize*URL_WIDGET_BSIZE );
		if(newtext != NULL) {
			url->text = newtext;
			url->allocated = newsize * URL_WIDGET_BSIZE;
		}
	}
	if( len+1 < url->allocated - URL_WIDGET_BSIZE
		 && url->allocated - URL_WIDGET_BSIZE > URL_WIDGET_BSIZE*2 ) {
		newsize = (len / (URL_WIDGET_BSIZE-1) )+1;
		newtext = realloc( url->text, newsize*URL_WIDGET_BSIZE );
		if(newtext != NULL) {
			url->text = newtext;
			url->allocated = newsize * URL_WIDGET_BSIZE;
		}
	}

	strncpy((char*)url->text, text, url->allocated-1 );
	url->used = MIN(len+1,url->allocated );
	tb_url_place_caret( gw, 0, true);
	url->scrollx = 0;
	mt_CompGetLGrect(&app, url->comp, WF_WORKXYWH, &work);
	ApplWrite( _AESapid, WM_REDRAW,  gw->root->handle->handle,
		(short)work.g_x, (short)work.g_y, (short)work.g_w, (short)work.g_h );
}


/* place the caret and adjust scrolling position */
void tb_url_place_caret( struct gui_window * gw, int steps, bool abs)
{
	LGRECT work;
	CMP_TOOLBAR tb = gw->root->toolbar;
	assert(tb!=NULL);
	mt_CompGetLGrect(&app, tb->url.comp, WF_WORKXYWH, &work);
	int ws = (work.g_w / tb->url.char_size)-1; /* widget size in chars */
	if(abs) {
		tb->url.caret_pos = steps;
	} else {
		tb->url.caret_pos = tb->url.caret_pos + steps;
	}
	if( (int)tb->url.caret_pos > (int)strlen(tb->url.text)  )
		tb->url.caret_pos = strlen(tb->url.text);
	if( tb->url.caret_pos > tb->url.allocated-2 )
		tb->url.caret_pos = tb->url.allocated-2;
	if( tb->url.caret_pos < 0)
			tb->url.caret_pos = 0;

	if( tb->url.caret_pos < tb->url.scrollx ) {
		/* the caret has moved out of the widget to the left */
		tb->url.scrollx -= ws;
	}
	if( tb->url.caret_pos > tb->url.scrollx + ws ) {
		/* the caret has moved out of the widget to the right */
		if(!abs)
			tb->url.scrollx += steps;
		else
			tb->url.scrollx = tb->url.caret_pos - ws;
	}
	if(tb->url.scrollx < 0)
		tb->url.scrollx = 0;
}


/* -------------------------------------------------------------------------- */
/* Public Module event handlers:                                              */
/* -------------------------------------------------------------------------- */

bool tb_url_input( struct gui_window * gw, short nkc )
{
	CMP_TOOLBAR tb = gw->root->toolbar;
	assert(tb!=NULL);
	LGRECT work;
	int start = 0;
	int i;
	char * newtext;
	short newsize;
	char backup;
	bool ctrl = (nkc & NKF_CTRL);
	bool shift = (nkc & NKF_SHIFT);
	bool alt = (nkc & NKF_ALT);
	bool ret = (ctrl) ? false : true;
	char code = (nkc & 0xFF);

	assert( gw != NULL );
	/* make sure we navigate within the root window on enter: */
	assert( gw->parent == NULL );

	if( (code == NK_LEFT) && !shift ){
		/* TODO: recognize shift + click */
		tb->url.selection_len = 0;
		tb_url_place_caret( gw, -1, false );
	}
	else if( (code == NK_RIGHT) && !shift ) {
		/* TODO: recognize shift + click */
		tb->url.selection_len = 0;
		tb_url_place_caret( gw, +1, false );
	}
	else if( (ctrl && code == 'C') ) {
		if( tb->url.selection_len != 0 ) {
			char * from;
			char tmp[abs(tb->url.selection_len)+1];
			int len;
			if( tb->url.selection_len < 0 ) {
				from = &tb->url.text[tb->url.caret_pos+tb->url.selection_len];
			} else {
				from = &tb->url.text[tb->url.caret_pos];
			}
			len = MIN( abs(tb->url.selection_len), (int)strlen(from) ) ;
			memcpy(&tmp, from, len);
			tmp[len] = 0;
			int r = scrap_txt_write(&app, (char*)&tmp);
			ret = true;
		}		
	}
	else if(  (ctrl && code == 'V') || code == NK_INS  ) {
		char * clip = scrap_txt_read( &app );
		if( clip != NULL ) {
			size_t l = strlen( clip );
			unsigned int i = 0;
			for( i = 0; i<l; i++) {
				tb_url_input( gw, clip[i] );
			}
			free( clip );
			ret = true;
		}
	}
	else if( (code == NK_DEL) ) {
		if( tb->url.selection_len != 0 ) {
				if( tb->url.selection_len < 0 ) {
					strcpy(
						&tb->url.text[tb->url.caret_pos+tb->url.selection_len],
						&tb->url.text[tb->url.caret_pos]
					);
					tb_url_place_caret( gw, tb->url.selection_len, false );
				} else {
					strcpy(
						&tb->url.text[tb->url.caret_pos], 
						&tb->url.text[tb->url.caret_pos+tb->url.selection_len]
					);
				}
				tb->url.used = strlen( tb->url.text ) + 1;
		} else  {
			if( tb->url.caret_pos < tb->url.used -1) {
				strcpy(
					&tb->url.text[tb->url.caret_pos+tb->url.selection_len], 
					&tb->url.text[tb->url.caret_pos+1]
				);
				tb->url.used--;
			}
		}
		tb->url.selection_len = 0;
	}
	else if( code == NK_BS ) {
		if( tb->url.caret_pos > 0 && 
			tb->url.selection_len != 0 ) {
				if( tb->url.selection_len < 0 ) {
					strcpy(&tb->url.text[tb->url.caret_pos+tb->url.selection_len], &tb->url.text[tb->url.caret_pos]);
					tb_url_place_caret( gw, tb->url.selection_len, false );
				} else {
					strcpy(&tb->url.text[tb->url.caret_pos], &tb->url.text[tb->url.caret_pos+tb->url.selection_len]);
				}
				tb->url.used = strlen( tb->url.text ) + 1;
		} else  {
			tb->url.text[tb->url.caret_pos-1] = 0;
			tb->url.used--;
			strcat(tb->url.text, &tb->url.text[tb->url.caret_pos]);
			tb_url_place_caret( gw , -1, false );
		}
		tb->url.selection_len = 0;
	}
	else if( code == NK_ESC ) {
		tb->url.text[0] = 0;
		tb->url.scrollx = 0;
		tb->url.used = 1;
		tb_url_place_caret( gw, 0, true );
	}
	else if( code == NK_CLRHOME ) {
		tb_url_place_caret( gw, 0, true );
	}
	else if( code == NK_M_END ) {
		tb_url_place_caret( gw, 
			strlen((char*)&tb->url.text)-1, 
			true 
		);
	}
	else if( code == NK_ENTER || code == NK_RET ) {
		tb_url_place_caret( gw, 0, true );
		window_set_focus( gw, BROWSER, gw->browser->bw);
		browser_window_go(gw->browser->bw, (const char*)tb->url.text, 0, true);
	}
	else if( code > 30 ) {
		if( tb->url.used+1 > tb->url.allocated ){
			newsize = ( (tb->url.used+1) / (URL_WIDGET_BSIZE-1))+1;
			newtext = realloc(tb->url.text, newsize*URL_WIDGET_BSIZE );
			if(newtext) {
				tb->url.text = newtext;
				tb->url.allocated = newsize * URL_WIDGET_BSIZE;
			}
		}
		i = tb->url.caret_pos;
		backup = tb->url.text[tb->url.caret_pos];
		while( i < tb->url.allocated - 1) {
			tb->url.text[i] = code;
			if( tb->url.text[i] == (char)0 )
				break;
			code = backup;
			i++;
			backup = tb->url.text[i];
		}
		tb->url.used++;
		tb->url.text[tb->url.allocated-1] = 0;
		tb_url_place_caret( gw, +1, false );
		tb->url.selection_len = 0;
	} else {
		ret = false;
	}
	if(tb->url.used < 1)
		tb->url.used = 1; /* at least one byte (0) is used */
	mt_CompGetLGrect(&app, tb->url.comp, WF_WORKXYWH, &work);
	ApplWrite( _AESapid, WM_REDRAW,  gw->root->handle->handle,
		work.g_x, work.g_y, work.g_w, work.g_h );
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


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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windom.h>
#include <cflib.h>

#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/plotters.h"
#include "desktop/textinput.h"
#include "desktop/tree.h"
#include "desktop/tree_url_node.h"
#include "desktop/textinput.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "atari/gui.h"
#include "atari/treeview.h"
#include "atari/plot.h"
#include "atari/misc.h"

extern int mouse_hold_start[3];
extern browser_mouse_state bmstate;
extern short last_drag_x;
extern short last_drag_y;
extern GEM_PLOTTER plotter;

static void atari_treeview_resized(struct tree *tree,int w,int h,void *pw);
static void atari_treeview_scroll_visible(int y, int h, void *pw);
static void atari_treeview_get_dimensions(int *width, int *height,void *pw);

static const struct treeview_table atari_tree_callbacks = {
	atari_treeview_request_redraw,
	atari_treeview_resized,
	atari_treeview_scroll_visible,
	atari_treeview_get_dimensions
};

static void __CDECL evnt_tv_keybd( WINDOW *win, short buff[8], void * data )
{
	bool r=false;
	long kstate = 0;
	long kcode = 0;
	unsigned short nkc = 0;
	unsigned short nks = 0;
	unsigned char ascii;
	NSTREEVIEW tv = (NSTREEVIEW) data;
	kstate = evnt.mkstate;
	kcode = evnt.keybd;
	nkc= gem_to_norm( (short)kstate, (short)kcode );
	ascii = (nkc & 0xFF);
	nkc = (nkc & (NKF_CTRL|NKF_SHIFT|0xFF));
	if( (nkc & (NKF_SHIFT|NKF_CTRL) ) == 0 ) {
		switch( ascii ) {
			case NK_BS:
				r = tree_keypress( tv->tree, KEY_DELETE_LEFT);
			break;

			case NK_DEL:
				r = tree_keypress( tv->tree, KEY_DELETE_RIGHT);
			break;

			case NK_ENTER:
				r = tree_keypress( tv->tree, KEY_NL);
			break;

			case NK_RET:
				r = tree_keypress( tv->tree, KEY_CR);
			break;

			case NK_ESC:
				r = tree_keypress( tv->tree, KEY_ESCAPE);		
			break;

			case NK_RIGHT:
				r = tree_keypress( tv->tree, KEY_RIGHT );
			break;	

			case NK_LEFT:
				r = tree_keypress( tv->tree, KEY_LEFT );
			break;

			default:
			break;
		}
	}
	if ( r == false && ( (nkc & NKF_CTRL)==0)  ) {
		int ucs4 = atari_to_ucs4(ascii);
		tree_keypress( tv->tree, ucs4 );
	}	
}


static void __CDECL evnt_tv_redraw( WINDOW *win, short buff[8], void * data )
{
	GRECT work, clip;
	NSTREEVIEW tv = (NSTREEVIEW) data;
	if( tv == NULL )
		return;
	WindGetGrect( win, WF_WORKXYWH, &work );
	clip = work;
	if ( !rc_intersect( (GRECT*)&buff[4], &clip ) ) return;
	clip.g_x -= work.g_x;
	clip.g_y -= work.g_y;
	if( clip.g_x < 0 ) {
		clip.g_w = work.g_w + clip.g_x;
		clip.g_x = 0;
	}
	if( clip.g_y < 0 ) {
		clip.g_h = work.g_h + clip.g_y;
		clip.g_y = 0;
	}
	if( clip.g_h > 0 && clip.g_w > 0 ) { 
		atari_treeview_request_redraw( win->xpos*win->w_u + clip.g_x, win->ypos*win->h_u + clip.g_y, 
			clip.g_w, clip.g_h, tv
		);
	}
}


static void __CDECL evnt_tv_mbutton( WINDOW *win, short buff[8], void * data )
{
	GRECT work;
	bool ignore=false;
	NSTREEVIEW tv = (NSTREEVIEW) data;
	if( tv == NULL )
		return;
	if( evnt.mbut & 2 ) {
		/* do not handle right click */
		return;
	}

	WindGetGrect( tv->window, WF_WORKXYWH, &work );
	int rx = (evnt.mx-work.g_x)+(win->xpos*win->w_u);
	int ry = (evnt.my-work.g_y)+(win->ypos*win->h_u);

	if( rx >= 0 && ry >= 0 && evnt.mx < work.g_x + work.g_w &&
		evnt.my < work.g_y + work.g_h ){

		short mx, my, dummy, mbut;
		uint32_t tnow = clock()*1000 / CLOCKS_PER_SEC;

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

		graf_mkstate(&dummy, &dummy, &mbut, &dummy);
		if( evnt.nb_click == 2 ) {
			bmstate =  BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_DOUBLE_CLICK;
			mouse_hold_start[0] = 0;
		} else {
			if( mbut & 1 ) {
				bmstate |= BROWSER_MOUSE_DRAG_ON;
				if( mouse_hold_start[0] == 0) {
					mouse_hold_start[0] = tnow;
					tv->startdrag.x = rx;
					tv->startdrag.y = ry;
					bmstate |=  BROWSER_MOUSE_DRAG_1;
					gem_set_cursor(&gem_cursors.cross);
				} else {
					/* todo: add more isual indication (grafbox?) */
					ignore = true;		
					gem_set_cursor(&gem_cursors.cross);	
				}
			} else {
				if( bmstate & BROWSER_MOUSE_DRAG_ON ){
					bmstate = 0;
					tree_drag_end(tv->tree, bmstate, tv->startdrag.x, tv->startdrag.y, rx, ry);
					gem_set_cursor(&gem_cursors.arrow);
					ignore = true;
				} else {
					bmstate =  BROWSER_MOUSE_CLICK_1;	
					mouse_hold_start[0] = 0;
				}
			}
		}
		if( !ignore ) {
			tree_mouse_action(tv->tree, bmstate, rx, ry );
		}
		bmstate &= ~(BROWSER_MOUSE_DOUBLE_CLICK | BROWSER_MOUSE_CLICK_1 );
	} 
}

static void __CDECL evnt_tv_m1( WINDOW *win, short buff[8], void * data)
{
	GRECT work;
	NSTREEVIEW tv = (NSTREEVIEW) data;
	if( tv == NULL )
		return;

	if( bmstate & BROWSER_MOUSE_DRAG_ON ) {

		short mbut, dummy;
		graf_mkstate(&dummy, &dummy, &mbut, &dummy);
		if( !(mbut & 1) ){
			WindGetGrect( tv->window, WF_WORKXYWH, &work );
			int rx = (evnt.mx-work.g_x)+(win->xpos*win->w_u);
			int ry = (evnt.my-work.g_y)+(win->ypos*win->h_u);
			bmstate = 0;
			tree_drag_end(tv->tree, bmstate, tv->startdrag.x, tv->startdrag.y, rx, ry);
			gem_set_cursor(&gem_cursors.arrow);
		}
	}	
}

NSTREEVIEW atari_treeview_create( uint32_t flags, WINDOW *win ) 
{
	if( win == NULL )
		return( NULL );
	NSTREEVIEW new = malloc(sizeof(struct atari_treeview));
	if (new == NULL)
		return NULL;
	memset( new, 0, sizeof(struct atari_treeview));
	new->tree = tree_create(flags, &atari_tree_callbacks, new);
	if (new->tree == NULL) {
		free(new);
		return NULL;
	}
	new->window = win;

	win->w_u = 16;
	win->h_u = 16;
	
	EvntDataAdd( new->window, WM_XBUTTON, evnt_tv_mbutton, new, EV_BOT );
	EvntDataAttach( new->window, WM_REDRAW, evnt_tv_redraw, new );
	EvntDataAttach( new->window, WM_XKEYBD, evnt_tv_keybd, new );
	EvntDataAttach( new->window, WM_XM1, evnt_tv_m1, new );

	return(new);
}

void atari_treeview_open( NSTREEVIEW tv )
{
	if( tv->window != NULL ) {
		tree_set_redraw(tv->tree, true);
	}
}

void atari_treeview_close( NSTREEVIEW tv )
{
	if( tv->window != NULL ) {
		tree_set_redraw(tv->tree, false);
	}
}

void atari_treeview_destroy( NSTREEVIEW tv ) 
{
	if( tv != NULL ){
		tv->disposing = true;
		LOG(("tree: %p", tv));
		if( tv->tree != NULL ) {
			tree_delete(tv->tree);
			tv->tree = NULL;
		}
		free( tv );
	}
}

bool atari_treeview_mevent( NSTREEVIEW tv, browser_mouse_state bms, int x, int y)
{
	if( tv == NULL )
		return ( false );
	GRECT work;
	WindGetGrect( tv->window, WF_WORKXYWH, &work );
	int rx = (x-work.g_x)+(tv->window->xpos*tv->window->w_u);
	int ry = (y-work.g_y)+(tv->window->ypos*tv->window->h_u);
	tree_mouse_action(tv->tree, bms, rx, ry );
	tv->click.x = rx;
	tv->click.y = ry;
	return( true );
}



void atari_treeview_redraw( NSTREEVIEW tv)
{
	if (tv != NULL) {
		if( tv->redraw && ((plotter->flags & PLOT_FLAG_OFFSCREEN) == 0) ) {
			short todo[4];
			GRECT work;
			WindGetGrect( tv->window, WF_WORKXYWH, &work );

			plotter->resize(plotter, work.g_w, work.g_h);
			plotter->move(plotter, work.g_x, work.g_y );

			todo[0] = work.g_x;
			todo[1] = work.g_y;
			todo[2] = todo[0] + work.g_w-1;
			todo[3] = todo[1] + work.g_h-1;
			vs_clip(plotter->vdi_handle, 1, (short*)&todo );
			
			if( wind_get(tv->window->handle, WF_FIRSTXYWH, 
							&todo[0], &todo[1], &todo[2], &todo[3] )!=0 ) {
				while (todo[2] && todo[3]) {
					
					/* convert screen to treeview coords: */
					todo[0] = todo[0] - work.g_x + tv->window->xpos*tv->window->w_u;
					todo[1] = todo[1] - work.g_y + tv->window->ypos*tv->window->h_u;
					if( todo[0] < 0 ){
						todo[2] = todo[2] + todo[0];
						todo[0] = 0;
					}
					if( todo[1] < 0 ){
						todo[3] = todo[3] + todo[1];
						todo[1] = 0;
					}

					if (rc_intersect((GRECT *)&tv->rdw_area,(GRECT *)&todo)) {
						tree_draw(tv->tree, -tv->window->xpos*16, -tv->window->ypos*16, 
							todo[0], todo[1], todo[2], todo[3] 
						);
					}
					if (wind_get(tv->window->handle, WF_NEXTXYWH, 
							&todo[0], &todo[1], &todo[2], &todo[3])==0) {
						break;
					}
				}
			} else {
				return;
			}
			vs_clip(plotter->vdi_handle, 0, (short*)&todo);
			tv->redraw = false;
			tv->rdw_area.g_x = 65000;
			tv->rdw_area.g_y = 65000;
			tv->rdw_area.g_w = -1;
			tv->rdw_area.g_h = -1;
		} else {
			/* just copy stuff from the offscreen buffer */
		}
	}
}


/**
 * Callback to force a redraw of part of the treeview window.
 *
 * \param  x		Min X Coordinate of area to be redrawn.
 * \param  y		Min Y Coordinate of area to be redrawn.
 * \param  width	Width of area to be redrawn.
 * \param  height	Height of area to be redrawn.
 * \param  pw		The treeview object to be redrawn.
 */
void atari_treeview_request_redraw(int x, int y, int w, int h, void *pw)
{
	if (pw != NULL) {
		NSTREEVIEW tv = (NSTREEVIEW) pw;
		if( tv->redraw == false ){
			tv->redraw = true;
			tv->rdw_area.g_x = x;
			tv->rdw_area.g_y = y;
			tv->rdw_area.g_w = w;
			tv->rdw_area.g_h = h;
		} else {
			/* merge the redraw area to the new area.: */
			int newx1 = x+w;
			int newy1 = y+h;
			int oldx1 = tv->rdw_area.g_x + tv->rdw_area.g_w;
			int oldy1 = tv->rdw_area.g_y + tv->rdw_area.g_h;
			tv->rdw_area.g_x = MIN(tv->rdw_area.g_x, x);
			tv->rdw_area.g_y = MIN(tv->rdw_area.g_y, y);
			tv->rdw_area.g_w = ( oldx1 > newx1 ) ? oldx1 - tv->rdw_area.g_x : newx1 - tv->rdw_area.g_x;  
			tv->rdw_area.g_h = ( oldy1 > newy1 ) ? oldy1 - tv->rdw_area.g_y : newy1 - tv->rdw_area.g_y;
		}
	}
}


/**
 * Callback to notify us of a new overall tree size.
 *
 * \param  tree		The tree being resized.
 * \param  width	The new width of the window.
 * \param  height	The new height of the window.
 * \param  *pw		The treeview object to be resized.
 */

void atari_treeview_resized(struct tree *tree, int width, int height, void *pw)
{
	if (pw != NULL) {
		NSTREEVIEW tv = (NSTREEVIEW) pw;
		if( tv->disposing )
			return;
		tv->extent.x = width;
		tv->extent.y = height;
		tv->window->ypos_max = (height / tv->window->w_u)+0.5;
		tv->window->xpos_max = (width / tv->window->h_u)+0.5;
		WindSlider( tv->window, HSLIDER|VSLIDER );
	}
}


/**
 * Callback to request that a section of the tree is scrolled into view.
 *
 * \param  y			The Y coordinate of top of the area in NS units.
 * \param  height		The height of the area in NS units.
 * \param  *pw			The treeview object affected.
 */

void atari_treeview_scroll_visible(int y, int height, void *pw)
{
	/* we don't support dragging within the treeview */
	/* so we don't need to implement this */
}

/**
 * Callback to return the tree window dimensions to the treeview system.
 *
 * \param  *width		Return the window width.
 * \param  *height		Return the window height.
 * \param  *pw			The treeview object to use.
 */

void atari_treeview_get_dimensions(int *width, int *height,
		void *pw)
{
	if (pw != NULL && (width != NULL || height != NULL)) {
		NSTREEVIEW tv = (NSTREEVIEW) pw;
		GRECT work;
		WindGetGrect( tv->window, WF_WORKXYWH, &work );
		*width = work.g_w;
		*height = work.g_h;
	}
}


/**
 * Translates a content_type to the name of a respective icon
 *
 * \param content_type	content type
 * \param buffer	buffer for the icon name
 */
void tree_icon_name_from_content_type(char *buffer, content_type type)
{
	switch (type) {
		case CONTENT_HTML:
		case CONTENT_TEXTPLAIN:
		case CONTENT_CSS:
		case CONTENT_IMAGE:
		default:
			strcpy( buffer, "content.png" );
			break;
	}
}

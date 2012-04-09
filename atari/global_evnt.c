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
#include "desktop/hotlist.h"
#include "desktop/save_complete.h"
#include "desktop/options.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"

#include "atari/gui.h"
#include "atari/browser_win.h"
#include "atari/toolbar.h"
#include "atari/browser.h"
#include "atari/hotlist.h"
#include "atari/misc.h"
#include "atari/global_evnt.h"
#include "atari/browser_win.h"
#include "atari/res/netsurf.rsh"
#include "atari/search.h"
#include "atari/findfile.h"
#include "atari/settings.h"
#include "cflib.h"

extern const char * cfg_homepage_url;
extern struct gui_window *input_window;
extern OBJECT * 	h_gem_menu;
extern int mouse_click_time[3];
extern int mouse_hold_start[3];
extern browser_mouse_state bmstate;
extern short last_drag_x;
extern short last_drag_y;
extern bool html_redraw_debug;

/* Zero based resource tree ids: */
#define T_ABOUT 0
#define T_FILE MAINMENU_T_FILE - MAINMENU_T_FILE + 1
#define T_EDIT MAINMENU_T_EDIT - MAINMENU_T_FILE + 1
#define T_VIEW MAINMENU_T_VIEW - MAINMENU_T_FILE + 1
#define T_NAV	MAINMENU_T_NAVIGATE - MAINMENU_T_FILE + 1
#define T_UTIL MAINMENU_T_UTIL - MAINMENU_T_FILE + 1
#define T_HELP MAINMENU_T_NAVIGATE - MAINMENU_T_FILE + 1
/* Count of the above defines: */
#define NUM_MENU_TITLES 7

/* Global event handlers: */
static void __CDECL global_evnt_apterm( WINDOW * win, short buff[8] );
static void __CDECL global_evnt_menu( WINDOW * win, short buff[8] );
static void __CDECL global_evnt_m1( WINDOW * win, short buff[8] );
static void __CDECL global_evnt_keybd( WINDOW * win, short buff[8],void * data);

/* Menu event handlers: */
static void __CDECL menu_about(WINDOW *win, int item, int title, void *data);


/* Menu event handlers: */
static void __CDECL menu_about(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	char buf[PATH_MAX];
	strcpy((char*)&buf, "file://");
	strncat((char*)&buf, (char*)"./doc/README.TXT", PATH_MAX - (strlen("file://")+1) );
	browser_window_create((char*)&buf, 0, 0, true, false);
}

static void __CDECL menu_new_win(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	browser_window_create(cfg_homepage_url, 0, 0, true, false);
}

static void __CDECL menu_open_url(WINDOW *win, int item, int title, void *data)
{
	struct gui_window * gw;
	struct browser_window * bw ;
	LOG(("%s", __FUNCTION__));

	gw = input_window;
	if( gw == NULL ) {
		bw = browser_window_create("", 0, 0, true, false);
		gw = bw->window;

	}
	/* Loose focus: */
	window_set_focus( gw, WIDGET_NONE, NULL );

	/* trigger on-focus event (select all text): */
	window_set_focus( gw, URL_WIDGET, &gw->root->toolbar->url );

	/* delete selection: */
	tb_url_input( gw, NK_DEL );
}

static void __CDECL menu_open_file(WINDOW *win, int item, int title, void *data)
{
	struct gui_window * gw;
	struct browser_window * bw ;

	LOG(("%s", __FUNCTION__));

	const char * filename = file_select( messages_get("OpenFile"), "" );
	if( filename != NULL ){
		char * url = local_file_to_url( filename );
		if( url ){
			bw = browser_window_create(url, NULL, NULL, true, false);
			free( url );
		}
	}
}

static void __CDECL menu_close_win(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window == NULL )
		return;
	gui_window_destroy( input_window );
}

static void __CDECL menu_save_page(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	static bool init = true;
	bool is_folder=false;
	const char * path;

	if( !input_window )
		return;

	if( init ){
		init = false;
		save_complete_init();
	}

	do {
		// TODO: localize string
		path = file_select( "Select folder", "" );
		if( path ) {
			printf("testing: %s\n", path );
			// dumb check if the selection is an folder:
			/*FILE * fp;
			fp = fopen( path, "r" );
			if( !fp ){
				is_folder = true;
			} else {
				fclose( fp );
				form_alert(1, "[1][Please select an folder or abort!][OK]");
			}
			*/
			is_folder = true;
		}
	} while( !is_folder && path != NULL );

	if( path != NULL ){
		save_complete( input_window->browser->bw->current_content, path );
	}

}

static void __CDECL menu_quit(WINDOW *win, int item, int title, void *data)
{
	short buff[8];
	memset( &buff, 0, sizeof(short)*8 );
	LOG(("%s", __FUNCTION__));
	global_evnt_apterm( NULL, buff  );
}

static void __CDECL menu_cut(WINDOW *win, int item, int title, void *data)
{
	if( input_window != NULL )
		browser_window_key_press( input_window->browser->bw, KEY_CUT_SELECTION);
}

static void __CDECL menu_copy(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window != NULL )
		browser_window_key_press( input_window->browser->bw, KEY_COPY_SELECTION);
}

static void __CDECL menu_paste(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window != NULL )
		browser_window_key_press( input_window->browser->bw, KEY_PASTE);
}

static void __CDECL menu_find(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window != NULL )
		open_browser_search( input_window );
}

static void __CDECL menu_choices(WINDOW *win, int item, int title, void *data)
{
	static WINDOW * settings_dlg = NULL;
	LOG(("%s", __FUNCTION__));
	settings_dlg = open_settings();
}

static void __CDECL menu_stop(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window == NULL )
		return;
	tb_stop_click( input_window );

}

static void __CDECL menu_reload(WINDOW *win, int item, int title, void *data)
{
	if( input_window == NULL)
		return;
	tb_reload_click( input_window );
	LOG(("%s", __FUNCTION__));
}

static void __CDECL menu_toolbars(WINDOW *win, int item, int title, void *data)
{
	static int state = 0;
	LOG(("%s", __FUNCTION__));
	if( input_window != null && input_window->root->toolbar != null ){
		state = !state;
		tb_hide( input_window, state );
	}
}

static void __CDECL menu_savewin(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
}

static void __CDECL menu_debug_render(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	html_redraw_debug = !html_redraw_debug;
	if( input_window != NULL ) {
		if ( input_window->browser != NULL && input_window->browser->bw != NULL) {
			LGRECT rect;
			browser_get_rect( input_window, BR_CONTENT, &rect );
			browser_window_reformat(input_window->browser->bw, false,
									rect.g_w, rect.g_h );
			MenuIcheck(NULL, MAINMENU_M_DEBUG_RENDER,
						(html_redraw_debug) ? 1 : 0 );
		}
	}
}

static void __CDECL menu_fg_images(WINDOW *win, int item, int title, void *data)
{
	nsoption_set_bool(foreground_images, !nsoption_bool(foreground_images));
	MenuIcheck( NULL, MAINMENU_M_FG_IMAGES, (nsoption_bool(foreground_images)) ? 1 : 0);
}

static void __CDECL menu_bg_images(WINDOW *win, int item, int title, void *data)
{
	nsoption_set_bool(background_images, !nsoption_bool(background_images));
	MenuIcheck( NULL, MAINMENU_M_BG_IMAGES, (nsoption_bool(background_images)) ? 1 : 0);
}

static void __CDECL menu_back(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window == NULL )
		return;
	tb_back_click( input_window );
}

static void __CDECL menu_forward(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window == NULL )
		return;
	tb_forward_click( input_window );
}

static void __CDECL menu_home(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window == NULL )
		return;
	tb_home_click( input_window );
}

static void __CDECL menu_lhistory(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window == NULL )
		return;
}

static void __CDECL menu_ghistory(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	char buf[PATH_MAX];
	strcpy((char*)&buf, "file://");
	strncat((char*)&buf, nsoption_charp(url_file), PATH_MAX - (strlen("file://")+1) );
	browser_window_create((char*)&buf, 0, 0, true, false);
}

static void __CDECL menu_add_bookmark(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window ) {
		if( input_window->browser->bw->current_content != NULL ){
			atari_hotlist_add_page(
				nsurl_access(hlcache_handle_get_url( input_window->browser->bw->current_content)),
				NULL
			);
		}
	}
}

static void __CDECL menu_bookmarks(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	hotlist_open();
}

static void __CDECL menu_vlog(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
	verbose_log = !verbose_log;
	MenuIcheck(NULL, MAINMENU_M_VLOG, (verbose_log) ? 1 : 0 );
}

static void __CDECL menu_help_content(WINDOW *win, int item, int title, void *data)
{
	LOG(("%s", __FUNCTION__));
}

static struct s_menu_item_evnt menu_evnt_tbl[] =
{
	{T_ABOUT,MAINMENU_M_ABOUT, "About", menu_about, {0,0,0}, NULL },
	{T_FILE, MAINMENU_M_NEWWIN, "NewWindow", menu_new_win, {0,0,0}, NULL},
	{T_FILE, MAINMENU_M_OPENURL, "OpenURL", menu_open_url, {'G',0,K_CTRL}, NULL},
	{T_FILE, MAINMENU_M_OPENFILE, "OpenFile", menu_open_file, {'O',0,K_CTRL}, NULL},
	{T_FILE, MAINMENU_M_CLOSEWIN, "CloseWindow", menu_close_win, {0,0,0}, NULL},
	{T_FILE, MAINMENU_M_SAVEPAGE, "Save", menu_save_page, {0,NK_F3,0}, NULL},
	{T_FILE, MAINMENU_M_QUIT, "Quit", menu_quit, {'Q',0,K_CTRL}, NULL},
	{T_EDIT, MAINMENU_M_CUT, "Cut", menu_cut, {'X',0,K_CTRL}, NULL},
	{T_EDIT, MAINMENU_M_COPY, "Copy", menu_copy, {'C',0,K_CTRL}, NULL},
	{T_EDIT, MAINMENU_M_PASTE, "Paste", menu_paste, {'V',0,K_CTRL}, NULL},
	{T_EDIT, MAINMENU_M_FIND, "FindText", menu_find, {0,NK_F4,0}, NULL},
	{T_VIEW, MAINMENU_M_RELOAD, "Reload", menu_reload, {0,NK_F5,0}, NULL},
	{T_VIEW, MAINMENU_M_TOOLBARS, "Toolbars", menu_toolbars, {0,NK_F1,K_CTRL}, NULL},
	{T_VIEW, MAINMENU_M_SAVEWIN, "", menu_savewin, {0,0,0}, NULL},
	{T_VIEW, MAINMENU_M_DEBUG_RENDER, "", menu_debug_render, {0,0,0}, NULL},
	{T_VIEW, MAINMENU_M_FG_IMAGES, "", menu_fg_images, {0,0,0}, NULL},
	{T_VIEW, MAINMENU_M_BG_IMAGES, "", menu_bg_images, {0,0,0}, NULL},
	{T_VIEW, MAINMENU_M_STOP, "Stop", menu_stop, {0,NK_ESC,K_ALT}, NULL},
	{T_NAV, MAINMENU_M_BACK, "Back", menu_back, {0,NK_LEFT,K_ALT}, NULL},
	{T_NAV, MAINMENU_M_FORWARD, "Forward", menu_forward, {0,NK_RIGHT,K_ALT}, NULL},
	{T_NAV, MAINMENU_M_HOME, "Home", menu_home, {0,NK_CLRHOME,0}, NULL},
	{T_UTIL, MAINMENU_M_LHISTORY, "HistLocal", menu_lhistory, {0,NK_F7,0}, NULL},
	{T_UTIL, MAINMENU_M_GHISTORY, "HistGlobal", menu_ghistory, {0,NK_F7,K_CTRL}, NULL},
	{T_UTIL, MAINMENU_M_ADD_BOOKMARK, "HotlistAdd", menu_add_bookmark, {'D',0,K_CTRL}, NULL},
	{T_UTIL, MAINMENU_M_BOOKMARKS, "HotlistShow", menu_bookmarks, {0,NK_F6,0}, NULL},
	{T_UTIL, MAINMENU_M_CHOICES, "Choices", menu_choices, {0,0,0}, NULL},
	{T_UTIL, MAINMENU_M_VLOG, "Verbose Log", menu_vlog, {'V',0,K_ALT}, NULL},
	{T_HELP, MAINMENU_M_HELP_CONTENT, "Help", menu_help_content, {0,NK_F1,0}, NULL},
	{T_HELP, -1, "", NULL,{0,0,0}, NULL }
};

void __CDECL global_evnt_apterm( WINDOW * win, short buff[8] )
{
	int i = 0;
	LOG((""));
	netsurf_quit = true;
}


static void __CDECL global_evnt_m1( WINDOW * win, short buff[8] )
{
	struct gui_window * gw = input_window;
	static bool prev_url = false;
	static short prev_x=0;
	static short prev_y=0;
	bool within = false;
	LGRECT urlbox, bwbox, sbbox;
	int nx, ny;

	if( gw == NULL)
		return;

	if( prev_x == evnt.mx && prev_y == evnt.my ){
		return;
	}

	short ghandle = wind_find( evnt.mx, evnt.my );
	if( input_window->root->handle->handle == ghandle ){

		// The window found at x,y is an gui_window
		// and it's the input window.

		browser_get_rect( gw, BR_CONTENT, &bwbox );

		if( evnt.mx > bwbox.g_x && evnt.mx < bwbox.g_x + bwbox.g_w &&
			evnt.my > bwbox.g_y &&  evnt.my < bwbox.g_y + bwbox.g_h ){
			within = true;
			browser_window_mouse_track(
							input_window->browser->bw,
							0,
							evnt.mx - bwbox.g_x + gw->browser->scroll.current.x,
							evnt.my - bwbox.g_y + gw->browser->scroll.current.y
						);
		}

		if( gw->root->toolbar && within == false ) {
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
	char sascii;
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
	sascii = keybd2ascii( evnt.keybd, K_LSHIFT);
	while( menu_evnt_tbl[i].rid != -1 && done == false) {
		if(menu_evnt_tbl[i].nsid[0] != 0 ) {
			if( kstate == menu_evnt_tbl[i].accel.mod && menu_evnt_tbl[i].accel.ascii != 0) {
				if( menu_evnt_tbl[i].accel.ascii == sascii) {
					menu_evnt_tbl[i].menu_func( NULL, menu_evnt_tbl[i].rid, MAINMENU, buff);
					done = true;
					break;
				}
			} else {
				/* the accel code hides in the keycode: */
				if( menu_evnt_tbl[i].accel.keycode != 0) {
					if( menu_evnt_tbl[i].accel.keycode == (nkc & 0xFF) &&
						kstate == menu_evnt_tbl[i].accel.mod &&
						menu_evnt_tbl[i].menu_func != NULL) {
							menu_evnt_tbl[i].menu_func( NULL,
								menu_evnt_tbl[i].rid,
								MAINMENU, buff
							);
							done = true;
							break;
					}
				}
			}
		}
		i++;
	}
}

/*
	Parse encoded menu key shortcut

	The format is:

	"[" 		-   marks start of the shortcut
	"@,^,<" 	-   If the keyshortcut is only valid
					with modifier keys, one of these characters must directly
					follow the start mark.
					Meaning:
					@ -> Alternate
					^ -> Control
	"#"			-   keycode or ascii character.
					The value is handled as keycode if the character value
					is <= 28 ( Atari chracter table )
					or if it is interpreted as function key string.
					(strings: F1 - F10)

*/
static void register_menu_str( struct s_menu_item_evnt * mi )
{
	char * str = ObjcString( h_gem_menu, mi->rid, NULL );
	int l = strlen(str);
	int i = l;
	int x = -1;
	struct s_accelerator * accel = &mi->accel;



	while( i>2 ){
		if( str[i] == '['){
			x = i;
			break;
		}
		i--;
	}
	if( x > -1 ){
		mi->menustr = malloc( l+1 );
		strcpy(mi->menustr, str );
		mi->menustr[x]=' ';
		x++;
		if( str[x] == '@' ){
			accel->mod = K_ALT;
			mi->menustr[x] = 0x07;
			x++;
		}
		else if( str[x] == '^' ) {
			accel->mod = K_CTRL;
			x++;
		}
		if( str[x] <= 28 ){
			// parse symbol
			unsigned short keycode=0;
			switch( str[x] ){
					case 0x03:
					accel->keycode = NK_RIGHT;
				break;
					case 0x04:
					accel->keycode = NK_LEFT;
				break;
					case 0x1B:
					accel->keycode = NK_ESC;
				break;
					default:
				break;
			}
		} else {
			if(str[x] == 'F' && ( str[x+1] >= '1' && str[x+1] <= '9') ){
				// parse function key
				short fkey = atoi(  &str[x+1] );
				if( (fkey >= 0) && (fkey <= 10) ){
					accel->keycode = NK_F1 - 1 + fkey;
				}
			} else {
				accel->ascii = str[x];
			}
		}
	}
}


void __CDECL global_evnt_menu( WINDOW * win, short buff[8] )
{
	int title = buff[ 3];
	INT16 x,y;
	char *str;
	struct gui_window * gw = window_list;
	int i=0;
	MenuTnormal( NULL, title, 1);
	while( gw ) {
		window_set_focus( gw, WIDGET_NONE, NULL  );
		gw = gw->next;
	}
	while( menu_evnt_tbl[i].rid != -1) {
		if( menu_evnt_tbl[i].rid ==  buff[4] ) {
			menu_evnt_tbl[i].menu_func(win, (int)buff[4], (int)buff[3], NULL );
			break;
		}
		i++;
	}
}

void main_menu_update( void )
{
	MenuIcheck( NULL, MAINMENU_M_DEBUG_RENDER, (html_redraw_debug) ? 1 : 0);
	MenuIcheck( NULL, MAINMENU_M_FG_IMAGES, (nsoption_bool(foreground_images)) ? 1 : 0);
	MenuIcheck( NULL, MAINMENU_M_BG_IMAGES, (nsoption_bool(background_images)) ? 1 : 0);
}


/* Bind global and menu events to event handler functions, create accelerators */
void bind_global_events( void )
{
	int i, len;
	int maxlen[NUM_MENU_TITLES]={0};
	char * m, *u, *t;
	char spare[128];
	memset( (void*)&evnt_data, 0, sizeof(struct s_evnt_data) );
	EvntDataAttach( NULL, WM_XKEYBD, global_evnt_keybd, (void*)&evnt_data );
	EvntAttach( NULL, AP_TERM, global_evnt_apterm );
	EvntAttach( NULL, MN_SELECTED,  global_evnt_menu );
	EvntAttach( NULL, WM_XM1, global_evnt_m1 );

	/* parse and update menu items:  */
	i = 0;
	while( menu_evnt_tbl[i].rid != -1 ) {
		char * str = ObjcString( h_gem_menu, menu_evnt_tbl[i].rid, NULL );
		register_menu_str( &menu_evnt_tbl[i] );
		if( menu_evnt_tbl[i].menustr != NULL ){
			MenuText( NULL, menu_evnt_tbl[i].rid, menu_evnt_tbl[i].menustr );
		}
		i++;
	}
	main_menu_update();
}

void unbind_global_events( void )
{
	int i;
	i=0;
	while(menu_evnt_tbl[i].rid != -1) {
		if( menu_evnt_tbl[i].menustr != NULL )
			free(menu_evnt_tbl[i].menustr);
		i++;
	}
}


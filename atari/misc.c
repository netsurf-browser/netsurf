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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mint/osbind.h>
#include <windom.h>

#include "desktop/cookies.h"
#include "desktop/mouse.h"
#include "desktop/cookies.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/url.h"
#include "utils/log.h"
#include "content/fetch.h"
#include "atari/gui.h"
#include "atari/toolbar.h"
#include "atari/browser.h"
#include "atari/misc.h"
#include "atari/encoding.h"
#include "cflib.h"

extern void * h_gem_rsrc;

void warn_user(const char *warning, const char *detail)
{
	size_t len = 1 + ((warning != NULL) ? strlen(messages_get(warning)) :
			0) + ((detail != 0) ? strlen(detail) : 0);
	char message[len];
	snprintf(message, len, messages_get(warning), detail);
	printf("%s\n", message);
}

void die(const char *error)
{
	printf("%s\n", error);
	exit(1);
}

/**
 * Return the filename part of a full path
 *
 * \param path full path and filename
 * \return filename (will be freed with free())
 */

char *filename_from_path(char *path)
{
	char *leafname;

	leafname = strrchr(path, '\\');
	if( !leafname )
		leafname = strrchr(path, '/');
	if (!leafname)
		leafname = path;
	else
		leafname += 1;

	return strdup(leafname);
}

/**
 * Add a path component/filename to an existing path
 *
 * \param path buffer containing path + free space
 * \param length length of buffer "path"
 * \param newpart string containing path component to add to path
 * \return true on success
 */

bool path_add_part(char *path, int length, const char *newpart)
{
	if(path[strlen(path) - 1] != '/')
		strncat(path, "/", length);

	strncat(path, newpart, length);

	return true;
}

/*
  TBD: make use of this function or remove it...
*/
struct gui_window * find_gui_window( unsigned long handle, short mode ){

	struct gui_window * gw;
	gw = window_list;

	if( handle == 0 ){
		return( NULL );
	}
	else if( mode == BY_WINDOM_HANDLE ){
		WINDOW * win = (WINDOW*) handle;
        while( gw != NULL) {
                if( gw->root->handle == win ) {
					return( gw );
                }
                else
					gw = gw->next;
        }
	}
	else if( mode == BY_GEM_HANDLE ){
		short ghandle = (short)handle;
        while( gw != NULL) {
                if( gw->root->handle != NULL
					&& gw->root->handle->handle == ghandle ) {
					return( gw );
                }
                else
					gw = gw->next;
        }
	}

        return( NULL );
}


struct gui_window * find_cmp_window( COMPONENT * c )
{
	struct gui_window * gw;
	gw = window_list;
	while( gw != NULL ) {
		assert( gw->browser != NULL );
		if( gw->browser->comp == c ) {
			return( gw );
		}
		else
			gw = gw->next;
	}
	return( NULL );
}


/* -------------------------------------------------------------------------- */
/* GEM Utillity functions:                                                    */
/* -------------------------------------------------------------------------- */

/* Return a string from resource file */
char *get_rsc_string( int idx) {
	char *txt;
	RsrcGaddr( h_gem_rsrc, R_STRING, idx,  &txt );
	return txt;
}

OBJECT *get_tree( int idx) {
  OBJECT *tree;
  RsrcGaddr( h_gem_rsrc, R_TREE, idx, &tree);
  return tree;
}

void gem_set_cursor( MFORM_EX * cursor )
{
	static unsigned char flags = 255;
	static int number = 255;
	if( flags == cursor->flags && number == cursor->number )
		return;
	if( cursor->flags & MFORM_EX_FLAG_USERFORM ) {
		MouseSprite( cursor->tree, cursor->number);
	} else {
		graf_mouse(cursor->number, NULL );
	}
	number = cursor->number;
	flags = cursor->flags;
}

long nkc_to_input_key(short nkc, long * ucs4_out)
{
	unsigned char ascii = (nkc & 0xFF);
	nkc = (nkc & (NKF_CTRL|NKF_SHIFT|0xFF));
	long ik = 0;
	*ucs4_out = 0;

	/* shift + cntrl key: */
	if( ((nkc & NKF_CTRL) == NKF_CTRL) && ((nkc & (NKF_SHIFT))!=0) ) {

	}
	/* cntrl key only: */
	else if( (nkc & NKF_CTRL) == NKF_CTRL ) {
		switch ( ascii ) {
			case 'A':
				ik = KEY_SELECT_ALL;
			break;

			case 'C':
				ik = KEY_COPY_SELECTION;
			break;

			case 'X':
				ik = KEY_CUT_SELECTION;
			break;

			case 'V':
				ik = KEY_PASTE;
			break;

			default:
			break;
		}
	}
	/* shift key only: */
	else if( (nkc & NKF_SHIFT) != 0 ) {
		switch( ascii ) {
			case NK_TAB:
				ik = KEY_SHIFT_TAB;
			break;

			case NK_LEFT:
				ik = KEY_LINE_START;
			break;

			case NK_RIGHT:
				ik = KEY_LINE_END;
			break;

			case NK_UP:
				ik = KEY_PAGE_UP;
			break;

			case NK_DOWN:
				ik = KEY_PAGE_DOWN;
			break;

			default:
			break;
		}
	}
	/* No modifier keys: */
	else {
		switch( ascii ) {

			case NK_INS:
				ik = KEY_PASTE;
				break;

			case NK_BS:
				ik = KEY_DELETE_LEFT;
			break;

			case NK_DEL:
				ik = KEY_DELETE_RIGHT;
			break;

			case NK_TAB:
				ik = KEY_TAB;
			break;


			case NK_ENTER:
				ik = KEY_NL;
			break;

			case NK_RET:
				ik = KEY_CR;
			break;

			case NK_ESC:
				ik = KEY_ESCAPE;
			break;

			case NK_CLRHOME:
				ik = KEY_TEXT_START;
			break;

			case NK_RIGHT:
				ik = KEY_RIGHT;
			break;

			case NK_LEFT:
				ik = KEY_LEFT;
			break;

			case NK_UP:
				ik = KEY_UP;
			break;

			case NK_DOWN:
				ik = KEY_DOWN;
			break;

			case NK_M_PGUP:
				ik = KEY_PAGE_UP;
			break;

			case NK_M_PGDOWN:
				ik = KEY_PAGE_DOWN;
			break;

			default:
			break;
		}
	}

	if( ik == 0 && ( (nkc & NKF_CTRL)==0)  ) {
		if (ascii >= 9 ) {
			*ucs4_out = atari_to_ucs4(ascii);
		}
	}
	return ( ik );
}

/**
 * Show default file selector
 *
 * \param title  The selector title.
 * \param name	 Default file name
 * \return a static char pointer or null if the user aborted the selection.
 */
const char * file_select( const char * title, const char * name ) {
	static char path[PATH_MAX]=""; // First usage : current directory
	static char fullname[PATH_MAX]="";
	char tmpname[255];
	char * use_title = (char*)title;

	if( strlen(name)>254)
		return( NULL );

	strcpy( tmpname, name );

	if( use_title == NULL ){
		use_title = (char*)"";
	}

	if( FselInput( path, tmpname, (char*)"",  use_title, NULL, NULL)) {
		strncpy( fullname, path, PATH_MAX-1 );
		strncat( fullname, tmpname, PATH_MAX-strlen(fullname)-1 );
		return( (const char*)&fullname  );
	}
	return( NULL );
}


void dbg_lgrect( char * str, LGRECT * r )
{
	printf("%s: x: %d, y: %d, w: %d, h: %d\n", str,
		r->g_x, r->g_y, r->g_w, r->g_h );
}

void dbg_grect( char * str, GRECT * r )
{
	printf("%s: x: %d, y: %d, w: %d, h: %d\n", str,
		r->g_x, r->g_y, r->g_w, r->g_h );
}

void dbg_pxy( char * str, short * pxy )
{
	printf("%s: x: %d, y: %d, w: %d, h: %d\n", str,
		pxy[0], pxy[1], pxy[2], pxy[3] );
}

/* some LDG functions here to reduce dependencies */
void * ldg_open( char * name, short * global )
{
	return( NULL );
}

void * ldg_find( char * name, short * ldg )
{
	return( NULL );
}

int ldg_close( void * ldg, short * global )
{
	return( 0 );
}

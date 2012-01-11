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

#ifndef NS_ATARI_MISC_H
#define NS_ATARI_MISC_H

#include "cflib.h"
#include "desktop/textinput.h"
#include "atari/gui.h"

#define SBUF8_TO_LBUF8(sbuf,lbuf)\
	lbuf[0] = (long)sbuf[0];\
	lbuf[1] = (long)sbuf[1];\
	lbuf[2] = (long)sbuf[2];\
	lbuf[3] = (long)sbuf[3];\
	lbuf[4] = (long)sbuf[4];\
	lbuf[5] = (long)sbuf[5];\
	lbuf[6] = (long)sbuf[6];\
	lbuf[7] = (long)sbuf[7];


/* Modes for find_gui_window: */
#define BY_WINDOM_HANDLE 0x0
#define BY_GEM_HANDLE    0x1

struct gui_window * find_gui_window( unsigned long, short mode );
struct gui_window * find_cmp_window( COMPONENT * c );
OBJECT *get_tree( int idx );
char *get_rsc_string( int idx );
void gem_set_cursor( MFORM_EX * cursor );
void dbg_grect( char * str, GRECT * r );
void dbg_lgrect( char * str, LGRECT * r );
void dbg_pxy( char * str, short * pxy );
void * ldg_open( char * name, short * global );
void * ldg_find( char * name, short * ldg );
int ldg_close( void * ldg, short * global );
long nkc_to_input_key(short nkc, long * ucs4_out);
const char * file_select( const char * title, const char * name );
#endif

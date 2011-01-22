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

typedef struct {
	long c;
	long v;
} COOKIE;

#define TOS4VER 0x03000 /* this is assumed to be the last single tasking OS */

#define SBUF8_TO_LBUF8(sbuf,lbuf)\
	lbuf[0] = (long)sbuf[0];\
	lbuf[1] = (long)sbuf[1];\
	lbuf[2] = (long)sbuf[2];\
	lbuf[3] = (long)sbuf[3];\
	lbuf[4] = (long)sbuf[4];\
	lbuf[5] = (long)sbuf[5];\
	lbuf[6] = (long)sbuf[6];\
	lbuf[7] = (long)sbuf[7];


struct gui_window * find_root_gui_window( WINDOW * win );
struct gui_window * find_cmp_window( COMPONENT * c );
OBJECT *get_tree( int idx );
char *get_rsc_string( int idx );
void gem_set_cursor( MFORM_EX * cursor );
void dbg_grect( char * str, GRECT * r );
void init_os_info(void);
int tos_getcookie( long tag, long * value );
#endif
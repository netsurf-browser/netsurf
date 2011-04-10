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

#ifndef NS_ATARI_HOTLIST_H
#define NS_ATARI_HOTLIST_H
#include <stdbool.h>
#include <windom.h>
#include "desktop/tree.h"
#include "atari/treeview.h"
/* The hotlist window, toolbar and treeview data. */

struct atari_hotlist {
	WINDOW * window;
	NSTREEVIEW tv;		/*< The hotlist treeview handle.  */
	bool open;
	bool init;
	char path[PATH_MAX];
};

extern struct atari_hotlist hl;

void hotlist_init( void );
void hotlist_open( void );
void hotlist_close( void );
void hotlist_destroy( void );
void atari_hotlist_add_page( const char * url, const char * title );

inline void hotlist_redraw( void );
inline void hotlist_redraw( void )
{
	atari_treeview_redraw( hl.tv );
}

#endif
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

#ifndef NS_ATARI_HISTORY_H
#define NS_ATARI_HISTORY_H

#include <stdbool.h>
#include <windom.h>
#include "desktop/tree.h"
#include "atari/treeview.h"

struct s_atari_global_history {
	WINDOW * window;
	NSTREEVIEW tv;		/*< The history treeview handle.  */
	bool open;
	bool init;
};

extern struct s_atari_global_history gl_history;

bool global_history_init( void );
void global_history_destroy( void );
void global_history_open( void );
void global_history_close( void );

inline void global_history_redraw( void );
inline void global_history_redraw( void )
{
	atari_treeview_redraw( gl_history.tv );
}


#endif

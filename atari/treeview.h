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

#ifndef NS_ATARI_TREEVIEW_H
#define NS_ATARI_TREEVIEW_H

#include <stdbool.h>
#include <windom.h>
#include "desktop/tree.h"
#include "atari/gui.h"

/* defined in front end code */
/*
extern const char tree_directory_icon_name[];
extern const char tree_content_icon_name[];
*/

struct atari_treeview
{
	struct tree * tree;
	WINDOW * window;
	struct s_browser_redrw_info redraw;
	/*tree_drag_type drag;*/
};

typedef struct atari_treeview * NSTREEVIEW;

NSTREEVIEW atari_treeview_create( uint32_t flags, WINDOW * win );
void atari_treeview_destroy( NSTREEVIEW tv );
void atari_treeview_open( NSTREEVIEW tv );
void atari_treeview_close( NSTREEVIEW tv );
struct tree * atari_treeview_get_tree( NSTREEVIEW tv );
WINDOW * atari_tree_get_window( NSTREEVIEW tv );


#endif
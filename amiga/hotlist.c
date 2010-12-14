/*
 * Copyright 2008, 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <proto/exec.h>
#include "amiga/hotlist.h"
#include "desktop/hotlist.h"
#include "amiga/tree.h"

void ami_hotlist_initialise(const char *hotlist_file)
{
	hotlist_window = ami_tree_create(hotlist_get_tree_flags(), NULL);

	if(!hotlist_window) return;

	hotlist_initialise(ami_tree_get_tree(hotlist_window),
			   hotlist_file,
			   tree_directory_icon_name);
}

void ami_hotlist_free(const char *hotlist_file)
{
	hotlist_cleanup(hotlist_file);
	ami_tree_destroy(hotlist_window);
	hotlist_window = NULL;
}

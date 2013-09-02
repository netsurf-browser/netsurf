/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net>
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


#include <stdlib.h>

#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/history_global_core.h"
#include "desktop/global_history.h"
#include "desktop/plotters.h"
#include "desktop/tree.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/log.h"



/**
 * Initialises the global history tree.
 *
 * \param data		user data for the callbacks
 * \param start_redraw  callback function called before every redraw
 * \param end_redraw	callback function called after every redraw
 * \return true on success, false on memory exhaustion
 */
bool history_global_initialise(struct tree *tree, const char* folder_icon_name)
{
	if (tree == NULL)
		return false;

	return true;
}


/**
 * Deletes the global history tree.
 */
void history_global_cleanup(void)
{
}


/* Actions to be connected to front end specific toolbars */

/**
 * Save the global history in a human-readable form under the given location.
 *
 * \param path the path where the history will be saved
 */
bool history_global_export(const char *path)
{
	return global_history_export(path, NULL) == NSERROR_OK;
}

/**
 * Delete nodes which are currently selected.
 */
void history_global_delete_selected(void)
{
}

/**
 * Delete all nodes.
 */
void history_global_delete_all(void)
{
}

/**
 * Select all nodes in the tree.
 */
void history_global_select_all(void)
{
}

/**
 * Unselect all nodes.
 */
void history_global_clear_selection(void)
{
}

/**
 * Expand grouping folders and history entries.
 */
void history_global_expand_all(void)
{
}

/**
 * Expand grouping folders only.
 */
void history_global_expand_directories(void)
{
}

/**
 * Expand history entries only.
 */
void history_global_expand_addresses(void)
{
}

/**
 * Collapse grouping folders and history entries.
 */
void history_global_collapse_all(void)
{
}

/**
 * Collapse grouping folders only.
 */
void history_global_collapse_directories(void)
{
}

/**
 * Collapse history entries only.
 */
void history_global_collapse_addresses(void)
{
}

/**
 * Open the selected entries in seperate browser windows.
 *
 * \param  tabs  open multiple entries in tabs in the new window
 */
void history_global_launch_selected(bool tabs)
{
}

/*
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
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

#include <ctype.h>
#include <stdlib.h>

#include "utils/nsoption.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/hotlist.h"
#include "desktop/hotlist_old.h"
#include "desktop/plotters.h"
#include "desktop/textinput.h"
#include "desktop/tree.h"

#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/log.h"

/**
 * Save the hotlist in a human-readable form under the given location.
 *
 * \param path the path where the hotlist will be saved
 */
bool hotlist_old_export(const char *path)
{
	nserror err;
	err = hotlist_export(path, NULL);
	return (err == NSERROR_OK);
}

/**
 * Edit the node which is currently selected. Works only if one node is
 * selected.
 */
void hotlist_old_edit_selected(void)
{
	/* Update new hotlist */
	hotlist_edit_selection();
}

/**
 * Delete nodes which are currently selected.
 */
void hotlist_old_delete_selected(void)
{
	hotlist_keypress(KEY_DELETE_LEFT);
}

/**
 * Select all nodes in the tree.
 */
void hotlist_old_select_all(void)
{
	hotlist_keypress(KEY_SELECT_ALL);
}

/**
 * Unselect all nodes.
 */
void hotlist_old_clear_selection(void)
{
	hotlist_keypress(KEY_CLEAR_SELECTION);
}

/**
 * Expand grouping folders and history entries.
 */
void hotlist_old_expand_all(void)
{
}

/**
 * Expand grouping folders only.
 */
void hotlist_old_expand_directories(void)
{
}

/**
 * Expand history entries only.
 */
void hotlist_old_expand_addresses(void)
{
}

/**
 * Collapse grouping folders and history entries.
 */
void hotlist_old_collapse_all(void)
{
}

/**
 * Collapse grouping folders only.
 */
void hotlist_old_collapse_directories(void)
{
}

/**
 * Collapse history entries only.
 */
void hotlist_old_collapse_addresses(void)
{
}

/**
 * Add a folder node.
 *
 * \param selected create the folder in the currently-selected node
 */
void hotlist_old_add_folder(bool selected)
{
	hotlist_add_folder(NULL, false, 0);
}

/**
 * Add an entry node.
 *
 * \param selected add the entry in the currently-selected node
 */
void hotlist_old_add_entry(bool selected)
{
	nsurl *url;

	if (nsurl_create("http://netsurf-browser.org/", &url) != NSERROR_OK)
		return;

	hotlist_add_entry(url, "New untitled entry", false, 0);
	nsurl_unref(url);
}

/**
 * Adds the currently viewed page to the hotlist
 */
void hotlist_old_add_page(const char *url)
{
	nsurl *nsurl;

	if (url == NULL)
		return;

	if (nsurl_create(url, &nsurl) != NSERROR_OK)
		return;

	/* Update new hotlist */
	hotlist_add_url(nsurl);
	nsurl_unref(nsurl);
}

/**
 * Adds the currently viewed page to the hotlist at the given co-ordinates
 * \param url	url of the page
 * \param x	X cooridinate with respect to tree origin
 * \param y	Y cooridinate with respect to tree origin
 */
void hotlist_old_add_page_xy(const char *url, int x, int y)
{
	nsurl *nsurl;

	if (url == NULL)
		return;

	if (nsurl_create(url, &nsurl) != NSERROR_OK)
		return;

	/* Update new hotlist */
	hotlist_add_entry(nsurl, NULL, true, y);
	nsurl_unref(nsurl);
}

/**
 * Open the selected entries in separate browser windows.
 *
 * \param  tabs  open multiple entries in tabs in the new window
 */
void hotlist_old_launch_selected(bool tabs)
{
	hotlist_keypress(KEY_CR);
}

/**
 * Set the hotlist's default folder to the selected node.
 *
 * \param  clear  reset the default to tree root
 */
bool hotlist_old_set_default_folder(bool clear)
{
	return false;
}

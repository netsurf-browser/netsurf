/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
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

/** \file
 * Cookies (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/cookies_old.h"
#include "utils/nsoption.h"
#include "desktop/tree.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/schedule.h"
#include "utils/url.h"
#include "utils/utils.h"

/**
 * Initialises cookies tree.
 *
 * \param data		user data for the callbacks
 * \param start_redraw  callback function called before every redraw
 * \param end_redraw	callback function called after every redraw
 * \return true on success, false on memory exhaustion
 */
bool cookies_initialise(struct tree *tree, const char* folder_icon_name, const char* cookie_icon_name)
{

	if (tree == NULL)
		return false;

	return true;
}


/**
 * Get flags with which the cookies tree should be created;
 *
 * \return the flags
 */
unsigned int cookies_get_tree_flags(void)
{
	return TREE_COOKIES;
}


/**
 * Free memory and release all other resources.
 */
void cookies_cleanup(void)
{
}

/* Actions to be connected to front end specific toolbars */

/**
 * Delete nodes which are currently selected.
 */
void cookies_delete_selected(void)
{
}

/**
 * Delete all nodes.
 */
void cookies_delete_all(void)
{
}

/**
 * Select all nodes in the tree.
 */
void cookies_select_all(void)
{
}

/**
 * Unselect all nodes.
 */
void cookies_clear_selection(void)
{
}

/**
 * Expand both domain and cookie nodes.
 */
void cookies_expand_all(void)
{
}

/**
 * Expand domain nodes only.
 */
void cookies_expand_domains(void)
{
}

/**
 * Expand cookie nodes only.
 */
void cookies_expand_cookies(void)
{
}

/**
 * Collapse both domain and cookie nodes.
 */
void cookies_collapse_all(void)
{
}

/**
 * Collapse domain nodes only.
 */
void cookies_collapse_domains(void)
{
}

/**
 * Collapse cookie nodes only.
 */
void cookies_collapse_cookies(void)
{
}

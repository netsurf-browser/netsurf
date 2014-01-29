/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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
 * Free text search (core)
 */
#include "utils/config.h"

#include <ctype.h>
#include <string.h>
#include <dom/dom.h>
#include "content/content.h"
#include "content/hlcache.h"
#include "desktop/browser_private.h"
#include "utils/nsoption.h"
#include "desktop/search.h"
#include "desktop/selection.h"
#include "render/box.h"
#include "render/html.h"
#include "render/search.h"
#include "render/textplain.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"




/**
 * Starts or continues an existing search.
 *
 * \param bw 		the browser_window to search
 * \param callbacks 	callbacks vtable to update frontend according to results
 * \param gui_data	a pointer returned to the callbacks
 * \param flags		search flags
 * \param string	string to search for
 */
void browser_window_search(struct browser_window *bw,
		struct gui_search_callbacks *gui_callbacks, void *gui_data,
		search_flags_t flags, const char *string)
{
	assert(gui_callbacks != NULL);

	if (bw == NULL || bw->current_content == NULL)
		return;

	content_search(bw->current_content, gui_callbacks, gui_data,
			flags, string);
}


/**
 * Clear up a search.  Frees any memory used by the search
 *
 * \param bw 		the browser_window to search
 * \param callbacks 	callbacks vtable to update frontend according to results
 * \param gui_data	a pointer returned to the callbacks
 * \param flags		search flags
 * \param string	string to search for
 */
void browser_window_search_clear(struct browser_window *bw)
{
	if (bw == NULL || bw->current_content == NULL)
		return;

	content_search_clear(bw->current_content);
}

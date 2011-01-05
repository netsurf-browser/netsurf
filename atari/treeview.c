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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windom.h>

#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/plotters.h"
#include "desktop/textinput.h"
#include "desktop/tree.h"
#include "desktop/tree_url_node.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "atari/gui.h"
#include "atari/treeview.h"

const char tree_directory_icon_name[] = "directory.png";
const char tree_content_icon_name[] = "content.png";

static void atari_treeview_request_redraw(int x,int y,int w,int h,void *pw);
static void atari_treeview_resized(struct tree *tree,int w,int h,void *pw);
static void atari_treeview_scroll_visible(int y, int h, void *pw);
static void atari_treeview_get_dimensions(int *width, int *height,void *pw);

static const struct treeview_table atari_tree_callbacks = {
	atari_treeview_request_redraw,
	atari_treeview_resized,
	atari_treeview_scroll_visible,
	atari_treeview_get_dimensions
};


NSTREEVIEW atari_treeview_create( uint32_t flags, WINDOW *win ) 
{
	LOG(("flags: %d", flags));
	NSTREEVIEW new = malloc(sizeof(struct atari_treeview));
	if (new == NULL)
		return NULL;
	memset( new, 0, sizeof(struct atari_treeview));
	new->tree = tree_create(flags, &atari_tree_callbacks, new);
	if (new->tree == NULL) {
		free(new);
		return NULL;
	}
	new->window = win;
	return(new);
}

void atari_treeview_open( NSTREEVIEW tv )
{
	LOG(("tree: %p", tv));
	if( tv->window != NULL ) {

	}
}

void atari_treeview_close( NSTREEVIEW tv )
{
	if( tv->window != NULL ) {

	}
}

void atari_treeview_destroy( NSTREEVIEW tv ) 
{
	LOG(("tree: %p", tv));
	if( tv->tree != NULL ) {
		tree_delete(tv->tree);
		tv->tree = NULL;
	}
}

struct tree * atari_treeview_get_tree( NSTREEVIEW tv )
{
	return( tv->tree );
}

WINDOW * atari_tree_get_window( NSTREEVIEW tv )
{
	return( tv->window );
}


/**
 * Callback to force a redraw of part of the treeview window.
 *
 * \param  x		Min X Coordinate of area to be redrawn.
 * \param  y		Min Y Coordinate of area to be redrawn.
 * \param  width	Width of area to be redrawn.
 * \param  height	Height of area to be redrawn.
 * \param  pw		The treeview object to be redrawn.
 */
void atari_treeview_request_redraw(int x, int y, int w, int h, void *pw)
{
	LOG(("tree: %p", pw));
	if (pw != NULL) {
		NSTREEVIEW tv = (NSTREEVIEW) pw;
		tv->redraw.required = true;
	}
}

/**
 * Callback to notify us of a new overall tree size.
 *
 * \param  tree		The tree being resized.
 * \param  width	The new width of the window.
 * \param  height	The new height of the window.
 * \param  *pw		The treeview object to be resized.
 */

void atari_treeview_resized(struct tree *tree, int width, int height, void *pw)
{
	LOG(("tree: %p", pw));
	if (pw != NULL) {
		NSTREEVIEW tv = (NSTREEVIEW) pw;
	}
}


/**
 * Callback to request that a section of the tree is scrolled into view.
 *
 * \param  y			The Y coordinate of top of the area in NS units.
 * \param  height		The height of the area in NS units.
 * \param  *pw			The treeview object affected.
 */

void atari_treeview_scroll_visible(int y, int height, void *pw)
{
	LOG(("tree: %p", pw));
	if (pw != NULL) {
		NSTREEVIEW tv = (NSTREEVIEW) pw;
	}
}

/**
 * Callback to return the tree window dimensions to the treeview system.
 *
 * \param  *width		Return the window width.
 * \param  *height		Return the window height.
 * \param  *pw			The treeview object to use.
 */

void atari_treeview_get_dimensions(int *width, int *height,
		void *pw)
{
	LOG(("tree: %p", pw));
	if (pw != NULL && (width != NULL || height != NULL)) {
		NSTREEVIEW tv = (NSTREEVIEW) pw;
	}
}


/**
 * Translates a content_type to the name of a respective icon
 *
 * \param content_type	content type
 * \param buffer	buffer for the icon name
 */
void tree_icon_name_from_content_type(char *buffer, content_type type)
{
	switch (type) {
		case CONTENT_HTML:
		case CONTENT_TEXTPLAIN:
		case CONTENT_CSS:
#if defined(WITH_MNG) || defined(WITH_PNG)
		case CONTENT_PNG:
#endif
#ifdef WITH_MNG
		case CONTENT_JNG:
		case CONTENT_MNG:
#endif
#ifdef WITH_JPEG
		case CONTENT_JPEG:
#endif
#ifdef WITH_GIF
		case CONTENT_GIF:
#endif
#ifdef WITH_BMP
		case CONTENT_BMP:
		case CONTENT_ICO:
#endif
#ifdef WITH_SPRITE
		case CONTENT_SPRITE:
#endif
#ifdef WITH_DRAW
		case CONTENT_DRAW:
#endif
#ifdef WITH_ARTWORKS
		case CONTENT_ARTWORKS:
#endif
#ifdef WITH_NS_SVG
		case CONTENT_SVG:
#endif
		default:
			sprintf(buffer, tree_content_icon_name);
			break;
	}
}

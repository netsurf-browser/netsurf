/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
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
 * Generic tree handling (implementation).
 */


#define __STDBOOL_H__	1
extern "C" {
#include "utils/config.h"
#include "desktop/tree.h"
#include "desktop/tree_url_node.h"
}

const char tree_directory_icon_name[] = "directory.png";
const char tree_content_icon_name[] = "content.png";




/**
 * Translates a content_type to the name of a respective icon
 *
 * \param content_type	content type
 * \param buffer	buffer for the icon name
 */
void tree_icon_name_from_content_type(char *buffer, content_type type)
{
	// TODO: design/acquire icons
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

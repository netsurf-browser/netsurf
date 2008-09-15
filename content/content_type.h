/*
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
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
 * Declaration of content_type enum.
 *
 * The content_type enum is defined here to prevent cyclic dependencies.
 */

#ifndef _NETSURF_DESKTOP_CONTENT_TYPE_H_
#define _NETSURF_DESKTOP_CONTENT_TYPE_H_

#include "utils/config.h"


/** The type of a content. */
typedef enum {
	CONTENT_HTML,
	CONTENT_TEXTPLAIN,
	CONTENT_CSS,
#ifdef WITH_JPEG
	CONTENT_JPEG,
#endif
#ifdef WITH_GIF
	CONTENT_GIF,
#endif
#ifdef WITH_BMP
	CONTENT_BMP,
	CONTENT_ICO,
#endif
#if defined(WITH_MNG) || defined(WITH_PNG)
	CONTENT_PNG,
#endif
#ifdef WITH_MNG
	CONTENT_JNG,
	CONTENT_MNG,
#endif
#if defined(WITH_SPRITE) || defined(WITH_NSSPRITE)
	CONTENT_SPRITE,
#endif
#ifdef WITH_DRAW
	CONTENT_DRAW,
#endif
#ifdef WITH_PLUGIN
	CONTENT_PLUGIN,
#endif
	CONTENT_DIRECTORY,
#ifdef WITH_THEME_INSTALL
	CONTENT_THEME,
#endif
#ifdef WITH_ARTWORKS
	CONTENT_ARTWORKS,
#endif
#if defined(WITH_NS_SVG) || defined(WITH_RSVG)
	CONTENT_SVG,
#endif
	/* these must be the last two */
	CONTENT_OTHER,
	CONTENT_UNKNOWN  /**< content-type not received yet */
} content_type;


#endif

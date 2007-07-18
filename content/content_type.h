/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
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
#ifdef WITH_MNG
	CONTENT_PNG,
	CONTENT_JNG,
	CONTENT_MNG,
#endif
#ifdef WITH_SPRITE
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
#ifdef WITH_NS_SVG
	CONTENT_SVG,
#endif
	/* these must be the last two */
	CONTENT_OTHER,
	CONTENT_UNKNOWN  /**< content-type not received yet */
} content_type;


#endif

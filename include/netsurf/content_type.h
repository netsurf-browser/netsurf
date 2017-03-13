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

/**
 * \file
 * Declaration of content type enumerations.
 *
 * The content enumerations are defined here.
 */

#ifndef NETSURF_CONTENT_TYPE_H
#define NETSURF_CONTENT_TYPE_H

/** Debugging dump operations */
enum content_debug {
	CONTENT_DEBUG_RENDER, /** Debug the contents rendering. */
	CONTENT_DEBUG_DOM,    /** Debug the contents Document Object. */
	CONTENT_DEBUG_REDRAW  /** Debug redraw operations. */
};

/** Content encoding information types */
enum content_encoding_type {
	CONTENT_ENCODING_NORMAL, /** The content encoding */
	CONTENT_ENCODING_SOURCE  /** The content encoding source */
};

/** The type of a content. */
typedef enum {
	/** no type for content */
	CONTENT_NONE		= 0x00,

	/** content is HTML */
	CONTENT_HTML		= 0x01,

	/** content is plain text */
	CONTENT_TEXTPLAIN	= 0x02,

	/** content is CSS */
	CONTENT_CSS		= 0x04,

	/** All images */
	CONTENT_IMAGE		= 0x08,

	/** Navigator API Plugins */
	CONTENT_PLUGIN		= 0x10,

	/** RISC OS themes content */
	CONTENT_THEME		= 0x20,

	/** Javascript */
	CONTENT_JS		= 0x40,

	/** All script types. */
	CONTENT_SCRIPT		= 0x40,

	/** Any content matches */
	CONTENT_ANY		= 0x7f
} content_type;


#endif

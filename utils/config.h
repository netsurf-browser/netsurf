/*
 * Copyright 2003-7 John M Bell <jmb202@ecs.soton.ac.uk>
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

#ifndef _NETSURF_UTILS_CONFIG_H_
#define _NETSURF_UTILS_CONFIG_H_

/* Try to detect which features the target OS supports */

#define HAVE_STRNDUP
#if defined(__FreeBSD__) || (defined(__SRV4) && defined(__sun))
	/* FreeBSD and Solaris do not have this function, so
	 * we implement it ourselves in util.c
	 */
#undef HAVE_STRNDUP
char *strndup(const char *s, size_t n);
#endif

/* This section toggles build options on and off.
 * Simply undefine a symbol to turn the relevant feature off.
 *
 * IF ADDING A FEATURE HERE, ADD IT TO Docs/Doxyfile LINE 892 AS WELL.
 */

/* HTTP Auth */
#define WITH_AUTH

/* SSL */
#if !defined(small)
#define WITH_SSL
#endif

/* Image renderering modules */
#define WITH_BMP
#define WITH_JPEG
#define WITH_MNG
#define WITH_GIF
#if defined(riscos)
    #define WITH_NS_SVG	/* internal SVG renderer */
    #define WITH_DRAW
    #define WITH_SPRITE
    #define WITH_ARTWORKS
#endif

/* Platform specific features */
#if defined(riscos)
    /* Plugin module */
    #define WITH_PLUGIN
    /* Acorn URI protocol support */
    #define WITH_URI
    /* ANT URL protocol support */
    #define WITH_URL
    /* Free text search */
    #define WITH_SEARCH
    /* Printing support */
    #define WITH_PRINT
    /* Theme auto-install */
    #define WITH_THEME_INSTALL
#else
    /* We're likely to have a working mmap() */
    #define WITH_MMAP
    #define WITH_NSSPRITE
    #if !defined(DEBUG_BUILD)
        /* Use librsvg and Cairo for rendering SVG */
        #define WITH_RSVG
    #endif
#endif

#if defined(WITH_NS_SVG) && defined(WITH_RSVG)
    #error Cannot build WITH_NS_SVG and WITH_RSVG both enabled
#endif

#if defined(WITH_NSSPRITE) && defined(WITH_SPRITE)
    #error Cannot build WITH_NSSPRITE and WITH_SPRITE both enabled
#endif

#if defined(riscos) || defined(DEBUG_BUILD)
    /* Export modules */
    #define WITH_SAVE_COMPLETE
    #define WITH_DRAW_EXPORT
    #define WITH_TEXT_EXPORT
#endif

#endif

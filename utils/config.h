/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003,4 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
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
#define WITH_NS_SVG	/* internal SVG renderer */
#if defined(riscos) || defined(ncos)
    #define WITH_DRAW
    #define WITH_SPRITE
    #define WITH_ARTWORKS
#endif

/* Platform specific features */
#if defined(riscos) || defined(ncos)
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
#endif

#if defined(riscos) || defined(ncos) || defined(DEBUG_BUILD)
    /* Export modules */
    #define WITH_SAVE_COMPLETE
    #define WITH_DRAW_EXPORT
    #define WITH_TEXT_EXPORT
#endif

#endif

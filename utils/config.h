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

#include <stddef.h>

/* Try to detect which features the target OS supports */

#define HAVE_STRNDUP
#if defined(__FreeBSD__) || (defined(__SRV4) && defined(__sun)) || \
	defined(__APPLE__) || defined(__HAIKU__) || defined(__BEOS__)
	/* FreeBSD and Solaris do not have this function, so
	 * we implement it ourselves in util.c
	 */
#undef HAVE_STRNDUP
char *strndup(const char *s, size_t n);
#endif

/* This section toggles build options on and off.
 * Simply undefine a symbol to turn the relevant feature off.
 *
 * IF ADDING A FEATURE HERE, ADD IT TO Docs/Doxyfile's "PREDEFINED" DEFINITION AS WELL.
 */

/* Platform specific features */
#if defined(riscos)
    /* ANT URL protocol support */
    #define WITH_URL
    /* Free text search */
    #define WITH_SEARCH
    /* Theme auto-install */
    #define WITH_THEME_INSTALL
#elif defined(__HAIKU__) || defined(__BEOS__)
    /* for intptr_t */
    #include <inttypes.h>
    #if defined(__HAIKU__)
        /*not yet: #define WITH_MMAP*/
    #endif
#else
    /* We're likely to have a working mmap() */
    #define WITH_MMAP
#endif
#if defined(riscos) || defined(DEBUG_BUILD)
    /* Export modules */
    #define WITH_SAVE_COMPLETE
    #define WITH_DRAW_EXPORT
#endif

/* Configuration sanity checks: */
#if defined(WITH_NS_SVG) && defined(WITH_RSVG)
    #error Cannot build WITH_NS_SVG and WITH_RSVG both enabled
#endif

#if defined(WITH_NSSPRITE) && defined(WITH_SPRITE)
    #error Cannot build WITH_NSSPRITE and WITH_SPRITE both enabled
#endif

#endif

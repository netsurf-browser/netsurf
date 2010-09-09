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
#include <dirent.h>

/* Try to detect which features the target OS supports */

#if (defined(_GNU_SOURCE))
#define HAVE_STRNDUP
#else
#undef HAVE_STRNDUP
char *strndup(const char *s, size_t n);
#endif

#if (defined(_GNU_SOURCE))
#define HAVE_STRCASESTR
#else
#undef HAVE_STRCASESTR
char *strcasestr(const char *haystack, const char *needle);
#endif

/* fdopendir is actually present on most unix systems but unless
 * _POSIX_C_SOURCE is set to 2008 it is not declared in the system
 * headers. It is unavailable on RISC OS which requires fallback code
 */
#if (_POSIX_C_SOURCE - 0) >= 200809L
#define HAVE_FDOPENDIR
#else
#if defined(riscos)
#undef HAVE_FDOPENDIR
#else
#define HAVE_FDOPENDIR
DIR *fdopendir(int fd);
#endif
#endif

/* For some reason, UnixLib defines this unconditionally. 
 * Assume we're using UnixLib if building for RISC OS. */
#if (defined(_GNU_SOURCE) || defined(riscos))
#define HAVE_STRCHRNUL
#else
#undef HAVE_STRCHRNUL
char *strchrnul(const char *s, int c);
#endif


#define HAVE_UTSNAME
#if (defined(_WIN32))
#undef HAVE_UTSNAME
#endif

#define HAVE_MKDIR
#if (defined(_WIN32))
#undef HAVE_MKDIR
#endif

#define HAVE_SIGPIPE
#if (defined(_WIN32))
#undef HAVE_SIGPIPE
#endif

#define HAVE_STDOUT
#if (defined(_WIN32))
#undef HAVE_STDOUT
#endif


/* This section toggles build options on and off.
 * Simply undefine a symbol to turn the relevant feature off.
 *
 * IF ADDING A FEATURE HERE, ADD IT TO Docs/Doxyfile's "PREDEFINED" DEFINITION AS WELL.
 */

/* Platform specific features */
#if defined(riscos)
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

#if defined(gtk)
	#define WITH_THEME_INSTALL
#endif


/* Configuration sanity checks: */
#if defined(WITH_NS_SVG) && defined(WITH_RSVG)
    #error Cannot build WITH_NS_SVG and WITH_RSVG both enabled
#endif

#if defined(WITH_NSSPRITE) && defined(WITH_SPRITE)
    #error Cannot build WITH_NSSPRITE and WITH_SPRITE both enabled
#endif

#endif

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#ifndef _NETSURF_UTILS_CONFIG_H_
#define _NETSURF_UTILS_CONFIG_H_

/* This file toggles build options on and off.
 * Simply undefine a symbol to turn the relevant feature off.
 *
 * IF ADDING A FEATURE HERE, ADD IT TO Docs/Doxyfile LINE 892 AS WELL.
 */

/* HTTP POST support */
#define WITH_POST

/* Image renderering modules */
#define WITH_DRAW
#define WITH_GIF
#define WITH_JPEG
#define WITH_PNG
#define WITH_SPRITE

/* Plugin module */
#define WITH_PLUGIN

/* Frames */
#undef WITH_FRAMES

/* HTTP Auth */
#define WITH_AUTH

/* Cookies */
#define WITH_COOKIES

/* About page */
#define WITH_ABOUT

/* Acorn URI protocol support */
#define WITH_URI

/* ANT URL protocol support */
#define WITH_URL

/* Export modules */
#define WITH_SAVE_COMPLETE
#define WITH_DRAW_EXPORT

#endif


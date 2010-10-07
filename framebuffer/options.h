/*
 * Copyright 2008, 2010 Daniel Silverstone <dsilvers@netsurf-browser.org>
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

#ifndef _NETSURF_FRAMEBUFFER_OPTIONS_H_
#define _NETSURF_FRAMEBUFFER_OPTIONS_H_

#include "desktop/options.h"

/* surface options */

extern int option_fb_depth;
extern int option_fb_refresh;
extern char *option_fb_device;
extern char *option_fb_input_devpath;
extern char *option_fb_input_glob;

/* toolkit options */

extern int option_fb_furniture_size; /* toolkit furniture size */
extern int option_fb_toolbar_size; /* toolbar furniture size */
extern char *option_fb_toolbar_layout; /* toolbar layout */
extern bool option_fb_osk; /* enable on screen keyboard */

/* font options */

/* render font monochrome */
extern bool option_fb_font_monochrome; 
/** size of font glyph cache in kilobytes. */
extern int option_fb_font_cachesize; 

extern char *option_fb_face_sans_serif; /* default sans face */
extern char *option_fb_face_sans_serif_bold; /* bold sans face */
extern char *option_fb_face_sans_serif_italic; /* bold sans face */
extern char *option_fb_face_sans_serif_italic_bold; /* bold sans face */

extern char *option_fb_face_serif; /* serif face */
extern char *option_fb_face_serif_bold; /* bold serif face */

extern char *option_fb_face_monospace; /* monospace face */
extern char *option_fb_face_monospace_bold; /* bold monospace face */

extern char *option_fb_face_cursive; /* cursive face */
extern char *option_fb_face_fantasy; /* fantasy face */


#define EXTRA_OPTION_DEFINE				\
	int option_fb_depth = 32;			\
	int option_fb_refresh = 70;			\
	char *option_fb_device = 0;			\
	char *option_fb_input_devpath = 0;		\
	char *option_fb_input_glob = 0;			\
	int option_fb_furniture_size = 18;		\
	int option_fb_toolbar_size = 30;		\
	char *option_fb_toolbar_layout;			\
	bool option_fb_osk = false;			\
	bool option_fb_font_monochrome = false;		\
	int option_fb_font_cachesize = 2048;		\
	char *option_fb_face_sans_serif;		\
	char *option_fb_face_sans_serif_bold;		\
	char *option_fb_face_sans_serif_italic;		\
	char *option_fb_face_sans_serif_italic_bold;	\
	char *option_fb_face_serif;			\
	char *option_fb_face_serif_bold;		\
	char *option_fb_face_monospace;			\
	char *option_fb_face_monospace_bold;		\
	char *option_fb_face_cursive;			\
	char *option_fb_face_fantasy;			

#define EXTRA_OPTION_TABLE                                              \
    { "fb_depth", OPTION_INTEGER, &option_fb_depth },			\
    { "fb_refresh", OPTION_INTEGER, &option_fb_refresh },		\
    { "fb_device", OPTION_STRING, &option_fb_device },                  \
    { "fb_input_devpath", OPTION_STRING, &option_fb_input_devpath },    \
    { "fb_input_glob", OPTION_STRING, &option_fb_input_glob },          \
    { "fb_furniture_size", OPTION_INTEGER, &option_fb_furniture_size }, \
    { "fb_toolbar_size", OPTION_INTEGER, &option_fb_toolbar_size },     \
    { "fb_toolbar_layout", OPTION_STRING, &option_fb_toolbar_layout },	\
    { "fb_osk", OPTION_BOOL, &option_fb_osk },				\
    { "fb_font_monochrome", OPTION_BOOL, &option_fb_font_monochrome },  \
    { "fb_font_cachesize", OPTION_INTEGER, &option_fb_font_cachesize }, \
    { "fb_face_sans_serif", OPTION_STRING, &option_fb_face_sans_serif }, \
    { "fb_face_sans_serif_bold", OPTION_STRING, &option_fb_face_sans_serif_bold }, \
    { "fb_face_sans_serif_italic", OPTION_STRING, &option_fb_face_sans_serif_italic }, \
    { "fb_face_sans_serif_italic_bold", OPTION_STRING, &option_fb_face_sans_serif_italic_bold }, \
    { "fb_face_serif", OPTION_STRING, &option_fb_face_serif },          \
    { "fb_serif_bold", OPTION_STRING, &option_fb_face_serif_bold },	\
    { "fb_face_monospace", OPTION_STRING, &option_fb_face_monospace },  \
    { "fb_face_monospace_bold", OPTION_STRING, &option_fb_face_monospace_bold }, \
    { "fb_face_cursive", OPTION_STRING, &option_fb_face_cursive },  \
    { "fb_face_fantasy", OPTION_STRING, &option_fb_face_fantasy }

#endif

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

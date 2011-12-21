/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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

#ifndef NS_ATARI_OPTIONS_H_
#define NS_ATARI_OPTIONS_H_

#include "desktop/options.h"
extern char * option_atari_screen_driver;
extern char * option_atari_font_driver;
extern int option_atari_font_monochrom;
extern int option_atari_dither;
extern int option_atari_transparency;
extern char *option_atari_face_sans_serif; /* default sans face */
extern char *option_atari_face_sans_serif_bold; /* bold sans face */
extern char *option_atari_face_sans_serif_italic; /* bold sans face */
extern char *option_atari_face_sans_serif_italic_bold; /* bold sans face */
extern char *option_atari_face_monospace; /* monospace face */
extern char *option_atari_face_monospace_bold; /* monospace face */
extern char *option_atari_face_serif; /* serif face */
extern char *option_atari_face_serif_bold; /* bold serif face */
extern char *option_atari_face_cursive;
extern char *option_atari_face_fantasy;
extern char *option_atari_editor;
extern char *option_downloads_path;
extern char *option_url_file;
extern char *option_hotlist_file;
extern char *option_tree_icons_path;


#define EXTRA_OPTION_DEFINE \
char * option_atari_screen_driver = (char*)"vdi";\
char * option_atari_font_driver = (char*)"vdi";\
int option_atari_font_monochrom = 0;\
int option_atari_dither = 1;\
int option_atari_transparency = 1;\
char *option_atari_face_sans_serif;\
char *option_atari_face_sans_serif_bold;\
char *option_atari_face_sans_serif_italic;\
char *option_atari_face_sans_serif_italic_bold;\
char *option_atari_face_monospace;\
char *option_atari_face_monospace_bold;\
char *option_atari_face_serif;\
char *option_atari_face_serif_bold;\
char *option_atari_face_cursive; \
char *option_atari_face_fantasy; \
char *option_atari_editor = (char*)"";\
char *option_downloads_path = (char*)""; \
char *option_url_file = (char*)"url.db";\
char *option_hotlist_file = (char*)"hotlist";\
char *option_tree_icons_path = (char*)"./res/icons";

#define EXTRA_OPTION_TABLE \
	{ "atari_screen_driver", OPTION_STRING, &option_atari_screen_driver },\
	{ "atari_font_driver", OPTION_STRING, &option_atari_font_driver },\
	{ "atari_font_monochrom", OPTION_INTEGER, &option_atari_font_monochrom },\
	{ "atari_transparency", OPTION_INTEGER, &option_atari_transparency },\
	{ "atari_dither", OPTION_INTEGER, &option_atari_dither },\
	{ "atari_editor", OPTION_STRING, &option_atari_editor },\
	{ "font_face_sans_serif", OPTION_STRING, &option_atari_face_sans_serif },\
	{ "font_face_sans_serif_bold", OPTION_STRING, &option_atari_face_sans_serif_bold },\
	{ "font_face_sans_serif_italic", OPTION_STRING, &option_atari_face_sans_serif_italic },\
	{ "font_face_sans_serif_italic_bold", OPTION_STRING, &option_atari_face_sans_serif_italic_bold },\
	{ "font_face_monospace", OPTION_STRING, &option_atari_face_monospace },\
	{ "font_face_monospace_bold", OPTION_STRING, &option_atari_face_monospace_bold },\
	{ "font_face_serif", OPTION_STRING, &option_atari_face_serif },\
	{ "font_face_serif_bold", OPTION_STRING, &option_atari_face_serif_bold },\
	{ "font_face_cursive", OPTION_STRING, &option_atari_face_cursive },\
	{ "font_face_fantasy", OPTION_STRING, &option_atari_face_fantasy },\
	{ "downloads_path", OPTION_STRING, &option_downloads_path },\
	{ "url_file", OPTION_STRING, &option_url_file },\
	{ "hotlist_file", OPTION_STRING, &option_hotlist_file },\
	{ "tree_icons_path", OPTION_STRING, &option_tree_icons_path }
#endif


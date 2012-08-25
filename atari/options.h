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

#ifndef _NETSURF_DESKTOP_OPTIONS_INCLUDING_
#error "Frontend options header cannot be included directly"
#endif

#ifndef NS_ATARI_OPTIONS_H_
#define NS_ATARI_OPTIONS_H_


#define NSOPTION_EXTRA_DEFINE										\
	char *atari_font_driver;										\
	int atari_font_monochrom;										\
	int atari_dither;												\
	int atari_transparency;											\
	int atari_image_toolbar;										\
	colour atari_toolbar_bg;											\
	char *atari_image_toolbar_folder;								\
	char *atari_face_sans_serif; /* default sans face */			\
	char *atari_face_sans_serif_bold; /* bold sans face */			\
	char *atari_face_sans_serif_italic; /* bold sans face */		\
	char *atari_face_sans_serif_italic_bold; /* bold sans face */	\
	char *atari_face_monospace; /* monospace face */				\
	char *atari_face_monospace_bold; /* monospace face */			\
	char *atari_face_serif; /* serif face */						\
	char *atari_face_serif_bold; /* bold serif face */				\
	char *atari_face_cursive;										\
	char *atari_face_fantasy;										\
	char *atari_editor;												\
	char *downloads_path;											\
	char *url_file;													\
	char *hotlist_file;												\
	char *tree_icons_path

#define NSOPTION_EXTRA_DEFAULTS						\
	.atari_font_driver = (char*)"freetype",			\
	.atari_font_monochrom = 0,						\
	.atari_dither = 1,								\
	.atari_transparency = 1,						\
	.atari_image_toolbar_folder = (char*)"default",	\
	.atari_image_toolbar = 1,						\
	.atari_toolbar_bg = 0xbbbbbb,					\
	.atari_face_sans_serif = NULL,					\
	.atari_face_sans_serif_bold = NULL,				\
	.atari_face_sans_serif_italic = NULL,			\
	.atari_face_sans_serif_italic_bold = NULL,		\
	.atari_face_monospace = NULL,					\
	.atari_face_monospace_bold = NULL,				\
	.atari_face_serif = NULL,						\
	.atari_face_serif_bold = NULL,					\
	.atari_face_cursive = NULL,						\
	.atari_face_fantasy = NULL,						\
	.atari_editor = (char*)"",						\
	.downloads_path = (char*)"",					\
	.url_file = (char*)"url.db",					\
	.hotlist_file = (char*)"hotlist",				\
	.tree_icons_path = (char*)"./res/icons"

#define NSOPTION_EXTRA_TABLE \
	{ "atari_font_driver", OPTION_STRING, &nsoptions.atari_font_driver },\
	{ "atari_font_monochrom", OPTION_INTEGER, &nsoptions.atari_font_monochrom },\
	{ "atari_image_toolbar", OPTION_INTEGER, &nsoptions.atari_image_toolbar },\
	{ "atari_toolbar_bg", OPTION_COLOUR, &nsoptions.atari_toolbar_bg },\
	{ "atari_transparency", OPTION_INTEGER, &nsoptions.atari_transparency },\
	{ "atari_dither", OPTION_INTEGER, &nsoptions.atari_dither },\
	{ "atari_editor", OPTION_STRING, &nsoptions.atari_editor },\
	{ "atari_image_toolbar_folder", OPTION_STRING, &nsoptions.atari_image_toolbar_folder },\
	{ "font_face_sans_serif", OPTION_STRING, &nsoptions.atari_face_sans_serif },\
	{ "font_face_sans_serif_bold", OPTION_STRING, &nsoptions.atari_face_sans_serif_bold },\
	{ "font_face_sans_serif_italic", OPTION_STRING, &nsoptions.atari_face_sans_serif_italic },\
	{ "font_face_sans_serif_italic_bold", OPTION_STRING, &nsoptions.atari_face_sans_serif_italic_bold },\
	{ "font_face_monospace", OPTION_STRING, &nsoptions.atari_face_monospace },\
	{ "font_face_monospace_bold", OPTION_STRING, &nsoptions.atari_face_monospace_bold },\
	{ "font_face_serif", OPTION_STRING, &nsoptions.atari_face_serif },\
	{ "font_face_serif_bold", OPTION_STRING, &nsoptions.atari_face_serif_bold },\
	{ "font_face_cursive", OPTION_STRING, &nsoptions.atari_face_cursive },\
	{ "font_face_fantasy", OPTION_STRING, &nsoptions.atari_face_fantasy },\
	{ "downloads_path", OPTION_STRING, &nsoptions.downloads_path },\
	{ "url_file", OPTION_STRING, &nsoptions.url_file },\
	{ "hotlist_file", OPTION_STRING, &nsoptions.hotlist_file },\
	{ "tree_icons_path", OPTION_STRING, &nsoptions.tree_icons_path }

#endif


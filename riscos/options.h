/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * RISC OS specific options.
 */

#ifndef _NETSURF_RISCOS_OPTIONS_H_
#define _NETSURF_RISCOS_OPTIONS_H_

#include "netsurf/desktop/options.h"

extern bool option_use_mouse_gestures;
extern bool option_allow_text_selection;
extern bool option_show_toolbar;
extern char *option_theme;
extern char *option_language;
extern bool option_dither_sprites;
extern bool option_filter_sprites;
extern bool option_thumbnail_32bpp;
extern int option_thumbnail_oversampling;

#define EXTRA_OPTION_DEFINE \
bool option_use_mouse_gestures = false;\
bool option_allow_text_selection = true;\
bool option_show_toolbar = true;\
char *option_theme = 0;\
char *option_language = 0;\
bool option_dither_sprites = true;\
bool option_filter_sprites = false;\
bool option_thumbnail_32bpp = true;\
int option_thumbnail_oversampling = 0;

#define EXTRA_OPTION_TABLE \
{ "use_mouse_gestures",     OPTION_BOOL,    &option_use_mouse_gestures },\
{ "allow_text_selection",   OPTION_BOOL,    &option_allow_text_selection },\
{ "show_toolbar",           OPTION_BOOL,    &option_show_toolbar },\
{ "theme",                  OPTION_STRING,  &option_theme },\
{ "language",               OPTION_STRING,  &option_language },\
{ "dither_sprites",         OPTION_BOOL,    &option_dither_sprites },\
{ "filter_sprites",         OPTION_BOOL,    &option_filter_sprites },\
{ "thumbnail_32bpp",        OPTION_BOOL,    &option_thumbnail_32bpp },\
{ "thumbnail_oversampling", OPTION_INTEGER, &option_thumbnail_oversampling }

#endif

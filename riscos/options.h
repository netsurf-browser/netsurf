/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * RISC OS specific options.
 */

#ifndef _NETSURF_RISCOS_OPTIONS_H_
#define _NETSURF_RISCOS_OPTIONS_H_

#include "netsurf/desktop/options.h"

extern bool option_use_mouse_gestures;
extern bool option_allow_text_selection;
extern char *option_theme;
extern char *option_language;
extern bool option_dither_sprites;
extern bool option_filter_sprites;
extern bool option_thumbnail_32bpp;
extern int option_thumbnail_oversampling;
extern bool option_history_tooltip;
extern int option_scale;
extern int option_toolbar_status_width;
extern bool option_toolbar_show_status;
extern bool option_toolbar_show_buttons;
extern bool option_toolbar_show_address;
extern bool option_toolbar_show_throbber;
extern bool option_animate_images;
extern int option_window_x;
extern int option_window_y;
extern int option_window_width;
extern int option_window_height;
extern int option_window_screen_width;
extern int option_window_screen_height;
extern bool option_window_stagger;
extern bool option_window_size_clone;
extern int option_minimum_gif_delay;
extern bool option_background_images;
extern bool option_background_blending;
extern bool option_buffer_animations;

#define EXTRA_OPTION_DEFINE \
bool option_use_mouse_gestures = false;\
bool option_allow_text_selection = true;\
char *option_theme = 0;\
char *option_language = 0;\
bool option_dither_sprites = true;\
bool option_filter_sprites = false;\
bool option_thumbnail_32bpp = true;\
int option_thumbnail_oversampling = 0;\
bool option_history_tooltip = true; \
int option_scale = 100; \
int option_toolbar_status_width = 640; \
bool option_toolbar_show_status = true; \
bool option_toolbar_show_buttons = true; \
bool option_toolbar_show_address = true; \
bool option_toolbar_show_throbber = true; \
bool option_animate_images = true; \
int option_window_x = 0; \
int option_window_y = 0; \
int option_window_width = 0; \
int option_window_height = 0; \
int option_window_screen_width = 0; \
int option_window_screen_height = 0; \
bool option_window_stagger = true; \
bool option_window_size_clone = true; \
int option_minimum_gif_delay = 10; \
bool option_background_images = true; \
bool option_background_blending = true; \
bool option_buffer_animations = true;

#define EXTRA_OPTION_TABLE \
{ "use_mouse_gestures",     OPTION_BOOL,    &option_use_mouse_gestures },\
{ "allow_text_selection",   OPTION_BOOL,    &option_allow_text_selection },\
{ "theme",                  OPTION_STRING,  &option_theme },\
{ "language",               OPTION_STRING,  &option_language },\
{ "dither_sprites",         OPTION_BOOL,    &option_dither_sprites },\
{ "filter_sprites",         OPTION_BOOL,    &option_filter_sprites },\
{ "thumbnail_32bpp",        OPTION_BOOL,    &option_thumbnail_32bpp },\
{ "thumbnail_oversampling", OPTION_INTEGER, &option_thumbnail_oversampling },\
{ "history_tooltip",        OPTION_BOOL,    &option_history_tooltip }, \
{ "scale",                  OPTION_INTEGER, &option_scale }, \
{ "toolbar_show_status",    OPTION_BOOL,    &option_toolbar_show_status }, \
{ "toolbar_status_width",   OPTION_INTEGER, &option_toolbar_status_width }, \
{ "toolbar_show_buttons",   OPTION_BOOL,    &option_toolbar_show_buttons }, \
{ "toolbar_show_address",   OPTION_BOOL,    &option_toolbar_show_address }, \
{ "toolbar_show_throbber",  OPTION_BOOL,    &option_toolbar_show_throbber }, \
{ "animate_images",         OPTION_BOOL,    &option_animate_images }, \
{ "window_x",               OPTION_INTEGER, &option_window_x }, \
{ "window_y",               OPTION_INTEGER, &option_window_y }, \
{ "window_width",           OPTION_INTEGER, &option_window_width }, \
{ "window_height",          OPTION_INTEGER, &option_window_height }, \
{ "window_screen_width",    OPTION_INTEGER, &option_window_screen_width }, \
{ "window_screen_height",   OPTION_INTEGER, &option_window_screen_height }, \
{ "window_stagger",         OPTION_BOOL,    &option_window_stagger }, \
{ "window_size_clone",      OPTION_BOOL,    &option_window_size_clone }, \
{ "minimum_gif_delay",      OPTION_INTEGER, &option_minimum_gif_delay }, \
{ "background_images",      OPTION_BOOL,    &option_background_images }, \
{ "background_blending",    OPTION_BOOL,    &option_background_blending }, \
{ "buffer_animations",      OPTION_BOOL,    &option_buffer_animations }

#endif

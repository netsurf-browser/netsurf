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

#include "netsurf/css/css.h"
#include "netsurf/desktop/options.h"
#include "netsurf/riscos/tinct.h"

extern bool option_use_mouse_gestures;
extern bool option_allow_text_selection;
extern char *option_theme;
extern char *option_language;
extern int option_fg_plot_style;	/* tinct flagword */
extern int option_bg_plot_style;	/* tinct flagword */
extern bool option_thumbnail_32bpp;
extern bool option_history_tooltip;
extern int option_scale;
extern int option_toolbar_status_width;
extern bool option_toolbar_show_status;
extern bool option_toolbar_show_buttons;
extern bool option_toolbar_show_address;
extern bool option_toolbar_show_throbber;
extern int option_window_x;
extern int option_window_y;
extern int option_window_width;
extern int option_window_height;
extern int option_window_screen_width;
extern int option_window_screen_height;
extern bool option_window_stagger;
extern bool option_window_size_clone;
extern bool option_background_images;
extern bool option_background_blending;
extern bool option_buffer_animations;
extern bool option_buffer_everything;
extern char *option_homepage_url;
extern bool option_open_browser_at_startup;
extern bool option_no_plugins;
extern char *option_font_sans;
extern char *option_font_serif;
extern char *option_font_mono;
extern char *option_font_cursive;
extern char *option_font_fantasy;
extern int option_font_default;		/* a css_font_family */
extern char *option_font_default_italic;
extern char *option_font_default_bold;
extern char *option_font_default_bold_italic;
extern int option_screen_cache;
extern bool option_block_popups;
extern bool option_url_suggestion;

#define EXTRA_OPTION_DEFINE \
bool option_use_mouse_gestures = false;\
bool option_allow_text_selection = true;\
char *option_theme = 0;\
char *option_language = 0;\
int option_fg_plot_style = tinct_ERROR_DIFFUSE;\
int option_bg_plot_style = tinct_DITHER;\
bool option_thumbnail_32bpp = true;\
bool option_history_tooltip = true; \
int option_scale = 100; \
int option_toolbar_status_width = 5000; \
bool option_toolbar_show_status = true; \
bool option_toolbar_show_buttons = true; \
bool option_toolbar_show_address = true; \
bool option_toolbar_show_throbber = true; \
int option_window_x = 0; \
int option_window_y = 0; \
int option_window_width = 0; \
int option_window_height = 0; \
int option_window_screen_width = 0; \
int option_window_screen_height = 0; \
bool option_window_stagger = true; \
bool option_window_size_clone = true; \
bool option_background_images = true; \
bool option_background_blending = true; \
bool option_buffer_animations = true; \
bool option_buffer_everything = false; \
char *option_homepage_url = 0; \
bool option_open_browser_at_startup = false; \
bool option_no_plugins = false; \
char *option_font_sans = 0; \
char *option_font_serif = 0; \
char *option_font_mono = 0; \
char *option_font_cursive = 0; \
char *option_font_fantasy = 0; \
int option_font_default = CSS_FONT_FAMILY_SANS_SERIF; \
int option_screen_cache = 0; \
bool option_block_popups = false; \
bool option_url_suggestion = true;

#define EXTRA_OPTION_TABLE \
{ "use_mouse_gestures",     OPTION_BOOL,    &option_use_mouse_gestures },\
{ "allow_text_selection",   OPTION_BOOL,    &option_allow_text_selection },\
{ "theme",                  OPTION_STRING,  &option_theme },\
{ "language",               OPTION_STRING,  &option_language },\
{ "plot_fg_quality",        OPTION_INTEGER, &option_fg_plot_style },\
{ "plot_bg_quality",        OPTION_INTEGER, &option_bg_plot_style },\
{ "thumbnail_32bpp",        OPTION_BOOL,    &option_thumbnail_32bpp },\
{ "history_tooltip",        OPTION_BOOL,    &option_history_tooltip }, \
{ "scale",                  OPTION_INTEGER, &option_scale }, \
{ "toolbar_show_status",    OPTION_BOOL,    &option_toolbar_show_status }, \
{ "toolbar_status_size",    OPTION_INTEGER, &option_toolbar_status_width }, \
{ "toolbar_show_buttons",   OPTION_BOOL,    &option_toolbar_show_buttons }, \
{ "toolbar_show_address",   OPTION_BOOL,    &option_toolbar_show_address }, \
{ "toolbar_show_throbber",  OPTION_BOOL,    &option_toolbar_show_throbber }, \
{ "window_x",               OPTION_INTEGER, &option_window_x }, \
{ "window_y",               OPTION_INTEGER, &option_window_y }, \
{ "window_width",           OPTION_INTEGER, &option_window_width }, \
{ "window_height",          OPTION_INTEGER, &option_window_height }, \
{ "window_screen_width",    OPTION_INTEGER, &option_window_screen_width }, \
{ "window_screen_height",   OPTION_INTEGER, &option_window_screen_height }, \
{ "window_stagger",         OPTION_BOOL,    &option_window_stagger }, \
{ "window_size_clone",      OPTION_BOOL,    &option_window_size_clone }, \
{ "background_images",      OPTION_BOOL,    &option_background_images }, \
{ "background_blending",    OPTION_BOOL,    &option_background_blending }, \
{ "buffer_animations",      OPTION_BOOL,    &option_buffer_animations }, \
{ "buffer_everything",      OPTION_BOOL,    &option_buffer_everything }, \
{ "homepage_url",           OPTION_STRING,  &option_homepage_url }, \
{ "open_browser_at_startup",OPTION_BOOL,    &option_open_browser_at_startup }, \
{ "no_plugins",             OPTION_BOOL,    &option_no_plugins }, \
{ "font_sans",              OPTION_STRING,  &option_font_sans }, \
{ "font_serif",             OPTION_STRING,  &option_font_serif }, \
{ "font_mono",              OPTION_STRING,  &option_font_mono }, \
{ "font_cursive",           OPTION_STRING,  &option_font_cursive }, \
{ "font_fantasy",           OPTION_STRING,  &option_font_fantasy }, \
{ "font_default",           OPTION_INTEGER, &option_font_default }, \
{ "screen_cache",           OPTION_INTEGER, &option_screen_cache }, \
{ "block_popups",           OPTION_BOOL,    &option_block_popups }, \
{ "url_suggestion",         OPTION_BOOL,    &option_url_suggestion }

#endif

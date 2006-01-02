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
extern bool option_history_tooltip;
extern int option_scale;
extern int option_toolbar_status_width;
extern bool option_toolbar_show_status;
extern bool option_toolbar_show_buttons;
extern bool option_toolbar_show_address;
extern bool option_toolbar_show_throbber;
extern char *option_toolbar_browser;
extern char *option_toolbar_hotlist;
extern char *option_toolbar_history;
extern int option_window_x;
extern int option_window_y;
extern int option_window_width;
extern int option_window_height;
extern int option_window_screen_width;
extern int option_window_screen_height;
extern bool option_window_stagger;
extern bool option_window_size_clone;
extern bool option_background_images;
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
extern bool option_block_popups;
extern bool option_url_suggestion;
extern int option_image_memory_direct;	/* -1 means auto-detect */
extern int option_image_memory_compressed;	/* -1 means auto-detect */
extern bool option_strip_extensions;

#define EXTRA_OPTION_DEFINE \
bool option_use_mouse_gestures = false;\
bool option_allow_text_selection = true;\
char *option_theme = 0;\
char *option_language = 0;\
int option_fg_plot_style = tinct_ERROR_DIFFUSE;\
int option_bg_plot_style = tinct_DITHER;\
bool option_history_tooltip = true; \
int option_scale = 100; \
int option_toolbar_status_width = 5000; \
bool option_toolbar_show_status = true; \
bool option_toolbar_show_buttons = true; \
bool option_toolbar_show_address = true; \
bool option_toolbar_show_throbber = true; \
char *option_toolbar_browser = 0; \
char *option_toolbar_hotlist = 0; \
char *option_toolbar_history = 0; \
int option_window_x = 0; \
int option_window_y = 0; \
int option_window_width = 0; \
int option_window_height = 0; \
int option_window_screen_width = 0; \
int option_window_screen_height = 0; \
bool option_window_stagger = true; \
bool option_window_size_clone = true; \
bool option_background_images = true; \
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
bool option_block_popups = false; \
bool option_url_suggestion = true; \
int option_image_memory_direct = -1; \
int option_image_memory_compressed = -1; \
bool option_strip_extensions = true;

#define EXTRA_OPTION_TABLE \
{ "use_mouse_gestures",     OPTION_BOOL,    &option_use_mouse_gestures },\
{ "allow_text_selection",   OPTION_BOOL,    &option_allow_text_selection },\
{ "theme",                  OPTION_STRING,  &option_theme },\
{ "language",               OPTION_STRING,  &option_language },\
{ "plot_fg_quality",        OPTION_INTEGER, &option_fg_plot_style },\
{ "plot_bg_quality",        OPTION_INTEGER, &option_bg_plot_style },\
{ "history_tooltip",        OPTION_BOOL,    &option_history_tooltip }, \
{ "scale",                  OPTION_INTEGER, &option_scale }, \
{ "toolbar_show_status",    OPTION_BOOL,    &option_toolbar_show_status }, \
{ "toolbar_status_size",    OPTION_INTEGER, &option_toolbar_status_width }, \
{ "toolbar_show_buttons",   OPTION_BOOL,    &option_toolbar_show_buttons }, \
{ "toolbar_show_address",   OPTION_BOOL,    &option_toolbar_show_address }, \
{ "toolbar_show_throbber",  OPTION_BOOL,    &option_toolbar_show_throbber }, \
{ "toolbar_browser",	    OPTION_STRING,  &option_toolbar_browser }, \
{ "toolbar_hotlist",	    OPTION_STRING,  &option_toolbar_hotlist }, \
{ "toolbar_history",	    OPTION_STRING,  &option_toolbar_history }, \
{ "window_x",               OPTION_INTEGER, &option_window_x }, \
{ "window_y",               OPTION_INTEGER, &option_window_y }, \
{ "window_width",           OPTION_INTEGER, &option_window_width }, \
{ "window_height",          OPTION_INTEGER, &option_window_height }, \
{ "window_screen_width",    OPTION_INTEGER, &option_window_screen_width }, \
{ "window_screen_height",   OPTION_INTEGER, &option_window_screen_height }, \
{ "window_stagger",         OPTION_BOOL,    &option_window_stagger }, \
{ "window_size_clone",      OPTION_BOOL,    &option_window_size_clone }, \
{ "background_images",      OPTION_BOOL,    &option_background_images }, \
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
{ "block_popups",           OPTION_BOOL,    &option_block_popups }, \
{ "url_suggestion",         OPTION_BOOL,    &option_url_suggestion }, \
{ "image_memory_direct",    OPTION_INTEGER, &option_image_memory_direct }, \
{ "image_memory_compressed",OPTION_INTEGER, &option_image_memory_compressed }, \
{ "strip_extensions",       OPTION_BOOL,    &option_strip_extensions }

#endif

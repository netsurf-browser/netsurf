/*
 * This file is part of NetSurf, http://netsurf-browser.org/
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

#include "desktop/options.h"
#include "riscos/tinct.h"

extern bool option_allow_text_selection;
extern char *option_theme;
extern char *option_language;
extern int option_fg_plot_style;	/* tinct flagword */
extern int option_bg_plot_style;	/* tinct flagword */
extern bool option_history_tooltip;
extern bool option_toolbar_show_buttons;
extern bool option_toolbar_show_address;
extern bool option_toolbar_show_throbber;
extern char *option_toolbar_browser;
extern char *option_toolbar_hotlist;
extern char *option_toolbar_history;
extern char *option_toolbar_cookies;
extern bool option_window_stagger;
extern bool option_window_size_clone;
extern bool option_background_images;
extern bool option_buffer_animations;
extern bool option_buffer_everything;
extern bool option_open_browser_at_startup;
extern bool option_no_plugins;
extern bool option_block_popups;
extern int option_image_memory_direct;	/* -1 means auto-detect */
extern int option_image_memory_compressed;	/* -1 means auto-detect */
extern bool option_strip_extensions;
extern bool option_confirm_overwrite;
extern char *option_url_path;
extern char *option_url_save;
extern char *option_hotlist_path;
extern char *option_hotlist_save;
extern char *option_recent_path;
extern char *option_recent_save;
extern char *option_theme_path;
extern char *option_theme_save;
extern bool option_thumbnail_iconise;

#define EXTRA_OPTION_DEFINE \
bool option_use_mouse_gestures = false;\
bool option_allow_text_selection = true;\
char *option_theme = 0;\
char *option_language = 0;\
int option_fg_plot_style = tinct_ERROR_DIFFUSE;\
int option_bg_plot_style = tinct_DITHER;\
bool option_history_tooltip = true; \
bool option_toolbar_show_buttons = true; \
bool option_toolbar_show_address = true; \
bool option_toolbar_show_throbber = true; \
char *option_toolbar_browser = 0; \
char *option_toolbar_hotlist = 0; \
char *option_toolbar_history = 0; \
char *option_toolbar_cookies = 0; \
bool option_window_stagger = true; \
bool option_window_size_clone = true; \
bool option_background_images = true; \
bool option_buffer_animations = true; \
bool option_buffer_everything = false; \
bool option_open_browser_at_startup = false; \
bool option_no_plugins = false; \
bool option_block_popups = false; \
int option_image_memory_direct = -1; \
int option_image_memory_compressed = -1; \
bool option_strip_extensions = true; \
bool option_confirm_overwrite = true; \
char *option_url_path = 0; \
char *option_url_save = 0; \
char *option_hotlist_path = 0; \
char *option_hotlist_save = 0; \
char *option_recent_path = 0; \
char *option_recent_save = 0; \
char *option_theme_path = 0; \
char *option_theme_save = 0; \
bool option_thumbnail_iconise = true;

#define EXTRA_OPTION_TABLE \
{ "use_mouse_gestures",     OPTION_BOOL,    &option_use_mouse_gestures },\
{ "allow_text_selection",   OPTION_BOOL,    &option_allow_text_selection },\
{ "theme",                  OPTION_STRING,  &option_theme },\
{ "language",               OPTION_STRING,  &option_language },\
{ "plot_fg_quality",        OPTION_INTEGER, &option_fg_plot_style },\
{ "plot_bg_quality",        OPTION_INTEGER, &option_bg_plot_style },\
{ "history_tooltip",        OPTION_BOOL,    &option_history_tooltip }, \
{ "toolbar_show_buttons",   OPTION_BOOL,    &option_toolbar_show_buttons }, \
{ "toolbar_show_address",   OPTION_BOOL,    &option_toolbar_show_address }, \
{ "toolbar_show_throbber",  OPTION_BOOL,    &option_toolbar_show_throbber }, \
{ "toolbar_browser",	    OPTION_STRING,  &option_toolbar_browser }, \
{ "toolbar_hotlist",	    OPTION_STRING,  &option_toolbar_hotlist }, \
{ "toolbar_history",	    OPTION_STRING,  &option_toolbar_history }, \
{ "toolbar_cookies",	    OPTION_STRING,  &option_toolbar_cookies }, \
{ "window_stagger",         OPTION_BOOL,    &option_window_stagger }, \
{ "window_size_clone",      OPTION_BOOL,    &option_window_size_clone }, \
{ "background_images",      OPTION_BOOL,    &option_background_images }, \
{ "buffer_animations",      OPTION_BOOL,    &option_buffer_animations }, \
{ "buffer_everything",      OPTION_BOOL,    &option_buffer_everything }, \
{ "open_browser_at_startup",OPTION_BOOL,    &option_open_browser_at_startup }, \
{ "no_plugins",             OPTION_BOOL,    &option_no_plugins }, \
{ "block_popups",           OPTION_BOOL,    &option_block_popups }, \
{ "image_memory_direct",    OPTION_INTEGER, &option_image_memory_direct }, \
{ "image_memory_compressed",OPTION_INTEGER, &option_image_memory_compressed }, \
{ "strip_extensions",       OPTION_BOOL,    &option_strip_extensions }, \
{ "confirm_overwrite",      OPTION_BOOL,    &option_confirm_overwrite }, \
{ "url_path",               OPTION_STRING,  &option_url_path }, \
{ "url_save",               OPTION_STRING,  &option_url_save }, \
{ "hotlist_path",           OPTION_STRING,  &option_hotlist_path }, \
{ "hotlist_save",           OPTION_STRING,  &option_hotlist_save }, \
{ "recent_path",            OPTION_STRING,  &option_recent_path }, \
{ "recent_save",            OPTION_STRING,  &option_recent_save }, \
{ "theme_path",             OPTION_STRING,  &option_theme_path }, \
{ "theme_save",             OPTION_STRING,  &option_theme_save }, \
{ "thumbnail_iconise",      OPTION_BOOL,    &option_thumbnail_iconise }

#endif

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Toolbar themes (interface).
 *
 * A theme consists of a simple sprite file. There is one current theme, which
 * is changed by ro_theme_load(). A toolbar can then be created and manipulated.
 */

#ifndef _NETSURF_RISCOS_THEME_H_
#define _NETSURF_RISCOS_THEME_H_

#include "oslib/osspriteop.h"
#include "netsurf/desktop/gui.h"

struct toolbar;

struct theme_entry {
	char *name;			/**< theme name */
	char *author;			/**< theme author */
	osspriteop_area *sprite_area;	/**< sprite area for theme */
	int throbber_width;		/**< width of the throbber */
	int throbber_height;		/**< height of the throbber */
	int throbber_frames;		/**< frames of animation for the throbber */
	int browser_background;		/**< background colour of browser toolbar */
	int hotlist_background;		/**< background colour of hotlist toolbar */
	int status_background;		/**< background colour of status window */
	int status_foreground;		/**< colour of status window text */
	bool default_settings;		/**< no theme was loaded, defaults used */
	struct theme_entry *next;	/**< next entry in theme list */

};

void ro_theme_apply(struct theme_entry *theme);
struct theme_entry *ro_theme_load(char *pathname);
void ro_theme_create_browser_toolbar(struct gui_window *g);
void ro_theme_create_hotlist_toolbar(void);
int ro_theme_update_toolbar(struct toolbar *toolbar, wimp_w window);
int ro_theme_resize_toolbar(struct toolbar *toolbar, wimp_w window);
struct theme_entry *ro_theme_list(unsigned int *entries);
void ro_theme_free(struct theme_entry *theme);

#endif

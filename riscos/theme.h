/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Window themes and toolbars (interface).
 */

#include <stdbool.h>

#ifndef _NETSURF_RISCOS_THEME_H_
#define _NETSURF_RISCOS_THEME_H_

typedef enum {
  	THEME_BROWSER_TOOLBAR,
  	THEME_HOTLIST_TOOLBAR
} toolbar_type;


struct toolbar_icon {
	int icon_number;			/**< wimp icon number */
	bool display;				/**< whether to display the icon */
	int width;				/**< icon width */
	int height;				/**< icon height */
	char name[12];				/**< icon name */
	char validation[40];			/**< validation string */
	struct toolbar_icon *next;		/**< next toolbar icon, or NULL for no more */
};

struct toolbar {
	int status_width;			/**< status width percentage * 100 */
	bool display_buttons;			/**< display standard buttons */
	bool display_url;			/**< display URL bar (if applicable) */
	bool display_throbber;			/**< display throbber (if applicable) */
	bool display_status;			/**< display status bar (if applicable) */
	int status_current;			/**< the size of the status window in OS units */
	int toolbar_current;			/**< the size of the toolbar window in OS units */
	int height;				/**< vertical extent of the toolbar (read only) */
	wimp_w toolbar_handle;			/**< toolbar window handle */
	wimp_w status_handle;			/**< status window handle (if applicable) */
	wimp_w parent_handle;			/**< parent window handle (read only) */
	bool reformat_buttons;			/**< buttons need reformatting */
	char *url_buffer;			/**< buffer for status text (read only) */
	char *throbber_buffer;			/**< buffer for status text (read only) */
	char *status_buffer;			/**< buffer for status text (read only) */
	struct toolbar_icon *icon;		/**< first toolbar icon (read only) */
  	struct theme *theme;			/**< themem or NULL for no theme (read only) */
	toolbar_type type;			/**< toolbar type (read only) */
};


struct theme {
	char *details;				/**< theme details */
	char *author;				/**< theme author */
	osspriteop_area *sprite_area;		/**< sprite area for theme */
	bool throbber_right;			/**< throbber is on the right (left otherwise) */
	int throbber_width;			/**< width of the throbber */
	int throbber_height;			/**< height of the throbber */
	int throbber_frames;			/**< frames of animation for the throbber */
	int browser_background;			/**< background colour of browser toolbar */
	int hotlist_background;			/**< background colour of hotlist toolbar */
	int status_background;			/**< background colour of status window */
	int status_foreground;			/**< colour of status window text */
	int users;				/**< number of users for the theme */
};

struct theme_descriptor {
	char *filename;				/**< theme filename */
	struct theme *theme;			/**< corresponding theme (must be opened) */
	struct theme_descriptor *previous;	/**< previous descriptor in the list */
	struct theme_descriptor *next;		/**< next descriptor in the list */
};

void ro_gui_theme_initialise(void);
void ro_gui_theme_finalise(void);
struct theme_descriptor *ro_gui_theme_find(const char *filename);
struct theme_descriptor *ro_gui_theme_get_available(void);

bool ro_gui_theme_open(struct theme_descriptor *descriptor, bool list);
bool ro_gui_theme_apply(struct theme_descriptor *descriptor);
void ro_gui_theme_close(struct theme_descriptor *descriptor, bool list);

struct toolbar *ro_gui_theme_create_toolbar(struct theme_descriptor *descriptor, toolbar_type type);
bool ro_gui_theme_update_toolbar(struct theme_descriptor *descriptor, struct toolbar *toolbar);
bool ro_gui_theme_attach_toolbar(struct toolbar *toolbar, wimp_w parent);
void ro_gui_theme_resize_toolbar_status(struct toolbar *toolbar);
bool ro_gui_theme_process_toolbar(struct toolbar *toolbar, int width);
void ro_gui_theme_destroy_toolbar(struct toolbar *toolbar);


#endif

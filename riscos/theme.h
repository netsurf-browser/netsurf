/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
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

/** \file
 * Window themes and toolbars (interface).
 */

#include <stdbool.h>

#ifndef _NETSURF_RISCOS_THEME_H_
#define _NETSURF_RISCOS_THEME_H_

/* icon numbers for browser toolbars */
#define ICON_TOOLBAR_BACK 0
#define ICON_TOOLBAR_FORWARD 1
#define ICON_TOOLBAR_STOP 2
#define ICON_TOOLBAR_RELOAD 3
#define ICON_TOOLBAR_HOME 4
#define ICON_TOOLBAR_HISTORY 5
#define ICON_TOOLBAR_SAVE 6
#define ICON_TOOLBAR_PRINT 7
#define ICON_TOOLBAR_BOOKMARK 8
#define ICON_TOOLBAR_SCALE 9
#define ICON_TOOLBAR_SEARCH 10
#define ICON_TOOLBAR_UP 11
#define ICON_TOOLBAR_LAST 12
#define ICON_TOOLBAR_SURROUND 12  // Must be after highest toolbar icon
#define ICON_TOOLBAR_FAVICON 13
#define ICON_TOOLBAR_URL 14
#define ICON_TOOLBAR_SUGGEST 15
#define ICON_TOOLBAR_THROBBER 16

/* icon numbers for hotlist/history toolbars */
#define ICON_TOOLBAR_DELETE 0
#define ICON_TOOLBAR_EXPAND 1
#define ICON_TOOLBAR_OPEN 2
#define ICON_TOOLBAR_COOKIES_LAST 4
#define ICON_TOOLBAR_LAUNCH 3
#define ICON_TOOLBAR_HISTORY_LAST 4
#define ICON_TOOLBAR_CREATE 4 // must be after last history icon
#define ICON_TOOLBAR_HOTLIST_LAST 5

/* editing toolbar separator number */
#define ICON_TOOLBAR_SEPARATOR_BROWSER 11
#define ICON_TOOLBAR_SEPARATOR_HOTLIST 5
#define ICON_TOOLBAR_SEPARATOR_HISTORY 4
#define ICON_TOOLBAR_SEPARATOR_COOKIES 3

typedef enum {
	THEME_BROWSER_TOOLBAR,
	THEME_HOTLIST_TOOLBAR,
	THEME_HISTORY_TOOLBAR,
	THEME_COOKIES_TOOLBAR,
	THEME_BROWSER_EDIT_TOOLBAR,
	THEME_HOTLIST_EDIT_TOOLBAR,
	THEME_HISTORY_EDIT_TOOLBAR,
	THEME_COOKIES_EDIT_TOOLBAR
} toolbar_type;

struct theme_file_header {
	unsigned int magic_value;
	unsigned int parser_version;
	char name[32];
	char author[64];
	char browser_bg;
	char hotlist_bg;
	char status_bg;
	char status_fg;
	char theme_flags;
	char future_expansion_1;
	char future_expansion_2;
	char future_expansion_3;
	unsigned int compressed_sprite_size;
	unsigned int decompressed_sprite_size;
};

struct toolbar_icon {
	int icon_number;			/**< wimp icon number */
	bool display;				/**< whether to display the icon */
	int x;					/**< icon x position (valid only when displayed) */
	int y;					/**< icon y position (valid only when displayed) */
	int width;				/**< icon width */
	int height;				/**< icon height */
	char name[12];				/**< icon name */
	char validation[40];			/**< validation string */
	struct toolbar_icon *next;		/**< next toolbar icon, or NULL for no more */
};

struct theme {
	osspriteop_area *sprite_area;		/**< sprite area for theme */
	int throbber_width;			/**< width of the throbber */
	int throbber_height;			/**< height of the throbber */
	int throbber_frames;			/**< frames of animation for the throbber */
	int users;				/**< number of users for the theme */
};

struct toolbar {
	bool display_buttons;			/**< display standard buttons */
	bool display_url;			/**< display URL bar (if applicable) */
	bool display_throbber;			/**< display throbber (if applicable) */
	int toolbar_current;			/**< the size of the toolbar window in OS units */
	int height;				/**< vertical extent of the toolbar (read only) */
	int max_height;				/**< allowed vertical extent (read only) */
	int old_height;				/**< height on last test (read only) */
	wimp_w toolbar_handle;			/**< toolbar window handle */
	wimp_w parent_handle;			/**< parent window handle (read only) */
	bool reformat_buttons;			/**< buttons need reformatting */
	char *url_buffer;			/**< buffer for status text (read only) */
	char *throbber_buffer;			/**< buffer for status text (read only) */
	struct toolbar_icon *icon;		/**< first toolbar icon (read only) */
	struct toolbar_icon *suggest;		/**< suggestion toolbar icon (read only) */
	struct theme_descriptor *descriptor;	/**< theme descriptor (read only) */
	toolbar_type type;			/**< toolbar type (read only) */
	struct toolbar *editor;			/**< toolbar editor */
};

struct theme_descriptor {
	char *leafname;				/**< theme leafname */
	char *filename;				/**< theme filename */
	char name[32];				/**< theme name */
	char author[64];			/**< theme author */
	int browser_background;			/**< background colour of browser toolbar */
	int hotlist_background;			/**< background colour of hotlist toolbar */
	int status_background;			/**< background colour of status window */
	int status_foreground;			/**< colour of status window text */
	bool throbber_right;			/**< throbber is on the right (left otherwise) */
	bool throbber_redraw;			/**< throbber requires forcible updating */
	unsigned int decompressed_size;		/**< decompressed sprite size */
	unsigned int compressed_size;		/**< compressed sprite size */
	struct theme *theme;			/**< corresponding theme (must be opened) */
	struct theme_descriptor *previous;	/**< previous descriptor in the list */
	struct theme_descriptor *next;		/**< next descriptor in the list */
};

void ro_gui_theme_initialise(void);
void ro_gui_theme_finalise(void);
struct theme_descriptor *ro_gui_theme_find(const char *leafname);
struct theme_descriptor *ro_gui_theme_get_available(void);
bool ro_gui_theme_read_file_header(struct theme_descriptor *descriptor,
		struct theme_file_header *file_header);

bool ro_gui_theme_open(struct theme_descriptor *descriptor, bool list);
bool ro_gui_theme_apply(struct theme_descriptor *descriptor);
void ro_gui_theme_close(struct theme_descriptor *descriptor, bool list);

struct toolbar *ro_gui_theme_create_toolbar(struct theme_descriptor *descriptor, toolbar_type type);
bool ro_gui_theme_update_toolbar(struct theme_descriptor *descriptor, struct toolbar *toolbar);
bool ro_gui_theme_attach_toolbar(struct toolbar *toolbar, wimp_w parent);
bool ro_gui_theme_process_toolbar(struct toolbar *toolbar, int width);
void ro_gui_theme_destroy_toolbar(struct toolbar *toolbar);

void ro_gui_theme_toggle_edit(struct toolbar *toolbar);
void ro_gui_theme_toolbar_editor_sync(struct toolbar *toolbar);
void ro_gui_theme_toolbar_editor_click(struct toolbar *toolbar, wimp_pointer *pointer);
void ro_gui_theme_toolbar_editor_drag_end(wimp_dragged *drag);

int ro_gui_theme_height_change(struct toolbar *toolbar);

struct toolbar_icon *ro_gui_theme_toolbar_get_icon(struct toolbar *toolbar, int x, int y);

#define ro_gui_theme_toolbar_height(toolbar) toolbar->height + \
		(toolbar->editor ? toolbar->editor->height : 0) > toolbar->max_height ? \
		toolbar->max_height : toolbar->height + \
		(toolbar->editor ? toolbar->editor->height : 0)
#define ro_gui_theme_toolbar_full_height(toolbar) toolbar->height + \
		(toolbar->editor ? toolbar->editor->height : 0)
#endif

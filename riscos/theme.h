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
  	struct theme_descriptor *descriptor;	/**< theme descriptor (read only) */
	toolbar_type type;			/**< toolbar type (read only) */
	bool locked;				/**< toolbar is locked from editing */
};

struct theme_descriptor {
	char *filename;				/**< theme filename (leaf only) */
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
struct theme_descriptor *ro_gui_theme_find(const char *filename);
struct theme_descriptor *ro_gui_theme_get_available(void);
bool ro_gui_theme_read_file_header(struct theme_descriptor *descriptor,
		struct theme_file_header *file_header);

bool ro_gui_theme_open(struct theme_descriptor *descriptor, bool list);
bool ro_gui_theme_apply(struct theme_descriptor *descriptor);
void ro_gui_theme_close(struct theme_descriptor *descriptor, bool list);
void ro_gui_theme_redraw(struct toolbar *toolbar, wimp_draw *redraw);

struct toolbar *ro_gui_theme_create_toolbar(struct theme_descriptor *descriptor, toolbar_type type);
bool ro_gui_theme_update_toolbar(struct theme_descriptor *descriptor, struct toolbar *toolbar);
bool ro_gui_theme_attach_toolbar(struct toolbar *toolbar, wimp_w parent);
void ro_gui_theme_resize_toolbar_status(struct toolbar *toolbar);
bool ro_gui_theme_process_toolbar(struct toolbar *toolbar, int width);
void ro_gui_theme_destroy_toolbar(struct toolbar *toolbar);

struct toolbar_icon *ro_gui_theme_toolbar_get_icon(struct toolbar *toolbar, int x, int y);
bool ro_gui_theme_toolbar_separator_following(struct toolbar_icon *icon);

#endif

/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
 * Interface to platform-specific gui functions.
 */

#ifndef _NETSURF_DESKTOP_GUI_H_
#define _NETSURF_DESKTOP_GUI_H_

#include <stdbool.h>

#include <libwapcaplet/libwapcaplet.h>
#include <libcss/libcss.h>

#include "utils/config.h"
#include "content/hlcache.h"
#include "desktop/download.h"
#include "desktop/mouse.h"
#include "desktop/search.h"
#include "utils/errors.h"

typedef enum {
	GUI_SAVE_SOURCE,
	GUI_SAVE_DRAW,
	GUI_SAVE_PDF,
	GUI_SAVE_TEXT,
	GUI_SAVE_COMPLETE,
	GUI_SAVE_OBJECT_ORIG,
	GUI_SAVE_OBJECT_NATIVE,
	GUI_SAVE_LINK_URI,
	GUI_SAVE_LINK_URL,
	GUI_SAVE_LINK_TEXT,
	GUI_SAVE_HOTLIST_EXPORT_HTML,
	GUI_SAVE_HISTORY_EXPORT_HTML,
	GUI_SAVE_TEXT_SELECTION,
	GUI_SAVE_CLIPBOARD_CONTENTS
} gui_save_type;

typedef enum {
	GDRAGGING_NONE,
	GDRAGGING_SCROLLBAR,
	GDRAGGING_SELECTION,
	GDRAGGING_OTHER
} gui_drag_type;

struct gui_window;
struct gui_download_window;
struct browser_window;
struct form_control;

/** Graphical user interface window function table
 *
 * function table implementing window operations
 */
struct gui_window_table {

	/* Mandantory entries */

	/** create a gui window for a browsing context */
	struct gui_window *(*create)(struct browser_window *bw, struct browser_window *clone, bool new_tab);

	/** destroy previously created gui window */
	void (*destroy)(struct gui_window *g);

	/**
	 * Force a redraw of the entire contents of a window.
	 *
	 * \param g gui_window to redraw
	 */
	void (*redraw)(struct gui_window *g);

	/**
	 * Redraw an area of a window.
	 *
	 * \param g gui_window
	 * \param rect area to redraw
	 */
	void (*update)(struct gui_window *g, const struct rect *rect);

	/**
	 * Get the scroll position of a browser window.
	 *
	 * \param  g   gui_window
	 * \param  sx  receives x ordinate of point at top-left of window
	 * \param  sy  receives y ordinate of point at top-left of window
	 * \return true iff successful
	 */
	bool (*get_scroll)(struct gui_window *g, int *sx, int *sy);

	/**
	 * Set the scroll position of a browser window.
	 *
	 * \param  g   gui_window to scroll
	 * \param  sx  point to place at top-left of window
	 * \param  sy  point to place at top-left of window
	 */
	void (*set_scroll)(struct gui_window *g, int sx, int sy);


	/* Optional entries */

	/** set the window title. */
	void (*set_title)(struct gui_window *g, const char *title);

	/** set the navigation url. */
	void (*set_url)(struct gui_window *g, const char *url);

	/** start the navigation throbber. */
	void (*start_throbber)(struct gui_window *g);

	/** stop the navigation throbber. */
	void (*stop_throbber)(struct gui_window *g);

	/** start a drag operation within a window */
	bool (*drag_start)(struct gui_window *g, gui_drag_type type, const struct rect *rect);

	/** save link operation */
	void (*save_link)(struct gui_window *g, const char *url, const char *title);

	/** set favicon */
	void (*set_icon)(struct gui_window *g, hlcache_handle *icon);

	/**
	 * Scrolls the specified area of a browser window into view.
	 *
	 * \param  g   gui_window to scroll
	 * \param  x0  left point to ensure visible
	 * \param  y0  bottom point to ensure visible
	 * \param  x1  right point to ensure visible
	 * \param  y1  top point to ensure visible
	 */
	void (*scroll_visible)(struct gui_window *g, int x0, int y0, int x1, int y1);

	/**
	 * Starts drag scrolling of a browser window
	 *
	 * \param g the window to scroll
	 */
	bool (*scroll_start)(struct gui_window *g);

	/**
	 * Called when the gui_window has new content.
	 *
	 * \param  g  the gui_window that has new content
	 */
	void (*new_content)(struct gui_window *g);

};

/** Graphical user interface function table
 *
 * function table implementing GUI interface to browser core
 */
struct gui_table {

	/* Mandantory entries */

	/* sub tables */
	struct gui_window_table *window; /* window sub table */

	/** called to let the frontend update its state and run any
	 * I/O operations.
	 */
	void (*poll)(bool active);


	/* Optional entries */

	/** called to allow the gui to cleanup */
	void (*quit)(void);

	/**
	 * set gui display of a retrieved favicon representing the
	 * search provider
	 *
	 * \param ico may be NULL for local calls; then access current
	 * cache from search_web_ico()
	 */
	void (*set_search_ico)(hlcache_handle *ico);
};

extern struct gui_table *guit; /* the gui vtable */

void gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
		bool scaled);
void gui_window_update_extent(struct gui_window *g);
void gui_window_set_status(struct gui_window *g, const char *text);
void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape);
void gui_window_place_caret(struct gui_window *g, int x, int y, int height,
		const struct rect *clip);
void gui_window_remove_caret(struct gui_window *g);


struct gui_download_window *gui_download_window_create(download_context *ctx,
		struct gui_window *parent);
nserror gui_download_window_data(struct gui_download_window *dw,
		const char *data, unsigned int size);
void gui_download_window_error(struct gui_download_window *dw,
		const char *error_msg);
void gui_download_window_done(struct gui_download_window *dw);

void gui_drag_save_object(gui_save_type type, hlcache_handle *c,
		struct gui_window *g);
void gui_drag_save_selection(struct gui_window *g, const char *selection);
void gui_start_selection(struct gui_window *g);
void gui_clear_selection(struct gui_window *g);

void gui_file_gadget_open(struct gui_window *g, hlcache_handle *hl,
	struct form_control *gadget);

void gui_launch_url(const char *url);

/**
 * Core asks front end for clipboard contents.
 *
 * \param  buffer  UTF-8 text, allocated by front end, ownership yeilded to core
 * \param  length  Byte length of UTF-8 text in buffer
 */
void gui_get_clipboard(char **buffer, size_t *length);

typedef struct nsnsclipboard_styles {
	size_t start;			/**< Start of run */

	plot_font_style_t style;	/**< Style to give text run */
} nsclipboard_styles;
/**
 * Core tells front end to put given text in clipboard
 *
 * \param  buffer    UTF-8 text, owned by core
 * \param  length    Byte length of UTF-8 text in buffer
 * \param  styles    Array of styles given to text runs, owned by core, or NULL
 * \param  n_styles  Number of text run styles in array
 */
void gui_set_clipboard(const char *buffer, size_t length,
		nsclipboard_styles styles[], int n_styles);



void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control);


struct ssl_cert_info;

void gui_cert_verify(nsurl *url, const struct ssl_cert_info *certs,
		unsigned long num, nserror (*cb)(bool proceed, void *pw),
		void *cbpw);

/**
 * Callback to translate resource to full url.
 *
 * Transforms a resource: path into a full URL. The returned URL
 * is used as the target for a redirect. The caller takes ownership of
 * the returned nsurl including unrefing it when finished with it.
 *
 * \param path The path of the resource to locate.
 * \return A string containing the full URL of the target object or
 *         NULL if no suitable resource can be found.
 */
nsurl* gui_get_resource_url(const char *path);

#endif

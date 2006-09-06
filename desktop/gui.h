/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Interface to platform-specific gui functions.
 */

#ifndef _NETSURF_DESKTOP_GUI_H_
#define _NETSURF_DESKTOP_GUI_H_

typedef enum {
	GUI_SAVE_SOURCE,
	GUI_SAVE_DRAW,
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

struct gui_window;
struct gui_download_window;

typedef enum { GUI_POINTER_DEFAULT, GUI_POINTER_POINT, GUI_POINTER_CARET,
	       GUI_POINTER_MENU, GUI_POINTER_UP, GUI_POINTER_DOWN,
	       GUI_POINTER_LEFT, GUI_POINTER_RIGHT, GUI_POINTER_RU,
	       GUI_POINTER_LD, GUI_POINTER_LU, GUI_POINTER_RD,
	       GUI_POINTER_CROSS, GUI_POINTER_MOVE, GUI_POINTER_WAIT,
	       GUI_POINTER_HELP, GUI_POINTER_NO_DROP, GUI_POINTER_NOT_ALLOWED,
               GUI_POINTER_PROGRESS } gui_pointer_shape;

#include <stdbool.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"

extern struct gui_window *search_current_window;

void gui_init(int argc, char** argv);
void gui_init2(int argc, char** argv);
void gui_multitask(void);
void gui_poll(bool active);
void gui_quit(void);

struct gui_window *gui_create_browser_window(struct browser_window *bw,
		struct browser_window *clone);
void gui_window_destroy(struct gui_window *g);
void gui_window_set_title(struct gui_window *g, const char *title);
void gui_window_redraw(struct gui_window *g, int x0, int y0, int x1, int y1);
void gui_window_redraw_window(struct gui_window *g);
void gui_window_update_box(struct gui_window *g,
		const union content_msg_data *data);
bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy);
void gui_window_set_scroll(struct gui_window *g, int sx, int sy);
void gui_window_scroll_visible(struct gui_window *g, int x0, int y0,
		int x1, int y1);
void gui_window_position_frame(struct gui_window *g, int x0, int y0,
		int x1, int y1);
void gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
		bool scaled);
void gui_window_update_extent(struct gui_window *g);
void gui_window_set_status(struct gui_window *g, const char *text);
void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape);
void gui_window_hide_pointer(struct gui_window *g);
void gui_window_set_url(struct gui_window *g, const char *url);
void gui_window_start_throbber(struct gui_window *g);
void gui_window_stop_throbber(struct gui_window *g);
void gui_window_place_caret(struct gui_window *g, int x, int y, int height);
void gui_window_remove_caret(struct gui_window *g);
void gui_window_new_content(struct gui_window *g);
bool gui_window_scroll_start(struct gui_window *g);
bool gui_window_box_scroll_start(struct gui_window *g,
		int x0, int y0, int x1, int y1);
bool gui_window_frame_resize_start(struct gui_window *g);
void gui_window_save_as_link(struct gui_window *g, struct content *c);

struct gui_download_window *gui_download_window_create(const char *url,
		const char *mime_type, struct fetch *fetch,
		unsigned int total_size);
void gui_download_window_data(struct gui_download_window *dw, const char *data,
		unsigned int size);
void gui_download_window_error(struct gui_download_window *dw,
		const char *error_msg);
void gui_download_window_done(struct gui_download_window *dw);

void gui_drag_save_object(gui_save_type type, struct content *c,
		struct gui_window *g);
void gui_drag_save_selection(struct selection *s, struct gui_window *g);
void gui_start_selection(struct gui_window *g);

void gui_paste_from_clipboard(struct gui_window *g, int x, int y);
bool gui_empty_clipboard(void);
bool gui_add_to_clipboard(const char *text, size_t length, bool space);
bool gui_commit_clipboard(void);
bool gui_copy_to_clipboard(struct selection *s);

void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control);

void gui_launch_url(const char *url);

bool gui_search_term_highlighted(struct gui_window *g,
		unsigned start_offset, unsigned end_offset,
		unsigned *start_idx, unsigned *end_idx);

#ifdef WITH_SSL
struct ssl_cert_info;

void gui_cert_verify(struct browser_window *bw, struct content *c,
		const struct ssl_cert_info *certs, unsigned long num);
#endif

#endif


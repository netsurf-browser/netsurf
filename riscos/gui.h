/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
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

#ifndef _NETSURF_RISCOS_GUI_H_
#define _NETSURF_RISCOS_GUI_H_

#include <stdbool.h>
#include <stdlib.h>
#include <oslib/osspriteop.h>
#include <oslib/wimp.h>
#include <rufl.h>
#include "desktop/browser.h"
#include "content/content_type.h"
#include "utils/config.h"

#define RISCOS5 0xAA

#define THUMBNAIL_WIDTH 100
#define THUMBNAIL_HEIGHT 86

extern int os_version;

extern const char * NETSURF_DIR;

struct toolbar;
struct status_bar;
struct plotter_table;
struct gui_window;
struct tree;
struct node;
struct history;
struct css_style;

extern wimp_t task_handle;	/**< RISC OS wimp task handle. */

extern wimp_w dialog_info, dialog_saveas, dialog_zoom, dialog_pageinfo,
	dialog_objinfo, dialog_tooltip, dialog_warning, dialog_openurl,
	dialog_debug, dialog_folder, dialog_entry, dialog_url_complete,
	dialog_search, dialog_print, dialog_theme_install;
extern struct gui_window *gui_track_gui_window;
extern wimp_w current_menu_window;
extern bool current_menu_open;
extern wimp_menu *recent_search_menu;	/* search.c */
extern wimp_w history_window;
extern struct form_control *current_gadget;
extern bool gui_redraw_debug;
extern osspriteop_area *gui_sprites;
extern bool dialog_folder_add, dialog_entry_add, hotlist_insert;
extern bool print_active, print_text_black;
extern struct tree *hotlist_tree, *global_history_tree, *cookies_tree;

typedef enum { GUI_DRAG_NONE, GUI_DRAG_SELECTION, GUI_DRAG_DOWNLOAD_SAVE,
		GUI_DRAG_SAVE, GUI_DRAG_SCROLL, GUI_DRAG_STATUS_RESIZE,
		GUI_DRAG_TREE_SELECT, GUI_DRAG_TREE_MOVE,
		GUI_DRAG_TOOLBAR_CONFIG, GUI_DRAG_FRAME } gui_drag_type;

extern gui_drag_type gui_current_drag_type;

/** desktop font, size and style being used */
extern char ro_gui_desktop_font_family[];
extern int ro_gui_desktop_font_size;
extern rufl_style ro_gui_desktop_font_style;


/** RISC OS data for a browser window. */
struct gui_window {
	/** Associated platform-independent browser window data. */
	struct browser_window *bw;

	struct toolbar *toolbar;	/**< Toolbar, or 0 if not present. */
	struct status_bar *status_bar;	/**< Status bar, or 0 if not present. */

	wimp_w window;		/**< RISC OS window handle. */

	int old_width;		/**< Width when last opened / os units. */
	int old_height;		/**< Height when last opened / os units. */
	bool update_extent;	/**< Update the extent on next opening */

	char title[256];	/**< Buffer for window title. */

	int throbber;		/**< Current frame of throbber animation. */
	int throbtime;		/**< Time of last throbber frame. */

	int iconise_icon;	/**< ID number of icon when window is iconised */

	char validation[12];	/**< Validation string for colours */

	/** Options. */
	struct {
		bool background_images;	/**< Display background images. */
		bool buffer_animations;	/**< Use screen buffering for animations. */
		bool buffer_everything;	/**< Use screen buffering for everything. */
	} option;

	struct gui_window *prev;	/**< Previous in linked list. */
	struct gui_window *next;	/**< Next in linked list. */
};


extern struct gui_window *ro_gui_current_redraw_gui;


/* in gui.c */
void ro_gui_open_window_request(wimp_open *open);
void ro_gui_open_help_page(const char *page);
void ro_gui_screen_size(int *width, int *height);
void ro_gui_view_source(struct content *content);
void ro_gui_dump_content(struct content *content);
void ro_gui_drag_box_start(wimp_pointer *pointer);
bool ro_gui_prequit(void);
const char *ro_gui_default_language(void);

/* in download.c */
void ro_gui_download_init(void);
void ro_gui_download_drag_end(wimp_dragged *drag);
void ro_gui_download_datasave_ack(wimp_message *message);
bool ro_gui_download_prequit(void);

/* in 401login.c */
#ifdef WITH_AUTH
void ro_gui_401login_init(void);
#endif

/* in sslcert.c */
#ifdef WITH_SSL
void ro_gui_cert_init(void);
void ro_gui_cert_open(struct tree *tree, struct node *node);
#endif

/* in window.c */
void ro_gui_window_quit(void);
void ro_gui_window_update_theme(void);
void ro_gui_window_mouse_at(struct gui_window *g, wimp_pointer *pointer);
bool ro_gui_toolbar_click(wimp_pointer *pointer);
void ro_gui_throb(void);
struct gui_window *ro_gui_window_lookup(wimp_w window);
struct gui_window *ro_gui_toolbar_lookup(wimp_w window);
void ro_gui_scroll_request(wimp_scroll *scroll);
bool ro_gui_window_to_window_pos(struct gui_window *g, int x, int y, os_coord *pos);
bool ro_gui_window_to_screen_pos(struct gui_window *g, int x, int y, os_coord *pos);
bool ro_gui_window_dataload(struct gui_window *g, wimp_message *message);
bool ro_gui_toolbar_dataload(struct gui_window *g, wimp_message *message);
void ro_gui_window_process_reformats(void);
void ro_gui_window_default_options(struct browser_window *bw);
void ro_gui_window_redraw_all(void);
void ro_gui_window_prepare_navigate_all(void);
browser_mouse_state ro_gui_mouse_click_state(wimp_mouse_state buttons);
bool ro_gui_shift_pressed(void);
bool ro_gui_ctrl_pressed(void);
void ro_gui_window_scroll_end(struct gui_window *g, wimp_dragged *drag);
void ro_gui_window_frame_resize_end(struct gui_window *g, wimp_dragged *drag);
void ro_gui_window_iconise(struct gui_window *g,
		wimp_full_message_window_info *wi);
bool ro_gui_window_navigate_up(struct gui_window *g, const char *url);
void ro_gui_window_update_boxes(void);

/* in history.c */
void ro_gui_history_init(void);
void ro_gui_history_open(struct browser_window *bw, struct history *history,
		bool pointer);
void ro_gui_history_mouse_at(wimp_pointer *pointer);

/* in hotlist.c */
void ro_gui_hotlist_initialise(void);
void ro_gui_hotlist_save(void);
void ro_gui_hotlist_prepare_folder_dialog(struct node *node);
void ro_gui_hotlist_prepare_entry_dialog(struct node *node);
bool ro_gui_hotlist_dialog_apply(wimp_w w);

/* in filetype.c */
int ro_content_filetype(struct content *content);
int ro_content_filetype_from_type(content_type type);

/* in schedule.c */
extern bool sched_active;
extern os_t sched_time;

/* in debugwin.c */
void ro_gui_debugwin_open(void);

/* in search.c */
void ro_gui_search_init(void);
void ro_gui_search_prepare(struct gui_window *g);
bool ro_gui_search_prepare_menu(void);

/* in print.c */
void ro_gui_print_init(void);
void ro_gui_print_prepare(struct gui_window *g);

/* in font.c */
void nsfont_init(void);
bool nsfont_exists(const char *font_family);
const char *nsfont_fallback_font(void);
bool nsfont_paint(const struct css_style *style, const char *string,
		size_t length, int x, int y, float scale);
void nsfont_read_style(const struct css_style *style,
		const char **font_family, unsigned int *font_size,
		rufl_style *font_style);
void ro_gui_wimp_get_desktop_font(void);

/* in plotters.c */
extern const struct plotter_table ro_plotters;
extern int ro_plot_origin_x;
extern int ro_plot_origin_y;
void ro_plot_set_scale(float scale);

/* in theme_install.c */
bool ro_gui_theme_install_apply(wimp_w w);

/* icon numbers */
#define ICON_STATUS_RESIZE 0
#define ICON_STATUS_TEXT 1

#define ICON_SAVE_ICON 0
#define ICON_SAVE_PATH 1
#define ICON_SAVE_OK 2
#define ICON_SAVE_CANCEL 3

#define ICON_PAGEINFO_TITLE 0
#define ICON_PAGEINFO_URL 1
#define ICON_PAGEINFO_ENC 2
#define ICON_PAGEINFO_TYPE 3
#define ICON_PAGEINFO_ICON 4

#define ICON_OBJINFO_URL 0
#define ICON_OBJINFO_TARGET 1
#define ICON_OBJINFO_TYPE 2
#define ICON_OBJINFO_ICON 3

#define ICON_WARNING_MESSAGE 0
#define ICON_WARNING_CONTINUE 1
#define ICON_WARNING_HELP 2

#define ICON_SEARCH_TEXT 0
#define ICON_SEARCH_CASE_SENSITIVE 1
#define ICON_SEARCH_FIND_NEXT 2
#define ICON_SEARCH_FIND_PREV 3
#define ICON_SEARCH_CANCEL 4
#define ICON_SEARCH_STATUS 5
#define ICON_SEARCH_MENU 8
#define ICON_SEARCH_SHOW_ALL 9

#define ICON_THEME_INSTALL_MESSAGE 0
#define ICON_THEME_INSTALL_INSTALL 1
#define ICON_THEME_INSTALL_CANCEL 2

#define ICON_OPENURL_URL 1
#define ICON_OPENURL_CANCEL 2
#define ICON_OPENURL_OPEN 3
#define ICON_OPENURL_MENU 4

#define ICON_ENTRY_NAME 1
#define ICON_ENTRY_URL 3
#define ICON_ENTRY_CANCEL 4
#define ICON_ENTRY_OK 5
#define ICON_ENTRY_RECENT 6

#define ICON_FOLDER_NAME 1
#define ICON_FOLDER_CANCEL 2
#define ICON_FOLDER_OK 3

#endif

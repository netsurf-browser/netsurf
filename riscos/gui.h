/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 */

#ifndef _NETSURF_RISCOS_GUI_H_
#define _NETSURF_RISCOS_GUI_H_

#include <stdbool.h>
#include <stdlib.h>
#include <oslib/osspriteop.h>
#include <oslib/wimp.h>
#include <rufl.h>
#include "netsurf/utils/config.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/options.h"
#include "netsurf/desktop/tree.h"

#define RISCOS5 0xAA

#define THUMBNAIL_WIDTH 100
#define THUMBNAIL_HEIGHT 86

extern int os_version;

extern const char * NETSURF_DIR;

struct toolbar;
struct plotter_table;

extern wimp_t task_handle;	/**< RISC OS wimp task handle. */

extern wimp_w dialog_info, dialog_saveas, dialog_zoom, dialog_pageinfo,
	dialog_objinfo, dialog_tooltip, dialog_warning, dialog_openurl,
	dialog_debug, dialog_folder, dialog_entry, dialog_url_complete,
	dialog_search, dialog_print, dialog_theme_install;
extern wimp_w current_menu_window;
extern bool current_menu_open;
extern wimp_menu *font_menu;	/* font.c */
extern wimp_menu *recent_search_menu;	/* search.c */
extern wimp_w history_window;
extern struct form_control *current_gadget;
extern bool gui_reformat_pending;
extern bool gui_redraw_debug;
extern osspriteop_area *gui_sprites;
extern bool dialog_folder_add, dialog_entry_add, hotlist_insert;
extern bool print_active, print_text_black;
extern struct tree *hotlist_tree, *global_history_tree;

typedef enum { GUI_DRAG_NONE, GUI_DRAG_SELECTION, GUI_DRAG_DOWNLOAD_SAVE,
		GUI_DRAG_SAVE, GUI_DRAG_SCROLL, GUI_DRAG_STATUS_RESIZE,
		GUI_DRAG_TREE_SELECT, GUI_DRAG_TREE_MOVE,
		GUI_DRAG_TOOLBAR_CONFIG } gui_drag_type;

extern gui_drag_type gui_current_drag_type;


/** RISC OS data for a browser window. */
struct gui_window {
	/** Associated platform-independent browser window data. */
	struct browser_window *bw;

	struct toolbar *toolbar;	/**< Toolbar, or 0 if not present. */

	wimp_w window;		/**< RISC OS window handle. */

	/** Window has been resized, and content needs reformatting. */
	bool reformat_pending;
	int old_width;		/**< Width when last opened / os units. */
	int old_height;		/**< Height when last opened / os units. */

	char title[256];	/**< Buffer for window title. */

	int throbber;		/**< Current frame of throbber animation. */
	int throbtime;		/**< Time of last throbber frame. */

	/** Options. */
	struct {
		float scale;		/**< Scale, 1.0 = 100%. */
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
void ro_gui_drag_box_start(wimp_pointer *pointer);
bool ro_gui_prequit(void);
const char *ro_gui_default_language(void);

/* in download.c */
void ro_gui_download_init(void);
struct gui_download_window * ro_gui_download_window_lookup(wimp_w w);
void ro_gui_download_window_click(struct gui_download_window *dw,
		wimp_pointer *pointer);
void ro_gui_download_drag_end(wimp_dragged *drag);
void ro_gui_download_datasave_ack(wimp_message *message);
bool ro_gui_download_window_destroy(struct gui_download_window *dw, bool quit);
bool ro_gui_download_prequit(void);
bool ro_gui_download_window_keypress(struct gui_download_window *dw, wimp_key *key);

/* in mouseactions.c */
void ro_gui_mouse_action(struct gui_window *g);

/* in textselection.c */
void ro_gui_selection_drag_end(struct gui_window *g, wimp_dragged *drag);
void ro_gui_selection_claim_entity(wimp_full_message_claim_entity *claim);
void ro_gui_selection_data_request(wimp_full_message_data_request *req);
bool ro_gui_save_clipboard(const char *path);
void ro_gui_selection_dragging(wimp_full_message_dragging *drag);
void ro_gui_selection_drag_claim(wimp_full_message_drag_claim *drag);

/* in 401login.c */
#ifdef WITH_AUTH
void ro_gui_401login_init(void);
#endif

/* in window.c */
void ro_gui_window_quit(void);
void ro_gui_window_click(struct gui_window *g, wimp_pointer *mouse);
void ro_gui_window_update_theme(void);
void ro_gui_window_update_dimensions(struct gui_window *g, int yscroll);
void ro_gui_window_open(struct gui_window *g, wimp_open *open);
void ro_gui_window_redraw(struct gui_window *g, wimp_draw *redraw);
void ro_gui_window_mouse_at(struct gui_window *g, wimp_pointer *pointer);
bool ro_gui_toolbar_click(wimp_pointer *pointer);
bool ro_gui_status_click(wimp_pointer *pointer);
void ro_gui_throb(void);
struct gui_window *ro_gui_window_lookup(wimp_w window);
struct gui_window *ro_gui_toolbar_lookup(wimp_w window);
struct gui_window *ro_gui_status_lookup(wimp_w window);
bool ro_gui_window_keypress(struct gui_window *g, int key, bool toolbar);
void ro_gui_scroll_request(wimp_scroll *scroll);
//#define window_x_units(x, state) (x - (state->visible.x0 - state->xscroll))
//#define window_y_units(y, state) (y - (state->visible.y1 - state->yscroll))
int window_x_units(int x, wimp_window_state *state);
int window_y_units(int y, wimp_window_state *state);
bool window_screen_pos(struct gui_window *g, int x, int y, os_coord *pos);
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
void ro_gui_window_set_scale(struct gui_window *g, float scale);

/* in history.c */
void ro_gui_history_init(void);
void ro_gui_history_quit(void);
void ro_gui_history_mode_change(void);
void ro_gui_history_open(struct browser_window *bw, struct history *history, bool pointer);
void ro_gui_history_mouse_at(wimp_pointer *pointer);

/* in hotlist.c */
void ro_gui_hotlist_initialise(void);
void ro_gui_hotlist_save(void);
void ro_gui_hotlist_prepare_folder_dialog(struct node *node);
void ro_gui_hotlist_prepare_entry_dialog(struct node *node);
bool ro_gui_hotlist_dialog_apply(wimp_w w);
int ro_gui_hotlist_help(int x, int y);

/* in save.c */
wimp_w ro_gui_saveas_create(const char *template_name);
void ro_gui_saveas_quit(void);
void ro_gui_save_prepare(gui_save_type save_type, struct content *c);
void ro_gui_save_start_drag(wimp_pointer *pointer);
void ro_gui_drag_icon(int x, int y, const char *sprite);
void ro_gui_save_drag_end(wimp_dragged *drag);
void ro_gui_send_datasave(gui_save_type save_type, wimp_full_message_data_xfer *message, wimp_t to);
void ro_gui_save_datasave_ack(wimp_message *message);
bool ro_gui_save_ok(wimp_w w);
void ro_gui_convert_save_path(char *dp, size_t len, const char *p);

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
void ro_gui_search_end(wimp_w w);

/* in print.c */
void ro_gui_print_init(void);
void ro_gui_print_prepare(struct gui_window *g);

/* in font.c */
void nsfont_init(void);
bool nsfont_exists(const char *font_family);
const char *nsfont_fallback_font(void);
bool nsfont_paint(struct css_style *style, const char *string,
		size_t length, int x, int y, float scale);
void nsfont_read_style(const struct css_style *style,
		const char **font_family, unsigned int *font_size,
		rufl_style *font_style);

/* in plotters.c */
extern const struct plotter_table ro_plotters;
extern int ro_plot_origin_x;
extern int ro_plot_origin_y;
void ro_plot_set_scale(float scale);

/* in theme_install.c */
bool ro_gui_theme_install_apply(wimp_w w);

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
#define ICON_TOOLBAR_URL 11  // Must be after highest toolbar icon
#define ICON_TOOLBAR_THROBBER 12
#define ICON_TOOLBAR_SUGGEST 13

/* icon numbers for hotlist/history toolbars */
#define ICON_TOOLBAR_DELETE 0
#define ICON_TOOLBAR_EXPAND 1
#define ICON_TOOLBAR_OPEN 2
#define ICON_TOOLBAR_LAUNCH 3
#define ICON_TOOLBAR_HISTORY_LAST 4
#define ICON_TOOLBAR_CREATE 4 // must be after last history icon
#define ICON_TOOLBAR_HOTLIST_LAST 5

/* editing toolbar separator number */
#define ICON_TOOLBAR_SEPARATOR_BROWSER 11
#define ICON_TOOLBAR_SEPARATOR_HOTLIST 5
#define ICON_TOOLBAR_SEPARATOR_HISTORY 4

/* icon numbers for toolbar status window */
#define ICON_STATUS_RESIZE 0
#define ICON_STATUS_TEXT 1

#define ICON_DOWNLOAD_ICON 0
#define ICON_DOWNLOAD_URL 1
#define ICON_DOWNLOAD_PATH 2
#define ICON_DOWNLOAD_DESTINATION 3
#define ICON_DOWNLOAD_PROGRESS 5
#define ICON_DOWNLOAD_STATUS 6

#define ICON_401LOGIN_LOGIN 0
#define ICON_401LOGIN_CANCEL 1
#define ICON_401LOGIN_HOST 2
#define ICON_401LOGIN_REALM 3
#define ICON_401LOGIN_USERNAME 4
#define ICON_401LOGIN_PASSWORD 5

#define ICON_ZOOM_VALUE 1
#define ICON_ZOOM_DEC 2
#define ICON_ZOOM_INC 3
#define ICON_ZOOM_75 5
#define ICON_ZOOM_100 6
#define ICON_ZOOM_150 7
#define ICON_ZOOM_200 8
#define ICON_ZOOM_CANCEL 9
#define ICON_ZOOM_OK 10

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

#define ICON_QUERY_MESSAGE 0
#define ICON_QUERY_YES 1
#define ICON_QUERY_NO 2
#define ICON_QUERY_HELP 3

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

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
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "netsurf/utils/config.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/options.h"

#define THEMES_DIR "<NetSurf$Dir>.Themes"

struct toolbar;

extern wimp_w dialog_info, dialog_saveas, dialog_config, dialog_config_br,
	dialog_config_prox, dialog_config_th, dialog_zoom, dialog_pageinfo,
	dialog_objinfo, dialog_tooltip, dialog_warning, dialog_config_th_pane,
	dialog_debug, dialog_folder, dialog_entry, dialog_search, dialog_print;
extern wimp_w history_window;
extern wimp_w hotlist_window;
extern wimp_menu *iconbar_menu, *browser_menu, *combo_menu, *hotlist_menu,
		*proxyauth_menu, *languages_menu;
extern int iconbar_menu_height;
extern struct form_control *current_gadget;
extern bool gui_reformat_pending;
extern bool gui_redraw_debug;
extern wimp_menu *current_menu;
extern osspriteop_area *gui_sprites;
extern struct toolbar *hotlist_toolbar;
extern bool dialog_folder_add, dialog_entry_add, hotlist_insert;

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
} gui_save_type;

typedef enum { GUI_DRAG_SELECTION, GUI_DRAG_DOWNLOAD_SAVE,
		GUI_DRAG_SAVE, GUI_DRAG_STATUS_RESIZE,
		GUI_DRAG_HOTLIST_SELECT, GUI_DRAG_HOTLIST_MOVE } gui_drag_type;
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

	char status[256];	/**< Buffer for status bar. */
	char title[256];	/**< Buffer for window title. */
	char url[256];		/**< Buffer for url entry field. */

	int throbber;		/**< Current frame of throbber animation. */
	char throb_buf[12];	/**< Buffer for throbber sprite name. */
	int throbtime;		/**< Time of last throbber frame. */

	/** Options. */
	struct {
		float scale;		/**< Scale, 1.0 = 100%. */
		bool dither_sprites;	/**< Images should be dithered. */
		bool filter_sprites;	/**< Images should be smoothed. */
		bool animate_images;	/**< Animations should run. */
		bool background_images;	/**< Display background images. */
		bool background_blending;	/**< Perform background blending on text. */
		bool buffer_animations;	/**< Use screen buffering for animations. */
		bool buffer_everything;	/**< Use screen buffering for everything. */
	} option;

	struct gui_window *prev;	/**< Previous in linked list. */
	struct gui_window *next;	/**< Next in linked list. */
};


extern struct gui_window *current_gui;
extern struct gui_window *ro_gui_current_redraw_gui;
extern struct gui_window *ro_gui_current_zoom_gui;


/* in gui.c */
void ro_gui_open_help_page(const char *page);
void ro_gui_screen_size(int *width, int *height);
void ro_gui_view_source(struct content *content);
void ro_gui_drag_box_start(wimp_pointer *pointer);

/* in menus.c */
void ro_gui_menus_init(void);
void ro_gui_create_menu(wimp_menu* menu, int x, int y, struct gui_window *g);
void ro_gui_popup_menu(wimp_menu *menu, wimp_w w, wimp_i i);
void ro_gui_menu_selection(wimp_selection* selection);
void ro_gui_menu_warning(wimp_message_menu_warning *warning);
void ro_gui_prepare_navigate(struct gui_window *gui);
void ro_gui_menu_prepare_scale(void);
void ro_gui_menu_prepare_pageinfo(void);

/* in dialog.c */
void ro_gui_dialog_init(void);
wimp_w ro_gui_dialog_create(const char *template_name);
wimp_window * ro_gui_dialog_load_template(const char *template_name);
void ro_gui_dialog_open(wimp_w w);
void ro_gui_dialog_open_persistant(wimp_w parent, wimp_w w, bool pointer);
void ro_gui_dialog_close_persistant(wimp_w parent);
void ro_gui_dialog_click(wimp_pointer *pointer);
void ro_gui_save_options(void);
bool ro_gui_dialog_keypress(wimp_key *key);
void ro_gui_dialog_close(wimp_w close);
void ro_gui_redraw_config_th_pane(wimp_draw *redraw);
void ro_gui_menu_prepare_hotlist(void);
void ro_gui_dialog_open_config(void);
void ro_gui_dialog_proxyauth_menu_selection(int item);
void ro_gui_dialog_languages_menu_selection(char *lang);

/* in download.c */
void ro_gui_download_init(void);
struct gui_download_window * ro_gui_download_window_lookup(wimp_w w);
void ro_gui_download_window_click(struct gui_download_window *dw,
		wimp_pointer *pointer);
void ro_gui_download_drag_end(wimp_dragged *drag);
void ro_gui_download_datasave_ack(wimp_message *message);
void ro_gui_download_window_destroy(struct gui_download_window *dw);

/* in mouseactions.c */
void ro_gui_mouse_action(struct gui_window *g);

/* in textselection.c */
void ro_gui_start_selection(wimp_pointer *pointer, wimp_window_state *state,
		struct gui_window *g);
void ro_gui_selection_drag_end(wimp_dragged *drag);
void ro_gui_copy_selection(struct gui_window *g);

/* in 401login.c */
#ifdef WITH_AUTH
void ro_gui_401login_init(void);
void ro_gui_401login_open(wimp_w parent, char* host, char * realm, char* fetchurl);
void ro_gui_401login_click(wimp_pointer *pointer);
bool ro_gui_401login_keypress(wimp_key *key);
#endif

/* in window.c */
void ro_gui_window_quit(void);
void ro_gui_window_click(struct gui_window *g, wimp_pointer *mouse);
void ro_gui_window_open(struct gui_window *g, wimp_open *open);
void ro_gui_window_redraw(struct gui_window *g, wimp_draw *redraw);
void ro_gui_window_mouse_at(struct gui_window *g, wimp_pointer *pointer);
void ro_gui_toolbar_click(struct gui_window *g, wimp_pointer *pointer);
void ro_gui_status_click(struct gui_window *g, wimp_pointer *pointer);
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
bool ro_gui_window_dataload(struct gui_window *g, wimp_message *message);
void ro_gui_window_process_reformats(void);
void ro_gui_window_default_options(struct browser_window *bw);

/* in history.c */
void ro_gui_history_init(void);
void ro_gui_history_quit(void);
void ro_gui_history_open(struct browser_window *bw,
		struct history *history, int wx, int wy);
void ro_gui_history_redraw(wimp_draw *redraw);
void ro_gui_history_click(wimp_pointer *pointer);
void ro_gui_history_mouse_at(wimp_pointer *pointer);

/* in hotlist.c */
void ro_gui_hotlist_init(void);
void ro_gui_hotlist_save(void);
void ro_gui_hotlist_show(void);
void ro_gui_hotlist_add(char *title, struct content *content);
void ro_gui_hotlist_redraw(wimp_draw *redraw);
void ro_gui_hotlist_click(wimp_pointer *pointer);
void ro_gui_hotlist_selection_drag_end(wimp_dragged *drag);
void ro_gui_hotlist_move_drag_end(wimp_dragged *drag);
bool ro_gui_hotlist_keypress(int key);
void ro_gui_hotlist_menu_closed(void);
void ro_gui_hotlist_toolbar_click(wimp_pointer* pointer);
int ro_gui_hotlist_get_selected(bool folders);
void ro_gui_hotlist_reset_statistics(void);
void ro_gui_hotlist_set_selected(bool selected);
void ro_gui_hotlist_set_expanded(bool expand, bool folders, bool links);
void ro_gui_hotlist_delete_selected(void);
void ro_gui_hotlist_save_as(const char *file);
void ro_gui_hotlist_prepare_folder_dialog(bool selected);
void ro_gui_hotlist_prepare_entry_dialog(bool selected);
void ro_gui_hotlist_dialog_click(wimp_pointer *pointer);
int ro_gui_hotlist_help(int x, int y);

/* in save.c */
void ro_gui_save_open(gui_save_type save_type, struct content *c,
		bool sub_menu, int x, int y, wimp_w parent, bool keypress);
void ro_gui_save_click(wimp_pointer *pointer);
void ro_gui_drag_icon(wimp_pointer *pointer);
void ro_gui_save_drag_end(wimp_dragged *drag);
void ro_gui_save_datasave_ack(wimp_message *message);

/* in filetype.c */
int ro_content_filetype(struct content *content);

/* in schedule.c */
extern bool sched_active;
extern os_t sched_time;
void schedule(int t, void (*callback)(void *p), void *p);
void schedule_remove(void (*callback)(void *p), void *p);
void schedule_run(void);

/* in debugwin.c */
void ro_gui_debugwin_open(void);
void ro_gui_debugwin_close(void);
void ro_gui_debugwin_redraw(wimp_draw *redraw);

/* in search.c */
void ro_gui_search_open(struct gui_window *g, int x, int y, bool sub_menu, bool keypress);
void ro_gui_search_click(wimp_pointer *pointer);
bool ro_gui_search_keypress(wimp_key *key);

/* in print.c */
void ro_gui_print_open(struct gui_window *g, int x, int y, bool sub_menu, bool keypress);
void ro_gui_print_click(wimp_pointer *pointer);
bool ro_gui_print_keypress(wimp_key *key);

/* toolbar types */
#define TOOLBAR_BROWSER 0
#define TOOLBAR_HOTLIST 1

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
#define ICON_TOOLBAR_URL 12  // Must be after highest toolbar icon
#define ICON_TOOLBAR_THROBBER 13

/* icon numbers for hotlist toolbars */
#define ICON_TOOLBAR_CREATE 0
#define ICON_TOOLBAR_DELETE 1
#define ICON_TOOLBAR_EXPAND 2
#define ICON_TOOLBAR_OPEN 3
#define ICON_TOOLBAR_LAUNCH 4
#define ICON_TOOLBAR_SORT 5
#define ICON_TOOLBAR_HOTLIST_LAST 6

/* icon numbers for toolbar status window */
#define ICON_STATUS_TEXT 0
#define ICON_STATUS_RESIZE 1

#define ICON_CONFIG_SAVE 0
#define ICON_CONFIG_CANCEL 1
#define ICON_CONFIG_BROWSER 3
#define ICON_CONFIG_PROXY 4
#define ICON_CONFIG_THEME 5

#define ICON_CONFIG_BR_FONTSIZE 1
#define ICON_CONFIG_BR_FONTSIZE_DEC 2
#define ICON_CONFIG_BR_FONTSIZE_INC 3
#define ICON_CONFIG_BR_MINSIZE 5
#define ICON_CONFIG_BR_MINSIZE_DEC 6
#define ICON_CONFIG_BR_MINSIZE_INC 7
#define ICON_CONFIG_BR_LANG 9
#define ICON_CONFIG_BR_LANG_PICK 10
#define ICON_CONFIG_BR_ALANG 13
#define ICON_CONFIG_BR_ALANG_PICK 14
#define ICON_CONFIG_BR_HOMEPAGE 16
#define ICON_CONFIG_BR_OPENBROWSER 17

#define ICON_CONFIG_PROX_HTTP 0
#define ICON_CONFIG_PROX_HTTPHOST 1
#define ICON_CONFIG_PROX_HTTPPORT 3
#define ICON_CONFIG_PROX_AUTHTYPE 5
#define ICON_CONFIG_PROX_AUTHTYPE_PICK 6
#define ICON_CONFIG_PROX_AUTHUSER 8
#define ICON_CONFIG_PROX_AUTHPASS 10

#define ICON_CONFIG_TH_GET 0
#define ICON_CONFIG_TH_MANAGE 1

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
#define ICON_ZOOM_50 5
#define ICON_ZOOM_80 6
#define ICON_ZOOM_100 7
#define ICON_ZOOM_120 8
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

#define ICON_SEARCH_TEXT 0
#define ICON_SEARCH_START 1
#define ICON_SEARCH_CASE_SENSITIVE 2
#define ICON_SEARCH_FORWARDS 3
#define ICON_SEARCH_BACKWARDS 4
#define ICON_SEARCH_CANCEL 5
#define ICON_SEARCH_FIND 6

#define ICON_PRINT_TO_BOTTOM 1
#define ICON_PRINT_SHEETS 2
#define ICON_PRINT_SHEETS_VALUE 3
#define ICON_PRINT_SHEETS_DOWN 4
#define ICON_PRINT_SHEETS_UP 5
#define ICON_PRINT_SHEETS_TEXT 6
#define ICON_PRINT_FG_IMAGES 7
#define ICON_PRINT_BG_IMAGES 8
#define ICON_PRINT_IN_BACKGROUND 9
#define ICON_PRINT_UPRIGHT 10
#define ICON_PRINT_SIDEWAYS 11
#define ICON_PRINT_COPIES 12
#define ICON_PRINT_COPIES_DOWN 13
#define ICON_PRINT_COPIES_UP 14
#define ICON_PRINT_CANCEL 15
#define ICON_PRINT_PRINT 16

#endif

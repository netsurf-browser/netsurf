/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
#include "netsurf/riscos/toolbar.h"

#define THEMES_DIR "<NetSurf$Dir>.Themes"

extern wimp_w dialog_info, dialog_saveas, dialog_config, dialog_config_br,
	dialog_config_prox, dialog_config_th, dialog_zoom, dialog_pageinfo,
	dialog_objinfo, dialog_tooltip, dialog_warning, dialog_config_th_pane,
	dialog_debug;
extern wimp_w history_window;
extern wimp_w hotlist_window;
extern wimp_menu *iconbar_menu, *browser_menu, *combo_menu;
extern int iconbar_menu_height;
extern struct form_control *current_gadget;
extern gui_window *window_list;
extern bool gui_reformat_pending;
extern bool gui_redraw_debug;
extern wimp_menu *current_menu;
extern gui_window *current_gui;
extern gui_window *ro_gui_current_redraw_gui;
extern osspriteop_area *gui_sprites;

typedef enum { GUI_BROWSER_WINDOW } gui_window_type;
typedef enum { GUI_SAVE_SOURCE, GUI_SAVE_DRAW, GUI_SAVE_TEXT,
		GUI_SAVE_COMPLETE,
		GUI_SAVE_OBJECT_ORIG, GUI_SAVE_OBJECT_NATIVE,
		GUI_SAVE_LINK_URI, GUI_SAVE_LINK_URL,
		GUI_SAVE_LINK_TEXT } gui_save_type;
extern gui_save_type gui_current_save_type;
typedef enum { GUI_DRAG_SELECTION, GUI_DRAG_DOWNLOAD_SAVE,
		GUI_DRAG_SAVE, GUI_DRAG_STATUS_RESIZE,
		GUI_DRAG_HOTLIST_SELECT, GUI_DRAG_HOTLIST_MOVE } gui_drag_type;
extern gui_drag_type gui_current_drag_type;

struct gui_window {
	gui_window_type type;

	wimp_w window;

	union {
		struct {
			struct toolbar *toolbar;
			int toolbar_width;
			struct browser_window* bw;
			bool reformat_pending;
			int old_width;
			int old_height;
		} browser;
	} data;

	char status[256];
	char title[256];
	char url[256];
	gui_window *next;

	int throbber;
	char throb_buf[12];
	float throbtime;

	enum { drag_NONE, drag_UNKNOWN, drag_BROWSER_TEXT_SELECTION }
			drag_status;

	/*	Options
	*/
	float scale;
	bool option_dither_sprites;
	bool option_filter_sprites;
	int option_toolbar_status_width;
	bool option_toolbar_show_status;
	bool option_toolbar_show_buttons;
	bool option_toolbar_show_address;
	bool option_toolbar_show_throbber;
	bool option_animate_images;
	bool option_background_images;
};


/* in gui.c */
void ro_gui_copy_selection(gui_window* g);
void ro_gui_open_help_page(const char *page);
void ro_gui_screen_size(int *width, int *height);
void ro_gui_view_source(struct content *content);
void ro_gui_drag_box_start(wimp_pointer *pointer);

/* in menus.c */
void ro_gui_menus_init(void);
void ro_gui_create_menu(wimp_menu* menu, int x, int y, gui_window* g);
void ro_gui_popup_menu(wimp_menu *menu, wimp_w w, wimp_i i);
void ro_gui_menu_selection(wimp_selection* selection);
void ro_gui_menu_warning(wimp_message_menu_warning *warning);
void ro_gui_prepare_navigate(gui_window *gui);
void ro_gui_menu_prepare_save(struct content *c);
void ro_gui_menu_prepare_scale(void);

/* in dialog.c */
void ro_gui_dialog_init(void);
wimp_w ro_gui_dialog_create(const char *template_name);
wimp_window * ro_gui_dialog_load_template(const char *template_name);
void ro_gui_dialog_open(wimp_w w);
void ro_gui_dialog_click(wimp_pointer *pointer);
void ro_gui_save_options(void);
bool ro_gui_dialog_keypress(wimp_key *key);
void ro_gui_dialog_close(wimp_w close);
void ro_gui_redraw_config_th_pane(wimp_draw *redraw);

/* in download.c */
void ro_gui_download_init(void);
struct gui_download_window * ro_gui_download_window_lookup(wimp_w w);
void ro_gui_download_window_click(struct gui_download_window *dw,
		wimp_pointer *pointer);
void ro_gui_download_drag_end(wimp_dragged *drag);
void ro_gui_download_datasave_ack(wimp_message *message);
void ro_gui_download_window_destroy(struct gui_download_window *dw);

/* in mouseactions.c */
void ro_gui_mouse_action(gui_window* g);

/* in textselection.c */
void ro_gui_start_selection(wimp_pointer *pointer, wimp_window_state *state,
		gui_window *g);
void ro_gui_selection_drag_end(wimp_dragged *drag);

/* in 401login.c */
#ifdef WITH_AUTH
void ro_gui_401login_init(void);
void ro_gui_401login_open(char* host, char * realm, char* fetchurl);
void ro_gui_401login_click(wimp_pointer *pointer);
bool ro_gui_401login_keypress(wimp_key *key);
#endif

/* in window.c */
void ro_gui_window_click(gui_window* g, wimp_pointer* mouse);
void ro_gui_window_open(gui_window* g, wimp_open* open);
void ro_gui_window_redraw(gui_window* g, wimp_draw* redraw);
void ro_gui_window_mouse_at(wimp_pointer* pointer);
void ro_gui_toolbar_click(gui_window* g, wimp_pointer* pointer);
void ro_gui_status_click(gui_window* g, wimp_pointer* pointer);
void ro_gui_throb(void);
gui_window* ro_lookup_gui_from_w(wimp_w window);
gui_window* ro_lookup_gui_toolbar_from_w(wimp_w window);
gui_window* ro_lookup_gui_status_from_w(wimp_w window);
gui_window *ro_gui_window_lookup(wimp_w w);
bool ro_gui_window_keypress(gui_window *g, int key, bool toolbar);
void ro_gui_scroll_request(wimp_scroll *scroll);
//#define window_x_units(x, state) (x - (state->visible.x0 - state->xscroll))
//#define window_y_units(y, state) (y - (state->visible.y1 - state->yscroll))
int window_x_units(int x, wimp_window_state *state);
int window_y_units(int y, wimp_window_state *state);
bool ro_gui_window_dataload(gui_window *g, wimp_message *message);

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
void ro_gui_hotlist_show(void);
void ro_gui_hotlist_add(char *title, struct content *content);
void ro_gui_hotlist_redraw(wimp_draw *redraw);
void ro_gui_hotlist_click(wimp_pointer *pointer);
void ro_gui_hotlist_selection_drag_end(wimp_dragged *drag);
void ro_gui_hotlist_move_drag_end(wimp_dragged *drag);
bool ro_gui_hotlist_keypress(int key);

/* in save.c */
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

/* icon numbers */
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

#define ICON_STATUS_TEXT 0
#define ICON_STATUS_RESIZE 1

#define ICON_CONFIG_SAVE 0
#define ICON_CONFIG_CANCEL 1
#define ICON_CONFIG_BROWSER 2
#define ICON_CONFIG_PROXY 3
#define ICON_CONFIG_THEME 4

#define ICON_CONFIG_BR_OK 0
#define ICON_CONFIG_BR_CANCEL 1
#define ICON_CONFIG_BR_FONTSIZE 3
#define ICON_CONFIG_BR_FONTSIZE_DEC 4
#define ICON_CONFIG_BR_FONTSIZE_INC 5
#define ICON_CONFIG_BR_MINSIZE 7
#define ICON_CONFIG_BR_MINSIZE_DEC 8
#define ICON_CONFIG_BR_MINSIZE_INC 9
#define ICON_CONFIG_BR_LANG 11
#define ICON_CONFIG_BR_LANG_PICK 12
#define ICON_CONFIG_BR_ALANG 15
#define ICON_CONFIG_BR_ALANG_PICK 16

#define ICON_CONFIG_PROX_OK 0
#define ICON_CONFIG_PROX_CANCEL 1
#define ICON_CONFIG_PROX_HTTP 2
#define ICON_CONFIG_PROX_HTTPHOST 3
#define ICON_CONFIG_PROX_HTTPPORT 4

#define ICON_CONFIG_TH_OK 0
#define ICON_CONFIG_TH_CANCEL 1
#define ICON_CONFIG_TH_GET 2
#define ICON_CONFIG_TH_MANAGE 3

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

#endif

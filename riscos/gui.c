/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unixlib/features.h>
#include <unixlib/local.h>
#include "oslib/font.h"
#include "oslib/help.h"
#include "oslib/hourglass.h"
#include "oslib/inetsuite.h"
#include "oslib/os.h"
#include "oslib/osbyte.h"
#include "oslib/osfile.h"
#include "oslib/osfscontrol.h"
#include "oslib/osspriteop.h"
#include "oslib/plugin.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "oslib/uri.h"
#include "netsurf/utils/config.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/options.h"
#include "netsurf/render/font.h"
#include "netsurf/render/form.h"
#include "netsurf/render/html.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/help.h"
#include "netsurf/riscos/options.h"
#ifdef WITH_PLUGIN
#include "netsurf/riscos/plugin.h"
#endif
#include "netsurf/riscos/save_complete.h"
#include "netsurf/riscos/theme.h"
#ifdef WITH_URI
#include "netsurf/riscos/uri.h"
#endif
#ifdef WITH_URL
#include "netsurf/riscos/url_protocol.h"
#endif
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


const char *__dynamic_da_name = "NetSurf";	/**< For UnixLib. */
int __feature_imagefs_is_file = 1;              /**< For UnixLib. */

char *NETSURF_DIR;
wimp_menu *combo_menu;
struct form_control *current_gadget;
gui_window *over_window = 0;	/**< Window which the pointer is over. */
bool gui_reformat_pending = false;	/**< Some windows have been resized,
						and should be reformatted. */
gui_drag_type gui_current_drag_type;
wimp_t task_handle;	/**< RISC OS wimp task handle. */
static clock_t gui_last_poll;	/**< Time of last wimp_poll. */
osspriteop_area *gui_sprites;      /**< Sprite area containing pointer and hotlist sprites */

/** Accepted wimp user messages. */
static wimp_MESSAGE_LIST(28) task_messages = { {
  	message_HELP_REQUEST,
	message_DATA_SAVE,
	message_DATA_SAVE_ACK,
	message_DATA_LOAD,
	message_DATA_OPEN,
	message_MENU_WARNING,
#ifdef WITH_URI
	message_URI_PROCESS,
	message_URI_RETURN_RESULT,
#endif
#ifdef WITH_URL
	message_INET_SUITE_OPEN_URL,
#endif
#ifdef WITH_PLUGIN
	message_PLUG_IN_OPENING,
	message_PLUG_IN_CLOSED,
	message_PLUG_IN_RESHAPE_REQUEST,
	message_PLUG_IN_FOCUS,
	message_PLUG_IN_URL_ACCESS,
	message_PLUG_IN_STATUS,
	message_PLUG_IN_BUSY,
	message_PLUG_IN_STREAM_NEW,
	message_PLUG_IN_STREAM_WRITE,
	message_PLUG_IN_STREAM_WRITTEN,
	message_PLUG_IN_STREAM_DESTROY,
	message_PLUG_IN_OPEN,
	message_PLUG_IN_CLOSE,
	message_PLUG_IN_RESHAPE,
	message_PLUG_IN_STREAM_AS_FILE,
	message_PLUG_IN_NOTIFY,
	message_PLUG_IN_ABORT,
	message_PLUG_IN_ACTION,
	/* message_PLUG_IN_INFORMED, (not provided by oslib) */
#endif
	0
} };
struct ro_gui_poll_block {
	wimp_event_no event;
	wimp_block *block;
	struct ro_gui_poll_block *next;
};
struct ro_gui_poll_block *ro_gui_poll_queued_blocks = 0;


static void ro_gui_choose_language(void);
static void ro_gui_check_fonts(void);
static void ro_gui_pointers_init(void);
static void ro_gui_icon_bar_create(void);
static void ro_gui_handle_event(wimp_event_no event, wimp_block *block);
static void ro_gui_poll_queue(wimp_event_no event, wimp_block* block);
static void ro_gui_null_reason_code(void);
static void ro_gui_redraw_window_request(wimp_draw *redraw);
static void ro_gui_open_window_request(wimp_open *open);
static void ro_gui_close_window_request(wimp_close *close);
static void ro_gui_mouse_click(wimp_pointer *pointer);
static void ro_gui_icon_bar_click(wimp_pointer* pointer);
static void ro_gui_check_resolvers(void);
static void ro_gui_drag_end(wimp_dragged *drag);
static void ro_gui_keypress(wimp_key* key);
static void ro_gui_user_message(wimp_event_no event, wimp_message *message);
static void ro_msg_datasave(wimp_message* block);
static void ro_msg_dataload(wimp_message* block);
static void ro_msg_datasave_ack(wimp_message* message);
static void ro_msg_dataopen(wimp_message* block);
static char *ro_path_to_url(const char *path);


/**
 * Initialise the gui (RISC OS specific part).
 */

void gui_init(int argc, char** argv)
{
	char path[40];
	char theme_fname[256];
	os_error *error;
	int length;

	xhourglass_start(1);

	save_complete_init();

	options_read("Choices:WWW.NetSurf.Choices");

	ro_gui_choose_language();

	NETSURF_DIR = getenv("NetSurf$Dir");
	if ((length = snprintf(path, sizeof(path),
			"<NetSurf$Dir>.Resources.%s.Messages",
			option_language)) < 0 || length >= (int)sizeof(path))
		die("Failed to locate Messages resource.");
	messages_load(path);
	messages_load("<NetSurf$Dir>.Resources.LangNames");

	error = xwimp_initialise(wimp_VERSION_RO38, "NetSurf",
			(const wimp_message_list *) &task_messages, 0,
			&task_handle);
	if (error) {
		LOG(("xwimp_initialise failed: 0x%x: %s",
				error->errnum, error->errmess));
		die(error->errmess);
	}

	ro_gui_check_fonts();

	/* Issue a *Desktop to poke AcornURI into life */
	if (getenv("NetSurf$Start_URI_Handler"))
		xwimp_start_task("Desktop", 0);

	if (option_theme != NULL) {
		if ((length = snprintf(theme_fname, sizeof(theme_fname),
				"<NetSurf$Dir>.Themes.%s", option_theme)) >= 0
				&& length < (int)sizeof(theme_fname)
		/* check if theme directory exists */
				&& !is_dir(theme_fname)) {
			free(option_theme);
			option_theme = NULL;
		}
	}
	if (option_theme == NULL)
		strcpy(theme_fname, "<NetSurf$Dir>.Themes.Default");
	ro_theme_load(theme_fname);

	if ((length = snprintf(path, sizeof(path),
			"<NetSurf$Dir>.Resources.%s.Templates",
			option_language)) < 0 || length >= (int)sizeof(path))
		die("Failed to locate Templates resource.");
	error = xwimp_open_template(path);
	if (error) {
		LOG(("xwimp_open_template failed: 0x%x: %s",
				error->errnum, error->errmess));
		die(error->errmess);
	}
	ro_gui_dialog_init();
	ro_gui_download_init();
	ro_gui_menus_init();
#ifdef WITH_AUTH
	ro_gui_401login_init();
#endif
	ro_gui_history_init();
	wimp_close_template();
	ro_gui_pointers_init();
	ro_gui_hotlist_init();
	ro_gui_icon_bar_create();
	ro_gui_check_resolvers();
}


/**
 * Determine the language to use.
 *
 * RISC OS has no standard way of determining which language the user prefers.
 * We have to guess from the 'Country' setting.
 */

void ro_gui_choose_language(void)
{
	char path[40];
	const char *lang;
	int country;
	os_error *error;

	/* if option_language exists and is valid, use that */
	if (option_language) {
		if (2 < strlen(option_language))
			option_language[2] = 0;
		sprintf(path, "<NetSurf$Dir>.Resources.%s", option_language);
		if (is_dir(path)) {
			if (!option_accept_language)
				option_accept_language = strdup(option_language);
			return;
		}
		free(option_language);
		option_language = 0;
	}

	/* choose a language from the configured country number */
	error = xosbyte_read(osbyte_VAR_COUNTRY_NUMBER, &country);
	if (error) {
		LOG(("xosbyte_read failed: 0x%x: %s",
				error->errnum, error->errmess));
		country = 1;
	}
	switch (country) {
		case 6: /* France */
		case 18: /* Canada2 (French Canada?) */
			lang = "fr";
			break;
		default:
			lang = "en";
			break;
	}
	sprintf(path, "<NetSurf$Dir>.Resources.%s", lang);
	if (is_dir(path))
		option_language = strdup(lang);
	else
		option_language = strdup("en");
	assert(option_language);
	if (!option_accept_language)
		option_accept_language = strdup(option_language);
}


/**
 * Check that at least Homerton.Medium is available.
 */

void ro_gui_check_fonts(void)
{
	char s[252];
	font_f font;
	os_error *error;

	error = xfont_find_font("Homerton.Medium\\ELatin1",
			160, 160, 0, 0, &font, 0, 0);
	if (error) {
		if (error->errnum == error_FILE_NOT_FOUND) {
			xwimp_start_task("TaskWindow -wimpslot 200K -quit "
					"<NetSurf$Dir>.FixFonts", 0);
			die("FontBadInst");
		} else {
			snprintf(s, sizeof s, messages_get("FontError"),
					error->errmess);
			die(s);
		}
	}

	error = xfont_lose_font(font);
	if (error) {
		snprintf(s, sizeof s, messages_get("FontError"),
				error->errmess);
		die(s);
	}
}


/**
 * Initialise pointer sprite area.
 */

void ro_gui_pointers_init(void)
{
	int len;
	fileswitch_object_type obj_type;
	os_error *e;

	e = xosfile_read_stamped_no_path("<NetSurf$Dir>.Resources.Sprites",
			&obj_type, 0, 0, &len, 0, 0);
	if (e) {
		LOG(("xosfile_read_stamped_no_path: 0x%x: %s",
				e->errnum, e->errmess));
		die(e->errmess);
	}
	if (obj_type != fileswitch_IS_FILE)
		die("<NetSurf$Dir>.Resources.Pointers missing.");

	gui_sprites = malloc(len + 4);
	if (!gui_sprites)
		die("NoMemory");

	gui_sprites->size = len+4;
	gui_sprites->sprite_count = 0;
	gui_sprites->first = 16;
	gui_sprites->used = 16;

	e = xosspriteop_load_sprite_file(osspriteop_USER_AREA,
			gui_sprites, "<NetSurf$Dir>.Resources.Sprites");
	if (e) {
		LOG(("xosspriteop_load_sprite_file: 0x%x: %s",
				e->errnum, e->errmess));
		die(e->errmess);
	}
}


/**
 * Create an iconbar icon.
 */

void ro_gui_icon_bar_create(void)
{
	wimp_icon_create icon = {
		wimp_ICON_BAR_RIGHT,
		{ { 0, 0, 68, 68 },
		wimp_ICON_SPRITE | wimp_ICON_HCENTRED | wimp_ICON_VCENTRED |
				(wimp_BUTTON_CLICK << wimp_ICON_BUTTON_TYPE_SHIFT),
		{ "!netsurf" } } };
	wimp_create_icon(&icon);
}


/**
 * Warn the user if Inet$Resolvers is not set.
 */

void ro_gui_check_resolvers(void)
{
	char *resolvers;
	resolvers = getenv("Inet$Resolvers");
	if (resolvers && resolvers[0]) {
		LOG(("Inet$Resolvers '%s'", resolvers));
	} else {
		LOG(("Inet$Resolvers not set or empty"));
		warn_user("Resolvers", 0);
	}
}


/**
 * Close down the gui (RISC OS).
 */

void gui_quit(void)
{
	ro_gui_history_quit();
	free(gui_sprites);
	wimp_close_down(task_handle);
	xhourglass_off();
}


/**
 * Poll the OS for events (RISC OS).
 *
 * \param active return as soon as possible
 */

void gui_poll(bool active)
{
	wimp_event_no event;
	wimp_block block;
	const wimp_poll_flags mask = wimp_MASK_LOSE | wimp_MASK_GAIN;
	gui_window *g;

	/* Process queued events. */
	while (ro_gui_poll_queued_blocks) {
		struct ro_gui_poll_block *next;
		ro_gui_handle_event(ro_gui_poll_queued_blocks->event,
				ro_gui_poll_queued_blocks->block);
		next = ro_gui_poll_queued_blocks->next;
		free(ro_gui_poll_queued_blocks->block);
		free(ro_gui_poll_queued_blocks);
		ro_gui_poll_queued_blocks = next;
	}

	/* Poll wimp. */
	xhourglass_off();
	if (active) {
		event = wimp_poll(mask, &block, 0);
	} else if (sched_active && (over_window || gui_reformat_pending)) {
		os_t t = os_read_monotonic_time() + 10;
		if (sched_time < t)
			t = sched_time;
		event = wimp_poll_idle(mask, &block, t, 0);
	} else if (sched_active) {
		event = wimp_poll_idle(mask, &block, sched_time, 0);
	} else if (over_window || gui_reformat_pending) {
		os_t t = os_read_monotonic_time();
		event = wimp_poll_idle(mask, &block, t + 10, 0);
	} else {
		event = wimp_poll(wimp_MASK_NULL | mask, &block, 0);
	}
	xhourglass_on();
	gui_last_poll = clock();
	ro_gui_handle_event(event, &block);
	schedule_run();

	if (gui_reformat_pending && event == wimp_NULL_REASON_CODE) {
		for (g = window_list; g; g = g->next) {
			if (g->type == GUI_BROWSER_WINDOW && g->data.browser.reformat_pending) {
				content_reformat(g->data.browser.bw->current_content,
						g->data.browser.old_width / 2 / g->scale,
						1000);
				g->data.browser.reformat_pending = false;
			}
		}
		gui_reformat_pending = false;
	}
}


/**
 * Process a Wimp_Poll event.
 *
 * \param event wimp event number
 * \param block parameter block
 */

void ro_gui_handle_event(wimp_event_no event, wimp_block *block)
{
	switch (event) {
		case wimp_NULL_REASON_CODE:
			ro_gui_null_reason_code();
			break;

		case wimp_REDRAW_WINDOW_REQUEST:
			ro_gui_redraw_window_request(&block->redraw);
			break;

		case wimp_OPEN_WINDOW_REQUEST:
			ro_gui_open_window_request(&block->open);
			break;

		case wimp_CLOSE_WINDOW_REQUEST:
			ro_gui_close_window_request(&block->close);
			break;

		case wimp_POINTER_LEAVING_WINDOW:
			if (over_window == (gui_window*)history_window)
				wimp_close_window(dialog_tooltip);
			over_window = 0;
			gui_window_set_pointer(GUI_POINTER_DEFAULT);
			break;

		case wimp_POINTER_ENTERING_WINDOW:
			over_window = ro_lookup_gui_from_w(block->entering.w);
			if (over_window == 0 && block->entering.w == history_window)
				over_window = (gui_window*)history_window;
			break;

		case wimp_MOUSE_CLICK:
			ro_gui_mouse_click(&block->pointer);
			break;

		case wimp_USER_DRAG_BOX:
			ro_gui_drag_end(&(block->dragged));
			break;

		case wimp_KEY_PRESSED:
			ro_gui_keypress(&(block->key));
			break;

		case wimp_MENU_SELECTION:
			ro_gui_menu_selection(&(block->selection));
			break;

		case wimp_SCROLL_REQUEST:
			ro_gui_scroll_request(&(block->scroll));
			break;

		case wimp_USER_MESSAGE:
		case wimp_USER_MESSAGE_RECORDED:
		case wimp_USER_MESSAGE_ACKNOWLEDGE:
			ro_gui_user_message(event, &(block->message));
			break;
	}
}


/**
 * Check for important events and yield CPU (RISC OS).
 *
 * Required on RISC OS for cooperative multitasking.
 */

void gui_multitask(void)
{
	wimp_event_no event;
	wimp_block block;

	if (clock() < gui_last_poll + 10)
		return;

	xhourglass_off();
	event = wimp_poll(wimp_MASK_LOSE | wimp_MASK_GAIN, &block, 0);
	xhourglass_on();
	gui_last_poll = clock();

	switch (event) {
		case wimp_CLOSE_WINDOW_REQUEST:
			/* \todo close the window, and destroy content
			 * or abort loading of content */
			break;

		case wimp_KEY_PRESSED:
		case wimp_MENU_SELECTION:
			ro_gui_poll_queue(event, &block);
			break;

		default:
			ro_gui_handle_event(event, &block);
			break;
	}
}


/**
 * Add a wimp_block to the queue for later handling.
 */

void ro_gui_poll_queue(wimp_event_no event, wimp_block *block)
{
	struct ro_gui_poll_block *q =
			xcalloc(1, sizeof(struct ro_gui_poll_block));

	q->event = event;
	q->block = xcalloc(1, sizeof(*block));
	memcpy(q->block, block, sizeof(*block));
	q->next = NULL;

	if (ro_gui_poll_queued_blocks == NULL) {
		ro_gui_poll_queued_blocks = q;
		return;
	} else {
		struct ro_gui_poll_block *current =
				ro_gui_poll_queued_blocks;
		while (current->next != NULL)
			current = current->next;
		current->next = q;
	}
	return;
}


/**
 * Handle Null_Reason_Code events.
 */

void ro_gui_null_reason_code(void)
{
	ro_gui_throb();
	if (over_window) {
		wimp_pointer pointer;
		wimp_get_pointer_info(&pointer);
		ro_gui_window_mouse_at(&pointer);
	}
}


/**
 * Handle Redraw_Window_Request events.
 */

void ro_gui_redraw_window_request(wimp_draw *redraw)
{
	gui_window *g;

	if (redraw->w == dialog_config_th_pane)
		ro_gui_redraw_config_th_pane(redraw);
	else if (redraw->w == history_window)
		ro_gui_history_redraw(redraw);
	else if (redraw->w == hotlist_window)
		ro_gui_hotlist_redraw(redraw);
	else if (redraw->w == dialog_debug)
		ro_gui_debugwin_redraw(redraw);
	else {
		g = ro_lookup_gui_from_w(redraw->w);
		if (g != NULL)
			ro_gui_window_redraw(g, redraw);
		else {
			osbool more = wimp_redraw_window(redraw);
			while (more)
				more = wimp_get_rectangle(redraw);
		}
	}
}


/**
 * Handle Open_Window_Request events.
 */

void ro_gui_open_window_request(wimp_open *open)
{
	struct toolbar *toolbar;
	gui_window *g;

	g = ro_lookup_gui_from_w(open->w);
	if (g) {
		ro_gui_window_open(g, open);
	} else {
		wimp_open_window(open);
		g = ro_lookup_gui_status_from_w(open->w);
		if (g) {
			toolbar = g->data.browser.toolbar;
			if (toolbar) {
				toolbar->resize_status = 1;
				ro_theme_resize_toolbar(g);
			}
		}
	}
}


/**
 * Handle Close_Window_Request events.
 */

void ro_gui_close_window_request(wimp_close *close)
{
	gui_window *g;
	struct gui_download_window *dw;

	if (close->w == dialog_debug)
		ro_gui_debugwin_close();
	else if ((g = ro_gui_window_lookup(close->w)))
		browser_window_destroy(g->data.browser.bw);
	else if ((dw = ro_gui_download_window_lookup(close->w)))
		ro_gui_download_window_destroy(dw);
	else
		ro_gui_dialog_close(close->w);
}


/**
 * Handle Mouse_Click events.
 */

void ro_gui_mouse_click(wimp_pointer *pointer)
{
	gui_window *g = ro_gui_window_lookup(pointer->w);
	struct gui_download_window *dw;

	if (pointer->w == wimp_ICON_BAR)
		ro_gui_icon_bar_click(pointer);
	else if (pointer->w == history_window)
		ro_gui_history_click(pointer);
	else if (pointer->w == hotlist_window)
		ro_gui_hotlist_click(pointer);
	else if (g && g->type == GUI_BROWSER_WINDOW && g->window == pointer->w)
		ro_gui_window_click(g, pointer);
	else if (g && g->type == GUI_BROWSER_WINDOW &&
			g->data.browser.toolbar->toolbar_handle == pointer->w)
		ro_gui_toolbar_click(g, pointer);
	else if (g && g->type == GUI_BROWSER_WINDOW &&
			g->data.browser.toolbar->status_handle == pointer->w)
		ro_gui_status_click(g, pointer);
	else if ((dw = ro_gui_download_window_lookup(pointer->w)))
		ro_gui_download_window_click(dw, pointer);
	else if (pointer->w == dialog_saveas)
		ro_gui_save_click(pointer);
	else
		ro_gui_dialog_click(pointer);
}


/**
 * Handle Mouse_Click events on the iconbar icon.
 */

void ro_gui_icon_bar_click(wimp_pointer *pointer)
{
	if (pointer->buttons == wimp_CLICK_MENU) {
		ro_gui_create_menu(iconbar_menu, pointer->pos.x - 64,
				   96 + iconbar_menu_height, NULL);
	} else if (pointer->buttons == wimp_CLICK_SELECT) {
		char url[80];
		int length;

		if ((length = snprintf(url, sizeof(url),
				"file:/<NetSurf$Dir>/Docs/intro_%s",
				option_language)) >= 0 && length < (int)sizeof(url))
			browser_window_create(url, NULL);
	} else if (pointer->buttons == wimp_CLICK_ADJUST) {
		ro_gui_debugwin_open();
	}
}


/**
 * Handle User_Drag_Box events.
 */

void ro_gui_drag_end(wimp_dragged *drag)
{
	switch (gui_current_drag_type) {
		case GUI_DRAG_SELECTION:
			ro_gui_selection_drag_end(drag);
			break;

		case GUI_DRAG_DOWNLOAD_SAVE:
			ro_gui_download_drag_end(drag);
			break;

		case GUI_DRAG_SAVE:
			ro_gui_save_drag_end(drag);
			break;

		case GUI_DRAG_STATUS_RESIZE:
//			ro_gui_save_drag_end(drag);
			break;
	}
}


/**
 * Handle Key_Pressed events.
 */

void ro_gui_keypress(wimp_key *key)
{
	bool handled = false;
	gui_window *g = ro_gui_window_lookup(key->w);

	if (!g) {
		handled = ro_gui_dialog_keypress(key);
		if (!handled)
			wimp_process_key(key->c);
		return;
	}

	switch (g->type) {
		case GUI_BROWSER_WINDOW:
			handled = ro_gui_window_keypress(g, key->c,
					(bool) (g->data.browser.toolbar->toolbar_handle == key->w));
			break;
	}

	if (!handled)
		wimp_process_key(key->c);
}


/**
 * Handle the three User_Message events.
 */

void ro_gui_user_message(wimp_event_no event, wimp_message *message)
{
	switch (message->action) {
		case message_HELP_REQUEST:
			ro_gui_interactive_help_request(message);
			break;

		case message_DATA_SAVE:
			ro_msg_datasave(message);
			break;

		case message_DATA_SAVE_ACK:
			ro_msg_datasave_ack(message);
			break;

		case message_DATA_LOAD:
			ro_msg_dataload(message);
			break;

		case message_DATA_OPEN:
			ro_msg_dataopen(message);
			break;

		case message_MENU_WARNING:
			ro_gui_menu_warning((wimp_message_menu_warning *)
					&message->data);
			break;

#ifdef WITH_URI
		case message_URI_PROCESS:
			if (event != wimp_USER_MESSAGE_ACKNOWLEDGE)
				ro_uri_message_received(message);
			break;
		case message_URI_RETURN_RESULT:
			ro_uri_bounce(message);
			break;
#endif
#ifdef WITH_URL
		case message_INET_SUITE_OPEN_URL:
			if (event == wimp_USER_MESSAGE_ACKNOWLEDGE) {
				ro_url_bounce(message);
			}
			else {
				ro_url_message_received(message);
			}
			break;
#endif
#ifdef WITH_PLUGIN
		case message_PLUG_IN_OPENING:
		case message_PLUG_IN_CLOSED:
		case message_PLUG_IN_RESHAPE_REQUEST:
		case message_PLUG_IN_FOCUS:
		case message_PLUG_IN_URL_ACCESS:
		case message_PLUG_IN_STATUS:
		case message_PLUG_IN_BUSY:
		case message_PLUG_IN_STREAM_NEW:
		case message_PLUG_IN_STREAM_WRITE:
		case message_PLUG_IN_STREAM_WRITTEN:
		case message_PLUG_IN_STREAM_DESTROY:
		case message_PLUG_IN_OPEN:
		case message_PLUG_IN_CLOSE:
		case message_PLUG_IN_RESHAPE:
		case message_PLUG_IN_STREAM_AS_FILE:
		case message_PLUG_IN_NOTIFY:
		case message_PLUG_IN_ABORT:
		case message_PLUG_IN_ACTION:
			plugin_msg_parse(message,
					event == wimp_USER_MESSAGE_ACKNOWLEDGE);
			break;
#endif

		case message_QUIT:
			netsurf_quit = true;
			break;
	}
}


void gui_gadget_combo(struct browser_window* bw, struct form_control* g, unsigned long mx, unsigned long my)
{
	int count;
	struct form_option* o;
	wimp_pointer pointer;

	if (combo_menu != NULL)
		xfree(combo_menu);

	for (count = 0, o = g->data.select.items; o != NULL; ++count, o = o->next)
		/* no body */;

	combo_menu = xcalloc(1, wimp_SIZEOF_MENU(count));

	combo_menu->title_data.indirected_text.text =
			messages_get("SelectMenu");
	combo_menu->title_fg = wimp_COLOUR_BLACK;
	combo_menu->title_bg = wimp_COLOUR_LIGHT_GREY;
	combo_menu->work_fg = wimp_COLOUR_BLACK;
	combo_menu->work_bg = wimp_COLOUR_WHITE;
	combo_menu->width = 0;
	combo_menu->height = wimp_MENU_ITEM_HEIGHT;
	combo_menu->gap = wimp_MENU_ITEM_GAP;

	for (count = 0, o = g->data.select.items; o != NULL; ++count, o = o->next) {
		combo_menu->entries[count].menu_flags = 0;
		if (count == 0)
		  combo_menu->entries[count].menu_flags = wimp_MENU_TITLE_INDIRECTED;
		if (o->selected)
		  combo_menu->entries[count].menu_flags |= wimp_MENU_TICKED;
		if (o->next == NULL)
		  combo_menu->entries[count].menu_flags |= wimp_MENU_LAST;

		combo_menu->entries[count].sub_menu = wimp_NO_SUB_MENU;
		combo_menu->entries[count].icon_flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED | wimp_ICON_FILLED | wimp_ICON_VCENTRED | (wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) | (wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT) | (wimp_BUTTON_MENU_ICON << wimp_ICON_BUTTON_TYPE_SHIFT);
		combo_menu->entries[count].data.indirected_text.text = o->text;
		combo_menu->entries[count].data.indirected_text.validation = "\0";
		combo_menu->entries[count].data.indirected_text.size = strlen(o->text) + 1;
	}

	wimp_get_pointer_info(&pointer);
	current_gadget = g;
	ro_gui_create_menu(combo_menu, pointer.pos.x - 64, pointer.pos.y, bw->window);
}

void ro_msg_datasave(wimp_message* block)
{
	gui_window* gui;
	struct browser_window* bw;
	wimp_message_data_xfer* data;
	int x,y;
	struct box_selection* click_boxes;
	int found, plot_index;
	int i;
	wimp_window_state state;

	data = &block->data.data_xfer;

	gui = ro_lookup_gui_from_w(data->w);
	if (gui == NULL)
		return;

	bw = gui->data.browser.bw;

	state.w = data->w;
	wimp_get_window_state(&state);
	x = window_x_units(data->pos.x, &state) / 2;
	y = -window_y_units(data->pos.y, &state) / 2;

	found = 0;
	click_boxes = NULL;
	plot_index = 0;

	box_under_area(bw->current_content,
		 bw->current_content->data.html.layout->children,
		 (unsigned int)x, (unsigned int)y, 0, 0, &click_boxes,
		 &found, &plot_index);

	if (found == 0)
		return;

	for (i = found - 1; i >= 0; i--)
	{
		if (click_boxes[i].box->gadget != NULL)
		{
			if (click_boxes[i].box->gadget->type == GADGET_TEXTAREA && data->file_type == 0xFFF)
			{
				/* load the text in! */
				fprintf(stderr, "REPLYING TO MESSAGE MATE\n");
				block->action = message_DATA_SAVE_ACK;
				block->your_ref = block->my_ref;
				block->my_ref = 0;
				strcpy(block->data.data_xfer.file_name, "<Wimp$Scrap>");
				wimp_send_message(wimp_USER_MESSAGE, block, block->sender);
			}
		}
	}

	xfree(click_boxes);
}


/**
 * Handle Message_DataLoad (file dragged in).
 */

void ro_msg_dataload(wimp_message *message)
{
	char *url = 0;
	gui_window *gui;

	gui = ro_lookup_gui_from_w(message->data.data_xfer.w);

	if (gui) {
		if (ro_gui_window_dataload(gui, message))
			return;
	}

	if (message->data.data_xfer.file_type != 0xfaf &&
			message->data.data_xfer.file_type != 0x695 &&
			message->data.data_xfer.file_type != 0xaff &&
			message->data.data_xfer.file_type != 0xb60 &&
			message->data.data_xfer.file_type != 0xc85 &&
			message->data.data_xfer.file_type != 0xff9 &&
			message->data.data_xfer.file_type != 0xfff &&
			message->data.data_xfer.file_type != 0xf91 &&
			message->data.data_xfer.file_type != 0xb28)
		return;

	/* uri file
	 * Format: Each "line" is separated by a tab.
	 *         Comments are prefixed by a "#"
	 *
	 * Line:        Content:
	 *      1       URI
	 *      2       100 (version of file format * 100)
	 *      3       An URL (eg http;//www.google.com/)
	 *      4       Title associated with URL (eg Google)
	 */
	if (message->data.data_xfer.file_type == 0xf91) {
		char *buf, *temp;
		int lineno=0;
		buf = load(message->data.data_xfer.file_name);
		temp = strtok(buf, "\t");

		if (!temp) {
			xfree(buf);
			return;
		}

		if (temp[0] != '#') lineno = 1;

		while (temp && lineno<=2) {
			temp = strtok('\0', "\t");
			if (!temp) break;
			if (temp[0] == '#') continue; /* ignore commented lines */
			lineno++;
		}

		if (!temp) {
			xfree(buf);
			return;
		}

		url = xstrdup(temp);

		xfree(buf);
	}

	/* url file */
	if (message->data.data_xfer.file_type == 0xb28) {
		char *temp;
		FILE *fp = fopen(message->data.data_xfer.file_name, "r");

		if (!fp) return;

		url = xcalloc(256, sizeof(char));

		temp = fgets(url, 256, fp);

		fclose(fp);

		if (!temp) return;

		if (url[strlen(url)-1] == '\n') {
			url[strlen(url)-1] = '\0';
		}
	}

	/* send DataLoadAck */
	message->action = message_DATA_LOAD_ACK;
	message->your_ref = message->my_ref;
	wimp_send_message(wimp_USER_MESSAGE, message, message->sender);

	/* create a new window with the file */
	if (message->data.data_xfer.file_type != 0xb28 &&
	    message->data.data_xfer.file_type != 0xf91) {
		url = ro_path_to_url(message->data.data_xfer.file_name);
	}
	if (!url)
		return;

	if (gui) {
		gui_window_set_url(gui, url);
		browser_window_go(gui->data.browser.bw, url);
	}
	else {
		browser_window_create(url, NULL);
	}

	free(url);


#if 0
	gui_window* gui;
	struct browser_window* bw;
	wimp_message_data_xfer* data;
	int x,y;
	struct box_selection* click_boxes;
	int found, plot_index;
	int i;
	wimp_window_state state;

	data = &block->data.data_xfer;

	gui = ro_lookup_gui_from_w(data->w);
	if (gui == NULL)
		return;

	bw = gui->data.browser.bw;

	state.w = data->w;
	wimp_get_window_state(&state);
	x = window_x_units(data->pos.x, &state) / 2;
	y = -window_y_units(data->pos.y, &state) / 2;

	found = 0;
	click_boxes = NULL;
	plot_index = 0;

	box_under_area(bw->current_content,
		 bw->current_content->data.html.layout->children,
		 (unsigned int)x, (unsigned int)y, 0, 0, &click_boxes,
		 &found, &plot_index);

	if (found == 0)
		return;

	for (i = found - 1; i >= 0; i--)
	{
		if (click_boxes[i].box->gadget != NULL)
		{
			if (click_boxes[i].box->gadget->type == GADGET_TEXTAREA && data->file_type == 0xFFF)
			{
				/* load the text in! */
				/* TODO */
			}
		}
	}

	xfree(click_boxes);
#endif
}


/**
 * Handle Message_DataSaveAck.
 */

void ro_msg_datasave_ack(wimp_message *message)
{
	switch (gui_current_drag_type) {
		case GUI_DRAG_DOWNLOAD_SAVE:
			ro_gui_download_datasave_ack(message);
			break;

		case GUI_DRAG_SAVE:
			ro_gui_save_datasave_ack(message);
			break;

		default:
			break;
	}
}


/**
 * Handle Message_DataOpen (double-click on file in the Filer).
 */

void ro_msg_dataopen(wimp_message *message)
{
	char *url = 0;

	if (message->data.data_xfer.file_type != 0xfaf &&
	    message->data.data_xfer.file_type != 0xb28)
		/* ignore all but HTML and URL */
		return;

	/* url file */
	if (message->data.data_xfer.file_type == 0xb28) {
		char *temp;
		FILE *fp = fopen(message->data.data_xfer.file_name, "r");

		if (!fp) return;

		url = xcalloc(256, sizeof(char));

		temp = fgets(url, 256, fp);

		fclose(fp);

		if (!temp) return;

		if (url[strlen(url)-1] == '\n') {
			url[strlen(url)-1] = '\0';
		}
	}

	/* send DataLoadAck */
	message->action = message_DATA_LOAD_ACK;
	message->your_ref = message->my_ref;
	wimp_send_message(wimp_USER_MESSAGE, message, message->sender);

	/* create a new window with the file */
	if (message->data.data_xfer.file_type != 0xb28) {
		url = ro_path_to_url(message->data.data_xfer.file_name);
	}
	if (url) {
		browser_window_create(url, NULL);
		free(url);
	}
}


/**
 * Convert a RISC OS pathname to a file: URL.
 *
 * \param  path  RISC OS pathname
 * \return  URL, allocated on heap, or 0 on failure
 */

char *ro_path_to_url(const char *path)
{
	int spare;
	char *buffer = 0;
	char *url = 0;
	os_error *error;

	error = xosfscontrol_canonicalise_path(path, 0, 0, 0, 0, &spare);
	if (error) {
		LOG(("xosfscontrol_canonicalise_path failed: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("PathToURL", error->errmess);
		return 0;
	}

	buffer = malloc(1 - spare);
	url = malloc(1 - spare + 10);
	if (!buffer || !url) {
		LOG(("malloc failed"));
		warn_user("NoMemory", 0);
		free(buffer);
		free(url);
		return 0;
	}

	error = xosfscontrol_canonicalise_path(path, buffer, 0, 0, 1 - spare,
			0);
	if (error) {
		LOG(("xosfscontrol_canonicalise_path failed: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("PathToURL", error->errmess);
		free(buffer);
		free(url);
		return 0;
	}

	strcpy(url, "file:");
	__unixify(buffer, __RISCOSIFY_NO_REVERSE_SUFFIX, url + 5,
			1 - spare + 5, 0);
	free(buffer);
	return url;
}


/**
 * Find screen size in OS units.
 */

void ro_gui_screen_size(int *width, int *height)
{
	int xeig_factor, yeig_factor, xwind_limit, ywind_limit;

	os_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_XEIG_FACTOR, &xeig_factor);
	os_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_YEIG_FACTOR, &yeig_factor);
	os_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_XWIND_LIMIT, &xwind_limit);
	os_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_YWIND_LIMIT, &ywind_limit);
	*width = (xwind_limit + 1) << xeig_factor;
	*height = (ywind_limit + 1) << yeig_factor;
}


/**
 * Opens a language sensitive help page
 *
 * \param page  the page to open
 */
void ro_gui_open_help_page(const char *page)
{
	char url[80];
	int length;

	if ((length = snprintf(url, sizeof url,
			"file:/<NetSurf$Dir>/Docs/%s_%s",
			page, option_language)) >= 0 && length < (int)sizeof(url))
		browser_window_create(url, NULL);
}

/**
 * Send the source of a content to a text editor.
 */

void ro_gui_view_source(struct content *content)
{
	xosfile_save_stamped("<Wimp$Scrap>", 0xfff,
			content->source_data,
			content->source_data + content->source_size);
	xos_cli("Filer_Run <Wimp$Scrap>");
	xosfile_set_type("<Wimp$Scrap>", ro_content_filetype(content));
}


/**
 * Broadcast an URL that we can't handle.
 */

void gui_launch_url(const char *url)
{
	/* Try ant broadcast first */
	ro_url_broadcast(url);
}


static char warn_buffer[300];

/**
 * Display a warning for a serious problem (eg memory exhaustion).
 *
 * \param  warning  message key for warning message
 * \param  detail   additional message, or 0
 */

void warn_user(const char *warning, const char *detail)
{
	LOG(("%s %s", warning, detail));
	snprintf(warn_buffer, sizeof warn_buffer, "%s %s",
			messages_get(warning),
			detail ? detail : "");
	warn_buffer[sizeof warn_buffer - 1] = 0;
	ro_gui_set_icon_string(dialog_warning, ICON_WARNING_MESSAGE,
			warn_buffer);
	xwimp_set_icon_state(dialog_warning, ICON_WARNING_HELP,
			wimp_ICON_DELETED, wimp_ICON_DELETED);
	ro_gui_dialog_open(dialog_warning);
	xos_bell();
}


static os_error warn_error = { 1, "" };


/**
 * Display an error and exit.
 *
 * Should only be used during initialisation.
 */

void die(const char *error)
{
	strncpy(warn_error.errmess, messages_get(error), 252);
	xwimp_report_error_by_category(&warn_error,
			wimp_ERROR_BOX_OK_ICON |
			wimp_ERROR_BOX_GIVEN_CATEGORY |
			wimp_ERROR_BOX_CATEGORY_ERROR <<
				wimp_ERROR_BOX_CATEGORY_SHIFT,
			"NetSurf", "!netsurf",
			(osspriteop_area *) 1, 0, 0);
	exit(EXIT_FAILURE);
}

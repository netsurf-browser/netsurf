/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unixlib/local.h>
#include "oslib/hourglass.h"
#include "oslib/inetsuite.h"
#include "oslib/os.h"
#include "oslib/osfile.h"
#include "oslib/plugin.h"
#include "oslib/wimp.h"
#include "oslib/uri.h"
#include "netsurf/utils/config.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/options.h"
#include "netsurf/render/font.h"
#include "netsurf/render/form.h"
#include "netsurf/render/html.h"
#ifdef WITH_ABOUT
#include "netsurf/riscos/about.h"
#endif
#include "netsurf/riscos/constdata.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#ifdef WITH_PLUGIN
#include "netsurf/riscos/plugin.h"
#endif
#include "netsurf/riscos/theme.h"
#ifdef WITH_URI
#include "netsurf/riscos/uri.h"
#endif
#ifdef WITH_URL
#include "netsurf/riscos/url.h"
#endif
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


const char *__dynamic_da_name = "NetSurf";	/**< For UnixLib. */

char *NETSURF_DIR;
wimp_menu *combo_menu;
struct form_control *current_gadget;
gui_window *over_window = 0;	/**< Window which the pointer is over. */
bool gui_reformat_pending = false;	/**< Some windows have been resized,
						and should be reformatted. */
static wimp_t task_handle;	/**< RISC OS wimp task handle. */
/** Accepted wimp user messages. */
static const wimp_MESSAGE_LIST(25) task_messages = { {
	message_DATA_SAVE,
	message_DATA_SAVE_ACK,
	message_DATA_LOAD,
	message_DATA_OPEN,
#ifdef WITH_URI
	message_URI_PROCESS,
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


static void ro_gui_icon_bar_create(void);
static void ro_gui_handle_event(wimp_event_no event, wimp_block *block);
static void ro_gui_poll_queue(wimp_event_no event, wimp_block* block);
static void ro_gui_null_reason_code(void);
static void ro_gui_redraw_window_request(wimp_draw *redraw);
static void ro_gui_open_window_request(wimp_open *open);
static void ro_gui_close_window_request(wimp_close *close);
static void ro_gui_mouse_click(wimp_pointer *pointer);
static void ro_gui_icon_bar_click(wimp_pointer* pointer);
static void ro_gui_keypress(wimp_key* key);
static void ro_gui_user_message(wimp_event_no event, wimp_message *message);
static void ro_msg_datasave(wimp_message* block);
static void ro_msg_dataload(wimp_message* block);
static void ro_msg_datasave_ack(wimp_message* message);
static int ro_save_data(void *data, unsigned long length, char *file_name, bits file_type);
static void ro_msg_dataopen(wimp_message* block);
static char *ro_path_to_url(const char *path);


/**
 * Initialise the gui (RISC OS specific part).
 */

void gui_init(int argc, char** argv)
{
	char theme_fname[256];
	os_error *e;

	xhourglass_start(1);

	NETSURF_DIR = getenv("NetSurf$Dir");
	messages_load("<NetSurf$Dir>.Resources.en.Messages");

	task_handle = wimp_initialise(wimp_VERSION_RO38, "NetSurf",
  			(wimp_message_list*) &task_messages, 0);

	/* Issue a *Desktop to poke AcornURI into life */
	if (getenv("NetSurf$Start_URI_Handler"))
		xwimp_start_task("Desktop", 0);

	options_read("Choices:WWW.NetSurf.Choices");

	if (option_theme) {
		snprintf(theme_fname, sizeof(theme_fname),
				"<NetSurf$Dir>.Themes.%s", option_theme);
	        /* check if theme directory exists */
		if (!is_dir(theme_fname)) {
			free(option_theme);
			option_theme = 0;
			sprintf(theme_fname, "<NetSurf$Dir>.Themes.Default");
		}
	} else {
		strcpy(theme_fname, "<NetSurf$Dir>.Themes.Default");
	}
	ro_theme_load(theme_fname);

	e = xwimp_open_template("<NetSurf$Dir>.Resources.en.Templates");
	if(e) {
	  die(e->errmess);
	}
	ro_gui_dialog_init();
	ro_gui_download_init();
	ro_gui_menus_init();
#ifdef WITH_AUTH
	ro_gui_401login_init();
#endif
	ro_gui_history_init();
	wimp_close_template();
	ro_gui_icon_bar_create();
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
 * Close down the gui (RISC OS).
 */

void gui_quit(void)
{
#ifdef WITH_ABOUT
        about_quit();
#endif
	ro_gui_history_quit();
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
	} else if (over_window || gui_reformat_pending) {
		os_t t = os_read_monotonic_time();
		event = wimp_poll_idle(mask, &block, t + 10, 0);
	} else {
		event = wimp_poll(wimp_MASK_NULL | mask, &block, 0);
	}
	xhourglass_on();
	ro_gui_handle_event(event, &block);

        if (gui_reformat_pending && event == wimp_NULL_REASON_CODE) {
		for (g = window_list; g; g = g->next) {
			if (g->type == GUI_BROWSER_WINDOW && g->data.browser.reformat_pending) {
				content_reformat(g->data.browser.bw->current_content,
						browser_x_units(g->data.browser.old_width), 1000);
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
			over_window = 0;
			break;

		case wimp_POINTER_ENTERING_WINDOW:
			over_window = ro_lookup_gui_from_w(block->entering.w);
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

	xhourglass_off();
	event = wimp_poll(wimp_MASK_LOSE | wimp_MASK_GAIN, &block, 0);
	xhourglass_on();

	switch (event) {
		case wimp_CLOSE_WINDOW_REQUEST:
			/* \todo close the window, and destroy content
			 * or abort loading of content */
			break;

		case wimp_KEY_PRESSED:
		case wimp_MENU_SELECTION:
		case wimp_USER_MESSAGE:
		case wimp_USER_MESSAGE_RECORDED:
		case wimp_USER_MESSAGE_ACKNOWLEDGE:
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
        if (over_window != NULL
            || current_drag.type == draginfo_BROWSER_TEXT_SELECTION)
        {
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

	if (redraw->w == dialog_config_th)
		ro_gui_redraw_config_th(redraw);
	else if (redraw->w == history_window)
		ro_gui_history_redraw(redraw);
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
	gui_window *g;

	g = ro_lookup_gui_from_w(open->w);
	if (g)
		ro_gui_window_open(g, open);
	else
		wimp_open_window(open);
}


/**
 * Handle Close_Window_Request events.
 */

void ro_gui_close_window_request(wimp_close *close)
{
	gui_window *g;

	g = ro_lookup_gui_from_w(close->w);

	if (g) {
		browser_window_destroy(g->data.browser.bw
#ifdef WITH_FRAMES
				, true
#endif
				);
#ifdef WITH_COOKIES
	clean_cookiejar();
#endif
	} else
		ro_gui_dialog_close(close->w);
}


/**
 * Handle Mouse_Click events.
 */

void ro_gui_mouse_click(wimp_pointer *pointer)
{
	gui_window *g = ro_gui_window_lookup(pointer->w);

	if (pointer->w == wimp_ICON_BAR)
		ro_gui_icon_bar_click(pointer);
	else if (pointer->w == history_window)
		ro_gui_history_click(pointer);
	else if (g && g->type == GUI_BROWSER_WINDOW &&
			g->window == pointer->w) {
		if (g->redraw_safety == SAFE)
			ro_gui_window_click(g, pointer);
		else
			ro_gui_poll_queue(wimp_MOUSE_CLICK,
					(wimp_block *) pointer);
	} else if (g && g->type == GUI_BROWSER_WINDOW &&
			g->data.browser.toolbar == pointer->w)
		ro_gui_toolbar_click(g, pointer);
	else if (g && g->type == GUI_DOWNLOAD_WINDOW)
		ro_download_window_click(g, pointer);
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
		struct browser_window *bw;
		bw = create_browser_window(browser_TITLE | browser_TOOLBAR
					   | browser_SCROLL_X_ALWAYS |
					   browser_SCROLL_Y_ALWAYS, 640,
					   480
#ifdef WITH_FRAMES
					   , NULL
#endif
		    );
		gui_window_show(bw->window);
		browser_window_open_location(bw, HOME_URL);
		wimp_set_caret_position(bw->window->data.browser.toolbar,
					ICON_TOOLBAR_URL,
					0, 0, -1,
					(int) strlen(bw->window->url));
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
					(bool) (g->data.browser.toolbar == key->w));
			break;

		case GUI_DOWNLOAD_WINDOW:
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

#ifdef WITH_URI
		case message_URI_PROCESS:
			ro_uri_message_received(message);
			break;
#endif
#ifdef WITH_URL
		case message_INET_SUITE_OPEN_URL:
			ro_url_message_received(message);
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
	int count = 0;
	struct form_option* o;
	wimp_pointer pointer;

	if (combo_menu != NULL)
		xfree(combo_menu);

	o = g->data.select.items;
	while (o != NULL)
	{
		count++;
		o = o->next;
	}

	combo_menu = xcalloc(1, wimp_SIZEOF_MENU(count));

	combo_menu->title_data.indirected_text.text = "Select";
	combo_menu->title_fg = wimp_COLOUR_BLACK;
	combo_menu->title_bg = wimp_COLOUR_LIGHT_GREY;
	combo_menu->work_fg = wimp_COLOUR_BLACK;
	combo_menu->work_bg = wimp_COLOUR_WHITE;
	combo_menu->width = 0;
	combo_menu->height = wimp_MENU_ITEM_HEIGHT;
	combo_menu->gap = wimp_MENU_ITEM_GAP;

	o = g->data.select.items;
	count = 0;
	while (o != NULL)
	{
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
		combo_menu->entries[count].data.indirected_text.size = strlen(o->text);
		count++;
		o = o->next;
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
  	x = browser_x_units(window_x_units(data->pos.x, &state));
  	y = browser_y_units(window_y_units(data->pos.y, &state));

  	found = 0;
	click_boxes = NULL;
	plot_index = 0;

	box_under_area(bw->current_content->data.html.layout->children,
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

void ro_msg_dataload(wimp_message* block)
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
  	x = browser_x_units(window_x_units(data->pos.x, &state));
  	y = browser_y_units(window_y_units(data->pos.y, &state));

  	found = 0;
	click_boxes = NULL;
	plot_index = 0;

	box_under_area(bw->current_content->data.html.layout->children,
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

}


void ro_msg_datasave_ack(wimp_message *message)
{
  int save_status = 0;

  LOG(("ACK Message: filename = %s", message->data.data_xfer.file_name));

  if (current_drag.type == draginfo_DOWNLOAD_SAVE)
  {
    assert(current_drag.data.download.gui->data.download.download_status ==
                   download_COMPLETE);


    save_status = ro_save_data(current_drag.data.download.gui->data.download.content->data.other.data,
                 current_drag.data.download.gui->data.download.content->data.other.length,
                 message->data.data_xfer.file_name,
                 current_drag.data.download.gui->data.download.file_type);


    if (save_status != 1)
    {
      LOG(("Could not save download data"));
      //Report_Error
    }
    else
    {
      ro_download_window_close(current_drag.data.download.gui);
      current_drag.type = draginfo_NONE;
    }
  }
}

int ro_save_data(void *data, unsigned long length, char *file_name, bits file_type)
{
  os_error *written = NULL;

  void *end_data = (void*)((int)data + length);

  written = xosfile_save_stamped(file_name, file_type, data, end_data);

  if (written != NULL)
  {
    LOG(("Unable to create stamped file"));
    return 0;
  }

  return 1;
}


/**
 * Handle Message_DataOpen (double-click on file in the Filer).
 */

void ro_msg_dataopen(wimp_message *message)
{
	char *url;
	struct browser_window *bw;

	if (message->data.data_xfer.file_type != 0xfaf)
		/* ignore all but HTML */
		return;

	/* send DataLoadAck */
	message->action = message_DATA_LOAD_ACK;
	message->your_ref = message->my_ref;
	wimp_send_message(wimp_USER_MESSAGE, message, message->sender);

	/* create a new window with the file */
	bw = create_browser_window(browser_TITLE | browser_TOOLBAR |
			browser_SCROLL_X_ALWAYS | browser_SCROLL_Y_ALWAYS, 640, 480
#ifdef WITH_FRAMES
			, NULL
#endif
			);
	gui_window_show(bw->window);
	url = ro_path_to_url(message->data.data_xfer.file_name);
	browser_window_open_location(bw, url);
	free(url);
}


/**
 * Convert a RISC OS pathname to a file: URL.
 */

char *ro_path_to_url(const char *path)
{
	unsigned int len = 20 + strlen(path);
	char *url = xcalloc(len, 1);
	strcpy(url, "file://");
	__unixify(path, __RISCOSIFY_NO_REVERSE_SUFFIX, url + 7, len - 7, 0);
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


void ro_gui_open_help_page (void)
{
        struct browser_window *bw;
        bw = create_browser_window(browser_TITLE | browser_TOOLBAR |
                                   browser_SCROLL_X_ALWAYS |
                                   browser_SCROLL_Y_ALWAYS, 640, 480
#ifdef WITH_FRAMES
                                   , NULL
#endif
                                   );
        gui_window_show(bw->window);
        browser_window_open_location(bw, HELP_URL);
        wimp_set_caret_position(bw->window->data.browser.toolbar,
                                ICON_TOOLBAR_URL,
                                0,0,-1, (int) strlen(bw->window->url));
}


/**
 * Send the source of a content to a text editor.
 */
void ro_gui_view_source(struct content *content)
{

        if (content->type == CONTENT_HTML) {
               	xosfile_save_stamped("<Wimp$Scrap>", 0xfff,
        		        content->data.html.source,
        		        (content->data.html.source +
        		        content->data.html.length));
                xos_cli("Filer_Run <Wimp$Scrap>");
                xosfile_set_type("<Wimp$Scrap>", 0xfaf);
        }
        else if (content->type == CONTENT_CSS) {
               	xosfile_save_stamped("<Wimp$Scrap>", 0xfff,
        		        content->data.css.data,
        		        (content->data.css.data +
        		        content->data.css.length));
                xos_cli("Filer_Run <Wimp$Scrap>");
                xosfile_set_type("<Wimp$Scrap>", 0xf79);
        }
}


void ro_gui_drag_box_start(wimp_pointer *pointer)
{
  wimp_drag *drag_box;
  wimp_window_state *icon_window;
  wimp_icon_state *icon_icon;
  int x0, y0;

  /* TODO: support drag_a_sprite */

  icon_window = xcalloc(1, sizeof(*icon_window));
  icon_icon = xcalloc(1, sizeof(*icon_icon));
  drag_box = xcalloc(1, sizeof(*drag_box));

  drag_box->w = pointer->w;
  drag_box->type = wimp_DRAG_USER_FIXED;

  icon_window->w = pointer->w;
  wimp_get_window_state(icon_window);

  x0 = icon_window->visible.x0 - icon_window->xscroll;
  y0 = icon_window->visible.y1 - icon_window->yscroll;

  icon_icon->w = pointer->w;
  icon_icon->i = pointer->i;
  wimp_get_icon_state(icon_icon);

  drag_box->initial.x0 =   x0 + icon_icon->icon.extent.x0;
  drag_box->initial.y0 =   y0 + icon_icon->icon.extent.y0;
  drag_box->initial.x1 =   x0 + icon_icon->icon.extent.x1;
  drag_box->initial.y1 =   y0 + icon_icon->icon.extent.y1;

  drag_box->bbox.x0 = 0x80000000;
  drag_box->bbox.y0 = 0x80000000;
  drag_box->bbox.x1 = 0x7FFFFFFF;       // CHANGE
  drag_box->bbox.y1 = 0x7FFFFFFF;

        /*if(USE_DRAGASPRITE == DRAGASPRITE_AVAILABLE)
          xdragasprite_start((dragasprite_HPOS_CENTRE ^
                              dragasprite_VPOS_CENTRE ^
                              dragasprite_NO_BOUND ^
                              dragasprite_BOUND_POINTER),
                                    (osspriteop_area*) 1,
                                    "file_fff",
                                    (os_box*)&drag_box->initial,0);*/

  wimp_drag_box(drag_box);

  xfree(drag_box);
  xfree(icon_window);
  xfree(icon_icon);

}


int ro_x_units(unsigned long browser_units)
{
  return (browser_units << 1);
}

int ro_y_units(unsigned long browser_units)
{
  return -(browser_units << 1);
}

unsigned long browser_x_units(int ro_units)
{
  return (ro_units >> 1);
}

unsigned long browser_y_units(int ro_units)
{
  return -(ro_units >> 1);
}

int window_x_units(int scr_units, wimp_window_state* win)
{
  return scr_units - (win->visible.x0 - win->xscroll);
}

int window_y_units(int scr_units, wimp_window_state* win)
{
  return scr_units - (win->visible.y1 - win->yscroll);
}

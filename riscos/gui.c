/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 */

#include <assert.h>
#include <errno.h>
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
#include "oslib/pdriver.h"
#include "oslib/plugin.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "oslib/uri.h"
#include "netsurf/utils/config.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/options.h"
#include "netsurf/render/font.h"
#include "netsurf/render/html.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/help.h"
#include "netsurf/riscos/options.h"
#ifdef WITH_PLUGIN
#include "netsurf/riscos/plugin.h"
#endif
#ifdef WITH_PRINT
#include "netsurf/riscos/print.h"
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
/* default filename handling */
int __riscosify_control = __RISCOSIFY_NO_SUFFIX |
			__RISCOSIFY_NO_REVERSE_SUFFIX;

char *NETSURF_DIR;

char *default_stylesheet_url;
char *adblock_stylesheet_url;

/** The pointer is over a window which is tracking mouse movement. */
static bool gui_track = false;
/** Handle of window which the pointer is over. */
static wimp_w gui_track_wimp_w;
/** Browser window which the pointer is over, or 0 if none. */
static struct gui_window *gui_track_gui_window;

/** Some windows have been resized, and should be reformatted. */
bool gui_reformat_pending = false;

gui_drag_type gui_current_drag_type;
wimp_t task_handle;	/**< RISC OS wimp task handle. */
static clock_t gui_last_poll;	/**< Time of last wimp_poll. */
osspriteop_area *gui_sprites;      /**< Sprite area containing pointer and hotlist sprites */

/** Accepted wimp user messages. */
static wimp_MESSAGE_LIST(34) task_messages = { {
  	message_HELP_REQUEST,
	message_DATA_SAVE,
	message_DATA_SAVE_ACK,
	message_DATA_LOAD,
	message_DATA_LOAD_ACK,
	message_DATA_OPEN,
	message_MENU_WARNING,
	message_MENUS_DELETED,
	message_MODE_CHANGE,
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
#ifdef WITH_PRINT
	message_PRINT_SAVE,
	message_PRINT_ERROR,
	message_PRINT_TYPE_ODD,
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
static void ro_gui_sprites_init(void);
static void ro_gui_icon_bar_create(void);
static void ro_gui_handle_event(wimp_event_no event, wimp_block *block);
static void ro_gui_poll_queue(wimp_event_no event, wimp_block *block);
static void ro_gui_null_reason_code(void);
static void ro_gui_redraw_window_request(wimp_draw *redraw);
static void ro_gui_open_window_request(wimp_open *open);
static void ro_gui_close_window_request(wimp_close *close);
static void ro_gui_pointer_leaving_window(wimp_leaving *leaving);
static void ro_gui_pointer_entering_window(wimp_entering *entering);
static void ro_gui_mouse_click(wimp_pointer *pointer);
static void ro_gui_icon_bar_click(wimp_pointer *pointer);
static void ro_gui_check_resolvers(void);
static void ro_gui_drag_end(wimp_dragged *drag);
static void ro_gui_keypress(wimp_key *key);
static void ro_gui_user_message(wimp_event_no event, wimp_message *message);
static void ro_msg_dataload(wimp_message *block);
static char *ro_gui_uri_file_parse(const char *file_name);
static bool ro_gui_uri_file_parse_line(FILE *fp, char *b);
static char *ro_gui_url_file_parse(const char *file_name);
static void ro_msg_datasave_ack(wimp_message *message);
static void ro_msg_dataopen(wimp_message *block);
static char *ro_path_to_url(const char *path);


/**
 * Initialise the gui (RISC OS specific part).
 */

void gui_init(int argc, char** argv)
{
	char path[40];
	os_error *error;
	int length;
	struct theme_descriptor *descriptor = NULL;

	xhourglass_start(1);

#ifdef WITH_SAVE_COMPLETE
	save_complete_init();
#endif

        /* We don't have the universal boot sequence on NCOS */
#ifndef ncos
	options_read("Choices:WWW.NetSurf.Choices");
#else
	options_read("<User$Path>.Choices.NetSurf.Choices");
#endif
	ro_gui_choose_language();

	NETSURF_DIR = getenv("NetSurf$Dir");
	if ((length = snprintf(path, sizeof(path),
			"<NetSurf$Dir>.Resources.%s.Messages",
			option_language)) < 0 || length >= (int)sizeof(path))
		die("Failed to locate Messages resource.");
	messages_load(path);
	messages_load("<NetSurf$Dir>.Resources.LangNames");

	default_stylesheet_url = strdup("file:/<NetSurf$Dir>/Resources/CSS");
	adblock_stylesheet_url = strdup("file:/<NetSurf$Dir>/Resources/AdBlock");

        /* Totally pedantic, but base the taskname on the build options.
        */
#ifndef ncos
	error = xwimp_initialise(wimp_VERSION_RO38, "NetSurf",
			(const wimp_message_list *) &task_messages, 0,
			&task_handle);
#else
	error = xwimp_initialise(wimp_VERSION_RO38, "NCNetSurf",
			(const wimp_message_list *) &task_messages, 0,
			&task_handle);
#endif
	if (error) {
		LOG(("xwimp_initialise failed: 0x%x: %s",
				error->errnum, error->errmess));
		die(error->errmess);
	}

        /* We don't need to check the fonts on NCOS */
#ifndef ncos
	ro_gui_check_fonts();
#endif
	nsfont_fill_nametable(false);

	/* Issue a *Desktop to poke AcornURI into life */
	if (getenv("NetSurf$Start_URI_Handler"))
		xwimp_start_task("Desktop", 0);

	/*	Load our chosen theme
  	*/
  	ro_gui_theme_initialise();
  	descriptor = ro_gui_theme_find(option_theme);
  	if (!descriptor) descriptor = ro_gui_theme_find("NetSurf");
  	ro_gui_theme_apply(descriptor);

	/*	Open the templates
	*/
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
	ro_gui_sprites_init();
	ro_gui_hotlist_init();

        /* We don't create an Iconbar icon on NCOS */
#ifndef ncos
	ro_gui_icon_bar_create();
#endif
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
		case 7: /* Germany */
			lang = "de";
			break;
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
 * Load resource sprites (pointers and misc icons).
 */

void ro_gui_sprites_init(void)
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
 * Last-minute gui init, after all other modules have initialised.
 */

void gui_init2(int argc, char** argv)
{
	char *url = 0;
	bool open_window = option_open_browser_at_startup;

	/* parse command-line arguments */
	if (argc == 2) {
		LOG(("parameters: '%s'", argv[1]));
		/* this is needed for launching URI files */
		if (strcasecmp(argv[1], "-nowin") == 0)
			open_window = false;
	}
	else if (argc == 3) {
		LOG(("parameters: '%s' '%s'", argv[1], argv[2]));
		open_window = true;

		/* HTML files */
		if (strcasecmp(argv[1], "-html") == 0) {
			url = ro_path_to_url(argv[2]);
			if (!url) {
				LOG(("malloc failed"));
				die("Insufficient memory for URL");
			}
		}
		/* URL files */
		else if (strcasecmp(argv[1], "-urlf") == 0) {
			url = ro_gui_url_file_parse(argv[2]);
			if (!url) {
				LOG(("malloc failed"));
				die("Insufficient memory for URL");
			}
		}
		/* ANT URL Load */
		else if (strcasecmp(argv[1], "-url") == 0) {
			url = strdup(argv[2]);
			if (!url) {
				LOG(("malloc failed"));
				die("Insufficient memory for URL");
			}
		}
		/* Unknown => exit here. */
		else {
			LOG(("Unknown parameters: '%s' '%s'",
				argv[1], argv[2]));
			return;
		}
	}
	/* get user's homepage (if configured) */
	else if (option_homepage_url && option_homepage_url[0]) {
		url = calloc(strlen(option_homepage_url) + 5, sizeof(char));
		if (!url) {
			LOG(("malloc failed"));
			die("Insufficient memory for URL");
		}
		sprintf(url, "%s", option_homepage_url);
	}
	/* default homepage */
	else {
		url = calloc(80, sizeof(char));
		if (!url) {
			LOG(("malloc failed"));
			die("Insufficient memory for URL");
		}
		snprintf(url, 80, "file:/<NetSurf$Dir>/Docs/intro_%s",
			option_language);
	}

#ifdef WITH_KIOSK_BROWSING
	open_window = true;
#endif

	if (open_window)
			browser_window_create(url, NULL, 0);

	free(url);
}


/**
 * Close down the gui (RISC OS).
 */

void gui_quit(void)
{
	ro_gui_window_quit();
  	ro_gui_hotlist_save();
	ro_gui_history_quit();
	free(gui_sprites);
	xwimp_close_down(task_handle);
	free(default_stylesheet_url);
	free(adblock_stylesheet_url);
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
	} else if (sched_active && (gui_track || gui_reformat_pending)) {
		os_t t = os_read_monotonic_time() + 10;
		if (sched_time < t)
			t = sched_time;
		event = wimp_poll_idle(mask, &block, t, 0);
	} else if (sched_active) {
		event = wimp_poll_idle(mask, &block, sched_time, 0);
	} else if (gui_track || gui_reformat_pending) {
		os_t t = os_read_monotonic_time();
		event = wimp_poll_idle(mask, &block, t + 10, 0);
	} else {
		event = wimp_poll(wimp_MASK_NULL | mask, &block, 0);
	}
	xhourglass_on();
	gui_last_poll = clock();
	ro_gui_handle_event(event, &block);
	schedule_run();

	if (gui_reformat_pending && event == wimp_NULL_REASON_CODE)
		ro_gui_window_process_reformats();
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
			ro_gui_pointer_leaving_window(&block->leaving);
			break;

		case wimp_POINTER_ENTERING_WINDOW:
			ro_gui_pointer_entering_window(&block->entering);
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
	wimp_pointer pointer;
	os_error *error;

	ro_gui_throb();

	if (!gui_track)
		return;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (gui_track_wimp_w == history_window)
		ro_gui_history_mouse_at(&pointer);
	else if (gui_track_gui_window)
		ro_gui_window_mouse_at(gui_track_gui_window, &pointer);
}


/**
 * Handle Redraw_Window_Request events.
 */

void ro_gui_redraw_window_request(wimp_draw *redraw)
{
	struct gui_window *g;
	osbool more;
	os_error *error;

	if (redraw->w == history_window)
		ro_gui_history_redraw(redraw);
	else if (redraw->w == hotlist_window)
		ro_gui_hotlist_redraw(redraw);
	else if (redraw->w == dialog_debug)
		ro_gui_debugwin_redraw(redraw);
	else if ((g = ro_gui_window_lookup(redraw->w)) != NULL)
		ro_gui_window_redraw(g, redraw);
	else {
		error = xwimp_redraw_window(redraw, &more);
		if (error) {
			LOG(("xwimp_redraw_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
		while (more) {
			error = xwimp_get_rectangle(redraw, &more);
			if (error) {
				LOG(("xwimp_get_rectangle: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
				return;
			}
		}
	}
}


/**
 * Handle Open_Window_Request events.
 */

void ro_gui_open_window_request(wimp_open *open)
{
	struct gui_window *g;
	os_error *error;

	g = ro_gui_window_lookup(open->w);
	if (g) {
		ro_gui_window_open(g, open);
	} else {
		error = xwimp_open_window(open);
		if (error) {
			LOG(("xwimp_open_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}

		g = ro_gui_status_lookup(open->w);
		if (g && g->toolbar)
			ro_gui_theme_resize_toolbar_status(g->toolbar);
	}
}


/**
 * Handle Close_Window_Request events.
 */

void ro_gui_close_window_request(wimp_close *close)
{
	struct gui_window *g;
	struct gui_download_window *dw;

	/*	Check for children
	*/
	ro_gui_dialog_close_persistant(close->w);

	if (close->w == dialog_debug)
		ro_gui_debugwin_close();
	else if ((g = ro_gui_window_lookup(close->w)) != NULL)
		browser_window_destroy(g->bw);
	else if ((dw = ro_gui_download_window_lookup(close->w)) != NULL)
		ro_gui_download_window_destroy(dw);
	else
		ro_gui_dialog_close(close->w);
}


/**
 * Handle Pointer_Leaving_Window events.
 */

void ro_gui_pointer_leaving_window(wimp_leaving *leaving)
{
	os_error *error;

	if (gui_track_wimp_w == history_window) {
		error = xwimp_close_window(dialog_tooltip);
		if (error) {
			LOG(("xwimp_close_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}

	gui_track = false;
	gui_window_set_pointer(GUI_POINTER_DEFAULT);
}


/**
 * Handle Pointer_Entering_Window events.
 */

void ro_gui_pointer_entering_window(wimp_entering *entering)
{
	gui_track_wimp_w = entering->w;
	gui_track_gui_window = ro_gui_window_lookup(entering->w);
	gui_track = gui_track_gui_window || gui_track_wimp_w == history_window;
}


/**
 * Handle Mouse_Click events.
 */

void ro_gui_mouse_click(wimp_pointer *pointer)
{
	struct gui_window *g;
	struct gui_download_window *dw;

	if (pointer->w == wimp_ICON_BAR)
		ro_gui_icon_bar_click(pointer);
	else if (pointer->w == history_window)
		ro_gui_history_click(pointer);
	else if (pointer->w == hotlist_window)
		ro_gui_hotlist_click(pointer);
	else if (pointer->w == dialog_saveas)
		ro_gui_save_click(pointer);
	else if (hotlist_toolbar &&
			hotlist_toolbar->toolbar_handle == pointer->w)
		ro_gui_hotlist_toolbar_click(pointer);
	else if ((g = ro_gui_window_lookup(pointer->w)) != NULL)
		ro_gui_window_click(g, pointer);
	else if ((g = ro_gui_toolbar_lookup(pointer->w)) != NULL)
		ro_gui_toolbar_click(g, pointer);
	else if ((g = ro_gui_status_lookup(pointer->w)) != NULL)
		ro_gui_status_click(g, pointer);
	else if ((dw = ro_gui_download_window_lookup(pointer->w)) != NULL)
		ro_gui_download_window_click(dw, pointer);
	else
		ro_gui_dialog_click(pointer);
}


/**
 * Handle Mouse_Click events on the iconbar icon.
 */

void ro_gui_icon_bar_click(wimp_pointer *pointer)
{
	char url[80];
	int key_down = 0;

	if (pointer->buttons == wimp_CLICK_MENU) {
		ro_gui_create_menu(iconbar_menu, pointer->pos.x,
				   96 + iconbar_menu_height, NULL);

	} else if (pointer->buttons == wimp_CLICK_SELECT) {
		if (option_homepage_url && option_homepage_url[0]) {
			browser_window_create(option_homepage_url, NULL, 0);
		} else {
			snprintf(url, sizeof url,
					"file:/<NetSurf$Dir>/Docs/intro_%s",
					option_language);
			browser_window_create(url, NULL, 0);
		}

	} else if (pointer->buttons == wimp_CLICK_ADJUST) {
		xosbyte1(osbyte_SCAN_KEYBOARD, 0 ^ 0x80, 0, &key_down);
		if (key_down == 0) {
			ro_gui_hotlist_show();
		} else {
			ro_gui_debugwin_open();
		}
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
			break;

		case GUI_DRAG_HOTLIST_SELECT:
			ro_gui_hotlist_selection_drag_end(drag);
			break;

		case GUI_DRAG_HOTLIST_MOVE:
			ro_gui_hotlist_move_drag_end(drag);
			break;
	}
}


/**
 * Handle Key_Pressed events.
 */

void ro_gui_keypress(wimp_key *key)
{
	bool handled = false;
	struct gui_window *g;
	os_error *error;

	if (key->w == hotlist_window)
		handled = ro_gui_hotlist_keypress(key->c);
	else if ((g = ro_gui_window_lookup(key->w)) != NULL)
		handled = ro_gui_window_keypress(g, key->c, false);
	else if ((g = ro_gui_toolbar_lookup(key->w)) != NULL)
		handled = ro_gui_window_keypress(g, key->c, true);
        else
		handled = ro_gui_dialog_keypress(key);

	if (!handled) {
		error = xwimp_process_key(key->c);
		if (error) {
			LOG(("xwimp_process_key: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
}


/**
 * Handle the three User_Message events.
 */

void ro_gui_user_message(wimp_event_no event, wimp_message *message)
{
	struct content *c;
	switch (message->action) {
		case message_HELP_REQUEST:
			ro_gui_interactive_help_request(message);
			break;

		case message_DATA_SAVE_ACK:
			ro_msg_datasave_ack(message);
			break;

		case message_DATA_LOAD:
			if (event == wimp_USER_MESSAGE_ACKNOWLEDGE) {
#ifdef WITH_PRINT
				if (print_current_window)
					print_dataload_bounce(message);
#endif
			}
			else
				ro_msg_dataload(message);
			break;

		case message_DATA_LOAD_ACK:
#ifdef WITH_PRINT
			if (print_current_window)
				print_cleanup();
#endif
			break;

		case message_DATA_OPEN:
			ro_msg_dataopen(message);
			break;

		case message_MENU_WARNING:
			ro_gui_menu_warning((wimp_message_menu_warning *)
					&message->data);
			break;
		case message_MENUS_DELETED:
			if (current_menu == hotlist_menu) {
				ro_gui_hotlist_menu_closed();
			}
			break;
		case message_MODE_CHANGE:
			ro_gui_history_mode_change();
			for (c = content_list; c; c = c->next) {
				if ((c->type == CONTENT_HTML) &&
						(c->data.html.fonts))
					nsfont_reopen_set(c->data.html.fonts);
			}
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
			plugin_opening(message);
			break;
		case message_PLUG_IN_CLOSED:
			plugin_closed(message);
			break;
		case message_PLUG_IN_RESHAPE_REQUEST:
			plugin_reshape_request(message);
			break;
		case message_PLUG_IN_FOCUS:
			break;
		case message_PLUG_IN_URL_ACCESS:
			plugin_url_access(message);
			break;
		case message_PLUG_IN_STATUS:
			plugin_status(message);
			break;
		case message_PLUG_IN_BUSY:
			break;
		case message_PLUG_IN_STREAM_NEW:
			plugin_stream_new(message);
			break;
		case message_PLUG_IN_STREAM_WRITE:
			break;
		case message_PLUG_IN_STREAM_WRITTEN:
			plugin_stream_written(message);
			break;
		case message_PLUG_IN_STREAM_DESTROY:
			break;
		case message_PLUG_IN_OPEN:
			if (event == wimp_USER_MESSAGE_ACKNOWLEDGE)
				plugin_open_msg(message);
			break;
		case message_PLUG_IN_CLOSE:
			if (event == wimp_USER_MESSAGE_ACKNOWLEDGE)
				plugin_close_msg(message);
			break;
		case message_PLUG_IN_RESHAPE:
		case message_PLUG_IN_STREAM_AS_FILE:
		case message_PLUG_IN_NOTIFY:
		case message_PLUG_IN_ABORT:
		case message_PLUG_IN_ACTION:
			break;
#endif
#ifdef WITH_PRINT
		case message_PRINT_SAVE:
			if (event == wimp_USER_MESSAGE_ACKNOWLEDGE)
				print_save_bounce(message);
			break;
		case message_PRINT_ERROR:
			print_error(message);
			break;
		case message_PRINT_TYPE_ODD:
			print_type_odd(message);
			break;
#endif

		case message_QUIT:
			netsurf_quit = true;
			break;
	}
}


/**
 * Handle Message_DataLoad (file dragged in).
 */

void ro_msg_dataload(wimp_message *message)
{
	int file_type = message->data.data_xfer.file_type;
	char *url = 0;
	struct gui_window *g;
	os_error *error;

	g = ro_gui_window_lookup(message->data.data_xfer.w);

	if (g && ro_gui_window_dataload(g, message))
		return;

	if (file_type == 0xf91)			/* Acorn URI file */
		url = ro_gui_uri_file_parse(message->data.data_xfer.file_name);
	else if (file_type == 0xb28)		/* ANT URL file */
		url = ro_gui_url_file_parse(message->data.data_xfer.file_name);
	else if (file_type == 0xfaf ||
			file_type == 0xf78 ||
			file_type == 0xf79 ||
			file_type == 0xf83 ||
			file_type == 0x695 ||
			file_type == 0xaff ||
			file_type == 0xb60 ||
			file_type == 0xc85 ||
			file_type == 0xff9 ||
			file_type == 0xfff)	/* display the actual file */
		url = ro_path_to_url(message->data.data_xfer.file_name);
	else
		return;

	/* send DataLoadAck */
	message->action = message_DATA_LOAD_ACK;
	message->your_ref = message->my_ref;
	error = xwimp_send_message(wimp_USER_MESSAGE, message, message->sender);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (!url)
		/* error has already been reported by one of the three
		 * functions called above */
		return;

	if (g)
		browser_window_go(g->bw, url, 0);
	else
		browser_window_create(url, 0, 0);

	free(url);
}


/**
 * Parse an Acorn URI file.
 *
 * \param  file_name  file to read
 * \return  URL from file, or 0 on error and error reported
 */

char *ro_gui_uri_file_parse(const char *file_name)
{
	/* See the "Acorn URI Handler Functional Specification" for the
	 * definition of the URI file format. */
	char line[400];
	char *url;
	FILE *fp;

	fp = fopen(file_name, "rb");
	if (!fp) {
		LOG(("fopen(\"%s\", \"rb\"): %i: %s",
				file_name, errno, strerror(errno)));
		warn_user("LoadError", strerror(errno));
		return 0;
	}

	/* "URI" */
	if (!ro_gui_uri_file_parse_line(fp, line) || strcmp(line, "URI") != 0)
		goto uri_syntax_error;

	/* version */
	if (!ro_gui_uri_file_parse_line(fp, line) ||
			strspn(line, "0123456789") != strlen(line))
		goto uri_syntax_error;

	/* URI */
	if (!ro_gui_uri_file_parse_line(fp, line))
		goto uri_syntax_error;

	fclose(fp);

	url = strdup(line);
	if (!url) {
		warn_user("NoMemory", 0);
		return 0;
	}

	return url;

uri_syntax_error:
	fclose(fp);
	warn_user("URIError", 0);
	return 0;
}


/**
 * Read a "line" from an Acorn URI file.
 *
 * \param  fp  file pointer to read from
 * \param  b   buffer for line, size 400 bytes
 * \return  true on success, false on EOF
 */

bool ro_gui_uri_file_parse_line(FILE *fp, char *b)
{
	int c;
	unsigned int i = 0;

	c = getc(fp);
	if (c == EOF)
		return false;

	/* skip comment lines */
	while (c == '#') {
		do { c = getc(fp); } while (c != EOF && 32 <= c);
		if (c == EOF)
			return false;
		do { c = getc(fp); } while (c != EOF && c < 32);
		if (c == EOF)
			return false;
	}

	/* read "line" */
	do {
		if (i == 399)
			return false;
		b[i++] = c;
		c = getc(fp);
	} while (c != EOF && 32 <= c);

	/* skip line ending control characters */
	while (c != EOF && c < 32)
		c = getc(fp);

	if (c != EOF)
		ungetc(c, fp);

	b[i] = 0;
	return true;
}


/**
 * Parse an ANT URL file.
 *
 * \param  file_name  file to read
 * \return  URL from file, or 0 on error and error reported
 */

char *ro_gui_url_file_parse(const char *file_name)
{
	char line[400];
	char *url;
	FILE *fp;

	fp = fopen(file_name, "r");
	if (!fp) {
		LOG(("fopen(\"%s\", \"r\"): %i: %s",
				file_name, errno, strerror(errno)));
		warn_user("LoadError", strerror(errno));
		return 0;
	}

	if (!fgets(line, sizeof line, fp)) {
		if (ferror(fp)) {
			LOG(("fgets: %i: %s",
					errno, strerror(errno)));
			warn_user("LoadError", strerror(errno));
		} else
			warn_user("LoadError", messages_get("EmptyError"));
		fclose(fp);
		return 0;
	}

	fclose(fp);

	if (line[strlen(line) - 1] == '\n')
		line[strlen(line) - 1] = '\0';

	url = strdup(line);
	if (!url) {
		warn_user("NoMemory", 0);
		return 0;
	}

	return url;
}


/**
 * Handle Message_DataSaveAck.
 */

void ro_msg_datasave_ack(wimp_message *message)
{
#ifdef WITH_PRINT
	if (print_ack(message))
		return;
#endif

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
		browser_window_create(url, NULL, 0);
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
		browser_window_create(url, NULL, 0);
}

/**
 * Send the source of a content to a text editor.
 */

void ro_gui_view_source(struct content *content)
{
	if (!content || !content->source_data) {
		warn_user("MiscError", "No document source");
		return;
	}

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
#ifdef WITH_URL
	/* Try ant broadcast first */
	ro_url_broadcast(url);
#endif
}


/**
 * Display a warning for a serious problem (eg memory exhaustion).
 *
 * \param  warning  message key for warning message
 * \param  detail   additional message, or 0
 */

void warn_user(const char *warning, const char *detail)
{
	static char warn_buffer[300];

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


/**
 * Display an error and exit.
 *
 * Should only be used during initialisation.
 */

void die(const char *error)
{
	os_error warn_error;

	warn_error.errnum = 1; /* \todo: reasonable ? */
	strncpy(warn_error.errmess, messages_get(error),
			sizeof(warn_error.errmess)-1);
	warn_error.errmess[sizeof(warn_error.errmess)-1] = '\0';
	xwimp_report_error_by_category(&warn_error,
			wimp_ERROR_BOX_OK_ICON |
			wimp_ERROR_BOX_GIVEN_CATEGORY |
			wimp_ERROR_BOX_CATEGORY_ERROR <<
				wimp_ERROR_BOX_CATEGORY_SHIFT,
			"NetSurf", "!netsurf",
			(osspriteop_area *) 1, 0, 0);
	exit(EXIT_FAILURE);
}

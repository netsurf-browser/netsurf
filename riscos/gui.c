/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2005 Richard Wilson <info@tinct.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 */

#include <assert.h>
#include <errno.h>
#include <fpu_control.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <features.h>
#include <unixlib/local.h>
#include "curl/curl.h"
#include "oslib/font.h"
#include "oslib/help.h"
#include "oslib/hourglass.h"
#include "oslib/inetsuite.h"
#include "oslib/os.h"
#include "oslib/osbyte.h"
#include "oslib/osfile.h"
#include "oslib/osfscontrol.h"
#include "oslib/osgbpb.h"
#include "oslib/osspriteop.h"
#include "oslib/pdriver.h"
#include "oslib/plugin.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "oslib/uri.h"
#include "rufl.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/content/url_store.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/options.h"
#include "netsurf/desktop/tree.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/html.h"
#include "netsurf/riscos/bitmap.h"
#include "netsurf/riscos/buffer.h"
#include "netsurf/riscos/dialog.h"
#include "netsurf/riscos/filename.h"
#include "netsurf/riscos/global_history.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/help.h"
#include "netsurf/riscos/menus.h"
#include "netsurf/riscos/message.h"
#include "netsurf/riscos/options.h"
#ifdef WITH_PLUGIN
#include "netsurf/riscos/plugin.h"
#endif
#ifdef WITH_PRINT
#include "netsurf/riscos/print.h"
#endif
#include "netsurf/riscos/query.h"
#include "netsurf/riscos/save_complete.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/treeview.h"
#ifdef WITH_URI
#include "netsurf/riscos/uri.h"
#endif
#ifdef WITH_URL
#include "netsurf/riscos/url_protocol.h"
#endif
#include "netsurf/riscos/url_complete.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/riscos/wimp_event.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"



#ifndef FILETYPE_ACORN_URI
#define FILETYPE_ACORN_URI 0xf91
#endif
#ifndef FILETYPE_ANT_URL
#define FILETYPE_ANT_URL 0xb28
#endif
#ifndef FILETYPE_IEURL
#define FILETYPE_IEURL 0x1ba
#endif
#ifndef FILETYPE_HTML
#define FILETYPE_HTML 0xfaf
#endif
#ifndef FILETYPE_JNG
#define FILETYPE_JNG 0xf78
#endif
#ifndef FILETYPE_CSS
#define FILETYPE_CSS 0xf79
#endif
#ifndef FILETYPE_MNG
#define FILETYPE_MNG 0xf83
#endif
#ifndef FILETYPE_GIF
#define FILETYPE_GIF 0x695
#endif
#ifndef FILETYPE_PNG
#define FILETYPE_PNG 0xb60
#endif
#ifndef FILETYPE_JPEG
#define FILETYPE_JPEG 0xc85
#endif
#ifndef FILETYPE_ARTWORKS
#define FILETYPE_ARTWORKS 0xd94
#endif

int os_version = 0;

const char * const __dynamic_da_name = "NetSurf";	/**< For UnixLib. */
int __dynamic_da_max_size = 128 * 1024 * 1024;	/**< For UnixLib. */
int __feature_imagefs_is_file = 1;		/**< For UnixLib. */
/* default filename handling */
int __riscosify_control = __RISCOSIFY_NO_SUFFIX |
			__RISCOSIFY_NO_REVERSE_SUFFIX;

const char * NETSURF_DIR;

char *default_stylesheet_url;
char *adblock_stylesheet_url;

#ifndef ncos
static const char *task_name = "NetSurf";
#define CHOICES_PREFIX "<Choices$Write>.WWW.NetSurf."
#else
static const char *task_name = "NCNetSurf";
#define CHOICES_PREFIX "<User$Path>.Choices.NetSurf."
#endif


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
osspriteop_area *gui_sprites;	   /**< Sprite area containing pointer and hotlist sprites */

/** Previously registered signal handlers */
static struct {
	void (*sigabrt)(int);
	void (*sigfpe)(int);
	void (*sigill)(int);
	void (*sigint)(int);
	void (*sigsegv)(int);
	void (*sigterm)(int);
} prev_sigs;

/** Accepted wimp user messages. */
static wimp_MESSAGE_LIST(38) task_messages = { {
	message_HELP_REQUEST,
	message_DATA_SAVE,
	message_DATA_SAVE_ACK,
	message_DATA_LOAD,
	message_DATA_LOAD_ACK,
	message_DATA_OPEN,
	message_PRE_QUIT,
	message_SAVE_DESKTOP,
	message_MENU_WARNING,
	message_MENUS_DELETED,
	message_MODE_CHANGE,
	message_CLAIM_ENTITY,
	message_DATA_REQUEST,
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

static void ro_gui_create_dirs(void);
static void ro_gui_choose_language(void);
static void ro_gui_icon_bar_create(void);
static void ro_gui_signal(int sig);
static void ro_gui_cleanup(void);
static void ro_gui_handle_event(wimp_event_no event, wimp_block *block);
static void ro_gui_null_reason_code(void);
static void ro_gui_redraw_window_request(wimp_draw *redraw);
static void ro_gui_close_window_request(wimp_close *close);
static void ro_gui_pointer_leaving_window(wimp_leaving *leaving);
static void ro_gui_pointer_entering_window(wimp_entering *entering);
static void ro_gui_mouse_click(wimp_pointer *pointer);
static bool ro_gui_icon_bar_click(wimp_pointer *pointer);
static void ro_gui_check_resolvers(void);
static void ro_gui_drag_end(wimp_dragged *drag);
static void ro_gui_keypress(wimp_key *key);
static void ro_gui_user_message(wimp_event_no event, wimp_message *message);
static void ro_msg_dataload(wimp_message *block);
static char *ro_gui_uri_file_parse(const char *file_name, char **uri_title);
static bool ro_gui_uri_file_parse_line(FILE *fp, char *b);
static char *ro_gui_url_file_parse(const char *file_name);
static char *ro_gui_ieurl_file_parse(const char *file_name);
static void ro_msg_terminate_filename(wimp_full_message_data_xfer *message);
static void ro_msg_datasave(wimp_message *message);
static void ro_msg_datasave_ack(wimp_message *message);
static void ro_msg_dataopen(wimp_message *message);
static void ro_msg_prequit(wimp_message *message);
static void ro_msg_save_desktop(wimp_message *message);
static void ro_gui_view_source_bounce(wimp_message *message);


/**
 * Initialise the gui (RISC OS specific part).
 */

void gui_init(int argc, char** argv)
{
	char path[40];
	os_error *error;
	int length;
	char *nsdir_temp;

	/* re-enable all FPU exceptions/traps except inexact operations,
	 * which we're not interested in - UnixLib disables all FP
	 * exceptions by default */
	_FPU_SETCW(_FPU_IEEE & ~_FPU_MASK_PM);

	xhourglass_start(1);

	/* read OS version for code that adapts to conform to the OS
	 * (remember that it's preferable to check for specific features
	 * being present) */
	xos_byte(osbyte_IN_KEY, 0, 0xff, &os_version, NULL);

	atexit(ro_gui_cleanup);
	prev_sigs.sigabrt = signal(SIGABRT, ro_gui_signal);
	prev_sigs.sigfpe = signal(SIGFPE, ro_gui_signal);
	prev_sigs.sigill = signal(SIGILL, ro_gui_signal);
	prev_sigs.sigint = signal(SIGINT, ro_gui_signal);
	prev_sigs.sigsegv = signal(SIGSEGV, ro_gui_signal);
	prev_sigs.sigterm = signal(SIGTERM, ro_gui_signal);

	if (prev_sigs.sigabrt == SIG_ERR || prev_sigs.sigfpe == SIG_ERR ||
			prev_sigs.sigill == SIG_ERR ||
			prev_sigs.sigint == SIG_ERR ||
			prev_sigs.sigsegv == SIG_ERR ||
			prev_sigs.sigterm == SIG_ERR)
		die("Failed registering signal handlers");

	ro_filename_initialise();

#ifdef WITH_SAVE_COMPLETE
	save_complete_init();
#endif

	options_read("NetSurf:Choices");

	/* set defaults for absent strings */
	if (!option_theme)
		option_theme = strdup("Aletheia");
	if (!option_toolbar_browser)
		option_toolbar_browser = strdup("0123|58|9");
	if (!option_toolbar_hotlist)
		option_toolbar_hotlist = strdup("401|23");
	if (!option_toolbar_history)
		option_toolbar_history = strdup("01|23");
	if (!option_ca_bundle)
		option_ca_bundle = strdup("NetSurf:Resources.ca-bundle");
	if (!option_cookie_file)
		option_cookie_file = strdup("NetSurf:Cookies");
	if (!option_cookie_jar)
		option_cookie_jar = strdup(CHOICES_PREFIX "Cookies");
	if (!option_url_path)
		option_url_path = strdup("NetSurf:URL");
	if (!option_url_save)
		option_url_save = strdup(CHOICES_PREFIX "URL");
	if (!option_hotlist_path)
		option_hotlist_path = strdup("NetSurf:Hotlist");
	if (!option_hotlist_save)
		option_hotlist_save = strdup(CHOICES_PREFIX "Hotlist");
	if (!option_recent_path)
		option_recent_path = strdup("NetSurf:Recent");
	if (!option_recent_save)
		option_recent_save = strdup(CHOICES_PREFIX "Recent");
	if (!option_theme_path)
		option_theme_path = strdup("NetSurf:Themes");
	if (!option_theme_save)
		option_theme_save = strdup(CHOICES_PREFIX "Themes");

	if (!option_theme || ! option_toolbar_browser ||
			!option_toolbar_hotlist || !option_toolbar_history ||
			!option_ca_bundle || !option_cookie_file ||
			!option_cookie_jar || !option_url_path ||
			!option_url_save || !option_hotlist_path ||
			!option_hotlist_save || !option_recent_path ||
			!option_recent_save || !option_theme_path ||
			!option_theme_save)
		die("Failed initialising string options");

	/* create our choices directories */
	ro_gui_create_dirs();

	gui_sprites = ro_gui_load_sprite_file("NetSurf:Resources.Sprites");
	if (!gui_sprites)
		die("Unable to load Sprites.");
	ro_gui_choose_language();

	bitmap_initialise_memory();
	url_store_load(option_url_path);

	nsdir_temp = getenv("NetSurf$Dir");
	if (!nsdir_temp)
		die("Failed to locate NetSurf directory");
	NETSURF_DIR = strdup(nsdir_temp);
	if (!NETSURF_DIR)
		die("Failed duplicating NetSurf directory string");

	if ((length = snprintf(path, sizeof(path),
			"NetSurf:Resources.%s.Messages",
			option_language)) < 0 || length >= (int)sizeof(path))
		die("Failed to locate Messages resource.");
	messages_load(path);
	messages_load("NetSurf:Resources.LangNames");

	default_stylesheet_url = strdup("file:/NetSurf:/Resources/CSS");
	adblock_stylesheet_url = strdup("file:/NetSurf:/Resources/AdBlock");
	if (!default_stylesheet_url || !adblock_stylesheet_url)
		die("Failed initialising string constants.");

	error = xwimp_initialise(wimp_VERSION_RO38, task_name,
			(const wimp_message_list *) &task_messages, 0,
			&task_handle);
	if (error) {
		LOG(("xwimp_initialise: 0x%x: %s",
				error->errnum, error->errmess));
		die(error->errmess);
	}
	/* register our message handlers */
	ro_message_register_route(message_HELP_REQUEST,
			ro_gui_interactive_help_request);
	ro_message_register_route(message_DATA_OPEN,
			ro_msg_dataopen);
	ro_message_register_route(message_DATA_SAVE,
			ro_msg_datasave);
	ro_message_register_route(message_DATA_SAVE_ACK,
			ro_msg_datasave_ack);
	ro_message_register_route(message_PRE_QUIT,
			ro_msg_prequit);
	ro_message_register_route(message_SAVE_DESKTOP,
			ro_msg_save_desktop);
	/* end of handler registration */

	nsfont_init();

	/* Issue a *Desktop to poke AcornURI into life */
	if (getenv("NetSurf$Start_URI_Handler"))
		xwimp_start_task("Desktop", 0);

	/*	Open the templates
	*/
	if ((length = snprintf(path, sizeof(path),
			"NetSurf:Resources.%s.Templates",
			option_language)) < 0 || length >= (int)sizeof(path))
		die("Failed to locate Templates resource.");
	error = xwimp_open_template(path);
	if (error) {
		LOG(("xwimp_open_template failed: 0x%x: %s",
				error->errnum, error->errmess));
		die(error->errmess);
	}
	ro_gui_theme_initialise(); /* initialise themes before dialogs */
	ro_gui_dialog_init(); /* must be done after sprite loading */
	ro_gui_download_init();
	ro_gui_menu_init();
	ro_gui_query_init();
	ro_gui_history_init();
	wimp_close_template();

	ro_gui_tree_initialise(); /* must be done after sprite loading */

	ro_gui_icon_bar_create();
	ro_gui_check_resolvers();
}

/**
 * Create intermediate directories for Choices and User Data files
 */
void ro_gui_create_dirs(void)
{
	char *path;
	char buf[256];

	/* Choices */
	path = getenv("NetSurf$ChoicesSave");
	if (!path)
		die("Failed to find NetSurf Choices save path");

	snprintf(buf, sizeof(buf), "%s", path);
	path = buf;

	/* Given a path x.y.z, this will try to create x, x.y */
	while ((path = strchr(path, '.'))) {
		*path = '\0';
		xosfile_create_dir(buf, 0);
		*path++ = '.';
	}

	/* URL */
	snprintf(buf, sizeof(buf), "%s", option_url_save);
	path = buf;

	while ((path = strchr(path, '.'))) {
		*path = '\0';
		xosfile_create_dir(buf, 0);
		*path++ = '.';
	}

	/* Hotlist */
	snprintf(buf, sizeof(buf), "%s", option_hotlist_save);
	path = buf;

	while ((path = strchr(path, '.'))) {
		*path = '\0';
		xosfile_create_dir(buf, 0);
		*path++ = '.';
	}

	/* Recent */
	snprintf(buf, sizeof(buf), "%s", option_recent_save);
	path = buf;

	while ((path = strchr(path, '.'))) {
		*path = '\0';
		xosfile_create_dir(buf, 0);
		*path++ = '.';
	}

	/* Theme */
	snprintf(buf, sizeof(buf), "%s", option_theme_save);
	path = buf;

	while ((path = strchr(path, '.'))) {
		*path = '\0';
		xosfile_create_dir(buf, 0);
		*path++ = '.';
	}
	/* and the final directory part (as theme_save is a directory) */
	xosfile_create_dir(buf, 0);
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
		sprintf(path, "NetSurf:Resources.%s", option_language);
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
		case 30: /* Austria */
		case 35: /* Switzerland (70% German-speaking) */
			lang = "de";
			break;
		case 6: /* France */
		case 18: /* Canada2 (French Canada?) */
			lang = "fr";
			break;
		case 34: /* Netherlands */
			lang = "nl";
			break;
		default:
			lang = "en";
			break;
	}
	sprintf(path, "NetSurf:Resources.%s", lang);
	if (is_dir(path))
		option_language = strdup(lang);
	else
		option_language = strdup("en");
	assert(option_language);
	if (!option_accept_language)
		option_accept_language = strdup(option_language);
}


/**
 * Create an iconbar icon.
 */

void ro_gui_icon_bar_create(void)
{
#ifndef ncos
	wimp_icon_create icon = {
		wimp_ICON_BAR_RIGHT,
		{ { 0, 0, 68, 68 },
		wimp_ICON_SPRITE | wimp_ICON_HCENTRED | wimp_ICON_VCENTRED |
				(wimp_BUTTON_CLICK << wimp_ICON_BUTTON_TYPE_SHIFT),
		{ "!netsurf" } } };
	wimp_create_icon(&icon);
	ro_gui_wimp_event_register_mouse_click(wimp_ICON_BAR,
			ro_gui_icon_bar_click);
#endif
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
			url = path_to_url(argv[2]);
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
	bitmap_quit();
	url_store_save(option_url_save);
	ro_gui_window_quit();
	ro_gui_global_history_save();
	ro_gui_hotlist_save();
	ro_gui_history_quit();
	ro_gui_saveas_quit();
	rufl_quit();
	free(gui_sprites);
	xwimp_close_down(task_handle);
	free(default_stylesheet_url);
	free(adblock_stylesheet_url);
	xhourglass_off();
}


/**
 * Handles a signal
 */

void ro_gui_signal(int sig)
{
	struct content *c;
	void (*prev_handler)(int);

	if (sig == SIGFPE || sig == SIGABRT) {
		os_colour old_sand, old_glass;

		xhourglass_on();
		xhourglass_colours(0x0000ffff, 0x000000ff,
				&old_sand, &old_glass);
		for (c = content_list; c; c = c->next)
			if (c->type == CONTENT_HTML && c->data.html.layout) {
				LOG(("Dumping: '%s'", c->url));
				box_dump(c->data.html.layout, 0);
			}
		options_dump();
		xhourglass_colours(old_sand, old_glass, 0, 0);
		xhourglass_off();
	}
	ro_gui_cleanup();

	/* Get previous handler of this signal */
	switch (sig) {
		case SIGABRT:
			prev_handler = prev_sigs.sigabrt;
			break;
		case SIGFPE:
			prev_handler = prev_sigs.sigfpe;
			break;
		case SIGILL:
			prev_handler = prev_sigs.sigill;
			break;
		case SIGINT:
			prev_handler = prev_sigs.sigint;
			break;
		case SIGSEGV:
			prev_handler = prev_sigs.sigsegv;
			break;
		case SIGTERM:
			prev_handler = prev_sigs.sigterm;
			break;
		default:
			abort();
	}

	if (prev_handler != SIG_IGN && prev_handler != SIG_DFL) {
		/* User-registered handler, so call it direct */
		prev_handler(sig);
	} else if (prev_handler == SIG_DFL) {
		/* Default handler so set handler to that and raise() */
		prev_handler = signal(sig, SIG_DFL);
		if (prev_handler == SIG_ERR)
			abort();
		raise(sig);
	}
	/* If we reach here, previous handler was either SIG_IGN or
	 * the user-defined handler returned. In either case, we have
	 * nothing to do */
}


/**
 * Ensures the gui exits cleanly.
 */

void ro_gui_cleanup(void)
{
	ro_gui_buffer_close();
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

	/* Poll wimp. */
	xhourglass_off();
	if (active) {
		event = wimp_poll(mask, &block, 0);
	} else if (sched_active || gui_track || gui_reformat_pending ||
			bitmap_maintenance) {
		os_t t = os_read_monotonic_time();

		if (gui_track)
			switch (gui_current_drag_type) {
				case GUI_DRAG_SELECTION:
				case GUI_DRAG_SCROLL:
					t += 4;	/* for smoother update */
					break;

				default:
					t += 10;
					break;
			}
		else
			t += 10;

		if (sched_active && (sched_time - t) < 0)
			t = sched_time;

		event = wimp_poll_idle(mask, &block, t, 0);
	} else {
		event = wimp_poll(wimp_MASK_NULL | mask, &block, 0);
	}
	xhourglass_on();
	gui_last_poll = clock();
	ro_gui_handle_event(event, &block);
	schedule_run();

	if (gui_reformat_pending && event == wimp_NULL_REASON_CODE)
		ro_gui_window_process_reformats();
	else if (bitmap_maintenance_priority ||
			(bitmap_maintenance && event == wimp_NULL_REASON_CODE))
		bitmap_maintain();
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

	ro_gui_handle_event(event, &block);
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

	switch (gui_current_drag_type) {

		/* pointer is allowed to wander outside the initiating window
			for certain drag types */

		case GUI_DRAG_SELECTION:
		case GUI_DRAG_SCROLL:
			assert(gui_track_gui_window);
			ro_gui_window_mouse_at(gui_track_gui_window, &pointer);
			break;

		default:
			if (gui_track_wimp_w == history_window)
				ro_gui_history_mouse_at(&pointer);
			if (gui_track_wimp_w == dialog_url_complete)
				ro_gui_url_complete_mouse_at(&pointer);
			else if (gui_track_gui_window)
				ro_gui_window_mouse_at(gui_track_gui_window, &pointer);
			break;
	}
}


/**
 * Handle Redraw_Window_Request events.
 */

void ro_gui_redraw_window_request(wimp_draw *redraw)
{
	struct gui_window *g;

	if (ro_gui_wimp_event_redraw_window(redraw))
		return;

	g = ro_gui_window_lookup(redraw->w);
	if (g)
		ro_gui_window_redraw(g, redraw);
}


/**
 * Handle Open_Window_Request events.
 */

void ro_gui_open_window_request(wimp_open *open)
{
	struct gui_window *g;
	os_error *error;

	if (ro_gui_wimp_event_open_window(open))
		return;

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
	wimp_pointer pointer;
	os_error *error;
	char *temp_name, *r;
	char *filename;
	struct content *content = NULL;

	if ((g = ro_gui_window_lookup(close->w)) != NULL) {
		error = xwimp_get_pointer_info(&pointer);
		if (error) {
			LOG(("xwimp_get_pointer_info: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
		if (g->bw)
			content = g->bw->current_content;
		if (pointer.buttons & wimp_CLICK_ADJUST) {
			filename = url_to_path(content->url);
			if (filename) {
				temp_name = malloc(strlen(filename) + 32);
				if (temp_name) {
					sprintf(temp_name, "Filer_OpenDir %s", filename);
					r = temp_name + strlen(temp_name);
					while (r > temp_name) {
						if (*r == '.') {
							*r = '\0';
							break;
						}
						*r--;
					}
					error = xos_cli(temp_name);
					if (error) {
						LOG(("xos_cli: 0x%x: %s",
								error->errnum, error->errmess));
						warn_user("MiscError", error->errmess);
						return;
					}
					free(temp_name);
				}
				free(filename);
			} else {
				/* todo: go 'up' */ 
			}
		}
		if (ro_gui_shift_pressed())
			return;
		ro_gui_url_complete_close(NULL, 0);
		browser_window_destroy(g->bw);
	} else if ((dw = ro_gui_download_window_lookup(close->w)) != NULL) {
		ro_gui_download_window_destroy(dw, false);
	} else {
		ro_gui_dialog_close(close->w);
	}
	ro_gui_dialog_close_persistent(close->w);
}


/**
 * Handle Pointer_Leaving_Window events.
 */

void ro_gui_pointer_leaving_window(wimp_leaving *leaving)
{
	if (gui_track_wimp_w == history_window) {
		os_error *error = xwimp_close_window(dialog_tooltip);
		if (error) {
			LOG(("xwimp_close_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}

	switch (gui_current_drag_type) {
		case GUI_DRAG_SELECTION:
		case GUI_DRAG_SCROLL:
			/* ignore Pointer_Leaving_Window event that the Wimp mysteriously
				issues when a Wimp_DragBox drag operations is started */
			break;

		default:
			gui_track = false;
			gui_window_set_pointer(GUI_POINTER_DEFAULT);
			break;
	}
}


/**
 * Handle Pointer_Entering_Window events.
 */

void ro_gui_pointer_entering_window(wimp_entering *entering)
{
	gui_track_wimp_w = entering->w;
	gui_track_gui_window = ro_gui_window_lookup(entering->w);
	gui_track = gui_track_gui_window || gui_track_wimp_w == history_window ||
			gui_track_wimp_w == dialog_url_complete;
}


/**
 * Handle Mouse_Click events.
 */

void ro_gui_mouse_click(wimp_pointer *pointer)
{
	struct gui_window *g;
	struct gui_download_window *dw;
	struct gui_query_window *qw;

	if (ro_gui_wimp_event_mouse_click(pointer))
		return;
	else if ((g = ro_gui_window_lookup(pointer->w)) != NULL)
		ro_gui_window_click(g, pointer);
	else if ((dw = ro_gui_download_window_lookup(pointer->w)) != NULL)
		ro_gui_download_window_click(dw, pointer);
	else if ((qw = ro_gui_query_window_lookup(pointer->w)) != NULL)
		ro_gui_query_window_click(qw, pointer);
}


/**
 * Handle Mouse_Click events on the iconbar icon.
 */

bool ro_gui_icon_bar_click(wimp_pointer *pointer)
{
	char url[80];
	int key_down = 0;

	if (pointer->buttons == wimp_CLICK_MENU) {
		ro_gui_menu_create(iconbar_menu, pointer->pos.x,
				   96 + iconbar_menu_height, wimp_ICON_BAR);

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
		if (key_down == 0)
			ro_gui_menu_handle_action(pointer->w, HOTLIST_SHOW,
					false);
		else
			ro_gui_debugwin_open();
	}
	return true;
}


/**
 * Handle User_Drag_Box events.
 */

void ro_gui_drag_end(wimp_dragged *drag)
{
	switch (gui_current_drag_type) {
		case GUI_DRAG_SELECTION:
			ro_gui_selection_drag_end(gui_track_gui_window, drag);
			break;

		case GUI_DRAG_SCROLL:
			ro_gui_window_scroll_end(gui_track_gui_window, drag);
			break;

		case GUI_DRAG_DOWNLOAD_SAVE:
			ro_gui_download_drag_end(drag);
			break;

		case GUI_DRAG_SAVE:
			ro_gui_save_drag_end(drag);
			break;

		case GUI_DRAG_STATUS_RESIZE:
			break;

		case GUI_DRAG_TREE_SELECT:
			ro_gui_tree_selection_drag_end(drag);
			break;

		case GUI_DRAG_TREE_MOVE:
			ro_gui_tree_move_drag_end(drag);
			break;

		case GUI_DRAG_TOOLBAR_CONFIG:
			ro_gui_theme_toolbar_editor_drag_end(drag);
			break;

		default:
			assert(gui_current_drag_type == GUI_DRAG_NONE);
			break;
	}
}


/**
 * Handle Key_Pressed events.
 */

void ro_gui_keypress(wimp_key *key)
{
	struct gui_download_window *dw;
	struct gui_query_window *qw;
	bool handled = false;
	struct gui_window *g;
	os_error *error;

	if (ro_gui_wimp_event_keypress(key))
		handled = true;
	else if ((g = ro_gui_window_lookup(key->w)) != NULL)
		handled = ro_gui_window_keypress(g, key->c, false);
	else if ((g = ro_gui_toolbar_lookup(key->w)) != NULL)
		handled = ro_gui_window_keypress(g, key->c, true);
	else if ((qw = ro_gui_query_window_lookup(key->w)) != NULL)
		handled = ro_gui_query_window_keypress(qw, key);
	else if ((dw = ro_gui_download_window_lookup(key->w)) != NULL)
		handled = ro_gui_download_window_keypress(dw, key);

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
  	/* attempt automatic routing */
  	if (ro_message_handle_message(event, message))
  		return;
  
	switch (message->action) {
		case message_DATA_LOAD:
			ro_msg_terminate_filename((wimp_full_message_data_xfer*)message);

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

		case message_MENU_WARNING:
			ro_gui_menu_warning((wimp_message_menu_warning *)
					&message->data);
			break;

		case message_MENUS_DELETED:
			ro_gui_menu_closed(true);
			break;

		case message_MODE_CHANGE:
			ro_gui_history_mode_change();
			rufl_invalidate_cache();
			break;

		case message_CLAIM_ENTITY:
			ro_gui_selection_claim_entity((wimp_full_message_claim_entity*)message);
			break;

		case message_DATA_REQUEST:
			ro_gui_selection_data_request((wimp_full_message_data_request*)message);
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
 * Ensure that the filename in a data transfer message is NUL terminated
 * (some applications, especially BASIC programs use CR)
 *
 * \param  message  message to be corrected
 */

void ro_msg_terminate_filename(wimp_full_message_data_xfer *message)
{
	const char *ep = (char*)message + message->size;
	char *p = message->file_name;

	if ((size_t)message->size >= sizeof(*message))
		ep = (char*)message + sizeof(*message) - 1;

	while (p < ep && *p >= ' ') p++;
	*p = '\0';
}


/**
 * Handle Message_DataLoad (file dragged in).
 */

void ro_msg_dataload(wimp_message *message)
{
	int file_type = message->data.data_xfer.file_type;
	int tree_file_type = file_type;
	char *url = 0;
	char *title = NULL;
	struct gui_window *g;
	struct node *node;
	struct node *link;
	os_error *error;
	int x, y;
	bool before;
	struct url_content *data;

	g = ro_gui_window_lookup(message->data.data_xfer.w);
	if (g) {
		if (ro_gui_window_dataload(g, message))
			return;
	}
	else {
		g = ro_gui_toolbar_lookup(message->data.data_xfer.w);
		if (g && ro_gui_toolbar_dataload(g, message))
			return;
	}

	switch (file_type) {
		case FILETYPE_ACORN_URI:
			url = ro_gui_uri_file_parse(message->data.data_xfer.file_name,
					&title);
			tree_file_type = 0xfaf;
			break;
		case FILETYPE_ANT_URL:
			url = ro_gui_url_file_parse(message->data.data_xfer.file_name);
			tree_file_type = 0xfaf;
			break;
		case FILETYPE_IEURL:
			url = ro_gui_ieurl_file_parse(message->data.data_xfer.file_name);
			tree_file_type = 0xfaf;
			break;

		case FILETYPE_HTML:
		case FILETYPE_JNG:
		case FILETYPE_CSS:
		case FILETYPE_MNG:
		case FILETYPE_GIF:
		case osfile_TYPE_DRAW:
		case FILETYPE_PNG:
		case FILETYPE_JPEG:
		case osfile_TYPE_SPRITE:
		case osfile_TYPE_TEXT:
		case FILETYPE_ARTWORKS:
			/* display the actual file */
			url = path_to_url(message->data.data_xfer.file_name);
			break;

		default:
			return;
	}

	if (!url)
		/* error has already been reported by one of the
		 * functions called above */
		return;

	if (g) {
		browser_window_go(g->bw, url, 0);
	} else if ((hotlist_tree) && ((wimp_w)hotlist_tree->handle ==
			message->data.data_xfer.w)) {
		data = url_store_find(url);
		if (data) {
			ro_gui_tree_get_tree_coordinates(hotlist_tree,
					message->data.data_xfer.pos.x,
					message->data.data_xfer.pos.y,
					&x, &y);
			link = tree_get_link_details(hotlist_tree, x, y, &before);
			node = tree_create_URL_node(NULL, data, title);
			tree_link_node(link, node, before);
			tree_handle_node_changed(hotlist_tree, node, false, true);
			tree_redraw_area(hotlist_tree, node->box.x - NODE_INSTEP, 0,
					NODE_INSTEP, 16384);
			if ((!title) && (!data->title))
				ro_gui_tree_start_edit(hotlist_tree, &node->data, NULL);
		}
	} else {
		browser_window_create(url, 0, 0);
	}

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

	free(url);
}


/**
 * Parse an Acorn URI file.
 *
 * \param  file_name  file to read
 * \param  uri_title  pointer to receive title data, or NULL for no data
 * \return  URL from file, or 0 on error and error reported
 */

char *ro_gui_uri_file_parse(const char *file_name, char **uri_title)
{
	/* See the "Acorn URI Handler Functional Specification" for the
	 * definition of the URI file format. */
	char line[400];
	char *url = NULL;
	FILE *fp;

	*uri_title = NULL;
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
	url = strdup(line);
	if (!url) {
		warn_user("NoMemory", 0);
		fclose(fp);
		return 0;
	}

	/* title */
	if (!ro_gui_uri_file_parse_line(fp, line))
		goto uri_syntax_error;
	if (uri_title && line[0] && ((line[0] != '*') || line[1])) {
		*uri_title = strdup(line);
		if (!*uri_title) /* non-fatal */
			warn_user("NoMemory", 0);
	}
	fclose(fp);

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
 * Parse an IEURL file.
 *
 * \param  file_name  file to read
 * \return  URL from file, or 0 on error and error reported
 */

char *ro_gui_ieurl_file_parse(const char *file_name)
{
	char line[400];
	char *url = 0;
	FILE *fp;

	fp = fopen(file_name, "r");
	if (!fp) {
		LOG(("fopen(\"%s\", \"r\"): %i: %s",
				file_name, errno, strerror(errno)));
		warn_user("LoadError", strerror(errno));
		return 0;
	}

	while (fgets(line, sizeof line, fp)) {
		if (strncmp(line, "URL=", 4) == 0) {
			if (line[strlen(line) - 1] == '\n')
				line[strlen(line) - 1] = '\0';
			url = strdup(line + 4);
			if (!url) {
				fclose(fp);
				warn_user("NoMemory", 0);
				return 0;
			}
			break;
		}
	}
	if (ferror(fp)) {
		LOG(("fgets: %i: %s",
				errno, strerror(errno)));
		warn_user("LoadError", strerror(errno));
		fclose(fp);
		return 0;
	}

	fclose(fp);

	if (!url)
		warn_user("URIError", 0);

	return url;
}


/**
 * Handle Message_DataSave
 */

void ro_msg_datasave(wimp_message *message)
{
	wimp_full_message_data_xfer *dataxfer = (wimp_full_message_data_xfer*)message;

	ro_msg_terminate_filename(dataxfer);

	switch (dataxfer->file_type) {
		case FILETYPE_ACORN_URI:
		case FILETYPE_ANT_URL:
		case FILETYPE_IEURL:
		case FILETYPE_HTML:
		case FILETYPE_JNG:
		case FILETYPE_CSS:
		case FILETYPE_MNG:
		case FILETYPE_GIF:
		case osfile_TYPE_DRAW:
		case FILETYPE_PNG:
		case FILETYPE_JPEG:
		case osfile_TYPE_SPRITE:
		case osfile_TYPE_TEXT:
		case FILETYPE_ARTWORKS: {
			os_error *error;

			dataxfer->your_ref = dataxfer->my_ref;
			dataxfer->size = offsetof(wimp_full_message_data_xfer, file_name) + 16;
			dataxfer->action = message_DATA_SAVE_ACK;
			dataxfer->est_size = -1;
			memcpy(dataxfer->file_name, "<Wimp$Scrap>", 13);

			error = xwimp_send_message(wimp_USER_MESSAGE, (wimp_message*)dataxfer, message->sender);
			if (error) {
				LOG(("xwimp_send_message: 0x%x: %s", error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
			}
		}
		break;
	}
}


/**
 * Handle Message_DataSaveAck.
 */

void ro_msg_datasave_ack(wimp_message *message)
{
	ro_msg_terminate_filename((wimp_full_message_data_xfer*)message);

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
	int file_type = message->data.data_xfer.file_type;
	char *url = 0;
	size_t len;
	os_error *error;

	if (file_type == 0xb28)			/* ANT URL file */
		url = ro_gui_url_file_parse(message->data.data_xfer.file_name);
	else if (file_type == 0xfaf)		/* HTML file */
		url = path_to_url(message->data.data_xfer.file_name);
	else if (file_type == 0x1ba)		/* IEURL file */
		url = ro_gui_ieurl_file_parse(message->
				data.data_xfer.file_name);
	else if (file_type == 0x2000) {		/* application */
		len = strlen(message->data.data_xfer.file_name);
		if (len < 9 || strcmp(".!NetSurf",
				message->data.data_xfer.file_name + len - 9))
			return;
		if (option_homepage_url && option_homepage_url[0]) {
			url = strdup(option_homepage_url);
		} else {
			url = malloc(80);
			if (url)
				snprintf(url, 80,
					"file:/<NetSurf$Dir>/Docs/intro_%s",
					option_language);
		}
		if (!url)
			warn_user("NoMemory", 0);
	} else
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
		/* error has already been reported by one of the
		 * functions called above */
		return;

	/* create a new window with the file */
	browser_window_create(url, NULL, 0);

	free(url);
}


/**
 * Handle PreQuit message
 *
 * \param  message  PreQuit message from Wimp
 */

void ro_msg_prequit(wimp_message *message)
{
	if (!ro_gui_prequit()) {
		os_error *error;

		/* we're objecting to the close down */
		message->your_ref = message->my_ref;
		error = xwimp_send_message(wimp_USER_MESSAGE_ACKNOWLEDGE,
						message, message->sender);
		if (error) {
			LOG(("xwimp_send_message: 0x%x:%s", error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
}


/**
 * Handle SaveDesktop message
 *
 * \param  message  SaveDesktop message from Wimp
 */

void ro_msg_save_desktop(wimp_message *message)
{
	os_error *error;

	error = xosgbpb_writew(message->data.save_desktopw.file,
				(const byte*)"Run ", 4, NULL);
	if (!error) {
		error = xosgbpb_writew(message->data.save_desktopw.file,
					(const byte*)NETSURF_DIR, strlen(NETSURF_DIR), NULL);
		if (!error)
			error = xos_bputw('\n', message->data.save_desktopw.file);
	}

	if (error) {
		LOG(("xosgbpb_writew/xos_bputw: 0x%x:%s", error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);

		/* we must cancel the save by acknowledging the message */
		message->your_ref = message->my_ref;
		error = xwimp_send_message(wimp_USER_MESSAGE_ACKNOWLEDGE,
						message, message->sender);
		if (error) {
			LOG(("xwimp_send_message: 0x%x:%s", error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
}


/**
 * Convert a RISC OS pathname to a file: URL.
 *
 * \param  path  RISC OS pathname
 * \return  URL, allocated on heap, or 0 on failure
 */

char *path_to_url(const char *path)
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
 * Convert a file: URL to a RISC OS pathname.
 *
 * \param  url  a file: URL
 * \return  RISC OS pathname, allocated on heap, or 0 on failure
 */

char *url_to_path(const char *url)
{
	char *temp_name, *r;
	char *filename;

	if (strncmp(url, "file:/", 5))
		return NULL;

	if (!strncmp(url, "file:///", 8))
		temp_name = curl_unescape(url + 7, strlen(url) - 7);
	else
		temp_name = curl_unescape(url + 5, strlen(url) - 5);

	if (!temp_name) {
		warn_user("NoMemory", 0);
		return NULL;
	}

	filename = malloc(strlen(temp_name) + 100);
	if (!filename) {
	  	curl_free(temp_name);
		warn_user("NoMemory", 0);
		return NULL;
	}
	r = __riscosify(temp_name, 0, __RISCOSIFY_NO_SUFFIX,
			filename, strlen(temp_name) + 100, 0);
	if (r == 0) {
		LOG(("__riscosify failed"));
		return NULL;
	}
	curl_free(temp_name);
	return filename;
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
	os_error *error;
  	char *temp_name;
  	wimp_full_message_data_xfer message;

	if (!content || !content->source_data) {
		warn_user("MiscError", "No document source");
		return;
	}
	
	/* try to load local files directly. */
	temp_name = url_to_path(content->url);
	if (temp_name) {
		snprintf(message.file_name, 212, "%s", temp_name);
		message.file_name[211] = '\0';
		free(temp_name);
	} else {
		/* We cannot release the requested filename until after it has finished
		   being used. As we can't easily find out when this is, we simply don't
		   bother releasing it and simply allow it to be re-used next time NetSurf
		   is started. The memory overhead from doing this is under 1 byte per
		   filename. */
		temp_name = ro_filename_request();
		if (!temp_name) {
		  	warn_user("NoMemory", 0);
		  	return;
		}
		snprintf(message.file_name, 212, "%s.%s",
				CACHE_FILENAME_PREFIX, temp_name);
		message.file_name[211] = '\0';
		error = xosfile_save_stamped(message.file_name,
				ro_content_filetype(content),
				content->source_data,
				content->source_data + content->source_size);
		if (error) {
			LOG(("xosfile_save_stamped failed: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
			return;
		}
	}

	/* begin the DataOpen protocol */
	message.your_ref = 0;
	message.size = 44 + ((strlen(message.file_name) + 4) & (~3u));
	message.action = message_DATA_OPEN;
	message.w = 0;
	message.i = 0;
	message.pos.x = 0;
	message.pos.y = 0;
	message.est_size = 0;
	message.file_type = 0xfff;
	ro_message_send_message(wimp_USER_MESSAGE_RECORDED, (wimp_message*)&message, 0,
			ro_gui_view_source_bounce);
}


void ro_gui_view_source_bounce(wimp_message *message) {
	char *filename;
	os_error *error;
	char command[256];

	/* run the file as text */
	filename = ((wimp_full_message_data_xfer *)message)->file_name;
	sprintf(command, "@RunType_FFF %s", filename);
	error = xwimp_start_task(command, 0);
	if (error) {
		LOG(("xwimp_start_task failed: 0x%x: %s",
					error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
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
	char warn_buffer[300];

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


/**
 * Test whether it's okay to shutdown, prompting the user if not.
 *
 * \return true iff it's okay to shutdown immediately
 */

bool ro_gui_prequit(void)
{
	return ro_gui_download_prequit();
}

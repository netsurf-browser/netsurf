/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 */

#define _GNU_SOURCE  /* for strndup */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <curl/curl.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include "netsurf/content/content.h"
#include "netsurf/content/fetch.h"
#include "netsurf/content/urldb.h"
#include "netsurf/desktop/401login.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/cookies.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/options.h"
#include "netsurf/gtk/gtk_gui.h"
#include "netsurf/gtk/gtk_options.h"
#include "netsurf/gtk/gtk_completion.h"
#include "netsurf/gtk/options.h"
#include "netsurf/gtk/gtk_throbber.h"
#include "netsurf/gtk/gtk_history.h"
#include "netsurf/render/box.h"
#include "netsurf/render/form.h"
#include "netsurf/render/html.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utf8.h"
#include "netsurf/utils/utils.h"

/* Where to search for shared resources.  Must have trailing / */
#define RESPATH "/usr/share/netsurf/"

bool gui_in_multitask = false;

char *default_stylesheet_url;
char *adblock_stylesheet_url;
char *options_file_location;
char *glade_file_location;

struct gui_window *search_current_window = 0;

GtkWindow *wndAbout;

GladeXML *gladeWindows;
GtkWindow *wndTooltip;
GtkLabel *labelTooltip;

/**
 * Locate a shared resource file by searching known places in order.
 *
 * \param  buf      buffer to write to.  must be at least PATH_MAX chars
 * \param  filename file to look for
 * \param  def      default to return if file not found
 * \return buf
 *
 * Search order is: ~/.netsurf/, $NETSURFRES/ (where NETSURFRES is an
 * environment variable), and finally the path specified by the #define
 * at the top of this file.
 */

static char *find_resource(char *buf, const char *filename, const char *def)
{
	char *cdir = getenv("HOME");
	char t[PATH_MAX];

	if (cdir != NULL) {
		strcpy(t, cdir);
		strcat(t, "/.netsurf/");
		strcat(t, filename);
		realpath(t, buf);
		if (access(buf, R_OK) == 0)
			return buf;
	}

	cdir = getenv("NETSURFRES");

	if (cdir != NULL) {
		realpath(cdir, buf);
		strcat(buf, "/");
		strcat(buf, filename);
		if (access(buf, R_OK) == 0)
			return buf;
	}

	strcpy(t, RESPATH);
	strcat(t, filename);
	realpath(t, buf);
	if (access(buf, R_OK) == 0)
		return buf;

	if (def[0] == '~') {
		snprintf(t, PATH_MAX, "%s%s", getenv("HOME"), def + 1);
		realpath(t, buf);
	} else {
		realpath(def, buf);
	}

	return buf;
}

/**
 * Check that ~/.netsurf/ exists, and if it doesn't, create it.
 */
static void check_homedir(void)
{
	char *hdir = getenv("HOME");
	char buf[BUFSIZ];

	if (hdir == NULL) {
		/* we really can't continue without a home directory. */
		LOG(("HOME is not set - nowhere to store state!"));
		die("NetSurf requires HOME to be set in order to run.\n");

	}

	snprintf(buf, BUFSIZ, "%s/.netsurf", hdir);
	if (access(buf, F_OK) != 0) {
		LOG(("You don't have a ~/.netsurf - creating one for you."));
		if (mkdir(buf, 0777) == -1) {
			LOG(("Unable to create %s", buf));
			die("NetSurf requires ~/.netsurf to exist, but it cannot be created.\n");
		}
	}
}

void gui_init(int argc, char** argv)
{
	char buf[PATH_MAX];
	PangoFontDescription *fontdesc;

	gtk_init(&argc, &argv);

	check_homedir();

	find_resource(buf, "netsurf.glade", "./gtk/res/netsurf.glade");
	LOG(("Using '%s' as Glade template file", buf));
	glade_file_location = strdup(buf);

	glade_init();
	gladeWindows = glade_xml_new(glade_file_location, NULL, NULL);
	if (gladeWindows == NULL)
		die("Unable to load Glade window definitions.\n");
	glade_xml_signal_autoconnect(gladeWindows);

	find_resource(buf, "netsurf.xpm", "./gtk/res/netsurf.xpm");
	gtk_window_set_default_icon_from_file(buf, NULL);

	wndTooltip = glade_xml_get_widget(gladeWindows, "wndTooltip");
	labelTooltip = glade_xml_get_widget(gladeWindows, "tooltip");

	nsgtk_completion_init();

	find_resource(buf, "throbber.gif", "./gtk/res/throbber.gif");
	nsgtk_throbber_initialise(buf);
	if (nsgtk_throbber == NULL)
		die("Unable to load throbber image.\n");

	find_resource(buf, "Choices", "~/.netsurf/Choices");
	LOG(("Using '%s' as Choices file", buf));
	options_file_location = strdup(buf);
	options_read(buf);

	/* check what the font settings are, setting them to a default font
	 * if they're not set - stops Pango whinging
	 */
#define SETFONTDEFAULT(x,y) (x) = ((x) != NULL) ? (x) : strdup((y))
	SETFONTDEFAULT(option_font_sans, "Sans");
	SETFONTDEFAULT(option_font_serif, "Serif");
	SETFONTDEFAULT(option_font_mono, "Monospace");
	SETFONTDEFAULT(option_font_cursive, "Serif");
	SETFONTDEFAULT(option_font_fantasy, "Serif");

	nsgtk_options_init();

	if (!option_cookie_file) {
		find_resource(buf, "Cookies", "~/.netsurf/Cookies");
		LOG(("Using '%s' as Cookies file", buf));
		option_cookie_file = strdup(buf);
	}
	if (!option_cookie_jar) {
		find_resource(buf, "Cookies", "~/.netsurf/Cookies");
		LOG(("Using '%s' as Cookie Jar file", buf));
		option_cookie_jar = strdup(buf);
	}
	if (!option_cookie_file || !option_cookie_jar)
		die("Failed initialising cookie options");

	if (!option_url_file) {
		find_resource(buf, "URLs", "~/.netsurf/URLs");
		LOG(("Using '%s' as URL file", buf));
		option_url_file = strdup(buf);
	}

	find_resource(buf, "messages", "./gtk/res/messages");
	LOG(("Using '%s' as Messages file", buf));
	messages_load(buf);

	/* set up stylesheet urls */
	find_resource(buf, "default.css", "./gtk/res/default.css");
	default_stylesheet_url = path_to_url(buf);
	LOG(("Using '%s' as Default CSS URL", default_stylesheet_url));

	find_resource(buf, "adblock.css", "./gtk/res/adblock.css");
	adblock_stylesheet_url = path_to_url(buf);
	LOG(("Using '%s' as AdBlock CSS URL", adblock_stylesheet_url));

	urldb_load(option_url_file);
	urldb_load_cookies(option_cookie_file);

	wndAbout = GTK_WINDOW(glade_xml_get_widget(gladeWindows, "wndAbout"));
	gtk_label_set_text(GTK_LABEL(
		glade_xml_get_widget(gladeWindows, "labelVersion")),
		netsurf_version);
	gtk_image_set_from_file(GTK_IMAGE(
		glade_xml_get_widget(gladeWindows, "imageLogo")),
		find_resource(buf, "netsurf-logo.png", "netsurf-logo.png"));
	fontdesc = pango_font_description_from_string("Monospace 8");
	gtk_widget_modify_font(GTK_WIDGET(
		glade_xml_get_widget(gladeWindows, "textviewGPL")), fontdesc);
	nsgtk_history_init();
}


void gui_init2(int argc, char** argv)
{
	const char *addr = "http://netsurf-browser.org/";

        if (option_homepage_url != NULL)
                addr = option_homepage_url;

	if (argc > 1) addr = argv[1];
	browser_window_create(addr, 0, 0, true);
}


void gui_poll(bool active)
{
	CURLMcode code;
	fd_set read_fd_set, write_fd_set, exc_fd_set;
	int max_fd;
	GPollFD *fd_list[1000];
	unsigned int fd_count = 0;

	if (active) {
		fetch_poll();
		FD_ZERO(&read_fd_set);
		FD_ZERO(&write_fd_set);
		FD_ZERO(&exc_fd_set);
		code = curl_multi_fdset(fetch_curl_multi,
				&read_fd_set,
				&write_fd_set,
				&exc_fd_set,
				&max_fd);
		assert(code == CURLM_OK);
		for (int i = 0; i <= max_fd; i++) {
			if (FD_ISSET(i, &read_fd_set)) {
				GPollFD *fd = malloc(sizeof *fd);
				fd->fd = i;
				fd->events = G_IO_IN | G_IO_HUP | G_IO_ERR;
				g_main_context_add_poll(0, fd, 0);
				fd_list[fd_count++] = fd;
			}
			if (FD_ISSET(i, &write_fd_set)) {
				GPollFD *fd = malloc(sizeof *fd);
				fd->fd = i;
				fd->events = G_IO_OUT | G_IO_ERR;
				g_main_context_add_poll(0, fd, 0);
				fd_list[fd_count++] = fd;
			}
			if (FD_ISSET(i, &exc_fd_set)) {
				GPollFD *fd = malloc(sizeof *fd);
				fd->fd = i;
				fd->events = G_IO_ERR;
				g_main_context_add_poll(0, fd, 0);
				fd_list[fd_count++] = fd;
			}
		}
	}
	gtk_main_iteration_do(true);
	for (unsigned int i = 0; i != fd_count; i++) {
		g_main_context_remove_poll(0, fd_list[i]);
		free(fd_list[i]);
	}
}


void gui_multitask(void)
{
	gui_in_multitask = true;
	while (gtk_events_pending())
		gtk_main_iteration();
	gui_in_multitask = false;
}


void gui_quit(void)
{
	urldb_save_cookies(option_cookie_jar);
	urldb_save(option_url_file);
	free(default_stylesheet_url);
	free(adblock_stylesheet_url);
	free(option_cookie_file);
	free(option_cookie_jar);
}



struct gui_download_window *gui_download_window_create(const char *url,
		const char *mime_type, struct fetch *fetch,
		unsigned int total_size)
{
	return 0;
}


void gui_download_window_data(struct gui_download_window *dw, const char *data,
		unsigned int size)
{
}


void gui_download_window_error(struct gui_download_window *dw,
		const char *error_msg)
{
}


void gui_download_window_done(struct gui_download_window *dw)
{
}


void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control)
{

	int i;
	struct form_option *option;

	LOG(("Trying to open select menu..."));

	for (i = 0, option = control->data.select.items; option;
		i++, option = option->next) {
		LOG(("Option: %s", option->text));
	}

}

void gui_window_save_as_link(struct gui_window *g, struct content *c)
{
}

void gui_launch_url(const char *url)
{
}


bool gui_search_term_highlighted(struct gui_window *g,
		unsigned start_offset, unsigned end_offset,
		unsigned *start_idx, unsigned *end_idx)
{
	return false;
}


void warn_user(const char *warning, const char *detail)
{
}

void die(const char * const error)
{
	fprintf(stderr, "%s", error);
	exit(EXIT_FAILURE);
}


void hotlist_visited(struct content *content)
{
}

void gui_cert_verify(struct browser_window *bw, struct content *c,
		const struct ssl_cert_info *certs, unsigned long num) {}

utf8_convert_ret utf8_to_local_encoding(const char *string, size_t len,
		char **result)
{
	assert(string && result);

	if (len == 0)
		len = strlen(string);

	*result = strndup(string, len);
	if (!(*result))
		return UTF8_CONVERT_NOMEM;

	return UTF8_CONVERT_OK;
}

utf8_convert_ret utf8_from_local_encoding(const char *string, size_t len,
		char **result)
{
	assert(string && result);

	if (len == 0)
		len = strlen(string);

	*result = strndup(string, len);
	if (!(*result))
		return UTF8_CONVERT_NOMEM;

	return UTF8_CONVERT_OK;
}

char *path_to_url(const char *path)
{
	char *r = malloc(strlen(path) + 7 + 1);

	strcpy(r, "file://");
	strcat(r, path);

	return r;
}

char *url_to_path(const char *url)
{
	return strdup(url + 5);
}

bool cookies_update(const char *domain, const struct cookie_data *data)
{
	return true;
}

void nsgtk_choices_apply_clicked(GtkWidget *widget) {
  LOG(("Apply button clicked!"));
}


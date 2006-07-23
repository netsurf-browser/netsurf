/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
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

GladeXML *gladeWindows;

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

	if (cdir != NULL) {
		strcpy(buf, cdir);
		strcat(buf, "/.netsurf/");
		strcat(buf, filename);
		if (access(buf, R_OK) == 0)
			return buf;
	}

	cdir = getenv("NETSURFRES");

	if (cdir != NULL) {
		strcpy(buf, cdir);
		strcat(buf, "/");
		strcat(buf, filename);
		if (access(buf, R_OK) == 0)
			return buf;
	}

	strcpy(buf, RESPATH);
	strcat(buf, filename);
	if (access(buf, R_OK) == 0)
		return buf;

	strcpy(buf, def);
	return buf;
}

void gui_init(int argc, char** argv)
{
	char buf[PATH_MAX];

	gtk_init(&argc, &argv);
	
	/* TODO: make this search for the file using the resource finding
	 * function above
	 */
	glade_file_location = strdup("./gtk/netsurf.glade");
	
	glade_init();
	gladeWindows = glade_xml_new(glade_file_location, NULL, NULL);

	glade_xml_signal_autoconnect(gladeWindows);

	find_resource(buf, "Choices", "Choices");
	LOG(("Using '%s' as Choices file", buf));
	options_file_location = strdup(buf);
	options_read(buf);

	nsgtk_options_init();

	if (!option_cookie_file) {
		find_resource(buf, "Cookies", "Cookies");
		LOG(("Using '%s' as Cookies file", buf));
		option_cookie_file = strdup(buf);
	}
	if (!option_cookie_jar) {
		find_resource(buf, "Cookies", "Cookies");
		LOG(("Using '%s' as Cookie Jar file", buf));
		option_cookie_jar = strdup(buf);
	}
	if (!option_cookie_file || !option_cookie_jar)
		die("Failed initialising cookie options");

	find_resource(buf, "messages", "messages");
	LOG(("Using '%s' as Messages file", buf));
	messages_load(buf);

	/* set up stylesheet urls */
	find_resource(buf, "Default.css", "Default.css");
	default_stylesheet_url = path_to_url(buf);
	LOG(("Using '%s' as Default CSS URL", default_stylesheet_url));

	find_resource(buf, "AdBlock.css", "AdBlock.css");
	adblock_stylesheet_url = path_to_url(buf);
	LOG(("Using '%s' as AdBlock CSS URL", adblock_stylesheet_url));


	urldb_load_cookies(option_cookie_file);
}


void gui_init2(int argc, char** argv)
{
	const char *addr = "http://netsurf.sourceforge.net/";

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


void gui_401login_open(struct browser_window *bw, struct content *c,
		const char *realm) {}
void gui_cert_verify(struct browser_window *bw, struct content *c,
		const struct ssl_cert_info *certs, unsigned long num) {}

void global_history_add(const char *url) {}

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


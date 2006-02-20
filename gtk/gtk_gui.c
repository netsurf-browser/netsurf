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
#include <curl/curl.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include "netsurf/content/content.h"
#include "netsurf/content/fetch.h"
#include "netsurf/content/url_store.h"
#include "netsurf/desktop/401login.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/options.h"
#include "netsurf/gtk/gtk_gui.h"
#include "netsurf/render/box.h"
#include "netsurf/render/form.h"
#include "netsurf/render/html.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utf8.h"
#include "netsurf/utils/utils.h"


bool gui_in_multitask = false;

char *default_stylesheet_url;
char *adblock_stylesheet_url;

struct gui_window *search_current_window = 0;


void gui_init(int argc, char** argv)
{
	char *home;
	char buf[1024];

	/* All our resources are stored in ~/.netsurf/ */
	home = getenv("HOME");
	if (!home)
		die("Couldn't find HOME");

	gtk_init(&argc, &argv);

	snprintf(buf, sizeof buf, "%s/.netsurf/Choices", home);
	options_read(buf);

	if (!option_cookie_file) {
		snprintf(buf, sizeof buf, "%s/.netsurf/Cookies", home);
		option_cookie_file = strdup(buf);
	}
	if (!option_cookie_jar) {
		snprintf(buf, sizeof buf, "%s/.netsurf/Cookies", home);
		option_cookie_jar = strdup(buf);
	}
	if (!option_cookie_file || !option_cookie_jar)
		die("Failed initialising cookie options");

	snprintf(buf, sizeof buf, "%s/.netsurf/messages", home);
	messages_load(buf);

	/* set up stylesheet urls */
	snprintf(buf, sizeof buf, "file:///%s/.netsurf/Default.css", home);
	default_stylesheet_url = strdup(buf);
	snprintf(buf, sizeof buf, "file:///%s/.netsurf/AdBlock.css", home);
	adblock_stylesheet_url = strdup(buf);
	if (!default_stylesheet_url || !adblock_stylesheet_url)
		die("Failed duplicating stylesheet strings");
}


void gui_init2(int argc, char** argv)
{
	browser_window_create("http://netsurf.sourceforge.net/", 0, 0);
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
	free(default_stylesheet_url);
	free(adblock_stylesheet_url);
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
	fprintf(stderr, error);
	exit(EXIT_FAILURE);
}


void hotlist_visited(struct content *content)
{
}


struct history *history_create(void) { return 0; }
void history_add(struct history *history, struct content *content,
	char *frag_id) {}
void history_update(struct history *history, struct content *content) {}
void history_destroy(struct history *history) {}
void history_back(struct browser_window *bw, struct history *history) {}
void history_forward(struct browser_window *bw, struct history *history) {}

void gui_401login_open(struct browser_window *bw, struct content *c,
                       char *realm) {}

void schedule(int t, void (*callback)(void *p), void *p) {}
void schedule_remove(void (*callback)(void *p), void *p) {}
void schedule_run(void) {}

void global_history_add(struct url_content *data) {}

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

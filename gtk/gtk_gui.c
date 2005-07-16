/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include "netsurf/content/content.h"
#include "netsurf/desktop/401login.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/options.h"
#include "netsurf/render/box.h"
#include "netsurf/render/form.h"
#include "netsurf/render/html.h"
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

	snprintf(buf, sizeof buf, "%s/.netsurf/messages", home);
	messages_load(buf);

	/* set up stylesheet urls */
	snprintf(buf, sizeof buf, "file:///%s/.netsurf/Default.css", home);
	default_stylesheet_url = strdup(buf);
	snprintf(buf, sizeof buf, "file:///%s/.netsurf/AdBlock.css", home);
	adblock_stylesheet_url = strdup(buf);
}


void gui_init2(int argc, char** argv)
{
	browser_window_create("http://netsurf.sourceforge.net/", 0, 0);
}


void gui_poll(bool active)
{
	gtk_main_iteration_do(!active);
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


bool gui_search_term_highlighted(struct gui_window *g, struct box *box,
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

void global_history_add(struct gui_window *g) {}

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

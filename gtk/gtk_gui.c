/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

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
#include "netsurf/render/box.h"
#include "netsurf/render/form.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


bool gui_in_multitask = false;


void gui_init(int argc, char** argv)
{
	gtk_init(&argc, &argv);
	messages_load("messages");
}


void gui_init2(int argc, char** argv)
{
	browser_window_create("http://netsurf.sourceforge.net/", 0, 0);
}


void gui_poll(bool active)
{
	/*netsurf_quit =*/ gtk_main_iteration_do(!active);
}


void gui_multitask(void)
{
	gui_in_multitask = true;
	while (gtk_events_pending())
		/*netsurf_quit =*/ gtk_main_iteration();
	gui_in_multitask = false;
}


void gui_quit(void)
{
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

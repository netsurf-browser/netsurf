/*
 * Copyright 2008 Michael Lester <element3260@gmail.com>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GTK_DOWNLOAD_H
#define GTK_DOWNLOAD_H

#include <gtk/gtk.h>

enum {
	NSGTK_DOWNLOAD_PROGRESS,
	NSGTK_DOWNLOAD_INFO,
	NSGTK_DOWNLOAD_REMAINING,
	NSGTK_DOWNLOAD_SPEED,
	NSGTK_DOWNLOAD_PULSE,
	NSGTK_DOWNLOAD_STATUS,
	NSGTK_DOWNLOAD,

	NSGTK_DOWNLOAD_N_COLUMNS
};

typedef enum {
	NSGTK_DOWNLOAD_NONE,
	NSGTK_DOWNLOAD_WORKING,
	NSGTK_DOWNLOAD_ERROR,
	NSGTK_DOWNLOAD_COMPLETE,
	NSGTK_DOWNLOAD_CANCELED
} nsgtk_download_status;

typedef enum {
	NSGTK_DOWNLOAD_PAUSE 	= 1 << 0,
	NSGTK_DOWNLOAD_RESUME	= 1 << 1,
	NSGTK_DOWNLOAD_CANCEL 	= 1 << 2,
	NSGTK_DOWNLOAD_CLEAR 	= 1 << 3
} nsgtk_download_actions;

struct gui_download_window {
	struct fetch *fetch;
	nsgtk_download_actions sensitivity;
	nsgtk_download_status status;
	
	GString *name;
	GString *time_left;
	gint size_total;
	gint size_downloaded;
	gint progress;
	gfloat time_remaining;
	gfloat start_time;
	gfloat speed;
	gchar *filename;
	
	GtkTreeRowReference *row;
	GIOChannel *write;
	GError *error;
};

typedef	void (*nsgtk_download_selection_action)(struct gui_download_window *dl);

void nsgtk_download_init(void);
void nsgtk_download_destroy (void);
bool nsgtk_check_for_downloads(GtkWindow *parent);
void nsgtk_download_show(GtkWindow *parent);
void nsgtk_download_add(gchar *url, gchar *destination);

#endif

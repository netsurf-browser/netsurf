/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
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

#ifndef NETSURF_GTK_OPTIONS_H
#define NETSURF_GTK_OPTIONS_H

#include <gtk/gtk.h>

extern GtkDialog *wndPreferences;

GtkDialog* nsgtk_options_init(struct browser_window *bw, GtkWindow *parent);		/** Init options and load window */
void nsgtk_options_load(void);		/** Load current options into window */
void nsgtk_options_save(void);		/** Save options from window */

/** Callbacks */
static void dialog_response_handler (GtkDialog *dlg, gint res_id);
static gboolean on_dialog_close (GtkDialog *dlg, gboolean stay_alive);
#endif

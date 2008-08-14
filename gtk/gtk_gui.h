/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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

#include <stdbool.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

extern bool gui_in_multitask;
extern GladeXML *gladeWindows;
extern char *glade_file_location;
extern char *options_file_location;
extern char *res_dir_location;
extern char *print_options_file_location;

extern GtkWindow *wndAbout;

extern GtkWindow *wndTooltip;
extern GtkLabel *labelTooltip;

extern GtkDialog *wndOpenFile;

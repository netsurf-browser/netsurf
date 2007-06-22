/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 */

#include <stdbool.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

extern bool gui_in_multitask;
extern GladeXML *gladeWindows;
extern char *glade_file_location;
extern char *options_file_location;

extern GtkWindow *wndAbout;

extern GtkWindow *wndTooltip;
extern GtkLabel *labelTooltip;

extern GtkDialog *wndOpenFile;

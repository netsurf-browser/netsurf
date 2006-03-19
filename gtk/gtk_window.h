/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 */

#include <gtk/gtk.h>


extern GtkWidget *current_widget;
extern GdkDrawable *current_drawable;
extern GdkGC *current_gc;
#ifdef CAIRO_VERSION
extern cairo_t *current_cr;
#endif


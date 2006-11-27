/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Target independent plotting (GDK / GTK+ interface).
 */

#ifndef NETSURF_GTK_PLOTTERS_H
#define NETSURF_GTK_PLOTTERS_H 1

#include <gtk/gtk.h>

struct plotter_table;

extern const struct plotter_table nsgtk_plotters;

extern GtkWidget *current_widget;
extern GdkDrawable *current_drawable;
extern GdkGC *current_gc;
#ifdef CAIRO_VERSION
extern cairo_t *current_cr;
#endif

void nsgtk_plot_set_scale(float s);
float nsgtk_plot_get_scale(void);
void nsgtk_set_colour(colour c);
void nsgtk_plot_caret(int x, int y, int h);

#endif /* NETSURF_GTK_PLOTTERS_H */

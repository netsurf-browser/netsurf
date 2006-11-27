/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 */

#ifndef NETSURF_GTK_SCAFFOLDING_H
#define NETSURF_GTK_SCAFFOLDING_H 1

#include <gtk/gtk.h>
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/plotters.h"

typedef struct gtk_scaffolding nsgtk_scaffolding;

nsgtk_scaffolding *nsgtk_new_scaffolding(struct gui_window *toplevel);

gboolean nsgtk_scaffolding_is_busy(nsgtk_scaffolding *scaffold);

void nsgtk_attach_toplevel_viewport(nsgtk_scaffolding *g, GtkViewport *vp);

void nsgtk_scaffolding_destroy(nsgtk_scaffolding *scaffold);

#endif /* NETSURF_GTK_SCAFFOLDING_H */

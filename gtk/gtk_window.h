/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
 */

#ifndef NETSURF_GTK_WINDOW_H
#define NETSURF_GTK_WINDOW_H 1

#include "netsurf/desktop/gui.h"
#include "netsurf/gtk/gtk_scaffolding.h"

void nsgtk_window_reflow_content(struct gui_window *g);
void nsgtk_reflow_all_windows(void);

nsgtk_scaffolding *nsgtk_get_scaffold(struct gui_window *g);
struct browser_window *nsgtk_get_browser_for_gui(struct gui_window *g);

float nsgtk_get_scale_for_gui(struct gui_window *g);
int nsgtk_gui_window_update_targets(struct gui_window *g);

#endif /* NETSURF_GTK_WINDOW_H */

/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

#ifndef NETSURF_GTK_OPTIONS_H
#define NETSURF_GTK_OPTIONS_H

#include <gtk/gtk.h>

extern GtkWindow *wndChoices;

void nsgtk_options_init(void);		/** Init options and load window */
void nsgtk_options_load(void);		/** Load current options into window */
void nsgtk_options_save(void);		/** Save options from window */

#endif

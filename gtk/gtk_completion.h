/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

#ifndef _NETSURF_GTK_COMPLETION_H_
#define _NETSURF_GTK_COMPLETION_H_

#include <gtk/gtk.h>

extern GtkListStore *nsgtk_completion_list;

void nsgtk_completion_init(void);
void nsgtk_completion_update(const char *prefix);
gboolean nsgtk_completion_match(GtkEntryCompletion *completion,
				const gchar *key,
				GtkTreeIter *iter,
				gpointer user_data);
#endif

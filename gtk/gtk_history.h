/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

#ifndef __NSGTK_HISTORY_H__
#define __NSGTK_HISTORY_H__

#include <gtk/gtk.h>

extern GtkWindow *wndHistory;

void nsgtk_history_init(void);
void nsgtk_history_update(void);
void nsgtk_history_row_activated(GtkTreeView *, GtkTreePath *,
				GtkTreeViewColumn *, gpointer);

#endif /* __NSGTK_HISTORY_H__ */

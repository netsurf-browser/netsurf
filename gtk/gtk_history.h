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

#ifndef __NSGTK_HISTORY_H__
#define __NSGTK_HISTORY_H__

#include <gtk/gtk.h>

extern GtkWindow *wndHistory;


struct history_model {
	GtkListStore *history_list;
	GtkTreeModel *history_filter;
		GtkTreeModel *site_filter;
			GtkTreeModel *site_sort;
			GtkTreeView *site_treeview;
			GtkTreeSelection *site_selection;
	GtkListStore *domain_list;	
		GtkTreeModel *domain_filter;
			GHashTable *domain_hash;
			GtkTreeModel *domain_sort;
			GtkTreeView *domain_treeview;
			GtkTreeSelection *domain_selection;
};

bool nsgtk_history_init(void);

#endif /* __NSGTK_HISTORY_H__ */

/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

#include <gtk/gtk.h>
#include "gtk/gtk_completion.h"
#include "content/urldb.h"
#include "utils/log.h"
#include "desktop/options.h"

GtkListStore *nsgtk_completion_list;

static void nsgtk_completion_empty(void);
static bool nsgtk_completion_udb_callback(const char *url,
		const struct url_data *data);

void nsgtk_completion_init(void)
{
	nsgtk_completion_list = gtk_list_store_new(1, G_TYPE_STRING);

}

gboolean nsgtk_completion_match(GtkEntryCompletion *completion,
                                const gchar *key,
                                GtkTreeIter *iter,
                                gpointer user_data)
{
	char *b[4096];		/* no way of finding out its length :( */
	gtk_tree_model_get(GTK_TREE_MODEL(nsgtk_completion_list), iter,
			0, b, -1);

	/* TODO: work out why this works, when there's no code to implement
	 * it.  I boggle. */

	return TRUE;

}

void nsgtk_completion_empty(void)
{
  	gtk_list_store_clear(nsgtk_completion_list);
}

bool nsgtk_completion_udb_callback(const char *url, const struct url_data *data)
{
	GtkTreeIter iter;

	if (data->visits != 0) {
		gtk_list_store_append(nsgtk_completion_list, &iter);
		gtk_list_store_set(nsgtk_completion_list, &iter, 0, url, -1);
	}
	return true;
}

void nsgtk_completion_update(const char *prefix)
{
	nsgtk_completion_empty();
	if (option_url_suggestion == true)
		urldb_iterate_partial(prefix, nsgtk_completion_udb_callback);
}


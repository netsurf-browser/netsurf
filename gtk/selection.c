/*
 * Copyright 2008 Mike Lester <element3260@gmail.com>
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

#include <string.h>
#include <gtk/gtk.h>

#include "utils/log.h"

#include "desktop/gui.h"
#include "desktop/selection.h"
#include "desktop/browser.h"
#include "gtk/selection.h"
#include "gtk/window.h"
 
static GString *current_selection = NULL;
static GtkClipboard *clipboard;


bool gui_add_to_clipboard(const char *text, size_t length, bool space)
{
	/* add the text from this box */
	current_selection = g_string_append_len (current_selection,
		text, length);
	if (space) g_string_append (current_selection, " ");
	return true;
}

bool gui_copy_to_clipboard(struct selection *s)
{
	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	if (s->defined && selection_copy_to_clipboard(s))
		gui_commit_clipboard();
	return TRUE;
}

void gui_start_selection(struct gui_window *g)
{
	if (current_selection == NULL)
		current_selection = g_string_new(NULL);
	else
		g_string_set_size(current_selection, 0);
		
	gtk_widget_grab_focus(GTK_WIDGET(nsgtk_window_get_layout(g)));
}

void gui_clear_selection(struct gui_window *g)
{
}

void gui_paste_from_clipboard(struct gui_window *g, int x, int y)
{
	gchar *text;
	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
 	text = gtk_clipboard_wait_for_text (clipboard);
 	/* clipboard_wait... converts the string to utf8 for us */
 	if (text != NULL)
		browser_window_paste_text(nsgtk_get_browser_window(g), 
				text, strlen(text), true);
	g_free(text);
}

bool gui_empty_clipboard(void)
{
	if (!current_selection)
		current_selection = g_string_new(NULL);
	else
		g_string_set_size(current_selection, 0);

	return true;
}

bool gui_commit_clipboard(void)
{
	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard, current_selection->str, -1);
	gui_empty_clipboard();
	
	return true;
}
 

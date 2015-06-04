/*
 * Copyright 2015 Vincent Sanders <vince@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 * Implementation of gtk builtin resource handling.
 *
 * \todo resource handling in gtk3 has switched to using GResource
 */

#include <gtk/gtk.h>

#include "gtk/resources.h"

#ifdef __GNUC__
extern const guint8 menu_cursor_pixdata[] __attribute__ ((__aligned__ (4)));
#else
extern const guint8 menu_cursor_pixdata[];
#endif

GdkCursor *nsgtk_create_menu_cursor(void)
{
	GdkCursor *cursor = NULL;
	GdkPixbuf *pixbuf;

	pixbuf = gdk_pixbuf_new_from_inline(-1, menu_cursor_pixdata, FALSE, NULL);
	cursor = gdk_cursor_new_from_pixbuf(gdk_display_get_default(), pixbuf, 0, 3);
	g_object_unref (pixbuf);

	return cursor;
}

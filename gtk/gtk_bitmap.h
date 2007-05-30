/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
 */

#ifndef NS_GTK_BITMAP_H
#define NS_GTK_BITMAP_H

#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "image/bitmap.h"

GdkPixbuf *gtk_bitmap_get_primary(struct bitmap*);
GdkPixbuf *gtk_bitmap_get_pretile_x(struct bitmap*);
GdkPixbuf *gtk_bitmap_get_pretile_y(struct bitmap*);
GdkPixbuf *gtk_bitmap_get_pretile_xy(struct bitmap*);



#endif /* NS_GTK_BITMAP_H */

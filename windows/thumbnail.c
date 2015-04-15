/*
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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

#include "utils/config.h"

#include <windows.h>

#include "utils/log.h"
#include "desktop/browser.h"
#include "desktop/thumbnail.h"
#include "image/bitmap.h"

#include "windows/bitmap.h"
#include "windows/gui.h"
#include "windows/plot.h"
#include "content/hlcache.h"


bool 
thumbnail_create(hlcache_handle *content, 
		 struct bitmap *bitmap)
{
	int width;
	int height;
	HDC hdc, bufferdc, minidc;
	struct bitmap *fsbitmap;
	struct redraw_context ctx = {
		.interactive = false,
		.background_images = true,
		.plot = &win_plotters
	};

	width = min(content_get_width(content), 1024);
	height = ((width * bitmap->height) + (bitmap->width / 2)) /
			bitmap->width;

	LOG(("bitmap %p for content %p width %d, height %d",
	     bitmap, content, width, height));

	/* create two memory device contexts to put the bitmaps in */
	bufferdc = CreateCompatibleDC(NULL);
	if ((bufferdc == NULL)) {
		return false;
	}

	minidc = CreateCompatibleDC(NULL);
	if ((minidc == NULL)) {
		DeleteDC(bufferdc);
		return false;
	}

	/* create a full size bitmap and plot into it */
	fsbitmap = win32_bitmap_create(width, height, BITMAP_NEW | BITMAP_CLEAR_MEMORY | BITMAP_OPAQUE);

	SelectObject(bufferdc, fsbitmap->windib);

	hdc = plot_hdc;
	plot_hdc = bufferdc;
	thumbnail_redraw(content, width, height, &ctx);
	plot_hdc = hdc;
	
	/* scale bitmap bufferbm into minibm */
	SelectObject(minidc, bitmap->windib);

	bitmap->opaque = true;

	StretchBlt(minidc, 0, 0, bitmap->width, bitmap->height, bufferdc, 0, 0, width, height, SRCCOPY);
	
	DeleteDC(bufferdc);
	DeleteDC(minidc);
	win32_bitmap_destroy(fsbitmap);
			
	return true;
}

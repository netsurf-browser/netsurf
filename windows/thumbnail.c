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

#include "content/urldb.h"
#include "desktop/browser.h"
#include "utils/log.h"

#include "windows/bitmap.h"
#include "windows/gui.h"
#include "windows/plot.h"
#include "content/hlcache.h"


bool 
thumbnail_create(hlcache_handle *content, 
		 struct bitmap *bitmap,
		 const char *url)
{
	int width = content_get_width(content);
	int height = content_get_height(content);
	int i;
	uint8_t *pixdata;
	HDC hdc, minidc;
	HBITMAP bufferbm, minibm, minibm2;
	BITMAPINFO *bmi; 
	BITMAPINFOHEADER bmih;

	LOG(("creating thumbnail %p for url %s content %p", bitmap, url, content));

	bmi = malloc(sizeof(BITMAPINFOHEADER) + (bitmap->width * bitmap->height * 4));
	if (bmi == NULL) {
		return false;
	}

	bmih.biSize = sizeof(bmih);
	bmih.biWidth = bitmap->width;
	bmih.biHeight = - bitmap->height;
	bmih.biPlanes = 1;
	bmih.biBitCount = 32;
	bmih.biCompression = BI_RGB;
	bmih.biSizeImage = 4 * bitmap->height * bitmap->width;
	bmih.biXPelsPerMeter = 3600; /* 100 dpi */
	bmih.biYPelsPerMeter = 3600;
	bmih.biClrUsed = 0;
	bmih.biClrImportant = 0;
	bmi->bmiHeader = bmih;
	
	doublebuffering = true;

	if (bufferdc != NULL)
		DeleteDC(bufferdc);
	hdc = GetDC(current_hwnd);
	
	bufferdc = CreateCompatibleDC(hdc);
	if ((bufferdc == NULL) || (bmi == NULL)) {
		doublebuffering = false;
		ReleaseDC(current_hwnd, hdc);
		return false;
	}

	bufferbm = CreateCompatibleBitmap(hdc, width, height);
	if (bufferbm == NULL) {
		doublebuffering = false;
		ReleaseDC(current_hwnd, hdc);
		free(bmi);
		return false;
	}
	SelectObject(bufferdc, bufferbm);
	thumbnail = true;
	content_redraw(content, 0, 0, width, height, 0, 0,
			width, height, 1.0, 0xFFFFFF);
	thumbnail = false;
	
/*	scale bufferbm to minibm */

	minidc = CreateCompatibleDC(hdc);
	if (minidc == NULL) {
		doublebuffering = false;
		DeleteObject(bufferbm);
		ReleaseDC(current_hwnd, hdc);
		free(bmi);
		return false;
	}
	
	minibm = CreateCompatibleBitmap(hdc, bitmap->width, bitmap->height);
	if (minibm == NULL) {
		doublebuffering = false;
		DeleteObject(bufferbm);
		DeleteDC(minidc);
		ReleaseDC(current_hwnd, hdc);
		free(bmi);
		return false;
	}
	ReleaseDC(current_hwnd, hdc);
	
	SelectObject(minidc, minibm);
	
	StretchBlt(minidc, 0, 0, bitmap->width, bitmap->height, bufferdc, 0, 0,
			width, height, SRCCOPY);
	minibm2 = CreateCompatibleBitmap(minidc, bitmap->width, 
			bitmap->height);
	if (minibm2 == NULL) {
		doublebuffering = false;
		DeleteObject(bufferbm);
		DeleteObject(minibm);
		DeleteDC(minidc);
		free(bmi);
		return false;
	}
	SelectObject(minidc, minibm2);
	
/*	save data from minibm bmi */
	GetDIBits(minidc, minibm, 0, 1 - bitmap->height,
			bmi->bmiColors, bmi, DIB_RGB_COLORS);

	pixdata = (uint8_t *)(bitmap->pixdata);
	for (i = 0; i < bitmap->width * bitmap->height; i++) {
		pixdata[4 * i] = bmi->bmiColors[i].rgbRed;
		pixdata[4 * i + 1] = bmi->bmiColors[i].rgbGreen;
		pixdata[4 * i + 2] = bmi->bmiColors[i].rgbBlue;
		pixdata[4 * i + 3] = 0xFF;
	}
	doublebuffering = false;
		   
	DeleteObject(bufferbm);
	DeleteObject(minibm);
	DeleteObject(minibm2);
	DeleteDC(minidc);
	free(bmi);
	if (url)
		urldb_set_thumbnail(url, bitmap);
			
	doublebuffering = false;
	return true;
}

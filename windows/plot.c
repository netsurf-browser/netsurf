/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <windows.h>

#include "utils/log.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "desktop/gui.h"
#include "desktop/plotters.h"

#include "windows/bitmap.h"
#include "windows/font.h"
#include "windows/gui.h"
#include "windows/plot.h"

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

/* set NSWS_PLOT_DEBUG to 0 for no debugging, 1 for debugging */
#define NSWS_PLOT_DEBUG 0

HWND current_hwnd;
struct gui_window *current_gui;
bool doublebuffering;
bool thumbnail = false;
HDC bufferdc;
static float nsws_plot_scale = 1.0;
static RECT localhistory_clip;


static bool clip(int x0, int y0, int x1, int y1)
{

#if NSWS_PLOT_DEBUG
	LOG(("clip %d,%d to %d,%d thumbnail %d", x0, y0, x1, y1, thumbnail));
#endif
	RECT *clip = gui_window_clip_rect(current_gui);
	if (clip == NULL)
		clip = &localhistory_clip;
	x0 = MAX(x0, 0);
	y0 = MAX(y0, 0);
	if (!((current_gui == NULL) || (thumbnail))) {
		x1 = MIN(x1, gui_window_width(current_gui));
		y1 = MIN(y1, gui_window_height(current_gui));
	}
	clip->left = x0;
	clip->top = y0 ;
	clip->right = x1;
	clip->bottom = y1;
	
	return true;
}

static bool line(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
#if NSWS_PLOT_DEBUG
	LOG(("ligne from %d,%d to %d,%d thumbnail %d", x0, y0, x1, y1,
			thumbnail));
#endif
	RECT *clipr = gui_window_clip_rect(current_gui);
	if (clipr == NULL)
		clipr = &localhistory_clip;
	HRGN clipregion = CreateRectRgnIndirect(clipr);
	if (clipregion == NULL) {
		return false;
	}

	HDC hdc = doublebuffering ? bufferdc : GetDC(current_hwnd);
	if (hdc == NULL) {
		DeleteObject(clipregion);
		return false;
	}
	COLORREF col = (DWORD)(style->stroke_colour & 0x00FFFFFF);
	/* windows 0x00bbggrr */
	DWORD penstyle = PS_GEOMETRIC | ((style->stroke_type ==
			PLOT_OP_TYPE_DOT) ? PS_DOT :
			(style->stroke_type == PLOT_OP_TYPE_DASH) ? PS_DASH:
			0);
	LOGBRUSH lb = {BS_SOLID, col, 0};
	HPEN pen = ExtCreatePen(penstyle, style->stroke_width, &lb, 0, NULL);
	if (pen == NULL) {
		DeleteObject(clipregion);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HGDIOBJ bak = SelectObject(hdc, (HGDIOBJ) pen);
	if (bak == NULL) {
		DeleteObject(pen);
		DeleteObject(clipregion);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	RECT r;
	r.left = x0;
	r.top = y0;
	r.right = x1;
	r.bottom = y1;
	
	SelectClipRgn(hdc, clipregion);
	
	MoveToEx(hdc, x0, y0, (LPPOINT) NULL);

	LineTo(hdc, x1, y1);
	
	SelectClipRgn(hdc, NULL);
/*	ValidateRect(current_hwnd, &r);
*/	
	pen = SelectObject(hdc, bak);
	DeleteObject(pen);
	DeleteObject(clipregion);
	if (!doublebuffering)
		ReleaseDC(current_hwnd, hdc);
	return true;
}

static bool rectangle(int x0, int y0, int x1, int y1, const plot_style_t 
		*style)
{
	x1++;
	y1++;
	x0 = MAX(x0, 0);
	y0 = MAX(y0, 0);
	if (!((current_gui == NULL) || (thumbnail))) {
		x1 = MIN(x1, gui_window_width(current_gui));
		y1 = MIN(y1, gui_window_height(current_gui));
	}
	
#if NSWS_PLOT_DEBUG	
	LOG(("rectangle from %d,%d to %d,%d thumbnail %d", x0, y0, x1, y1,
			thumbnail));
#endif
	HDC hdc = doublebuffering ? bufferdc : GetDC(current_hwnd);
	if (hdc == NULL) {
		return false;
	}
	RECT *clipr = gui_window_clip_rect(current_gui);
	if (clipr == NULL)
		clipr = &localhistory_clip;
	HRGN clipregion = CreateRectRgnIndirect(clipr);
	if (clipregion == NULL) {
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	COLORREF pencol = (DWORD)(style->stroke_colour & 0x00FFFFFF);
	DWORD penstyle = PS_GEOMETRIC | 
			(style->stroke_type == PLOT_OP_TYPE_DOT ? PS_DOT :
			(style->stroke_type == PLOT_OP_TYPE_DASH ? PS_DASH :
			(style->stroke_type == PLOT_OP_TYPE_NONE ? PS_NULL : 
			0)));
	LOGBRUSH lb = {BS_SOLID, pencol, 0};
	LOGBRUSH lb1 = {BS_SOLID, style->fill_colour, 0};
	if (style->fill_type == PLOT_OP_TYPE_NONE)
		lb1.lbStyle = BS_HOLLOW;
	
	HPEN pen = ExtCreatePen(penstyle, style->stroke_width, &lb, 0, NULL);
	if (pen == NULL) {
		DeleteObject(clipregion);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HGDIOBJ penbak = SelectObject(hdc, (HGDIOBJ) pen);
	if (penbak == NULL) {
		DeleteObject(clipregion);
		DeleteObject(pen);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HBRUSH brush = CreateBrushIndirect(&lb1);
	if (brush  == NULL) {
		DeleteObject(clipregion);
		SelectObject(hdc, penbak);
		DeleteObject(pen);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HGDIOBJ brushbak = SelectObject(hdc, (HGDIOBJ) brush);
	if (brushbak == NULL) {
		DeleteObject(clipregion);
		SelectObject(hdc, penbak);
		DeleteObject(pen);
		DeleteObject(brush);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	RECT r;
	r.left = x0;
	r.top = y0;
	r.right = x1;
	r.bottom = y1;

	SelectClipRgn(hdc, clipregion);
	
	Rectangle(hdc, x0, y0, x1, y1);

	SelectClipRgn(hdc, NULL);
/*	ValidateRect(current_hwnd, &r);
*/	
	pen = SelectObject(hdc, penbak);
	brush = SelectObject(hdc, brushbak);
	DeleteObject(clipregion);
	DeleteObject(pen);
	DeleteObject(brush);
	if (!doublebuffering)
		ReleaseDC(current_hwnd, hdc);
	return true;
}


static bool polygon(const int *p, unsigned int n, const plot_style_t *style)
{
#if NSWS_PLOT_DEBUG
	LOG(("polygon %d points thumbnail %d", n, thumbnail));
#endif
	POINT points[n];
	unsigned int i;
	HDC hdc = doublebuffering ? bufferdc : GetDC(current_hwnd);
	if (hdc == NULL) {
		return false;
	}
	RECT *clipr = gui_window_clip_rect(current_gui);
	if (clipr == NULL)
		clipr = &localhistory_clip;
	HRGN clipregion = CreateRectRgnIndirect(clipr);
	if (clipregion == NULL) {
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	COLORREF pencol = (DWORD)(style->fill_colour & 0x00FFFFFF);
	COLORREF brushcol = (DWORD)(style->fill_colour & 0x00FFFFFF);
	HPEN pen = CreatePen(PS_GEOMETRIC | PS_NULL, 1, pencol);
	if (pen == NULL) {
		DeleteObject(clipregion);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HPEN penbak = SelectObject(hdc, pen);
	if (penbak == NULL) {
		DeleteObject(clipregion);
		DeleteObject(pen);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HBRUSH brush = CreateSolidBrush(brushcol);
	if (brush == NULL) {
		DeleteObject(clipregion);
		SelectObject(hdc, penbak);
		DeleteObject(pen);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HBRUSH brushbak = SelectObject(hdc, brush);
	if (brushbak == NULL) {
		DeleteObject(clipregion);
		SelectObject(hdc, penbak);
		DeleteObject(pen);
		DeleteObject(brush);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	SetPolyFillMode(hdc, WINDING);
	for (i = 0; i < n; i++) {
		points[i].x = (long) p[2 * i];
		points[i].y = (long) p[2 * i + 1];

#if NSWS_PLOT_DEBUG
		printf ("%ld,%ld ", points[i].x, points[i].y);
#endif
	}
	
	SelectClipRgn(hdc, clipregion);
	
	if (n >= 2)
		Polygon(hdc, points, n);

	SelectClipRgn(hdc, NULL);

	pen = SelectObject(hdc, penbak);
	brush = SelectObject(hdc, brushbak);
	DeleteObject(clipregion);
	DeleteObject(pen);
	DeleteObject(brush);
	if (!doublebuffering)
		ReleaseDC(current_hwnd, hdc);
#if NSWS_PLOT_DEBUG
		printf("\n");
#endif
	return true;
}


static bool text(int x, int y, const char *text, size_t length, 
		const plot_font_style_t *style)
{
#if NSWS_PLOT_DEBUG
	LOG(("words %s at %d,%d thumbnail %d", text, x, y, thumbnail));
#endif
	HDC hdc = doublebuffering ? bufferdc : GetDC(current_hwnd);
	if (hdc == NULL) {
		return false;
	}
	RECT *clipr = gui_window_clip_rect(current_gui);
	if (clipr == NULL)
		clipr = &localhistory_clip;
	HRGN clipregion = CreateRectRgnIndirect(clipr);
	if (clipregion == NULL) {
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
			
	HFONT fontbak, font = get_font(style);
	if (font == NULL) {
		DeleteObject(clipregion);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	int wlen;
	SIZE s;
	LPWSTR wstring;
	RECT r;
	fontbak = (HFONT) SelectObject(hdc, font);
	GetTextExtentPoint(hdc, text, length, &s);

	r.left = x;
	r.top = y  - (3 * s.cy) / 4;
	r.right = x + s.cx;
	r.bottom = y + s.cy / 4;

	SelectClipRgn(hdc, clipregion);
	
	SetTextAlign(hdc, TA_BASELINE | TA_LEFT);
	if ((style->background & 0xFF000000) != 0x01000000) 
		/* 100% alpha */
		SetBkColor(hdc, (DWORD) (style->background & 0x00FFFFFF));
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, (DWORD) (style->foreground & 0x00FFFFFF));

	wlen = MultiByteToWideChar(CP_UTF8, 0, text, length, NULL, 0);
	wstring = malloc(2 * (wlen + 1));
	if (wstring == NULL) {
		return false;
	}
	MultiByteToWideChar(CP_UTF8, 0, text, length, wstring, wlen);
	TextOutW(hdc, x, y, wstring, wlen);

	SelectClipRgn(hdc, NULL);
/*	ValidateRect(current_hwnd, &r);
*/
	free(wstring);
	font = SelectObject(hdc, fontbak);
	DeleteObject(clipregion);
	DeleteObject(font);
	if (!doublebuffering)
		ReleaseDC(current_hwnd, hdc);
	return true;
}

static bool disc(int x, int y, int radius, const plot_style_t *style)
{
#if NSWS_PLOT_DEBUG
	LOG(("disc at %d,%d radius %d thumbnail %d", x, y, radius, thumbnail));
#endif
	HDC hdc = doublebuffering ? bufferdc : GetDC(current_hwnd);
	if (hdc == NULL) {
		return false;
	}
	RECT *clipr = gui_window_clip_rect(current_gui);
	if (clipr == NULL)
		clipr = &localhistory_clip;
	HRGN clipregion = CreateRectRgnIndirect(clipr);
	if (clipregion == NULL) {
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
			
	COLORREF col = (DWORD)((style->fill_colour | style->stroke_colour)
			& 0x00FFFFFF);
	HPEN pen = CreatePen(PS_GEOMETRIC | PS_SOLID, 1, col);
	if (pen == NULL) {
		DeleteObject(clipregion);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HGDIOBJ penbak = SelectObject(hdc, (HGDIOBJ) pen);
	if (penbak == NULL) {
		DeleteObject(clipregion);
		DeleteObject(pen);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HBRUSH brush = CreateSolidBrush(col);
	if (brush == NULL) {
		DeleteObject(clipregion);
		SelectObject(hdc, penbak);
		DeleteObject(pen);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HGDIOBJ brushbak = SelectObject(hdc, (HGDIOBJ) brush);
	if (brushbak == NULL) {
		DeleteObject(clipregion);
		SelectObject(hdc, penbak);
		DeleteObject(pen);
		DeleteObject(brush);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	RECT r;
	r.left = x - radius;
	r.top = y - radius;
	r.right = x + radius;
	r.bottom = y + radius;

	SelectClipRgn(hdc, clipregion);
	
	if (style->fill_type == PLOT_OP_TYPE_NONE)
		Arc(hdc, x - radius, y - radius, x + radius, y + radius,
				x - radius, y - radius,
				x - radius, y - radius);
	else
		Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);

	SelectClipRgn(hdc, NULL);
/*	ValidateRect(current_hwnd, &r);
*/
	pen = SelectObject(hdc, penbak);
	brush = SelectObject(hdc, brushbak);
	DeleteObject(clipregion);
	DeleteObject(pen);
	DeleteObject(brush);
	if (!doublebuffering)
		ReleaseDC(current_hwnd, hdc);
	return true;
}

static bool arc(int x, int y, int radius, int angle1, int angle2,
	    		const plot_style_t *style)
{
#if NSWS_PLOT_DEBUG
	LOG(("arc centre %d,%d radius %d from %d to %d", x, y, radius,
			angle1, angle2));
#endif
	HDC hdc = doublebuffering ? bufferdc : GetDC(current_hwnd);
	if (hdc == NULL) {
		return false;
	}
	RECT *clipr = gui_window_clip_rect(current_gui);
	if (clipr == NULL)
		clipr = &localhistory_clip;
	HRGN clipregion = CreateRectRgnIndirect(clipr);
	if (clipregion == NULL) {
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	COLORREF col = (DWORD)(style->stroke_colour & 0x00FFFFFF);
	HPEN pen = CreatePen(PS_GEOMETRIC | PS_SOLID, 1, col);
	if (pen == NULL) {
		DeleteObject(clipregion);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HGDIOBJ penbak = SelectObject(hdc, (HGDIOBJ) pen);
	if (penbak == NULL) {
		DeleteObject(clipregion);
		DeleteObject(pen);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	RECT r;
	int q1, q2;
	double a1=1.0, a2=1.0, b1=1.0, b2=1.0;
	q1 = (int) ((angle1 + 45) / 90) - 45;
	q2 = (int) ((angle2 + 45) / 90) - 45;
	while (q1 > 4)
		q1 -= 4;
	while (q2 > 4)
		q2 -= 4;
	while (q1 <= 0)
		q1 += 4;
	while (q2 <= 0)
		q2 += 4;
	angle1 = ((angle1 + 45) % 90) - 45;
	angle2 = ((angle2 + 45) % 90) - 45;
	
	switch(q1) {
	case 1:
		a1 = 1.0;
		b1 = -tan((M_PI / 180) * angle1);
		break;
	case 2:
		b1 = -1.0;
		a1 = -tan((M_PI / 180) * angle1);
		break;
	case 3:
		a1 = -1.0;
		b1 = tan((M_PI / 180) * angle1);
		break;
	case 4:
		b1 = 1.0;
		a1 = tan((M_PI / 180) * angle1);
		break;
	}
	
	switch(q2) {
	case 1:
		a2 = 1.0;
		b2 = -tan((M_PI / 180) * angle2);
		break;
	case 2:
		b2 = -1.0;
		a2 = -tan((M_PI / 180) * angle2);
		break;
	case 3:
		a2 = -1.0;
		b2 = tan((M_PI / 180) * angle2);
		break;
	case 4:
		b2 = 1.0;
		a2 = tan((M_PI / 180) * angle2);
		break;
	}
	
	r.left = x - radius;
	r.top = y - radius;
	r.right = x + radius;
	r.bottom = y + radius;

	SelectClipRgn(hdc, clipregion);
	
	Arc(hdc, x - radius, y - radius, x + radius, y + radius,
				x + (int)(a1 * radius), y + (int)(b1 * radius),
				x + (int)(a2 * radius), y + (int)(b2 * radius));
	
	SelectClipRgn(hdc, NULL);
/*	ValidateRect(current_hwnd, &r);
*/
	pen = SelectObject(hdc, penbak);
	DeleteObject(clipregion);
	DeleteObject(pen);
	if (!doublebuffering)
		ReleaseDC(current_hwnd, hdc);
	return true;
}


static bool bitmap(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bitmap_flags_t flags)
{
#if NSWS_PLOT_DEBUG
	LOG(("%p bitmap %d,%d width %d height %d", current_hwnd, x, y, width,
			height));
#endif
	if (bitmap == NULL)
		return false;
	HDC hdc = doublebuffering ? bufferdc : GetDC(current_hwnd);
	if (hdc == NULL) {
		return false;
	}
	RECT *cliprect = gui_window_clip_rect(current_gui);
	if (cliprect == NULL)
		cliprect = &localhistory_clip;
	HRGN clipregion = CreateRectRgnIndirect(cliprect);
	if (clipregion == NULL) {
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HDC Memhdc = CreateCompatibleDC(hdc);
	if (Memhdc == NULL) {
		DeleteObject(clipregion);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	BITMAPINFOHEADER bmih;
	RECT r;
	int v, vv, vi, h, hh, width4, transparency;
	unsigned char alpha;
	bool modifying = false;

	SelectClipRgn(hdc, clipregion);
	if ((bitmap->width != width) || (bitmap->height != height)) {
		bitmap = bitmap_scale(bitmap, width, height);
		if (bitmap == NULL)
			return false;
		modifying = true;
	}
	
	if ((flags & BITMAPF_REPEAT_X) || (flags & BITMAPF_REPEAT_Y)) {
		struct bitmap *prebitmap = bitmap_pretile(bitmap,
				cliprect->right - x,
				cliprect->bottom - y, flags);
		if (prebitmap == NULL)
			return false;
		if (modifying) {
			free(bitmap->pixdata);
			free(bitmap);
		}
		modifying = true;
		bitmap = prebitmap;
	}
	BITMAP MemBM;
	BITMAPINFO *bmi = (BITMAPINFO *) malloc(sizeof(BITMAPINFOHEADER) + 
			(bitmap->width * bitmap->height * 4));
	if (bmi == NULL) {
		DeleteObject(clipregion);
		DeleteDC(Memhdc);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HBITMAP MemBMh = CreateCompatibleBitmap(
			hdc, bitmap->width, bitmap->height);
	if (MemBMh == NULL){
		DeleteObject(clipregion);
		free(bmi);
		DeleteDC(Memhdc);
		if (!doublebuffering)
			ReleaseDC(current_hwnd, hdc);
		return false;
	}

	/* save 'background' data for alpha channel work */
	SelectObject(Memhdc, MemBMh);
	BitBlt(Memhdc, 0, 0, bitmap->width, bitmap->height, hdc, x, y,
			SRCCOPY);
	GetObject(MemBMh, sizeof(BITMAP), &MemBM);
	
	bmih.biSize = sizeof(bmih);
	bmih.biWidth = bitmap->width;
	bmih.biHeight = bitmap->height;
	bmih.biPlanes = 1;
	bmih.biBitCount = 32;
	bmih.biCompression = BI_RGB;
	bmih.biSizeImage = 4 * bitmap->height * bitmap->width;
	bmih.biXPelsPerMeter = 3600; /* 100 dpi */
	bmih.biYPelsPerMeter = 3600;
	bmih.biClrUsed = 0;
	bmih.biClrImportant = 0;
	bmi->bmiHeader = bmih;
	
	GetDIBits(hdc, MemBMh, 0, bitmap->height, bmi->bmiColors, bmi,
			DIB_RGB_COLORS);
	
	/* then load 'foreground' bits from bitmap->pixdata */
	
	width4 = bitmap->width * 4;
	for (v = 0, vv = 0, vi = (bitmap->height - 1) * width4; 
			v < bitmap->height; 
			v++, vv += bitmap->width, vi -= width4) {
		for (h = 0, hh = 0; h < bitmap->width; h++, hh += 4) {
			alpha = bitmap->pixdata[vi + hh + 3];
/* multiplication of alpha value; subject to profiling could be optional */
			if (alpha == 0xFF) {
				bmi->bmiColors[vv + h].rgbBlue =
						bitmap->pixdata[vi + hh + 2];
				bmi->bmiColors[vv + h].rgbGreen =
						bitmap->pixdata[vi + hh + 1];
				bmi->bmiColors[vv + h].rgbRed = 
						bitmap->pixdata[vi + hh];
			} else if (alpha > 0) {
				transparency = 0x100 - alpha;
				bmi->bmiColors[vv + h].rgbBlue =
						(bmi->bmiColors[vv + h].rgbBlue
						* transparency + 
						(bitmap->pixdata[vi + hh + 2]) *
						alpha) >> 8;
				bmi->bmiColors[vv + h].rgbGreen =
						(bmi->bmiColors[vv + h].
						rgbGreen
						* transparency +
						(bitmap->pixdata[vi + hh + 1]) *
						alpha) >> 8;
				bmi->bmiColors[vv + h].rgbRed =
						(bmi->bmiColors[vv + h].rgbRed
						* transparency +
						bitmap->pixdata[vi + hh]
						* alpha) >> 8;
			}
/* alternative simple 2/3 stage alpha value handling */
/*			if (bitmap->pixdata[vi + hh + 3] > 0xAA) {
				bmi->bmiColors[vv + h].rgbBlue = 
						bitmap->pixdata[vi + hh + 2];
				bmi->bmiColors[vv + h].rgbGreen =
						bitmap->pixdata[vi + hh + 1];
				bmi->bmiColors[vv + h].rgbRed =
						bitmap->pixdata[vi + hh];
			} else if (bitmap->pixdata[vi + hh + 3] > 0x70){
				bmi->bmiColors[vv + h].rgbBlue = 
				(bmi->bmiColors[vv + h].rgbBlue +
				bitmap->pixdata[vi + hh + 2]) / 2;
				bmi->bmiColors[vv + h].rgbRed = 
				(bmi->bmiColors[vv + h].rgbRed +
				bitmap->pixdata[vi + hh]) / 2;
				bmi->bmiColors[vv + h].rgbGreen = 
				(bmi->bmiColors[vv + h].rgbGreen +
				bitmap->pixdata[vi + hh + 1]) / 2;
			} else if (bitmap->pixdata[vi + hh + 3] > 0x30){
				bmi->bmiColors[vv + h].rgbBlue = 
				(bmi->bmiColors[vv + h].rgbBlue * 3 +
				bitmap->pixdata[vi + hh + 2]) / 4;
				bmi->bmiColors[vv + h].rgbRed = 
				(bmi->bmiColors[vv + h].rgbRed * 3 +
				bitmap->pixdata[vi + hh]) / 4;
				bmi->bmiColors[vv + h].rgbGreen = 
				(bmi->bmiColors[vv + h].rgbGreen * 3 +
				bitmap->pixdata[vi + hh + 1]) / 4;
			} 
*/		}
	}
	SetDIBitsToDevice(hdc, x, y, bitmap->width, bitmap->height,
			0, 0, 0, bitmap->height, (const void *) bmi->bmiColors,
			bmi, DIB_RGB_COLORS);
	
	r.left = x;
	r.top = y;
	r.right = x + bitmap->width;
	r.bottom = y + bitmap->height;
	if (modifying && bitmap && bitmap->pixdata) {
		free(bitmap->pixdata);
		free(bitmap);
	}
	
/*	ValidateRect(current_hwnd, &r);
*/	free(bmi);
/*	SelectClipRgn(hdc, NULL);
*/	DeleteObject(clipregion);
	DeleteObject(MemBMh);
	DeleteDC(Memhdc);
	if (!doublebuffering)
		ReleaseDC(current_hwnd, hdc);
	return true;
}

static bool flush(void)
{
#if NSWS_PLOT_DEBUG
	LOG(("flush unimplemented"));
#endif
	return true;
}

static bool path(const float *p, unsigned int n, colour fill, float width,
		 colour c, const float transform[6])
{
#if NSWS_PLOT_DEBUG
	LOG(("path unimplemented"));
#endif
	return true;
}

void nsws_plot_set_scale(float s)
{
	nsws_plot_scale = s;
}

float nsws_plot_get_scale(void)
{
	return nsws_plot_scale;
}

struct plotter_table plot = {
	.rectangle = rectangle,
	.line = line,
	.polygon = polygon,
	.clip = clip,
	.text = text,
	.disc = disc,
	.arc = arc,
	.bitmap = bitmap,
	.flush = flush,
	.path = path,
	.option_knockout = true,
};

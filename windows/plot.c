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
bool thumbnail = false;
static float nsws_plot_scale = 1.0;
static RECT localhistory_clip;


static RECT plot_clip;

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


	plot_clip.left = x0;
	plot_clip.top = y0;
	plot_clip.right = x1 + 1; /* rectangle co-ordinates are exclusive */
	plot_clip.bottom = y1 + 1; /* rectangle co-ordinates are exclusive */

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

	HDC hdc = GetDC(current_hwnd);
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

		ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HGDIOBJ bak = SelectObject(hdc, (HGDIOBJ) pen);
	if (bak == NULL) {
		DeleteObject(pen);
		DeleteObject(clipregion);

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

	ReleaseDC(current_hwnd, hdc);
	return true;
}

static bool rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	x1++;
	y1++;
#if NSWS_PLOT_DEBUG
	LOG(("rectangle from %d,%d to %d,%d thumbnail %d", x0, y0, x1, y1,
	     thumbnail));
#endif
	HDC hdc = GetDC(current_hwnd);
	if (hdc == NULL) {
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
		ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HGDIOBJ penbak = SelectObject(hdc, (HGDIOBJ) pen);
	if (penbak == NULL) {
		DeleteObject(pen);

		ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HBRUSH brush = CreateBrushIndirect(&lb1);
	if (brush  == NULL) {
		SelectObject(hdc, penbak);
		DeleteObject(pen);

		ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HGDIOBJ brushbak = SelectObject(hdc, (HGDIOBJ) brush);
	if (brushbak == NULL) {
		SelectObject(hdc, penbak);
		DeleteObject(pen);
		DeleteObject(brush);

		ReleaseDC(current_hwnd, hdc);
		return false;
	}

	Rectangle(hdc, x0, y0, x1, y1);

	pen = SelectObject(hdc, penbak);
	brush = SelectObject(hdc, brushbak);
	DeleteObject(pen);
	DeleteObject(brush);

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
	HDC hdc = GetDC(current_hwnd);
	if (hdc == NULL) {
		return false;
	}
	RECT *clipr = gui_window_clip_rect(current_gui);
	if (clipr == NULL)
		clipr = &localhistory_clip;
	HRGN clipregion = CreateRectRgnIndirect(clipr);
	if (clipregion == NULL) {

		ReleaseDC(current_hwnd, hdc);
		return false;
	}
	COLORREF pencol = (DWORD)(style->fill_colour & 0x00FFFFFF);
	COLORREF brushcol = (DWORD)(style->fill_colour & 0x00FFFFFF);
	HPEN pen = CreatePen(PS_GEOMETRIC | PS_NULL, 1, pencol);
	if (pen == NULL) {
		DeleteObject(clipregion);
		ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HPEN penbak = SelectObject(hdc, pen);
	if (penbak == NULL) {
		DeleteObject(clipregion);
		DeleteObject(pen);

		ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HBRUSH brush = CreateSolidBrush(brushcol);
	if (brush == NULL) {
		DeleteObject(clipregion);
		SelectObject(hdc, penbak);
		DeleteObject(pen);

		ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HBRUSH brushbak = SelectObject(hdc, brush);
	if (brushbak == NULL) {
		DeleteObject(clipregion);
		SelectObject(hdc, penbak);
		DeleteObject(pen);
		DeleteObject(brush);

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
	HDC hdc = GetDC(current_hwnd);
	if (hdc == NULL) {
		return false;
	}
	RECT *clipr = gui_window_clip_rect(current_gui);
	if (clipr == NULL)
		clipr = &localhistory_clip;
	HRGN clipregion = CreateRectRgnIndirect(clipr);
	if (clipregion == NULL) {
		ReleaseDC(current_hwnd, hdc);
		return false;
	}

	HFONT fontbak, font = get_font(style);
	if (font == NULL) {
		DeleteObject(clipregion);

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

	ReleaseDC(current_hwnd, hdc);
	return true;
}

static bool disc(int x, int y, int radius, const plot_style_t *style)
{
#if NSWS_PLOT_DEBUG
	LOG(("disc at %d,%d radius %d thumbnail %d", x, y, radius, thumbnail));
#endif
	HDC hdc = GetDC(current_hwnd);
	if (hdc == NULL) {
		return false;
	}
	RECT *clipr = gui_window_clip_rect(current_gui);
	if (clipr == NULL)
		clipr = &localhistory_clip;
	HRGN clipregion = CreateRectRgnIndirect(clipr);
	if (clipregion == NULL) {

		ReleaseDC(current_hwnd, hdc);
		return false;
	}

	COLORREF col = (DWORD)((style->fill_colour | style->stroke_colour)
			       & 0x00FFFFFF);
	HPEN pen = CreatePen(PS_GEOMETRIC | PS_SOLID, 1, col);
	if (pen == NULL) {
		DeleteObject(clipregion);

		ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HGDIOBJ penbak = SelectObject(hdc, (HGDIOBJ) pen);
	if (penbak == NULL) {
		DeleteObject(clipregion);
		DeleteObject(pen);

		ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HBRUSH brush = CreateSolidBrush(col);
	if (brush == NULL) {
		DeleteObject(clipregion);
		SelectObject(hdc, penbak);
		DeleteObject(pen);

		ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HGDIOBJ brushbak = SelectObject(hdc, (HGDIOBJ) brush);
	if (brushbak == NULL) {
		DeleteObject(clipregion);
		SelectObject(hdc, penbak);
		DeleteObject(pen);
		DeleteObject(brush);

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
	HDC hdc = GetDC(current_hwnd);
	if (hdc == NULL) {
		return false;
	}
	RECT *clipr = gui_window_clip_rect(current_gui);
	if (clipr == NULL)
		clipr = &localhistory_clip;
	HRGN clipregion = CreateRectRgnIndirect(clipr);
	if (clipregion == NULL) {

		ReleaseDC(current_hwnd, hdc);
		return false;
	}
	COLORREF col = (DWORD)(style->stroke_colour & 0x00FFFFFF);
	HPEN pen = CreatePen(PS_GEOMETRIC | PS_SOLID, 1, col);
	if (pen == NULL) {
		DeleteObject(clipregion);

		ReleaseDC(current_hwnd, hdc);
		return false;
	}
	HGDIOBJ penbak = SelectObject(hdc, (HGDIOBJ) pen);
	if (penbak == NULL) {
		DeleteObject(clipregion);
		DeleteObject(pen);

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

	ReleaseDC(current_hwnd, hdc);
	return true;
}

static bool
plot_block(COLORREF col, int x, int y, int width, int height)
{
	HDC hdc;
	HRGN clipregion;
	HGDIOBJ original = NULL;

	/* Bail early if we can */
	if ((x >= plot_clip.right) ||
	    ((x + width) < plot_clip.left) ||
	    (y >= plot_clip.bottom) ||
	    ((y + height) < plot_clip.top)) {
		/* Image completely outside clip region */
		return true;	
	}	

	clipregion = CreateRectRgnIndirect(&plot_clip);
	if (clipregion == NULL) {
		return false;
	}

	hdc = GetDC(current_hwnd);
	if (hdc == NULL) {
		DeleteObject(clipregion);
		return false;
	}

	SelectClipRgn(hdc, clipregion);

	/* Saving the original pen object */
	original = SelectObject(hdc,GetStockObject(DC_PEN)); 

	SelectObject(hdc, GetStockObject(DC_PEN));
	SelectObject(hdc, GetStockObject(DC_BRUSH));
	SetDCPenColor(hdc, col);
	SetDCBrushColor(hdc, col);
	Rectangle(hdc,x,y,width,height);

	SelectObject(hdc,original); /* Restoring the original pen object */

	DeleteObject(clipregion);
	ReleaseDC(current_hwnd, hdc);
	return true;

}

/* blunt force truma way of achiving alpha blended plotting */
static bool 
plot_alpha_bitmap(HDC hdc, 
		  struct bitmap *bitmap, 
		  int x, int y, 
		  int width, int height)
{
	HDC Memhdc;
	BITMAPINFOHEADER bmih;
	int v, vv, vi, h, hh, width4, transparency;
	unsigned char alpha;
	bool isscaled = false; /* set if the scaled bitmap requires freeing */
	BITMAP MemBM;
	BITMAPINFO *bmi;
	HBITMAP MemBMh;

#if NSWS_PLOT_DEBUG
	LOG(("%p bitmap %d,%d width %d height %d", bitmap, x, y, width, height));
	LOG(("clipped %ld,%ld to %ld,%ld",plot_clip.left, plot_clip.top, plot_clip.right, plot_clip.bottom));
#endif

	assert(bitmap != NULL);

	Memhdc = CreateCompatibleDC(hdc);
	if (Memhdc == NULL) {
		return false;
	}

	if ((bitmap->width != width) || 
	    (bitmap->height != height)) {
		LOG(("scaling from %d,%d to %d,%d", 
		     bitmap->width, bitmap->height, width, height));
		bitmap = bitmap_scale(bitmap, width, height);
		if (bitmap == NULL)
			return false;
		isscaled = true;
	}

	bmi = (BITMAPINFO *) malloc(sizeof(BITMAPINFOHEADER) +
				    (bitmap->width * bitmap->height * 4));
	if (bmi == NULL) {
		DeleteDC(Memhdc);
		return false;
	}

	MemBMh = CreateCompatibleBitmap(hdc, bitmap->width, bitmap->height);
	if (MemBMh == NULL){
		free(bmi);
		DeleteDC(Memhdc);
		return false;
	}

	/* save 'background' data for alpha channel work */
	SelectObject(Memhdc, MemBMh);
	BitBlt(Memhdc, 0, 0, bitmap->width, bitmap->height, hdc, x, y, SRCCOPY);
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
		}
	}
	SetDIBitsToDevice(hdc, x, y, bitmap->width, bitmap->height,
			  0, 0, 0, bitmap->height, 
			  (const void *) bmi->bmiColors,
			  bmi, DIB_RGB_COLORS);

	if (isscaled && bitmap && bitmap->pixdata) {
		free(bitmap->pixdata);
		free(bitmap);
	}

	free(bmi);
	DeleteObject(MemBMh);
	DeleteDC(Memhdc);
	return true;
}


static bool 
plot_bitmap(struct bitmap *bitmap, int x, int y, int width, int height)
{
	int bltres;
	HDC hdc;
	HRGN clipregion;

	/* Bail early if we can */
	if ((x >= plot_clip.right) ||
	    ((x + width) < plot_clip.left) ||
	    (y >= plot_clip.bottom) ||
	    ((y + height) < plot_clip.top)) {
		/* Image completely outside clip region */
		return true;	
	}	

	clipregion = CreateRectRgnIndirect(&plot_clip);
	if (clipregion == NULL) {
		return false;
	}

	hdc = GetDC(current_hwnd);
	if (hdc == NULL) {
		DeleteObject(clipregion);
		return false;
	}

	SelectClipRgn(hdc, clipregion);

	if (bitmap->opaque) {
		/* opaque bitmap */
		if ((bitmap->width == width) && 
		    (bitmap->height == height)) {
			/* unscaled */
			bltres = SetDIBitsToDevice(hdc,
						   x, y,
						   width, height,
						   0, 0,
						   0,
						   height,
						   bitmap->pixdata,
						   (BITMAPINFO *)bitmap->pbmi,
						   DIB_RGB_COLORS);
		} else {
			/* scaled */
			SetStretchBltMode(hdc, COLORONCOLOR);
			bltres = StretchDIBits(hdc, 
					       x, y, 
					       width, height,
					       0, 0, 
					       bitmap->width, bitmap->height,
					       bitmap->pixdata, 
					       (BITMAPINFO *)bitmap->pbmi, 
					       DIB_RGB_COLORS, 
					       SRCCOPY);


		}
	} else {
		/* Bitmap with alpha.*/
#ifdef WINDOWS_GDI_ALPHA_WORKED
		BLENDFUNCTION blnd = {  AC_SRC_OVER, 0, 0xff, AC_SRC_ALPHA };
		HDC bmihdc;
		bmihdc = CreateCompatibleDC(hdc);
		SelectObject(bmihdc, bitmap->windib);
		bltres = AlphaBlend(hdc, 
			   x, y, 
			   width, height,
			   bmihdc,
			   0, 0, 
			   bitmap->width, bitmap->height,
			blnd);
		DeleteDC(bmihdc);
#else
		bltres = plot_alpha_bitmap(hdc, bitmap, x, y, width, height);
		LOG(("bltres = %d", bltres));
#endif

	}

	DeleteObject(clipregion);
	ReleaseDC(current_hwnd, hdc);
	return true;

}

static bool
windows_plot_bitmap(int x, int y,
		    int width, int height,
		    struct bitmap *bitmap, colour bg,
		    bitmap_flags_t flags)
{
	int xf,yf;
	bool repeat_x = (flags & BITMAPF_REPEAT_X);
	bool repeat_y = (flags & BITMAPF_REPEAT_Y);

	/* Bail early if we can */

	/* check if nothing to plot */
	if (width == 0 || height == 0)
		return true;

	/* x and y define coordinate of top left of of the initial explicitly
	 * placed tile. The width and height are the image scaling and the
	 * bounding box defines the extent of the repeat (which may go in all
	 * four directions from the initial tile).
	 */

	if (!(repeat_x || repeat_y)) {
		/* Not repeating at all, so just plot it */
		if ((bitmap->width == 1) && (bitmap->height == 1)) {
			if ((*(bitmap->pixdata + 3) & 0xff) == 0) {
				return true;
			}
			return plot_block((*(COLORREF *)bitmap->pixdata) & 0xffffff, x, y, x + width, y + height);

		} else {
			return plot_bitmap(bitmap, x, y, width, height);
		}
	}

	/* Optimise tiled plots of 1x1 bitmaps by replacing with a flat fill
	 * of the area.  Can only be done when image is fully opaque. */
	if ((bitmap->width == 1) && (bitmap->height == 1)) {
		if ((*(COLORREF *)bitmap->pixdata & 0xff000000) != 0) {
			return plot_block((*(COLORREF *)bitmap->pixdata) & 0xffffff, 
					  plot_clip.left, 
					  plot_clip.top, 
					  plot_clip.right, 
					  plot_clip.bottom);
		}
	}

	/* Optimise tiled plots of bitmaps scaled to 1x1 by replacing with
	 * a flat fill of the area.  Can only be done when image is fully
	 * opaque. */
	if ((width == 1) && (height == 1)) {
		if (bitmap->opaque) {
			/** TODO: Currently using top left pixel. Maybe centre
			 *        pixel or average value would be better. */
			return plot_block((*(COLORREF *)bitmap->pixdata) & 0xffffff, 
					  plot_clip.left, 
					  plot_clip.top, 
					  plot_clip.right, 
					  plot_clip.bottom);
		}
	}
/*
	LOG(("Tiled plotting %d,%d by %d,%d",x,y,width,height));
	LOG(("clipped %ld,%ld to %ld,%ld",plot_clip.left, plot_clip.top, plot_clip.right, plot_clip.bottom));
*/

	/* get left most tile position */
	if (repeat_x)
		for (; x > plot_clip.left; x -= width);

	/* get top most tile position */
	if (repeat_y)
		for (; y > plot_clip.top; y -= height);

/*
	LOG(("repeat from %d,%d to %ld,%ld", x, y, plot_clip.right, plot_clip.bottom));
*/

	/* tile down and across to extents */
	for (xf = x; xf < plot_clip.right; xf += width) {
		for (yf = y; yf < plot_clip.bottom; yf += height) {

			plot_bitmap(bitmap, xf, yf, width, height);
			if (!repeat_y)
				break;
		}
		if (!repeat_x)
	   		break;
	}
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
	.bitmap = windows_plot_bitmap,
	.flush = flush,
	.path = path,
	.option_knockout = true,
};

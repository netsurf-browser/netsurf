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

#include <windows.h>
#include <windowsx.h>
#define _WIN32_IE (0x0501)
#include <commctrl.h>

#include "desktop/browser.h"
#include "desktop/history_core.h"
#include "desktop/plotters.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "windows/localhistory.h"
#include "windows/gui.h"
#include "windows/plot.h"

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

struct nsws_localhistory {
	HWND	hwnd;		/**< the window handle */
	int	width;		/**< the width of the memory history */
	int 	height;		/**< the height of the memory history */
	int 	guiwidth;	/**< the width of the history window */
	int 	guiheight;	/**< the height of the history window */
	int	vscroll;	/**< the vertical scroll location */
	int	hscroll;	/**< the horizontal scroll location */
};

static struct nsws_localhistory localhistory;

LRESULT CALLBACK nsws_localhistory_event_callback(HWND hwnd, UINT msg,
		WPARAM wparam, LPARAM lparam);
static void nsws_localhistory_scroll_check(struct gui_window *w);
static void nsws_localhistory_clear(struct gui_window *w);

void nsws_localhistory_init(struct gui_window *w)
{
	LOG(("gui window %p", w));
	static const char localhistorywindowclassname[] = "nsws_localhistory_window";
	WNDCLASSEX we;
	HWND mainhwnd = gui_window_main_window(w);
	INITCOMMONCONTROLSEX icc;
	HICON hIcon = nsws_window_get_ico(true);
	HICON hIconS = nsws_window_get_ico(false);
	struct browser_window *bw = gui_window_browser_window(w);
	int margin = 50;
	RECT r;
	
	localhistory.width = 0;
	localhistory.height = 0;
	current_gui = NULL;
	current_hwnd = NULL;
	doublebuffering = false;
	if ((bw != NULL) && (bw->history != NULL))
		history_size(bw->history, &(localhistory.width),
				&(localhistory.height));
	
	GetWindowRect(mainhwnd, &r);
	SetWindowPos(mainhwnd, HWND_NOTOPMOST, 0, 0, 0, 0, 
			SWP_NOSIZE | SWP_NOMOVE);

	localhistory.guiwidth = MIN(r.right - r.left - margin,
				localhistory.width + margin);
	localhistory.guiheight = MIN(r.bottom - r.top - margin, 
				localhistory.height + margin);

	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_BAR_CLASSES | ICC_WIN95_CLASSES;
#if WINVER > 0x0501
	icc.dwICC |= ICC_STANDARD_CLASSES;
#endif
	InitCommonControlsEx(&icc);
	
	we.cbSize		= sizeof(WNDCLASSEX);
	we.style		= 0;
	we.lpfnWndProc		= nsws_localhistory_event_callback;
	we.cbClsExtra		= 0;
	we.cbWndExtra		= 0;
	we.hInstance		= hinstance;
	we.hIcon		= (hIcon == NULL) ? 
			LoadIcon(NULL, IDI_APPLICATION) : hIcon;
	we.hCursor		= LoadCursor(NULL, IDC_ARROW);
	we.hbrBackground	= (HBRUSH)(COLOR_WINDOW + 1);
	we.lpszMenuName		= NULL;
	we.lpszClassName	= localhistorywindowclassname;
	we.hIconSm		= (hIconS == NULL) ? 
			LoadIcon(NULL, IDI_APPLICATION) : hIconS;
	RegisterClassEx(&we);
	LOG(("creating local history window for hInstance %p", hinstance));
	localhistory.hwnd = CreateWindow(localhistorywindowclassname, 
			"NetSurf History",WS_THICKFRAME | WS_HSCROLL |
			WS_VSCROLL | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
			CS_DBLCLKS, r.left + margin/2, r.top + margin/2,
			localhistory.guiwidth, localhistory.guiheight, NULL,
			NULL, hinstance, NULL);
	LOG(("gui_window %p width %d height %d hwnd %p", w,
			localhistory.guiwidth, localhistory.guiheight,
			localhistory.hwnd));
	current_hwnd = localhistory.hwnd;
	ShowWindow(localhistory.hwnd, SW_SHOWNORMAL);
	UpdateWindow(localhistory.hwnd);
	gui_window_set_localhistory(w, &localhistory);
	nsws_localhistory_up(w);
}

LRESULT CALLBACK nsws_localhistory_event_callback(HWND hwnd, UINT msg,
		WPARAM wparam, LPARAM lparam)
{
	bool match = false;
	struct gui_window *w = window_list;
	struct browser_window *bw = NULL;
	struct nsws_localhistory *local;
	while (w != NULL) {
		local = gui_window_localhistory(w);
		if ((local != NULL) && (local->hwnd == hwnd)) {
			match = true;
			break;
		}
		w = gui_window_iterate(w);
	}
	if (match)
		bw = gui_window_browser_window(w);
	switch(msg) {
	case WM_CREATE:
		nsws_localhistory_scroll_check(w);
		break;
	case WM_SIZE:
		localhistory.guiheight = HIWORD(lparam);
		localhistory.guiwidth = LOWORD(lparam);
		nsws_localhistory_scroll_check(w);
		current_gui = NULL;
		current_hwnd = hwnd;
		plot.rectangle(0, 0, localhistory.guiwidth,
				localhistory.guiheight, plot_style_fill_white);
		break;
	case WM_MOVE: {
		RECT r, rmain;
		if (w != NULL) {
			current_gui = w;
			current_hwnd = gui_window_main_window(w);
			GetWindowRect(hwnd, &r);
			GetWindowRect(current_hwnd, &rmain);
			gui_window_redraw(w, 
					MIN(r.top - rmain.top , 0),
					MIN(r.left - rmain.left, 0),
					gui_window_height(w) - 
					MIN(rmain.bottom - r.bottom, 0),
					gui_window_width(w) - 
					MIN(rmain.right - r.right, 0));
			current_gui = NULL;
			current_hwnd = hwnd;
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
	}
	case WM_LBUTTONUP: {
		int x,y;
		x = GET_X_LPARAM(lparam);
		y = GET_Y_LPARAM(lparam);
		if (bw == NULL)
			break;
		current_hwnd = gui_window_main_window(w);
		current_gui = w;
		if ((bw != NULL) && (history_click(bw, bw->history, x, y, false)))
			DestroyWindow(hwnd);
		else {
			current_hwnd = hwnd;
			current_gui = NULL;
		}
	}
	case WM_MOUSEMOVE: {
		int x,y;
		x = GET_X_LPARAM(lparam);
		y = GET_Y_LPARAM(lparam);
/*		if (bw != NULL)
		history_hover(bw->history, x, y, (void *)hwnd);*/
		return DefWindowProc(hwnd, msg, wparam, lparam);
		break;
	}
	case WM_VSCROLL:
	{
		if ((w == NULL) || (bw == NULL))
			break;
		SCROLLINFO si;
		int mem;
		si.cbSize = sizeof(si);
		si.fMask = SIF_ALL;
		GetScrollInfo(hwnd, SB_VERT, &si);
		mem = si.nPos;
		switch (LOWORD(wparam))
		{
			case SB_TOP:
				si.nPos = si.nMin;
				break;
			case SB_BOTTOM:
				si.nPos = si.nMax;
				break;
			case SB_LINEUP:
				si.nPos -= 30;
				break;
			case SB_LINEDOWN:
				si.nPos += 30;
				break;
			case SB_PAGEUP:
				si.nPos -= localhistory.guiheight;
				break;
			case SB_PAGEDOWN:
				si.nPos += localhistory.guiheight;
				break;
			case SB_THUMBTRACK:
				si.nPos = si.nTrackPos;
				break;
			default:
				break;
		}
		si.nPos = MIN(si.nPos, localhistory.width);
		si.nPos = MAX(si.nPos, 0);
		si.fMask = SIF_POS;
		SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
		GetScrollInfo(hwnd, SB_VERT, &si);
		if (si.nPos != mem) {
			current_gui = NULL;
			current_hwnd = hwnd;
			localhistory.vscroll += si.nPos - mem;
			plot.rectangle(0, 0, localhistory.guiwidth,
					localhistory.guiheight,
					plot_style_fill_white);
			history_redraw_rectangle(bw->history,
					localhistory.hscroll,
					localhistory.vscroll,
					localhistory.guiwidth +
					localhistory.hscroll,
					localhistory.guiheight
					+ localhistory.vscroll,
					0, 0);
		}
		break;
	}
	case WM_HSCROLL:
	{
		if ((w == NULL) || (bw == NULL))
			break;
		SCROLLINFO si;
		int mem;
		si.cbSize = sizeof(si);
		si.fMask = SIF_ALL;
		GetScrollInfo(hwnd, SB_HORZ, &si);
		mem = si.nPos;
		switch (LOWORD(wparam))
		{
			case SB_LINELEFT:
				si.nPos -= 30;
				break;
			case SB_LINERIGHT:
				si.nPos += 30;
				break;
			case SB_PAGELEFT:
				si.nPos -= localhistory.guiwidth;
				break;
			case SB_PAGERIGHT:
				si.nPos += localhistory.guiwidth;
				break;
			case SB_THUMBTRACK:
				si.nPos = si.nTrackPos;
				break;
			default:
				break;
		}
		si.nPos = MIN(si.nPos, localhistory.height);
		si.nPos = MAX(si.nPos, 0);
		si.fMask = SIF_POS;
		SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
		GetScrollInfo(hwnd, SB_HORZ, &si);
		if (si.nPos != mem) {
			current_gui = NULL;
			current_hwnd = hwnd;
			localhistory.hscroll += si.nPos - mem;
			if (bw == NULL)
				break;
			plot.rectangle(0, 0, localhistory.guiwidth,
					localhistory.guiheight,
					plot_style_fill_white);
			history_redraw_rectangle(bw->history,
					localhistory.hscroll,
					localhistory.vscroll,
					localhistory.guiwidth +
					localhistory.hscroll,
					localhistory.guiheight
					+ localhistory.vscroll,
					0, 0);
		}
		break;
	}	
	case WM_PAINT: {
		current_gui = NULL;
		current_hwnd = hwnd;
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);
		if (bw != NULL)
			history_redraw_rectangle(bw->history,
				localhistory.hscroll,
				localhistory.vscroll,
				localhistory.hscroll + localhistory.guiwidth,
				localhistory.vscroll + localhistory.guiheight,
				0, 0);
		EndPaint(hwnd, &ps);
		return DefWindowProc(hwnd, msg, wparam, lparam);
		break;
	}
	case WM_CLOSE:
		nsws_localhistory_clear(w);
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		nsws_localhistory_clear(w);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	return 0;
}

void nsws_localhistory_up(struct gui_window *w)
{
	LOG(("gui window %p", w));
	HDC hdc = GetDC(NULL);
	struct browser_window *bw = gui_window_browser_window(w);
	
	localhistory.vscroll = 0;
	localhistory.hscroll = 0;
	
	if (bw != NULL)
		history_redraw(bw->history);
	
	nsws_localhistory_scroll_check(w);

	ReleaseDC(localhistory.hwnd, hdc);
}

void nsws_localhistory_scroll_check(struct gui_window *w)
{
	if (w == NULL)
		return;
	struct browser_window *bw = gui_window_browser_window(w);
	if ((bw == NULL) || (localhistory.hwnd == NULL))
		return;
	history_size(bw->history, &(localhistory.width), &(localhistory.height));
	
	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = localhistory.height;
	si.nPage = localhistory.guiheight;
	si.nPos = 0;
	SetScrollInfo(localhistory.hwnd, SB_VERT, &si, TRUE);
	
	si.nMax = localhistory.width;
	si.nPage = localhistory.guiwidth;
	SetScrollInfo(localhistory.hwnd, SB_HORZ, &si, TRUE);
	if (localhistory.guiheight >= localhistory.height)
		localhistory.vscroll = 0;
	if (localhistory.guiwidth >= localhistory.width)
		localhistory.hscroll = 0;
	SendMessage(localhistory.hwnd, WM_PAINT, 0, 0);
}

/*
void history_gui_set_pointer(gui_pointer_shape shape, void *p)
{
	struct nsws_pointers *pointers = nsws_get_pointers();
	if (pointers == NULL)
		return;
	switch(shape) {
	case GUI_POINTER_POINT:
		SetCursor(pointers->hand);
		break;
	default:
		SetCursor(pointers->arrow);
		break;
	}
}
*/

void nsws_localhistory_close(struct gui_window *w)
{
	struct nsws_localhistory *l = gui_window_localhistory(w);
	if (l != NULL)
		DestroyWindow(l->hwnd);
}

void nsws_localhistory_clear(struct gui_window *w)
{
	if (w != NULL)
		gui_window_set_localhistory(w, NULL);
}


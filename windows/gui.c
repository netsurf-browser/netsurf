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
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include <windows.h>
#include <windowsx.h>
#define _WIN32_IE (0x0501)
#include <commctrl.h>

#include <hubbub/hubbub.h>

#include "content/urldb.h"
#include "content/fetch.h"
#include "css/utils.h"
#include "desktop/gui.h"
#include "desktop/history_core.h"
#include "desktop/plotters.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "desktop/save_complete.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "render/html.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#include "windows/about.h"
#include "windows/gui.h"
#include "windows/findfile.h"
#include "windows/font.h"
#include "windows/localhistory.h"
#include "windows/plot.h"
#include "windows/prefs.h"
#include "windows/resourceid.h"

char *default_stylesheet_url;
char *adblock_stylesheet_url;
char *quirks_stylesheet_url;
char *options_file_location;

struct gui_window *input_window = NULL;
struct gui_window *search_current_window;
struct gui_window *window_list = NULL;

FARPROC urlproc;
WNDPROC	toolproc;

static char default_page[] = "http://www.netsurf-browser.org/welcome/";
static HICON hIcon, hIconS;
static int open_windows = 0;

static const char windowclassname_main[] = "nswsmainwindow";
static const char windowclassname_drawable[] = "nswsdrawablewindow";

#define NSWS_THROBBER_WIDTH 24
#define NSWS_URL_ENTER (WM_USER)

struct gui_window {
	/* The front's private data connected to a browser window */
	/* currently 1<->1 gui_window<->windows window [non-tabbed] */
	struct browser_window *bw; /** the browser_window */
	HWND main; /**< handle to the actual window */
	HWND toolbar; /**< toolbar handle */
	HWND urlbar; /**< url bar handle */
	HWND throbber; /** throbber handle */
	HWND drawingarea; /**< drawing area handle */
	HWND statusbar; /**< status bar handle */
	HWND vscroll; /**< vertical scrollbar handle */
	HWND hscroll; /**< horizontal scrollbar handle */
	HMENU mainmenu; /**< the main menu */
	HMENU rclick; /**< the right-click menu */
	HDC bufferdc; /**< the screen buffer */
	HBITMAP bufferbm; /**< the buffer bitmap */
	struct nsws_localhistory *localhistory;	/**< handle to local history window */
	int width; /**< width of window */
	int height; /**< height of drawing area */

	int toolbuttonc; /**< number of toolbar buttons */
	int toolbuttonsize; /**< width, height of buttons */
	bool throbbing; /**< whether currently throbbing */

	struct browser_mouse *mouse; /**< mouse state */

	HACCEL acceltable; /**< accelerators */

	float scale; /**< scale of content */

	int scrollx; /**< current scroll location */
	int scrolly; /**< current scroll location */

	RECT *fullscreen; /**< memorize non-fullscreen area */
	RECT redraw; /**< Area needing redraw. */
	RECT clip; /**< current clip rectangle */
	int requestscrollx, requestscrolly; /**< scolling requested. */
	struct gui_window *next, *prev; /**< global linked list */
};

static struct nsws_pointers nsws_pointer;

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

typedef enum {
	NSWS_ID_TOOLBAR = 1111,
	NSWS_ID_URLBAR,
	NSWS_ID_THROBBER,
	NSWS_ID_DRAWINGAREA,
	NSWS_ID_STATUSBAR,
	NSWS_ID_LAUNCH_URL,
} nsws_constants ;

LRESULT CALLBACK nsws_window_url_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK nsws_window_toolbar_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK nsws_window_event_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK nsws_window_drawable_event_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

HINSTANCE hinstance;

void gui_multitask(void)
{
/*	LOG(("gui_multitask")); */
}

/**
 * called synchronously to handle all redraw events
 */
static void redraw(void)
{
	struct gui_window *w = window_list;
	struct browser_window *bw;
	struct content *c;
	HDC hdc;

	while (w != NULL) {
		if ((w->redraw.right - w->redraw.left <= 0) ||
		    (w->redraw.bottom - w->redraw.top <= 0)) {
			w = w->next;
			continue;
		}

		bw = w->bw;
		if (bw == NULL) {
			w = w->next;
			continue;
		}

		c = bw->current_content;

		if ((c == NULL) || (c->locked)) {
			w = w->next;
			continue;
		}

		current_hwnd = w->drawingarea;
		w->scrolly += w->requestscrolly;
		w->scrollx += w->requestscrollx;
		w->scrolly = MAX(w->scrolly, 0);
		w->scrolly = MIN(w->scrolly, c->height * w->bw->scale - w->height);
		w->scrollx = MAX(w->scrollx, 0);
		w->scrollx = MIN(w->scrollx, c->width * w->bw->scale - w->width);
		/* redraw */
		current_redraw_browser = bw;
		nsws_plot_set_scale(bw->scale);

		hdc = GetDC(w->main);
		if (w->bufferbm == NULL) {
			w->bufferbm = CreateCompatibleBitmap(hdc, w->width,
							     w->height );
			SelectObject(w->bufferdc, w->bufferbm);
		}


		if ((w->bufferbm == NULL) || (w->bufferdc == NULL) ||
		    (hdc == NULL))
			doublebuffering = false;
		if (doublebuffering)
			bufferdc = w->bufferdc;
		content_redraw(c, -w->scrollx / w->bw->scale,
			       -w->scrolly / w->bw->scale,
			       w->width, w->height,
			       w->redraw.left - w->scrollx / w->bw->scale,
			       w->redraw.top - w->scrolly / w->bw->scale,
			       w->redraw.right - w->scrollx / w->bw->scale,
			       w->redraw.bottom - w->scrolly / w->bw->scale,
			       bw->scale, 0xFFFFFF);
		if (doublebuffering)
			/* blit buffer to screen */
			BitBlt(hdc, 0, 0, w->width, w->height,
			       w->bufferdc, 0, 0,
			       SRCCOPY);
		ReleaseDC(w->main, hdc);
		doublebuffering = false;

		w->requestscrolly = 0;
		w->requestscrollx = 0;
		w->redraw.left = w->redraw.top = INT_MAX;
		w->redraw.right = w->redraw.bottom = -(INT_MAX);
		w = w->next;
	}
}

void gui_poll(bool active)
{
	MSG Msg;
	if (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE) != 0) {
		if (!((current_gui == NULL) ||
		      (TranslateAccelerator(current_gui->main,
					    current_gui->acceltable, &Msg))))
			TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	schedule_run();

}



/**
 * callback for url bar events
 */
LRESULT CALLBACK 
nsws_window_url_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	DWORD i, ii;
	SendMessage(hwnd, EM_GETSEL, (WPARAM)&i, (LPARAM)&ii);
	int x,y;
	x = GET_X_LPARAM(lparam);
	y = GET_Y_LPARAM(lparam);

	if (msg == WM_PAINT) {
		SendMessage(hwnd, EM_SETSEL, (WPARAM)0, (LPARAM)-1);
		SendMessage(hwnd, EM_SETSEL, (WPARAM)i, (LPARAM)ii);
	}
	return CallWindowProc((WNDPROC) urlproc, hwnd, msg, wparam, lparam);
}

/* calculate the dimensions of the url bar relative to the parent toolbar */
static void
urlbar_dimensions(HWND hWndParent, int toolbuttonsize, int buttonc, int *x, int *y, int *width, int *height)
{
	RECT rc;
	const int cy_edit = 24;

	GetClientRect(hWndParent, &rc);
	*x = (toolbuttonsize + 2) * (buttonc + 1) + (NSWS_THROBBER_WIDTH>>1);
	*y = (((rc.bottom - rc.top) + 1) - cy_edit) >> 1;
	*width = ((rc.right - rc.left) + 1) - *x - (NSWS_THROBBER_WIDTH>>1) - NSWS_THROBBER_WIDTH;
	*height = cy_edit;
}

/* obtain gui window structure from windows window handle */
static struct gui_window *
nsws_get_gui_window(HWND hwnd)
{
	struct gui_window *gw;
	HWND phwnd;

	gw = GetProp(hwnd, TEXT("GuiWnd"));

	if (gw == NULL) {
		/* try the parent window instead */
		phwnd = GetParent(hwnd);
		gw = GetProp(phwnd, TEXT("GuiWnd"));
	}

	if (gw == NULL) {
		/* unable to fetch from property, try seraching the
		 * gui window list
		 */
		gw = window_list;
		while (gw != NULL) {
			if ((gw->main == hwnd) || (gw->toolbar == hwnd)) {
				break;
			}
			gw = gw->next;
		}
	}

	return gw;
}

/**
 * callback for toolbar events
 */
LRESULT CALLBACK
nsws_window_toolbar_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct gui_window *gw;
	int urlx, urly, urlwidth, urlheight;

	switch (msg) {
	case WM_SIZE:
		gw = nsws_get_gui_window(hwnd);
		urlbar_dimensions(hwnd, gw->toolbuttonsize, gw->toolbuttonc, &urlx, &urly, &urlwidth, &urlheight);
		/* resize url */
		if (gw->urlbar != NULL) {
			MoveWindow(gw->urlbar, urlx, urly, urlwidth, urlheight, true);
		}
		/* move throbber */
		if (gw->throbber != NULL) {
			MoveWindow(gw->throbber,
				   LOWORD(lparam) - NSWS_THROBBER_WIDTH - 4, 8,
				   NSWS_THROBBER_WIDTH, NSWS_THROBBER_WIDTH,
				   true);
		}

		break;
	}

	/* chain to the next handler */
	return CallWindowProc(toolproc, hwnd, msg, wparam, lparam);
}

/**
 * update state of forward/back buttons/menu items when page changes
 */
static void nsws_window_update_forward_back(struct gui_window *w)
{
	if (w->bw == NULL)
		return;
	bool forward = history_forward_available(w->bw->history);
	bool back = history_back_available(w->bw->history);
	if (w->mainmenu != NULL) {
		EnableMenuItem(w->mainmenu, NSWS_ID_NAV_FORWARD,
			       (forward ? MF_ENABLED : MF_GRAYED));
		EnableMenuItem(w->mainmenu, NSWS_ID_NAV_BACK,
			       (back ? MF_ENABLED : MF_GRAYED));
		EnableMenuItem(w->rclick, NSWS_ID_NAV_FORWARD,
			       (forward ? MF_ENABLED : MF_GRAYED));
		EnableMenuItem(w->rclick, NSWS_ID_NAV_BACK,
			       (back ? MF_ENABLED : MF_GRAYED));
	}
	if (w->toolbar != NULL) {
		SendMessage(w->toolbar, TB_SETSTATE,
			    (WPARAM) NSWS_ID_NAV_FORWARD,
			    MAKELONG((forward ? TBSTATE_ENABLED :
				      TBSTATE_INDETERMINATE), 0));
		SendMessage(w->toolbar, TB_SETSTATE,
			    (WPARAM) NSWS_ID_NAV_BACK,
			    MAKELONG((back ? TBSTATE_ENABLED :
				      TBSTATE_INDETERMINATE), 0));
	}
}

static void nsws_update_edit(struct gui_window *w)
{
	bool paste, copy, del;
	if (GetFocus() == w->urlbar) {
		DWORD i, ii;
		SendMessage(w->urlbar, EM_GETSEL, (WPARAM)&i,
			    (LPARAM)&ii);
		paste = true;
		copy = (i != ii);
		del = (i != ii);

	} else if ((w->bw != NULL) && (w->bw->sel != NULL)){
		paste = (w->bw->paste_callback != NULL);
		copy = w->bw->sel->defined;
		del = ((w->bw->sel->defined) &&
		       (w->bw->caret_callback != NULL));
	} else {
		paste = false;
		copy = false;
		del = false;
	}
	EnableMenuItem(w->mainmenu,
		       NSWS_ID_EDIT_PASTE,
		       (paste ? MF_ENABLED : MF_GRAYED));

	EnableMenuItem(w->rclick,
		       NSWS_ID_EDIT_PASTE,
		       (paste ? MF_ENABLED : MF_GRAYED));


	EnableMenuItem(w->mainmenu,
		       NSWS_ID_EDIT_COPY,
		       (copy ? MF_ENABLED : MF_GRAYED));


	EnableMenuItem(w->rclick,
		       NSWS_ID_EDIT_COPY,
		       (copy ? MF_ENABLED : MF_GRAYED));


	if (del == true) {
		EnableMenuItem(w->mainmenu, NSWS_ID_EDIT_CUT, MF_ENABLED);
		EnableMenuItem(w->mainmenu, NSWS_ID_EDIT_DELETE, MF_ENABLED);
		EnableMenuItem(w->rclick, NSWS_ID_EDIT_CUT, MF_ENABLED);
		EnableMenuItem(w->rclick, NSWS_ID_EDIT_DELETE, MF_ENABLED);
	} else {
		EnableMenuItem(w->mainmenu, NSWS_ID_EDIT_CUT, MF_GRAYED);
		EnableMenuItem(w->mainmenu, NSWS_ID_EDIT_DELETE, MF_GRAYED);
		EnableMenuItem(w->rclick, NSWS_ID_EDIT_CUT, MF_GRAYED);
		EnableMenuItem(w->rclick, NSWS_ID_EDIT_DELETE, MF_GRAYED);
	}
}

static bool
nsws_ctx_menu(struct gui_window *w, HWND hwnd, int x, int y)
{
	RECT rc; /* client area of window */
	POINT pt = { x, y }; /* location of mouse click */

	/* Get the bounding rectangle of the client area. */
	GetClientRect(hwnd, &rc);

	/* Convert the mouse position to client coordinates. */
	ScreenToClient(hwnd, &pt);

	/* If the position is in the client area, display a shortcut menu. */
	if (PtInRect(&rc, pt)) {
		ClientToScreen(hwnd, &pt);
		nsws_update_edit(w);
		TrackPopupMenu(GetSubMenu(w->rclick, 0),
			       TPM_CENTERALIGN | TPM_TOPALIGN,
			       x,
			       y,
			       0,
			       hwnd,
			       NULL);

		return true;
	}

	/* Return false if no menu is displayed. */
	return false;
}

/**
 * set accelerators
 */
static void nsws_window_set_accels(struct gui_window *w)
{
	int i, nitems = 13;
	ACCEL accels[nitems];
	for (i = 0; i < nitems; i++)
		accels[i].fVirt = FCONTROL | FVIRTKEY;
	accels[0].key = 0x51; /* Q */
	accels[0].cmd = NSWS_ID_FILE_QUIT;
	accels[1].key = 0x4E; /* N */
	accels[1].cmd = NSWS_ID_FILE_OPEN_WINDOW;
	accels[2].key = VK_LEFT;
	accels[2].cmd = NSWS_ID_NAV_BACK;
	accels[3].key = VK_RIGHT;
	accels[3].cmd = NSWS_ID_NAV_FORWARD;
	accels[4].key = VK_UP;
	accels[4].cmd = NSWS_ID_NAV_HOME;
	accels[5].key = VK_BACK;
	accels[5].cmd = NSWS_ID_NAV_STOP;
	accels[6].key = VK_SPACE;
	accels[6].cmd = NSWS_ID_NAV_RELOAD;
	accels[7].key = 0x4C; /* L */
	accels[7].cmd = NSWS_ID_FILE_OPEN_LOCATION;
	accels[8].key = 0x57; /* w */
	accels[8].cmd = NSWS_ID_FILE_CLOSE_WINDOW;
	accels[9].key = 0x41; /* A */
	accels[9].cmd = NSWS_ID_EDIT_SELECT_ALL;
	accels[10].key = VK_F8;
	accels[10].cmd = NSWS_ID_VIEW_SOURCE;
	accels[11].key = VK_RETURN;
	accels[11].fVirt = FVIRTKEY;
	accels[11].cmd = NSWS_ID_LAUNCH_URL;
	accels[12].key = VK_F11;
	accels[12].fVirt = FVIRTKEY;
	accels[12].cmd = NSWS_ID_VIEW_FULLSCREEN;

	w->acceltable = CreateAcceleratorTable(accels, nitems);
}

/**
 * set window icons
 */
static void nsws_window_set_ico(struct gui_window *w)
{
	char ico[PATH_MAX];
	nsws_find_resource(ico, "NetSurf32.ico", "windows/res/NetSurf32.ico");
	LOG(("setting ico as %s", ico));
	hIcon = LoadImage(NULL, ico, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
	if (hIcon != NULL)
		SendMessage(w->main, WM_SETICON, ICON_BIG, (LPARAM) hIcon);
	nsws_find_resource(ico, "NetSurf16.ico", "windows/res/NetSurf16.ico");
	LOG(("setting ico as %s", ico));
	hIconS = LoadImage(NULL, ico, IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
	if (hIconS != NULL)
		SendMessage(w->main, WM_SETICON, ICON_SMALL, (LPARAM)hIconS);
}


/**
 * creation of throbber
 */
static void nsws_window_throbber_create(struct gui_window *w)
{
	HWND hwnd;
	char avi[PATH_MAX];

	hwnd = CreateWindow(ANIMATE_CLASS,
			    "",
			    WS_CHILD | WS_VISIBLE | ACS_TRANSPARENT,
			    w->width - NSWS_THROBBER_WIDTH - 4,
			    8,
			    NSWS_THROBBER_WIDTH,
			    NSWS_THROBBER_WIDTH,
			    w->main,
			    (HMENU) NSWS_ID_THROBBER,
			    hinstance,
			    NULL);

	nsws_find_resource(avi, "throbber.avi", "windows/res/throbber.avi");
	LOG(("setting throbber avi as %s", avi));
	Animate_Open(hwnd, avi);
	if (w->throbbing)
		Animate_Play(hwnd, 0, -1, -1);
	else
		Animate_Seek(hwnd, 0);
	ShowWindow(hwnd, SW_SHOWNORMAL);
	w->throbber = hwnd;
}

static HIMAGELIST 
nsws_set_imagelist(HWND hwnd, UINT msg, int resid, int bsize, int bcnt)
{
	HIMAGELIST hImageList;
	HBITMAP hScrBM;

	hImageList = ImageList_Create(bsize, bsize, ILC_COLOR24 |ILC_MASK, 0, bcnt);
	hScrBM = LoadImage(hinstance, MAKEINTRESOURCE(resid),
			   IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	ImageList_AddMasked(hImageList, hScrBM, 0xcccccc);
	DeleteObject(hScrBM);

	SendMessage(hwnd, msg, (WPARAM)0, (LPARAM)hImageList);
	return hImageList;
}

static HWND
nsws_window_toolbar_create(struct gui_window *gw, HWND hWndParent)
{
	HWND hWndToolbar;
	/* Toolbar buttons */
	TBBUTTON tbButtons[] = {
		{0, NSWS_ID_NAV_BACK, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
		{1, NSWS_ID_NAV_FORWARD, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
		{2, NSWS_ID_NAV_HOME, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
		{3, NSWS_ID_NAV_RELOAD, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
		{4, NSWS_ID_NAV_STOP, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
	};

	/* Create the toolbar child window. */
	hWndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, "Toolbar",
				     WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT,
				     0, 0, 0, 0,
				     hWndParent, NULL, HINST_COMMCTRL, NULL);

	if (!hWndToolbar) {
		return NULL;
	}

	/* remember how many buttons are being created */
	gw->toolbuttonc = sizeof(tbButtons) / sizeof(TBBUTTON);

	/* Create the standard image list and assign to toolbar. */
	nsws_set_imagelist(hWndToolbar, TB_SETIMAGELIST, NSWS_ID_TOOLBAR_BITMAP, gw->toolbuttonsize, gw->toolbuttonc);

	/* Create the disabled image list and assign to toolbar. */
	nsws_set_imagelist(hWndToolbar, TB_SETDISABLEDIMAGELIST, NSWS_ID_TOOLBAR_GREY_BITMAP, gw->toolbuttonsize, gw->toolbuttonc);

	/* Create the hot image list and assign to toolbar. */
	nsws_set_imagelist(hWndToolbar, TB_SETHOTIMAGELIST, NSWS_ID_TOOLBAR_HOT_BITMAP, gw->toolbuttonsize, gw->toolbuttonc);

	/* Add buttons. */
	SendMessage(hWndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
	SendMessage(hWndToolbar, TB_ADDBUTTONS, (WPARAM)gw->toolbuttonc, (LPARAM)&tbButtons);


	int urlx, urly, urlwidth, urlheight;
	urlbar_dimensions(hWndToolbar, gw->toolbuttonsize, gw->toolbuttonc, &urlx, &urly, &urlwidth, &urlheight);

	// Create the edit control child window.
	gw->urlbar = CreateWindowEx(0L, "Edit", NULL,
				    WS_CHILD | WS_BORDER | WS_VISIBLE | ES_LEFT
				    | ES_AUTOVSCROLL | ES_MULTILINE,
				    urlx,
				    urly,
				    urlwidth,
				    urlheight,
				    hWndToolbar,
				    (HMENU)NSWS_ID_URLBAR,
				    hinstance, 0 );

	if (!gw->urlbar) {
		DestroyWindow(hWndToolbar);
		return NULL;
	}

	nsws_window_throbber_create(gw);

	/* set the gui window associated with this toolbar */
	SetProp(hWndToolbar, TEXT("GuiWnd"), (HANDLE)gw);

	/* subclass the message handler */
	toolproc = (WNDPROC)SetWindowLongPtr(hWndToolbar, GWLP_WNDPROC, (LONG_PTR)nsws_window_toolbar_callback);

	/* Return the completed toolbar */
	return hWndToolbar;
}

/**
 * creation of status bar
 */
static void nsws_window_statusbar_create(struct gui_window *w)
{
	HWND hwnd = CreateWindowEx(0, STATUSCLASSNAME, NULL, WS_CHILD |
				   WS_VISIBLE, 0, 0, 0, 0, w->main,
				   (HMENU) NSWS_ID_STATUSBAR, hinstance, NULL);
	SendMessage(hwnd, SB_SETTEXT, 0, (LPARAM)"NetSurf");
	w->statusbar = hwnd;
}

static void nsws_window_drawingarea_create(struct gui_window *w)
{
	RECT rtoolbar;
	RECT rstatusbar;

	GetClientRect(w->toolbar, &rtoolbar);
	GetClientRect(w->statusbar, &rstatusbar);

	w->drawingarea = CreateWindow(windowclassname_drawable,
				      NULL,
				      WS_VISIBLE|WS_CHILD,
				      0,
				      rtoolbar.bottom + 1,
				      w->width,
				      rstatusbar.top - rtoolbar.bottom,
				      w->main,
				      NULL,
				      hinstance,
				      NULL);
	if (w->drawingarea == NULL)
		die("arse");
}

/**
 * creation of vertical scrollbar
 */
static void nsws_window_vscroll_create(struct gui_window *w)
{
	w->vscroll = CreateWindow("SCROLLBAR", NULL, WS_CHILD | SBS_VERT,
				  0, 0, CW_USEDEFAULT, 300, w->main, NULL, hinstance,
				  NULL);
}

/**
 * creation of horizontal scrollbar
 */
static void nsws_window_hscroll_create(struct gui_window *w)
{
	w->hscroll = CreateWindow("SCROLLBAR", NULL, WS_CHILD | SBS_HORZ,
				  0, 0, 200, CW_USEDEFAULT, w->main, NULL, hinstance,
				  NULL);
}

static LRESULT nsws_drawable_mousemove(struct gui_window *gw, int x, int y)
{
	bool shift = ((GetKeyState(VK_SHIFT) & 0x8000) == 0x8000);
	bool ctrl = ((GetKeyState(VK_CONTROL) & 0x8000) == 0x8000);
	bool alt = ((GetKeyState(VK_MENU) & 0x8000) == 0x8000);

	if ((gw == NULL) ||
	    (gw->mouse == NULL) ||
	    (gw->bw == NULL))
		return 0;

	if ((gw->mouse->state & BROWSER_MOUSE_PRESS_1) != 0) {
		browser_window_mouse_click(gw->bw, BROWSER_MOUSE_DRAG_1,
					   gw->mouse->pressed_x,
					   gw->mouse->pressed_y);
		gw->mouse->state &= ~BROWSER_MOUSE_PRESS_1;
		gw->mouse->state |= BROWSER_MOUSE_HOLDING_1 |
			BROWSER_MOUSE_DRAG_ON;
	}
	else if ((gw->mouse->state & BROWSER_MOUSE_PRESS_2) != 0) {
		browser_window_mouse_click(gw->bw, BROWSER_MOUSE_DRAG_2,
					   gw->mouse->pressed_x,
					   gw->mouse->pressed_y);
		gw->mouse->state &= ~BROWSER_MOUSE_PRESS_2;
		gw->mouse->state |= BROWSER_MOUSE_HOLDING_2 |
			BROWSER_MOUSE_DRAG_ON;
	}
	if (((gw->mouse->state & BROWSER_MOUSE_MOD_1) != 0) && !shift)
		gw->mouse->state &= ~BROWSER_MOUSE_MOD_1;
	if (((gw->mouse->state & BROWSER_MOUSE_MOD_2) != 0) && !ctrl)
		gw->mouse->state &= ~BROWSER_MOUSE_MOD_2;
	if (((gw->mouse->state & BROWSER_MOUSE_MOD_3) != 0) && !alt)
		gw->mouse->state &= ~BROWSER_MOUSE_MOD_3;

	browser_window_mouse_track(gw->bw, gw->mouse->state,
				   (x + gw->scrollx) / gw->bw->scale,
				   (y + gw->scrolly) / gw->bw->scale);

	return 0;
}

static LRESULT nsws_drawable_mousedown(struct gui_window *gw, int x, int y, browser_mouse_state button)
{
	if ((gw == NULL) ||
	    (gw->mouse == NULL) ||
	    (gw->bw == NULL)) {
		nsws_localhistory_close(gw);
		return 0;
	}

	gw->mouse->state = button;
	if ((GetKeyState(VK_SHIFT) & 0x8000) == 0x8000)
		gw->mouse->state |= BROWSER_MOUSE_MOD_1;
	if ((GetKeyState(VK_CONTROL) & 0x8000) == 0x8000)
		gw->mouse->state |= BROWSER_MOUSE_MOD_2;
	if ((GetKeyState(VK_MENU) & 0x8000) == 0x8000)
		gw->mouse->state |= BROWSER_MOUSE_MOD_3;

	gw->mouse->pressed_x = (x + gw->scrollx) / gw->bw->scale;
	gw->mouse->pressed_y = (y + gw->scrolly) / gw->bw->scale;

	browser_window_mouse_click(gw->bw, gw->mouse->state,
				   (x + gw->scrollx) / gw->bw->scale ,
				   (y + gw->scrolly) / gw->bw->scale);

	return 0;
}

static LRESULT
nsws_drawable_mouseup(struct gui_window *gw,
		      int x,
		      int y,
		      browser_mouse_state press,
		      browser_mouse_state click)
{
	bool shift = ((GetKeyState(VK_SHIFT) & 0x8000) == 0x8000);
	bool ctrl = ((GetKeyState(VK_CONTROL) & 0x8000) == 0x8000);
	bool alt = ((GetKeyState(VK_MENU) & 0x8000) == 0x8000);

	if ((gw == NULL) ||
	    (gw->mouse == NULL) ||
	    (gw->bw == NULL))
		return 0;


	if ((gw->mouse->state & press) != 0) {
		gw->mouse->state &= ~press;
		gw->mouse->state |= click;
	}

	if (((gw->mouse->state & BROWSER_MOUSE_MOD_1) != 0) && !shift)
		gw->mouse->state &= ~BROWSER_MOUSE_MOD_1;
	if (((gw->mouse->state & BROWSER_MOUSE_MOD_2) != 0) && !ctrl)
		gw->mouse->state &= ~BROWSER_MOUSE_MOD_2;
	if (((gw->mouse->state & BROWSER_MOUSE_MOD_3) != 0) && !alt)
		gw->mouse->state &= ~BROWSER_MOUSE_MOD_3;

	if ((gw->mouse->state & click) != 0)
		browser_window_mouse_click(gw->bw,
					   gw->mouse->state,
					   (x + gw->scrollx) / gw->bw->scale,
					   (y + gw->scrolly) / gw->bw->scale);
	else
		browser_window_mouse_drag_end(gw->bw,
					      0,
					      (x + gw->scrollx) / gw->bw->scale,
					      (y + gw->scrolly) / gw->bw->scale);

	gw->mouse->state = 0;
	return 0;
}

static void nsws_drawable_paint(struct gui_window *gw, HWND hwnd)
{
	PAINTSTRUCT ps;

	BeginPaint(hwnd, &ps);
	gw->redraw.left = ps.rcPaint.left;
	gw->redraw.top = ps.rcPaint.top;
	gw->redraw.right = ps.rcPaint.right;
	gw->redraw.bottom = ps.rcPaint.bottom;

	/* set globals for the plotters */
	current_hwnd = gw->drawingarea;
	current_gui = gw;

	redraw();
	EndPaint(hwnd, &ps);

	plot.clip(0, 0, gw->width, gw->height); /* vrs - very suspect */
}

static void nsws_drawable_key(struct gui_window *gw, HWND hwnd, WPARAM wparam)
{
	if (GetFocus() != hwnd)
		return;

	uint32_t i;
	bool shift = ((GetKeyState(VK_SHIFT) & 0x8000) == 0x8000);
	bool capslock = ((GetKeyState(VK_CAPITAL) & 1) == 1);

	switch(wparam) {
	case VK_LEFT:
		i = KEY_LEFT;
		if (shift)
			SendMessage(hwnd, WM_HSCROLL,
				    MAKELONG(SB_LINELEFT, 0), 0);
		break;

	case VK_RIGHT:
		i = KEY_RIGHT;
		if (shift)
			SendMessage(hwnd, WM_HSCROLL,
				    MAKELONG(SB_LINERIGHT, 0), 0);
		break;

	case VK_UP:
		i = KEY_UP;
		if (shift)
			SendMessage(hwnd, WM_VSCROLL,
				    MAKELONG(SB_LINEUP, 0), 0);
		break;

	case VK_DOWN:
		i = KEY_DOWN;
		if (shift)
			SendMessage(hwnd, WM_VSCROLL,
				    MAKELONG(SB_LINEDOWN, 0), 0);
		break;

	case VK_HOME:
		i = KEY_LINE_START;
		if (shift)
			SendMessage(hwnd, WM_HSCROLL,
				    MAKELONG(SB_PAGELEFT, 0), 0);
		break;

	case VK_END:
		i = KEY_LINE_END;
		if (shift)
			SendMessage(hwnd, WM_HSCROLL,
				    MAKELONG(SB_PAGERIGHT, 0), 0);
		break;

	case VK_DELETE:
		i = KEY_DELETE_RIGHT;
		break;

	case VK_NEXT:
		i = wparam;
		SendMessage(hwnd, WM_VSCROLL, MAKELONG(SB_PAGEDOWN, 0),
			    0);
		break;

	case VK_PRIOR:
		i = wparam;
		SendMessage(hwnd, WM_VSCROLL, MAKELONG(SB_PAGEUP, 0),
			    0);
		break;

	default:
		i = wparam;
		break;
	}

	if ((i >= 'A') && (i <= 'Z') &&
	    (((!capslock) && (!shift)) ||
	     ((capslock) && (shift))))
		i += 'a' - 'A';

	if (gw != NULL)
		browser_window_key_press(gw->bw, i);

}


/* Called when activity occours within the drawable window. */
LRESULT CALLBACK nsws_window_drawable_event_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct gui_window *gw = window_list;

	while (gw != NULL) {
		if (gw->drawingarea == hwnd) {
			break;
		}
		gw = gw->next;
	}

	if (gw == NULL)
		return DefWindowProc(hwnd, msg, wparam, lparam);

	switch(msg) {

	case WM_MOUSEMOVE:
		nsws_drawable_mousemove(gw,
					GET_X_LPARAM(lparam),
					GET_Y_LPARAM(lparam));
		break;

	case WM_LBUTTONDOWN:
		nsws_drawable_mousedown(gw,
					GET_X_LPARAM(lparam),
					GET_Y_LPARAM(lparam),
					BROWSER_MOUSE_PRESS_1);
		SetFocus(hwnd);
		nsws_localhistory_close(gw);
		break;

	case WM_RBUTTONDOWN:
		nsws_drawable_mousedown(gw,
					GET_X_LPARAM(lparam),
					GET_Y_LPARAM(lparam),
					BROWSER_MOUSE_PRESS_2);
		SetFocus(hwnd);
		break;

	case WM_LBUTTONUP:
		nsws_drawable_mouseup(gw,
				      GET_X_LPARAM(lparam),
				      GET_Y_LPARAM(lparam),
				      BROWSER_MOUSE_PRESS_1,
				      BROWSER_MOUSE_CLICK_1);
		break;

	case WM_RBUTTONUP:
		nsws_drawable_mouseup(gw,
				      GET_X_LPARAM(lparam),
				      GET_Y_LPARAM(lparam),
				      BROWSER_MOUSE_PRESS_2,
				      BROWSER_MOUSE_CLICK_2);
		break;

	case WM_PAINT:
		nsws_drawable_paint(gw, hwnd);
		break;

	case WM_KEYDOWN:
		nsws_drawable_key(gw, hwnd, wparam);
		break;
	default:
		break;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

static void
nsws_window_resize(struct gui_window *w,
		   HWND hwnd,
		   WPARAM wparam,
		   LPARAM lparam)
{
	if ((w->toolbar == NULL) ||
	    (w->urlbar == NULL) ||
	    (w->statusbar == NULL))
		return;

	int x, y;
	RECT rmain, rstatus, rtool;
	GetClientRect(hwnd, &rmain);
	GetClientRect(w->toolbar, &rtool);
	GetWindowRect(w->statusbar, &rstatus);
	gui_window_get_scroll(w, &x, &y);
	w->height = HIWORD(lparam) -
		(rtool.bottom - rtool.top) -
		(rstatus.bottom - rstatus.top);
	w->width = LOWORD(lparam);

	if (w->drawingarea != NULL) {
		MoveWindow(w->drawingarea,
			   0,
			   rtool.bottom,
			   w->width,
			   w->height,
			   true);
	}

	if (w->statusbar != NULL) {
		MoveWindow(w->statusbar,
			   0,
			   rtool.bottom + w->height,
			   w->width,
			   (rstatus.bottom - rstatus.top + 1),
			   true);
	}

	nsws_window_update_forward_back(w);

	if (w->toolbar != NULL) {
		MoveWindow(w->toolbar,
			   0,
			   0,
			   w->width,
			   (rtool.bottom - rtool.top),
			   true);
	}

	/* update double buffering context */
	HDC hdc = GetDC(hwnd);
	if (w->bufferdc == NULL)
		w->bufferdc = CreateCompatibleDC(hdc);
	if (w->bufferbm != NULL) {
		DeleteObject(w->bufferbm);
		w->bufferbm = CreateCompatibleBitmap(hdc, w->width, w->height);
		SelectObject(w->bufferdc, w->bufferbm);
	}
	ReleaseDC(hwnd, hdc);

	/* update browser window to new dimensions */
	if (w->bw != NULL) {
		browser_window_reformat(w->bw, w->width, w->height);
		redraw();
	}
	gui_window_set_scroll(w, x, y);

	if (w->toolbar != NULL)
		SendMessage(w->toolbar, TB_SETSTATE,
			    (WPARAM) NSWS_ID_NAV_STOP,
			    MAKELONG(TBSTATE_INDETERMINATE, 0));

}

/**
 * callback for window events generally
 */
LRESULT CALLBACK nsws_window_event_callback(HWND hwnd, UINT msg, WPARAM wparam,
					    LPARAM lparam)
{
	bool match = false;
	bool historyactive = false;
	struct gui_window *w = window_list;
	while (w != NULL) {
		if (w->main == hwnd) {
			match = true;
			break;
		}
		w = w->next;
	}
	if (!match) { /* during initial window creation */
		w = window_list;
		while (w != NULL) {
			if (w->main == NULL) {
				w->main = hwnd;
				break;
			}
			w = w->next;
		}
	}
	if ((match) && (current_gui == NULL)) {
		/* local history window is active */
		if ((msg == WM_LBUTTONDOWN) || (msg == WM_PAINT))
			historyactive = true;
		else if ((msg == WM_NCHITTEST) || (msg == WM_SETCURSOR))
			return DefWindowProc(hwnd, msg, wparam, lparam);
		else
			return 0;
	}
	current_gui = w;
	switch(msg) {

	case WM_LBUTTONDBLCLK: {
		int x,y;
		x = GET_X_LPARAM(lparam);
		y = GET_Y_LPARAM(lparam);
		if ((w != NULL) && (w->bw != NULL) )
			browser_window_mouse_click(w->bw,
						   BROWSER_MOUSE_DOUBLE_CLICK,
						   (x + w->scrollx) / w->bw->scale,
						   (y + w->scrolly) / w->bw->scale);
		return DefWindowProc(hwnd, msg, wparam, lparam);
		break;
	}
	case WM_NCLBUTTONDOWN: {
		int x,y;
		x = GET_X_LPARAM(lparam);
		y = GET_Y_LPARAM(lparam);
		return DefWindowProc(hwnd, msg, wparam, lparam);
		break;
	}
	case WM_ENTERMENULOOP:
		nsws_update_edit(w);
		return DefWindowProc(hwnd, msg, wparam, lparam);

	case WM_CONTEXTMENU:
		if (!nsws_ctx_menu(w, hwnd, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
			return DefWindowProc(hwnd, msg, wparam, lparam);


		break;

	case WM_COMMAND:
	{
		switch(LOWORD(wparam)) {
		case NSWS_ID_FILE_QUIT:
			w = window_list;
			while (w != NULL) {
				PostMessage(w->main, WM_CLOSE, 0, 0);
				w = w->next;
			}
			netsurf_quit = true;
			break;
		case NSWS_ID_FILE_OPEN_LOCATION:
			SetFocus(w->urlbar);
			break;

		case NSWS_ID_FILE_OPEN_WINDOW:
			browser_window_create(NULL, w->bw, NULL, false, false);
			break;

		case NSWS_ID_FILE_CLOSE_WINDOW:
			PostMessage(hwnd, WM_CLOSE, 0, 0);
			break;
		case NSWS_ID_FILE_SAVE_PAGE:
			break;
		case NSWS_ID_FILE_SAVEAS_TEXT:
			break;
		case NSWS_ID_FILE_SAVEAS_PDF:
			break;
		case NSWS_ID_FILE_SAVEAS_DRAWFILE:
			break;
		case NSWS_ID_FILE_SAVEAS_POSTSCRIPT:
			break;
		case NSWS_ID_FILE_PRINT_PREVIEW:
			break;
		case NSWS_ID_FILE_PRINT:
			break;
		case NSWS_ID_EDIT_CUT:
			OpenClipboard(hwnd);
			EmptyClipboard();
			CloseClipboard();
			if (GetFocus() == w->urlbar)
				SendMessage(w->urlbar, WM_CUT, 0, 0);
			else if (w->bw != NULL)
				browser_window_key_press(w->bw,
							 KEY_CUT_SELECTION);
			break;
		case NSWS_ID_EDIT_COPY:
			OpenClipboard(hwnd);
			EmptyClipboard();
			CloseClipboard();
			if (GetFocus() == w->urlbar)
				SendMessage(w->urlbar, WM_COPY, 0, 0);
			else if (w->bw != NULL)
				gui_copy_to_clipboard(w->bw->sel);
			break;
		case NSWS_ID_EDIT_PASTE: {
			OpenClipboard(hwnd);
			HANDLE h = GetClipboardData(CF_TEXT);
			if (h != NULL) {
				char *content = GlobalLock(h);
				LOG(("pasting %s\n", content));
				GlobalUnlock(h);
			}
			CloseClipboard();
			if (GetFocus() == w->urlbar)
				SendMessage(w->urlbar, WM_PASTE, 0, 0);
			else
				gui_paste_from_clipboard(w, 0, 0);
			break;
		}
		case NSWS_ID_EDIT_DELETE:
			if (GetFocus() == w->urlbar)
				SendMessage(w->urlbar, WM_CUT, 0, 0);
			else
				browser_window_key_press(w->bw,
							 KEY_DELETE_RIGHT);
			break;
		case NSWS_ID_EDIT_SELECT_ALL:
			if (GetFocus() == w->urlbar)
				SendMessage(w->urlbar, EM_SETSEL, 0, -1);
			else
				selection_select_all(w->bw->sel);
			break;
		case NSWS_ID_EDIT_SEARCH:
			break;
		case NSWS_ID_EDIT_PREFERENCES:
			nsws_prefs_dialog_init(w->main);
			break;
		case NSWS_ID_NAV_BACK:
			if ((w->bw != NULL) && (history_back_available(
							w->bw->history))) {
				history_back(w->bw, w->bw->history);
			}
			nsws_window_update_forward_back(w);
			break;
		case NSWS_ID_NAV_FORWARD:
			if ((w->bw != NULL) && (history_forward_available(
							w->bw->history))) {
				history_forward(w->bw, w->bw->history);
			}
			nsws_window_update_forward_back(w);
			break;
		case NSWS_ID_NAV_HOME:
			browser_window_go(w->bw, default_page, 0, true);
			break;
		case NSWS_ID_NAV_STOP:
			browser_window_stop(w->bw);
			break;
		case NSWS_ID_NAV_RELOAD:
			browser_window_reload(w->bw, true);
			break;
		case NSWS_ID_NAV_LOCALHISTORY:
			nsws_localhistory_init(w);
			break;
		case NSWS_ID_NAV_GLOBALHISTORY:
			break;
		case NSWS_ID_VIEW_ZOOMPLUS: {
			int x, y;
			gui_window_get_scroll(w, &x, &y);
			if (w->bw != NULL) {
				browser_window_set_scale(w->bw,
							 w->bw->scale * 1.1, true);
				browser_window_reformat(w->bw, w->width,
							w->height);
			}
			gui_window_redraw_window(w);
			gui_window_set_scroll(w, x, y);
			break;
		}
		case NSWS_ID_VIEW_ZOOMMINUS: {
			int x, y;
			gui_window_get_scroll(w, &x, &y);
			if (w->bw != NULL) {
				browser_window_set_scale(w->bw,
							 w->bw->scale * 0.9, true);
				browser_window_reformat(w->bw, w->width,
							w->height);
			}
			gui_window_redraw_window(w);
			gui_window_set_scroll(w, x, y);
			break;
		}
		case NSWS_ID_VIEW_ZOOMNORMAL: {
			int x, y;
			gui_window_get_scroll(w, &x, &y);
			if (w->bw != NULL) {
				browser_window_set_scale(w->bw,
							 1.0, true);
				browser_window_reformat(w->bw, w->width,
							w->height);
			}
			gui_window_redraw_window(w);
			gui_window_set_scroll(w, x, y);
			break;
		}
		case NSWS_ID_VIEW_SOURCE:
			break;
		case NSWS_ID_VIEW_SAVE_WIN_METRICS: {
			RECT r;
			GetWindowRect(hwnd, &r);
			option_window_x = r.left;
			option_window_y = r.top;
			option_window_width = r.right - r.left;
			option_window_height = r.bottom - r.top;
			options_write(options_file_location);
			break;
		}
		case NSWS_ID_VIEW_FULLSCREEN: {
			RECT rdesk;
			if (w->fullscreen == NULL) {
				HWND desktop = GetDesktopWindow();
				w->fullscreen = malloc(sizeof(RECT));
				if ((desktop == NULL) ||
				    (w->fullscreen == NULL)) {
					warn_user("NoMemory", 0);
					break;
				}
				GetWindowRect(desktop, &rdesk);
				GetWindowRect(hwnd, w->fullscreen);
				DeleteObject(desktop);
				SetWindowLong(hwnd, GWL_STYLE, 0);
				SetWindowPos(hwnd, HWND_TOPMOST, 0, 0,
					     rdesk.right - rdesk.left,
					     rdesk.bottom - rdesk.top,
					     SWP_SHOWWINDOW);
			} else {
				SetWindowLong(hwnd, GWL_STYLE,
					      WS_OVERLAPPEDWINDOW |
					      WS_HSCROLL | WS_VSCROLL |
					      WS_CLIPCHILDREN |
					      WS_CLIPSIBLINGS | CS_DBLCLKS);
				SetWindowPos(hwnd, HWND_TOPMOST,
					     w->fullscreen->left,
					     w->fullscreen->top,
					     w->fullscreen->right -
					     w->fullscreen->left,
					     w->fullscreen->bottom -
					     w->fullscreen->top,
					     SWP_SHOWWINDOW |
					     SWP_FRAMECHANGED);
				free(w->fullscreen);
				w->fullscreen = NULL;
			}
			break;
		}
		case NSWS_ID_VIEW_DOWNLOADS:
			break;
		case NSWS_ID_VIEW_TOGGLE_DEBUG_RENDERING:
			html_redraw_debug = !html_redraw_debug;
			if (w->bw != NULL) {
				browser_window_reformat(
					w->bw, w->width, w->height);
				redraw();
			}
			break;
		case NSWS_ID_VIEW_DEBUGGING_SAVE_BOXTREE:
			break;
		case NSWS_ID_VIEW_DEBUGGING_SAVE_DOMTREE:
			break;
		case NSWS_ID_HELP_CONTENTS:
			break;
		case NSWS_ID_HELP_GUIDE:
			break;
		case NSWS_ID_HELP_INFO:
			break;
		case NSWS_ID_HELP_ABOUT:
			nsws_about_dialog_init(hinstance, hwnd);
			break;
		case NSWS_ID_LAUNCH_URL:
		{
			if (GetFocus() != w->urlbar)
				break;
			int len = SendMessage(w->urlbar, WM_GETTEXTLENGTH, 0,
					      0);
			char addr[len + 1];
			SendMessage(w->urlbar, WM_GETTEXT, (WPARAM) (len + 1),
				    (LPARAM) addr);
			LOG(("launching %s\n", addr));
			browser_window_go(w->bw, addr, 0, true);
			break;
		}
		case NSWS_ID_URLBAR:
			/* main message should already have been handled */
			break;
		default:
			break;
		}
		break;
	}
	case WM_HSCROLL:
	{
		if (w->requestscrollx != 0)
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
			si.nPos -= w->width;
			break;
		case SB_PAGERIGHT:
			si.nPos += w->width;
			break;
		case SB_THUMBTRACK:
			si.nPos = si.nTrackPos;
			break;
		default:
			break;
		}
		si.fMask = SIF_POS;
		if ((w->bw != NULL) && (w->bw->current_content != NULL))
			si.nPos = MIN(si.nPos,
				      w->bw->current_content->width *
				      w->bw->scale - w->width);
		si.nPos = MAX(si.nPos, 0);
		SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
		GetScrollInfo(hwnd, SB_HORZ, &si);
		if (si.nPos != mem)
			gui_window_set_scroll(w, w->scrollx +
					      w->requestscrollx + si.nPos - mem, w->scrolly);
		break;
	}
	case WM_VSCROLL:
	{
		if (w->requestscrolly != 0)
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
			si.nPos -= w->height;
			break;
		case SB_PAGEDOWN:
			si.nPos += w->height;
			break;
		case SB_THUMBTRACK:
			si.nPos = si.nTrackPos;
			break;
		default:
			break;
		}
		si.fMask = SIF_POS;
		if ((w->bw != NULL) && (w->bw->current_content != NULL))
			si.nPos = MIN(si.nPos,
				      w->bw->current_content->height *
				      w->bw->scale - w->height);
		si.nPos = MAX(si.nPos, 0);
		SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
		GetScrollInfo(hwnd, SB_VERT, &si);
		if (si.nPos != mem)
			gui_window_set_scroll(w, w->scrollx, w->scrolly +
					      w->requestscrolly + si.nPos - mem);
		break;
	}
	case WM_MOUSEWHEEL:
#ifdef MSH_MOUSEWHEEL
	case MSH_MOUSEWHEEL: /* w95 additional module MSWheel */
#endif
	{
		int i, z = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA,
			key = LOWORD(wparam);
		DWORD command;
		unsigned int newmessage = WM_VSCROLL;
		if (key == MK_SHIFT) {
			command = (z > 0) ? SB_LINERIGHT : SB_LINELEFT;
			newmessage = WM_HSCROLL;
		} else
			/* add MK_CONTROL -> zoom */
			command = (z > 0) ? SB_LINEUP : SB_LINEDOWN;
		z = (z < 0) ? -1 * z : z;
		for (i = 0; i < z; i++)
			SendMessage(hwnd, newmessage, MAKELONG(command, 0), 0);
		break;
	}
	case WM_CREATE:
	{
		HDC hdc = GetDC(hwnd);
		int dpi = GetDeviceCaps(hdc,LOGPIXELSY);
		if (dpi > 10)
			nscss_screen_dpi = INTTOFIX(dpi);
		ReleaseDC(hwnd, hdc);

		break;
	}

	case WM_PAINT:
	{
		DWORD ret = DefWindowProc(hwnd, msg, wparam, lparam);
		if (historyactive)
			current_gui = NULL;
		return ret;
	}

	case WM_SIZE:
		nsws_window_resize(w, hwnd, wparam, lparam);
		return DefWindowProc(hwnd, msg, wparam, lparam);

	case WM_CLOSE:
		if (--open_windows == 0) {
			netsurf_quit = true;
		}
		DestroyWindow(hwnd);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	return 0;
}


static void create_local_windows_classes(void) {
	WNDCLASSEX w;

	w.cbSize	= sizeof(WNDCLASSEX);
	w.style		= 0;
	w.lpfnWndProc	= nsws_window_event_callback;
	w.cbClsExtra	= 0;
	w.cbWndExtra	= 0;
	w.hInstance	= hinstance;
	w.hIcon		= LoadIcon(NULL, IDI_APPLICATION); /* -> NetSurf */
	w.hCursor	= LoadCursor(NULL, IDC_ARROW);
	w.hbrBackground	= (HBRUSH)(COLOR_MENU + 1);
	w.lpszMenuName	= NULL;
	w.lpszClassName = windowclassname_main;
	w.hIconSm	= LoadIcon(NULL, IDI_APPLICATION); /* -> NetSurf */
	RegisterClassEx(&w);

	w.lpfnWndProc	= nsws_window_drawable_event_callback;
	w.hIcon		= NULL;
	w.lpszMenuName	= NULL;
	w.lpszClassName = windowclassname_drawable;
	w.hIconSm	= NULL;

	RegisterClassEx(&w);

}

/**
 * creation of a new window
 */
static void nsws_window_create(struct gui_window *gw)
{
	if (gw == NULL)
		return;
	LOG(("nsws_window_create %p", gw));
	HWND hwnd;
	INITCOMMONCONTROLSEX icc;

	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_BAR_CLASSES | ICC_WIN95_CLASSES;
#if WINVER > 0x0501
	icc.dwICC |= ICC_STANDARD_CLASSES;
#endif
	InitCommonControlsEx(&icc);

	gw->mainmenu = LoadMenu(hinstance, MAKEINTRESOURCE(NSWS_ID_MAINMENU));
	gw->rclick = LoadMenu(hinstance, MAKEINTRESOURCE(NSWS_ID_CTXMENU));

	LOG(("creating window for hInstance %p", hinstance));
	hwnd = CreateWindowEx(0,
			      windowclassname_main,
			    "NetSurf Browser",
			    WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL |
			    WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CS_DBLCLKS,
			    CW_USEDEFAULT,
			    CW_USEDEFAULT,
			    gw->width,
			    gw->height,
			    NULL,
			    gw->mainmenu,
			    hinstance,
			    NULL);

	if ((option_window_width >= 100) &&
	    (option_window_height >= 100) &&
	    (option_window_x >= 0) &&
	    (option_window_y >= 0))
		SetWindowPos(hwnd, HWND_TOPMOST, option_window_x,
			     option_window_y, option_window_width,
			     option_window_height, SWP_SHOWWINDOW);

	nsws_window_set_accels(gw);
	nsws_window_set_ico(gw);
	gw->toolbar = nsws_window_toolbar_create(gw, hwnd);
	nsws_window_statusbar_create(gw);
	nsws_window_vscroll_create(gw);
	nsws_window_hscroll_create(gw);
	nsws_window_drawingarea_create(gw);

	ShowWindow(hwnd, SW_SHOWNORMAL);
	UpdateWindow(hwnd);
	gw->main = hwnd;
}

/**
 * create a new gui_window to contain a browser_window
 * \param bw the browser_window to connect to the new gui_window
 */
struct gui_window *
gui_create_browser_window(struct browser_window *bw,
			  struct browser_window *clone,
			  bool new_tab)
{
	struct gui_window *w;

	w = calloc(1, sizeof(struct gui_window));

	if (w == NULL)
		return NULL;

	/* connect gui window to browser window */
	w->bw = bw;

	w->width = 600;
	w->height = 600;
	w->toolbuttonsize = 24; /* includes padding of 4 every side */
	w->requestscrollx = 0;
	w->requestscrolly = 0;
	w->localhistory = NULL;

	w->mouse = malloc(sizeof(struct browser_mouse));
	if (w->mouse == NULL) {
		free(w);
		return NULL;
	}
	w->mouse->gui = w;
	w->mouse->state = 0;
	w->mouse->pressed_x = 0;
	w->mouse->pressed_y = 0;

	if (bw != NULL)
		switch(bw->browser_window_type) {
		case BROWSER_WINDOW_NORMAL:
			break;

		case BROWSER_WINDOW_FRAME:
			LOG(("create frame"));
			break;

		default:
			LOG(("unhandled type"));
		}

	if (window_list != NULL)
		window_list->prev = w;
	w->next = window_list;
	window_list = w;

	input_window = w;

	open_windows++;
	nsws_window_create(w);

	return w;
}




HICON nsws_window_get_ico(bool large)
{
	return large ? hIcon : hIconS;
}




/**
 * cache pointers for quick swapping
 */
static void nsws_window_init_pointers(void)
{
	nsws_pointer.hand = LoadCursor(NULL, IDC_HAND);
	nsws_pointer.ibeam = LoadCursor(NULL, IDC_IBEAM);
	nsws_pointer.cross = LoadCursor(NULL, IDC_CROSS);
	nsws_pointer.sizeall = LoadCursor(NULL, IDC_SIZEALL);
	nsws_pointer.sizewe = LoadCursor(NULL, IDC_SIZEWE);
	nsws_pointer.sizens = LoadCursor(NULL, IDC_SIZENS);
	nsws_pointer.sizenesw = LoadCursor(NULL, IDC_SIZENESW);
	nsws_pointer.sizenwse = LoadCursor(NULL, IDC_SIZENWSE);
	nsws_pointer.wait = LoadCursor(NULL, IDC_WAIT);
	nsws_pointer.appstarting = LoadCursor(NULL, IDC_APPSTARTING);
	nsws_pointer.no = LoadCursor(NULL, IDC_NO);
	nsws_pointer.help = LoadCursor(NULL, IDC_HELP);
	nsws_pointer.arrow = LoadCursor(NULL, IDC_ARROW);
}



HWND gui_window_main_window(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->main;
}

HWND gui_window_toolbar(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->toolbar;
}

HWND gui_window_urlbar(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->urlbar;
}

HWND gui_window_statusbar(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->statusbar;
}

HWND gui_window_drawingarea(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->drawingarea;
}

struct nsws_localhistory *gui_window_localhistory(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->localhistory;
}

void gui_window_set_localhistory(struct gui_window *w,
				 struct nsws_localhistory *l)
{
	if (w != NULL)
		w->localhistory = l;
}

RECT *gui_window_redraw_rect(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return &(w->redraw);
}

RECT *gui_window_clip_rect(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return &(w->clip);
}

int gui_window_width(struct gui_window *w)
{
	if (w == NULL)
		return 0;
	return w->width;
}

int gui_window_height(struct gui_window *w)
{
	if (w == NULL)
		return 0;
	return w->height;
}

int gui_window_scrollingx(struct gui_window *w)
{
	if (w == NULL)
		return 0;
	return w->requestscrollx;
}

int gui_window_scrollingy(struct gui_window *w)
{
	if (w == NULL)
		return 0;
	return w->requestscrolly;
}

struct gui_window *gui_window_iterate(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->next;
}

struct browser_window *gui_window_browser_window(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->bw;
}

/**
 * window cleanup code
 */
void gui_window_destroy(struct gui_window *w)
{
	if (w == NULL)
		return;

	if (w->prev != NULL)
		w->prev->next = w->next;
	else
		window_list = w->next;

	if (w->next != NULL)
		w->next->prev = w->prev;

	DestroyAcceleratorTable(w->acceltable);

	free(w);
	w = NULL;
}

/**
 * set window title
 * \param title the [url]
 */
void gui_window_set_title(struct gui_window *w, const char *title)
{
	if (w == NULL)
		return;
	LOG(("%p, title %s", w, title));
	char *fulltitle = malloc(strlen(title) +
				 SLEN("  -  NetSurf") + 1);
	if (fulltitle == NULL) {
		warn_user("NoMemory", 0);
		return;
	}
	strcpy(fulltitle, title);
	strcat(fulltitle, "  -  NetSurf");
	SendMessage(w->main, WM_SETTEXT, 0, (LPARAM)fulltitle);
	free(fulltitle);
}

/**
 * redraw a rectangle of the window
 */
void gui_window_redraw(struct gui_window *w, int x0, int y0, int x1, int y1)
{
	LOG(("redraw %p %d,%d %d,%d", w, x0, y0, x1, y1));
	if (w == NULL)
		return;
	w->redraw.left = x0;
	w->redraw.top = y0;
	w->redraw.right = x1;
	w->redraw.bottom = y1;
	redraw();
}

/**
 * redraw the whole window
 */
void gui_window_redraw_window(struct gui_window *w)
{
	LOG(("redraw window %p w=%d,h=%d", w, w->width, w->height));
	if (w == NULL)
		return;
	w->redraw.left = 0;
	w->redraw.top = 0;
	w->redraw.right = w->width;
	w->redraw.bottom = w->height;
	redraw();
}

void gui_window_update_box(struct gui_window *w,
			   const union content_msg_data *data)
{
	if (w == NULL)
		return;
	w->redraw.left = (long)data->redraw.x;
	w->redraw.top = (long)data->redraw.y;
	w->redraw.right =(long)(data->redraw.x + data->redraw.width);
	w->redraw.bottom = (long)(data->redraw.y + data->redraw.height);
	redraw();
}

bool gui_window_get_scroll(struct gui_window *w, int *sx, int *sy)
{
	LOG(("get scroll"));
	if (w == NULL)
		return false;
	*sx = w->scrollx;
	*sy = w->scrolly;

	return true;
}

/**
 * scroll the window
 * \param sx the new 'absolute' scroll location
 * \param sy the new 'absolute' scroll location
 */
void gui_window_set_scroll(struct gui_window *w, int sx, int sy)
{
	SCROLLINFO si;
	POINT p;

	if ((w == NULL) ||
	    (w->bw == NULL) ||
	    (w->bw->current_content == NULL))
		return;

	/* limit scale range */
	if (abs(w->bw->scale - 0.0) < 0.00001)
		w->bw->scale = 1.0;

	w->requestscrollx = sx - w->scrollx;
	w->requestscrolly = sy - w->scrolly;

	/* set the vertical scroll offset */
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = w->bw->current_content->height * w->bw->scale;
	si.nPage = w->height;
	si.nPos = MAX(w->scrolly + w->requestscrolly, 0);
	si.nPos = MIN(si.nPos, w->bw->current_content->height * w->bw->scale
		      - w->height);
	SetScrollInfo(w->main, SB_VERT, &si, TRUE);

	/* set the horizontal scroll offset */
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = w->bw->current_content->width * w->bw->scale;
	si.nPage = w->width;
	si.nPos = MAX(w->scrollx + w->requestscrollx, 0);
	si.nPos = MIN(si.nPos, w->bw->current_content->width * w->bw->scale
		      - w->width);
	SetScrollInfo(w->main, SB_HORZ, &si, TRUE);

	/* Set caret position */
	GetCaretPos(&p);
	HideCaret(w->drawingarea);
	SetCaretPos(p.x - w->requestscrollx, p.y - w->requestscrolly);
	ShowCaret(w->drawingarea);

	RECT r, redraw;
	r.top = 0;
	r.bottom = w->height + 1;
	r.left = 0;
	r.right = w->width + 1;
	ScrollWindowEx(w->drawingarea, - w->requestscrollx, - w->requestscrolly, &r,
		       NULL, NULL, &redraw, SW_INVALIDATE);
	gui_window_redraw(w, redraw.left + (w->requestscrollx + w->scrollx)
			  / w->bw->scale - 1,
			  redraw.top + (w->requestscrolly + w->scrolly)
			  / w->bw->scale - 1,
			  redraw.right + (w->requestscrollx + w->scrollx)
			  / w->bw->scale + 1,
			  redraw.bottom + (w->requestscrolly + w->scrolly)
			  / w->bw->scale + 1);
}

void gui_window_scroll_visible(struct gui_window *w, int x0, int y0,
			       int x1, int y1)
{
	LOG(("scroll visible %s:(%p, %d, %d, %d, %d)", __func__, w, x0,
	     y0, x1, y1));
}

void gui_window_position_frame(struct gui_window *w, int x0, int y0,
			       int x1, int y1)
{
	LOG(("position frame %s: %d, %d, %d, %d", w->bw->name,
	     x0, y0, x1, y1));

}

void gui_window_get_dimensions(struct gui_window *w, int *width, int *height,
			       bool scaled)
{
	LOG(("get dimensions %p w=%d h=%d", w, w->width, w->height));
	if (w == NULL)
		return;
	*width = w->width;
	*height = w->height;
}

void gui_window_update_extent(struct gui_window *w)
{

}

/**
 * set the status bar message
 */
void gui_window_set_status(struct gui_window *w, const char *text)
{
	if (w == NULL)
		return;
	SendMessage(w->statusbar, WM_SETTEXT, 0, (LPARAM)text);
}

/**
 * set the pointer shape
 */
void gui_window_set_pointer(struct gui_window *w, gui_pointer_shape shape)
{
	if (w == NULL)
		return;
	switch (shape) {
	case GUI_POINTER_POINT: /* link */
	case GUI_POINTER_MENU:
		SetCursor(nsws_pointer.hand);
		break;

	case GUI_POINTER_CARET: /* input */
		SetCursor(nsws_pointer.ibeam);
		break;

	case GUI_POINTER_CROSS:
		SetCursor(nsws_pointer.cross);
		break;

	case GUI_POINTER_MOVE:
		SetCursor(nsws_pointer.sizeall);
		break;

	case GUI_POINTER_RIGHT:
	case GUI_POINTER_LEFT:
		SetCursor(nsws_pointer.sizewe);
		break;

	case GUI_POINTER_UP:
	case GUI_POINTER_DOWN:
		SetCursor(nsws_pointer.sizens);
		break;

	case GUI_POINTER_RU:
	case GUI_POINTER_LD:
		SetCursor(nsws_pointer.sizenesw);
		break;

	case GUI_POINTER_RD:
	case GUI_POINTER_LU:
		SetCursor(nsws_pointer.sizenwse);
		break;

	case GUI_POINTER_WAIT:
		SetCursor(nsws_pointer.wait);
		break;

	case GUI_POINTER_PROGRESS:
		SetCursor(nsws_pointer.appstarting);
		break;

	case GUI_POINTER_NO_DROP:
	case GUI_POINTER_NOT_ALLOWED:
		SetCursor(nsws_pointer.no);
		break;

	case GUI_POINTER_HELP:
		SetCursor(nsws_pointer.help);
		break;

	default:
		SetCursor(nsws_pointer.arrow);
		break;
	}
}

struct nsws_pointers *nsws_get_pointers(void)
{
	return &nsws_pointer;
}

void gui_window_hide_pointer(struct gui_window *w)
{
}

void gui_window_set_url(struct gui_window *w, const char *url)
{
	if (w == NULL)
		return;
	SendMessage(w->urlbar, WM_SETTEXT, 0, (LPARAM) url);
}


void gui_window_start_throbber(struct gui_window *w)
{
	if (w == NULL)
		return;
	nsws_window_update_forward_back(w);

	if (w->mainmenu != NULL) {
		EnableMenuItem(w->mainmenu, NSWS_ID_NAV_STOP, MF_ENABLED);
		EnableMenuItem(w->mainmenu, NSWS_ID_NAV_RELOAD, MF_GRAYED);
	}
	if (w->rclick != NULL) {
		EnableMenuItem(w->rclick, NSWS_ID_NAV_STOP, MF_ENABLED);
		EnableMenuItem(w->rclick, NSWS_ID_NAV_RELOAD, MF_GRAYED);
	}
	if (w->toolbar != NULL) {
		SendMessage(w->toolbar, TB_SETSTATE, (WPARAM) NSWS_ID_NAV_STOP,
			    MAKELONG(TBSTATE_ENABLED, 0));
		SendMessage(w->toolbar, TB_SETSTATE,
			    (WPARAM) NSWS_ID_NAV_RELOAD,
			    MAKELONG(TBSTATE_INDETERMINATE, 0));
	}
	w->throbbing = true;
	Animate_Play(w->throbber, 0, -1, -1);
}

void gui_window_stop_throbber(struct gui_window *w)
{
	if (w == NULL)
		return;
	nsws_window_update_forward_back(w);
	if (w->mainmenu != NULL) {
		EnableMenuItem(w->mainmenu, NSWS_ID_NAV_STOP, MF_GRAYED);
		EnableMenuItem(w->mainmenu, NSWS_ID_NAV_RELOAD, MF_ENABLED);
	}
	if (w->rclick != NULL) {
		EnableMenuItem(w->rclick, NSWS_ID_NAV_STOP, MF_GRAYED);
		EnableMenuItem(w->rclick, NSWS_ID_NAV_RELOAD, MF_ENABLED);
	}
	if (w->toolbar != NULL) {
		SendMessage(w->toolbar, TB_SETSTATE, (WPARAM) NSWS_ID_NAV_STOP,
			    MAKELONG(TBSTATE_INDETERMINATE, 0));
		SendMessage(w->toolbar, TB_SETSTATE,
			    (WPARAM) NSWS_ID_NAV_RELOAD,
			    MAKELONG(TBSTATE_ENABLED, 0));
	}
	w->throbbing = false;
	Animate_Stop(w->throbber);
	Animate_Seek(w->throbber, 0);
}

/**
 * place caret in window
 */
void gui_window_place_caret(struct gui_window *w, int x, int y, int height)
{
	if (w == NULL)
		return;
	CreateCaret(w->drawingarea, (HBITMAP)NULL, 1, height * w->bw->scale);
	SetCaretPos(x * w->bw->scale - w->scrollx,
		    y * w->bw->scale - w->scrolly);
	ShowCaret(w->drawingarea);
}

/**
 * clear window caret
 */
void
gui_window_remove_caret(struct gui_window *w)
{
	if (w == NULL)
		return;
	HideCaret(w->drawingarea);
}

void
gui_window_set_icon(struct gui_window *g, struct content *icon)
{
}

void
gui_window_set_search_ico(struct content *ico)
{
}

bool
save_complete_gui_save(const char *path,
		       const char *filename,
		       size_t len,
		       const char *sourcedata,
		       content_type type)
{
	return false;
}

int
save_complete_htmlSaveFileFormat(const char *path,
				 const char *filename,
				 xmlDocPtr cur,
				 const char *encoding,
				 int format)
{
	return 0;
}


void gui_window_new_content(struct gui_window *w)
{
}

bool gui_window_scroll_start(struct gui_window *w)
{
	return true;
}

bool gui_window_box_scroll_start(struct gui_window *w,
				 int x0, int y0, int x1, int y1)
{
	return true;
}

bool gui_window_frame_resize_start(struct gui_window *w)
{
	LOG(("resize frame\n"));
	return true;
}

void gui_window_save_as_link(struct gui_window *w, struct content *c)
{
}

void gui_window_set_scale(struct gui_window *w, float scale)
{
	if (w == NULL)
		return;
	w->scale = scale;
	LOG(("%.2f\n", scale));
}

void gui_drag_save_object(gui_save_type type, struct content *c,
			  struct gui_window *w)
{
}

void gui_drag_save_selection(struct selection *s, struct gui_window *w)
{
}

void gui_start_selection(struct gui_window *w)
{
}

void gui_paste_from_clipboard(struct gui_window *w, int x, int y)
{
	HANDLE clipboard_handle;
	char *content;

	clipboard_handle = GetClipboardData(CF_TEXT);
	if (clipboard_handle != NULL) {
		content = GlobalLock(clipboard_handle);
		LOG(("pasting %s", content));
		GlobalUnlock(clipboard_handle);
	}
}

bool gui_empty_clipboard(void)
{
	return false;
}

bool gui_add_to_clipboard(const char *text, size_t length, bool space)
{
	HANDLE hnew;
	char *new, *original;
	HANDLE h = GetClipboardData(CF_TEXT);
	if (h == NULL)
		original = (char *)"";
	else
		original = GlobalLock(h);

	size_t len = strlen(original) + 1;
	hnew = GlobalAlloc(GHND, length + len);
	new = (char *)GlobalLock(hnew);
	snprintf(new, length + len, "%s%s", original, text);

	if (h != NULL) {
		GlobalUnlock(h);
		EmptyClipboard();
	}
	GlobalUnlock(hnew);
	SetClipboardData(CF_TEXT, hnew);
	return true;
}

bool gui_commit_clipboard(void)
{
	return false;
}

static bool
gui_selection_traverse_handler(const char *text,
			       size_t length,
			       struct box *box,
			       void *handle,
			       const char *space_text,
			       size_t space_length)
{
	if (space_text) {
		if (!gui_add_to_clipboard(space_text, space_length, false)) {
			return false;
		}
	}

	if (!gui_add_to_clipboard(text, length, box->space))
		return false;

	return true;
}

bool gui_copy_to_clipboard(struct selection *s)
{
	if ((s->defined) && (s->bw != NULL) && (s->bw->window != NULL) &&
	    (s->bw->window->main != NULL)) {
		OpenClipboard(s->bw->window->main);
		EmptyClipboard();
		if (selection_traverse(s, gui_selection_traverse_handler,
				       NULL)) {
			CloseClipboard();
			return true;
		}
	}
	return false;
}


void gui_create_form_select_menu(struct browser_window *bw,
				 struct form_control *control)
{
}

void gui_launch_url(const char *url)
{
}

void gui_cert_verify(struct browser_window *bw, struct content *c,
		     const struct ssl_cert_info *certs, unsigned long num)
{
}

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hLastInstance, LPSTR lpcli, int ncmd)
{
	char **argv = NULL;
	int argc = 0, argctemp = 0;
	size_t len;
	LPWSTR * argvw;

	if (SLEN(lpcli) > 0) {
		argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
	}

	hinstance = hInstance;
	setbuf(stderr, NULL);

	/* Construct a unix style argc/argv */
	argv = malloc(sizeof(char *) * argc);
	while (argctemp < argc) {
		len = wcstombs(NULL, argvw[argctemp], 0) + 1;
		if (len > 0)
			argv[argctemp] = malloc(len);
		if (argv[argctemp] != NULL) {
			wcstombs(argv[argctemp], argvw[argctemp], len);
			/* alter windows-style forward slash flags to
			 * hypen flags.
			 */
			if (argv[argctemp][0] == '/')
				argv[argctemp][0] = '-';
		}
		argctemp++;
	}
	return netsurf_main(argc, argv);
}


static void *myrealloc(void *ptr, size_t len, void *pw)
{
	return realloc(ptr, len);
}

void gui_quit(void)
{
	LOG(("gui_quit"));

	hubbub_finalise(myrealloc, NULL);
}

void gui_init(int argc, char** argv)
{
	char buf[PATH_MAX], sbuf[PATH_MAX];
	int len;

	LOG(("argc %d, argv %p", argc, argv));

	nsws_find_resource(buf, "Aliases", "./windows/res/Aliases");
	LOG(("Using '%s' as Aliases file", buf));

	hubbub_error he = hubbub_initialise(buf, myrealloc, NULL);
	LOG(("hubbub init %d", he));
	if (he != HUBBUB_OK)
		die("Unable to initialise HTML parsing library.\n");

	/* load browser messages */
	nsws_find_resource(buf, "messages", "./windows/res/messages");
	LOG(("Using '%s' as Messages file", buf));
	messages_load(buf);

	/* load browser options */
	nsws_find_resource(buf, "preferences", "~/.netsurf/preferences");
	LOG(("Using '%s' as Preferences file", buf));
	options_file_location = strdup(buf);
	options_read(buf);

	/* set up stylesheet urls */
	getcwd(sbuf, PATH_MAX);
	len = strlen(sbuf);
	strncat(sbuf, "windows/res/default.css", PATH_MAX - len);
	nsws_find_resource(buf, "default.css", sbuf);
	default_stylesheet_url = path_to_url(buf);
	LOG(("Using '%s' as Default CSS URL", default_stylesheet_url));

	getcwd(sbuf, PATH_MAX);
	len = strlen(sbuf);
	strncat(sbuf, "windows/res/quirks.css", PATH_MAX - len);
	nsws_find_resource(buf, "quirks.css", sbuf);
	quirks_stylesheet_url = path_to_url(buf);
	LOG(("Using '%s' as quirks stylesheet url", quirks_stylesheet_url ));


	create_local_windows_classes();

	option_target_blank = false;

}

void gui_init2(int argc, char** argv)
{
	struct browser_window *bw;
	const char *addr = NETSURF_HOMEPAGE;

	nsws_window_init_pointers();
	LOG(("argc %d, argv %p", argc, argv));

	if (argc > 1)
		addr = argv[1];
	else if (option_homepage_url != NULL && option_homepage_url[0]
		 != '\0')
		addr = option_homepage_url;
	else
		addr = default_page;

	LOG(("calling browser_window_create"));
	bw = browser_window_create(addr, 0, 0, true, false);
}

void gui_stdout(void)
{
	/* mwindows compile flag normally invalidates stdout unless
	   already redirected */
	if (_get_osfhandle(fileno(stdout)) == -1) {
		AllocConsole();
		freopen("CONOUT$", "w", stdout);
	}
}

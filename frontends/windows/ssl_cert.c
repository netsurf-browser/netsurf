/*
 * Copyright 2016 Vincent Sanders <vince@netsurf-browser.org>
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

/**
 * \file
 * Implementation of win32 certificate viewing using nsw32 core windows.
 */

#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

#include "utils/log.h"
#include "utils/nsoption.h"
#include "netsurf/keypress.h"
#include "netsurf/plotters.h"
#include "desktop/sslcert_viewer.h"

#include "windows/windbg.h"
#include "windows/plot.h"
#include "windows/corewindow.h"
#include "windows/gui.h"
#include "windows/resourceid.h"
#include "windows/ssl_cert.h"

/* spacing and sizes for dialog elements from
 * https://msdn.microsoft.com/en-us/library/windows/desktop/dn742486(v=vs.85).aspx#sizingandspacing
 */
/** dialog margin */
#define DLG_MRGN 11
/** warning icon height */
#define WRN_ICO_H 32
/** comand button width */
#define CMD_BTN_W 75
/** command button height */
#define CMD_BTN_H 23

static const char windowclassname_sslcert[] = "nswssslcertwindow";

/** win32 ssl certificate view context */
struct nsw32_sslcert_window {
	struct nsw32_corewindow core;

	/** SSL certificate viewer context data */
	struct sslcert_session_data *ssl_data;

	/** dialog window handle */
	HWND hWnd;

	/** accept button handle */
	HWND hAccept;

	/** reject button handle */
	HWND hReject;

	/** warning text  handle */
	HWND hTxt;
};


/**
 * callback for keypress on ssl certificate window
 *
 * \param nsw32_cw The nsw32 core window structure.
 * \param nskey The netsurf key code
 * \return NSERROR_OK on success otherwise appropriate error code
 */
static nserror
nsw32_sslcert_viewer_key(struct nsw32_corewindow *nsw32_cw, uint32_t nskey)
{
	struct nsw32_sslcert_window *crtvrfy_win;

	/* technically degenerate container of */
	crtvrfy_win = (struct nsw32_sslcert_window *)nsw32_cw;

	if (sslcert_viewer_keypress(crtvrfy_win->ssl_data, nskey)) {
		return NSERROR_OK;
	}
	return NSERROR_NOT_IMPLEMENTED;
}


/**
 * callback for mouse action on ssl certificate window
 *
 * \param nsw32_cw The nsw32 core window structure.
 * \param mouse_state netsurf mouse state on event
 * \param x location of event
 * \param y location of event
 * \return NSERROR_OK on success otherwise appropriate error code
 */
static nserror
nsw32_sslcert_viewer_mouse(struct nsw32_corewindow *nsw32_cw,
			   browser_mouse_state mouse_state,
			   int x, int y)
{
	struct nsw32_sslcert_window *crtvrfy_win;

	/* technically degenerate container of */
	crtvrfy_win = (struct nsw32_sslcert_window *)nsw32_cw;

	sslcert_viewer_mouse_action(crtvrfy_win->ssl_data, mouse_state, x, y);

	return NSERROR_OK;
}


/**
 * callback on draw event for ssl certificate window
 *
 * \param nsw32_cw The nsw32 core window structure.
 * \param scrollx The horizontal scroll offset.
 * \param scrolly The vertical scroll offset.
 * \param r The rectangle of the window that needs updating.
 * \return NSERROR_OK on success otherwise appropriate error code
 */
static nserror
nsw32_sslcert_viewer_draw(struct nsw32_corewindow *nsw32_cw,
			  int scrollx,
			  int scrolly,
			  struct rect *r)
{
	struct nsw32_sslcert_window *crtvrfy_win;
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &win_plotters
	};

	/* technically degenerate container of */
	crtvrfy_win = (struct nsw32_sslcert_window *)nsw32_cw;

	sslcert_viewer_redraw(crtvrfy_win->ssl_data,
			      -scrollx, -scrolly,
			      r, &ctx);

	return NSERROR_OK;
}


/**
 * callback on close event for ssl certificate window
 *
 * \param nsw32_cw The nsw32 core window structure.
 * \return NSERROR_OK on success otherwise appropriate error code
 */
static nserror
nsw32_sslcert_viewer_close(struct nsw32_corewindow *nsw32_cw)
{
	DestroyWindow(nsw32_cw->hWnd);

	return NSERROR_OK;
}


/* exported interface documented in nsw32/ssl_cert.h */
nserror nsw32_cert_verify(struct nsurl *url,
			  const struct ssl_cert_info *certs,
			  unsigned long num,
			  nserror (*cb)(bool proceed, void *pw),
			  void *cbpw)
{
	struct nsw32_sslcert_window *ncwin;
	nserror res;

	ncwin = malloc(sizeof(struct nsw32_sslcert_window));
	if (ncwin == NULL) {
		return NSERROR_NOMEM;
	}

	/* initialise certificate viewing interface */
	res = sslcert_viewer_create_session_data(num, url, cb, cbpw, certs,
						 &ncwin->ssl_data);
	if (res != NSERROR_OK) {
		free(ncwin);
		return res;
	}

	NSLOG(netsurf, INFO, "creating hInstance %p SSL window", hinst);
	ncwin->hWnd = CreateWindowEx(0,
				     windowclassname_sslcert,
				     "SSL Certificate viewer",
				     WS_OVERLAPPEDWINDOW |
				     WS_CLIPSIBLINGS |
				     WS_CLIPCHILDREN |
				     CS_DBLCLKS,
				     CW_USEDEFAULT,
				     CW_USEDEFAULT,
				     500,
				     400,
				     NULL,
				     NULL,
				     hinst,
				     NULL);
	if (ncwin->hWnd == NULL) {
		NSLOG(netsurf, INFO, "Window create failed");
		return NSERROR_NOMEM;
	}

	ncwin->core.title = NULL;
	ncwin->core.draw = nsw32_sslcert_viewer_draw;
	ncwin->core.key = nsw32_sslcert_viewer_key;
	ncwin->core.mouse = nsw32_sslcert_viewer_mouse;
	ncwin->core.close = nsw32_sslcert_viewer_close;

	res = nsw32_corewindow_init(hinst, ncwin->hWnd, &ncwin->core);
	if (res != NSERROR_OK) {
		free(ncwin);
		return res;
	}

	res = sslcert_viewer_init(ncwin->core.cb_table,
				  (struct core_window *)ncwin,
				  ncwin->ssl_data);
	if (res != NSERROR_OK) {
		free(ncwin);
		return res;
	}

	ncwin->hAccept = CreateWindowEx(0,
					"BUTTON",
					"Accept",
					WS_TABSTOP|WS_VISIBLE|
					WS_CHILD|BS_DEFPUSHBUTTON,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					CMD_BTN_W,
					CMD_BTN_H,
					ncwin->hWnd,
					(HMENU)IDC_SSLCERT_BTN_ACCEPT,
					hinst,
					NULL);
	HGDIOBJ hfDefault=GetStockObject(DEFAULT_GUI_FONT);
	SendMessage(ncwin->hAccept, WM_SETFONT, (WPARAM)hfDefault, MAKELPARAM(FALSE,0));
	ncwin->hReject = CreateWindowEx(0,
					"BUTTON",
					"Reject",
					WS_TABSTOP|WS_VISIBLE|
					WS_CHILD|BS_DEFPUSHBUTTON,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					CMD_BTN_W,
					CMD_BTN_H,
					ncwin->hWnd,
					(HMENU)IDC_SSLCERT_BTN_REJECT,
					hinst,
					NULL);
	SendMessage(ncwin->hReject, WM_SETFONT, (WPARAM)hfDefault, MAKELPARAM(FALSE,0));

	CreateWindowEx(0,
		       "STATIC",
		       IDI_WARNING,
		       WS_VISIBLE | WS_CHILD | SS_ICON,
		       DLG_MRGN,
		       DLG_MRGN,
		       CMD_BTN_W,
		       CMD_BTN_H,
		       ncwin->hWnd,
		       NULL,
		       NULL,
		       NULL);
	ncwin->hTxt = CreateWindowEx(0,
				     "STATIC",
				     "NetSurf failed to verify the authenticity of an SSL certificate. Verify the certificate details",
				     WS_VISIBLE | WS_CHILD | SS_LEFT,
				     DLG_MRGN + WRN_ICO_H + DLG_MRGN,
				     DLG_MRGN + 5,
				     400,
				     WRN_ICO_H - 5,
				     ncwin->hWnd,
				     NULL,
				     NULL,
				     NULL);
	SendMessage(ncwin->hTxt, WM_SETFONT, (WPARAM)hfDefault, MAKELPARAM(FALSE,0));

	SetProp(ncwin->hWnd, TEXT("CertWnd"), (HANDLE)ncwin);

	ShowWindow(ncwin->hWnd, SW_SHOWNORMAL);

	return NSERROR_OK;
}


/**
 * position and size ssl cert window widgets.
 *
 * \param hwnd The win32 handle of the window
 * \param certwin The certificate viewer context
 */
static void
nsw32_window_ssl_cert_size(HWND hwnd, struct nsw32_sslcert_window *certwin)
{
	RECT rc;
	GetClientRect(hwnd, &rc);
	/* position certificate drawable */
	MoveWindow(certwin->core.hWnd,
		   DLG_MRGN,
		   DLG_MRGN + WRN_ICO_H + DLG_MRGN,
		   rc.right - (DLG_MRGN + DLG_MRGN),
		   rc.bottom - (DLG_MRGN + WRN_ICO_H + DLG_MRGN + DLG_MRGN + CMD_BTN_H + DLG_MRGN),
		   TRUE);
	/* position accept button */
	MoveWindow(certwin->hAccept,
		   rc.right - (DLG_MRGN + CMD_BTN_W),
		   rc.bottom - (DLG_MRGN + CMD_BTN_H),
		   CMD_BTN_W,
		   CMD_BTN_H,
		   TRUE);
	/* position reject button */
	MoveWindow(certwin->hReject,
		   rc.right - (DLG_MRGN + CMD_BTN_W + 7 + CMD_BTN_W),
		   rc.bottom - (DLG_MRGN + CMD_BTN_H),
		   CMD_BTN_W,
		   CMD_BTN_H,
		   TRUE);
	/* position text */
	MoveWindow(certwin->hTxt,
		   DLG_MRGN + WRN_ICO_H + DLG_MRGN,
		   DLG_MRGN + 5,
		   rc.right - (DLG_MRGN + WRN_ICO_H + DLG_MRGN + DLG_MRGN),
		   WRN_ICO_H - 5,
		   TRUE);
}


/**
 * Destroy a certificate viewing window
 *
 * \param crtwin The certificate viewer context
 * \return NSERROR_OK on success otherwise appropriate error code
 */
static nserror nsw32_crtvrfy_destroy(struct nsw32_sslcert_window *crtwin)
{
	nserror res;

	res = sslcert_viewer_fini(crtwin->ssl_data);
	if (res == NSERROR_OK) {
		res = nsw32_corewindow_fini(&crtwin->core);
		DestroyWindow(crtwin->hWnd);
		free(crtwin);
	}
	return res;
}


/**
 * handle command message on ssl certificate viewing window.
 *
 * \param hwnd The win32 window handle.
 * \param crtwin certificate window context.
 * \param notification_code notifiction code
 * \param identifier notification identifier
 * \param ctrl_window The win32 control window handle
 * \return appropriate response for command
 */
static LRESULT
nsw32_window_ssl_cert_command(HWND hwnd,
			      struct nsw32_sslcert_window *crtwin,
			      int notification_code,
			      int identifier,
			      HWND ctrl_window)
{
	NSLOG(netsurf, INFO,
	      "notification_code %x identifier %x ctrl_window %p",
	      notification_code,
	      identifier,
	      ctrl_window);

	switch(identifier) {
	case IDC_SSLCERT_BTN_ACCEPT:
		sslcert_viewer_accept(crtwin->ssl_data);
		nsw32_crtvrfy_destroy(crtwin);
		break;

	case IDC_SSLCERT_BTN_REJECT:
		sslcert_viewer_reject(crtwin->ssl_data);
		nsw32_crtvrfy_destroy(crtwin);
		break;

	default:
		return 1; /* unhandled */
	}
	return 0; /* control message handled */
}


/**
 * callback for SSL certificate window win32 events
 *
 * \param hwnd The win32 window handle
 * \param msg The win32 message identifier
 * \param wparam The w win32 parameter
 * \param lparam The l win32 parameter
 */
static LRESULT CALLBACK
nsw32_window_ssl_cert_event_callback(HWND hwnd,
				     UINT msg,
				     WPARAM wparam,
				     LPARAM lparam)
{
	struct nsw32_sslcert_window *crtwin;
	crtwin = GetProp(hwnd, TEXT("CertWnd"));
	if (crtwin != NULL) {
		switch (msg) {
		case WM_SIZE:
			nsw32_window_ssl_cert_size(hwnd, crtwin);
			break;

		case WM_COMMAND:
			if (nsw32_window_ssl_cert_command(hwnd,
							  crtwin,
							  HIWORD(wparam),
							  LOWORD(wparam),
							  (HWND)lparam) == 0) {
				return 0;
			}
			break;

		case WM_CLOSE:
			sslcert_viewer_reject(crtwin->ssl_data);
			nsw32_crtvrfy_destroy(crtwin);
			return 0;
		}
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}


/* exported interface documented in nsw32/ssl_cert.h */
nserror nsws_create_cert_verify_class(HINSTANCE hInstance)
{
	nserror ret = NSERROR_OK;
	WNDCLASSEX wc;

	/* drawable area */
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = nsw32_window_ssl_cert_event_callback;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = NULL;
	wc.hCursor = NULL;
	wc.hbrBackground = (HBRUSH)(COLOR_MENU + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = windowclassname_sslcert;
	wc.hIconSm = NULL;

	if (RegisterClassEx(&wc) == 0) {
		win_perror("CertVerifyClass");
		ret = NSERROR_INIT_FAILED;
	}

	return ret;
}

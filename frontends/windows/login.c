/*
* Copyright 2018 Vincent Sanders <vince@netsurf-browser.org>
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
 * This is The win32 API basic authentication login dialog implementation.
 */

#include <stdio.h>

#include "utils/config.h"

#include <windows.h>

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsurl.h"
#include "desktop/version.h"

#include "windows/gui.h"
#include "windows/window.h"
#include "windows/login.h"
#include "windows/resourceid.h"

#include "windbg.h"

struct login_ctx {
	char *username;
	char *password;
	char *description;
	nserror (*cb)(const char *username, const char *password, void *cbctx);
	void *cbctx;
};


/**
 * free login dialog context
 */
static nserror
free_loginctx(struct login_ctx *ctx)
{
	free(ctx->username);
	free(ctx->password);
	free(ctx->description);
	free(ctx);

	return NSERROR_OK;
}


/**
 * generate the description of the login request
 */
static nserror
get_login_description(struct nsurl *url,
		      const char *realm,
		      char **out_str)
{
	char *url_s;
	size_t url_l;
	nserror res;
	const char *fmt = "The site %s is requesting your username and password. The realm is \"%s\"";
	char *str = NULL;
	int strlen;

	res = nsurl_get(url, NSURL_SCHEME | NSURL_HOST, &url_s, &url_l);
	if (res != NSERROR_OK) {
		return res;
	}

	strlen = snprintf(str, 0, fmt, url_s, realm) + 1;
	str = malloc(strlen);
	if (str == NULL) {
		res = NSERROR_NOMEM;
	} else {
		snprintf(str, strlen, fmt, url_s, realm);
		*out_str = str;
	}

	free(url_s);

	return res;
}

/**
 * win32 login dialog initialisation handler
 */
static BOOL
login_dialog_init(HWND hwndDlg, WPARAM wParam, LPARAM lParam)
{
	struct login_ctx *ctx;
	HWND hwndOwner;
	RECT rc, rcDlg, rcOwner;
	ctx = (struct login_ctx *)lParam;

	/* make context available in future calls */
	SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);

	/* set default contents */
	SetDlgItemText(hwndDlg, IDC_LOGIN_USERNAME, ctx->username);
	SetDlgItemText(hwndDlg, IDC_LOGIN_PASSWORD, ctx->password);
	SetDlgItemText(hwndDlg, IDC_LOGIN_DESCRIPTION, ctx->description);

	/* Get the owner window and dialog box rectangles. */
	if ((hwndOwner = GetParent(hwndDlg)) == NULL) {
		hwndOwner = GetDesktopWindow();
	}

	GetWindowRect(hwndOwner, &rcOwner);
	GetWindowRect(hwndDlg, &rcDlg);
	CopyRect(&rc, &rcOwner);

	/* Offset the owner and dialog box rectangles so that right
	 * and bottom values represent the width and height, and then
	 * offset the owner again to discard space taken up by the
	 * dialog box.
	 */

	OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
	OffsetRect(&rc, -rc.left, -rc.top);
	OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);

	/* The new position is the sum of half the remaining space and
	 * the owner's original position.
	 */
	SetWindowPos(hwndDlg,
		     HWND_TOP,
		     rcOwner.left + (rc.right / 2),
		     rcOwner.top + (rc.bottom / 2),
		     0, 0,          /* Ignores size arguments. */
		     SWP_NOSIZE);

	/* ensure username gets focus */
	if (GetDlgCtrlID((HWND) wParam) != IDC_LOGIN_USERNAME) {
		SetFocus(GetDlgItem(hwndDlg, IDC_LOGIN_USERNAME));
		return FALSE;
	}

	return TRUE;
}


/**
 * win32 login dialog ok handler
 */
static BOOL
login_dialog_ok(HWND hwndDlg, struct login_ctx *ctx)
{
	char username[255];
	char password[255];

	if (GetDlgItemText(hwndDlg,
			   IDC_LOGIN_USERNAME,
			   username,
			   sizeof(username)) == 0) {
		username[0]=0;
	}

	if (GetDlgItemText(hwndDlg,
			   IDC_LOGIN_PASSWORD,
			   password,
			   sizeof(password)) == 0) {
		password[0]=0;
	}

	NSLOG(netsurf, DEBUG,
	      "context %p, user:\"%s\" pw:\"%s\"", ctx, username, password);

	ctx->cb(username, password, ctx->cbctx);

	DestroyWindow(hwndDlg);

	nsw32_del_dialog(hwndDlg);

	free_loginctx(ctx);

	return TRUE;
}


/**
 * win32 login dialog cancel handler
 */
static BOOL
login_dialog_cancel(HWND hwndDlg, struct login_ctx *ctx)
{
	NSLOG(netsurf, DEBUG, "context %p", ctx);

	ctx->cb(NULL, NULL, ctx->cbctx);

	DestroyWindow(hwndDlg);

	nsw32_del_dialog(hwndDlg);

	free_loginctx(ctx);

	return TRUE;
}


/**
 * win32 API callback for login dialog
 */
static BOOL CALLBACK
login_dialog_callback(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	struct login_ctx *ctx;

	LOG_WIN_MSG(hwndDlg, message, wParam, lParam);

	/* obtain login dialog context */
	ctx = (struct login_ctx *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

	switch (message) {
	case WM_INITDIALOG:
		return login_dialog_init(hwndDlg, wParam, lParam);

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			return login_dialog_ok(hwndDlg, ctx);

		case IDCANCEL:
			return login_dialog_cancel(hwndDlg, ctx);
		}
	}
	return FALSE;
}


/**
 * Request credentials for http login
 */
nserror
nsw32_401login(nsurl *url,
	       const char *realm,
	       const char *username,
	       const char *password,
	       nserror (*cb)(const char *username,
			     const char *password,
			     void *cbctx),
	       void *cbctx)
{
	HWND hwndDlg;
	struct login_ctx *nctx;
	struct gui_window *gw;
	nserror res;

	/* locate parent window */
	gw = nsws_get_gui_window(GetActiveWindow());
	if (gw == NULL) {
		return NSERROR_INIT_FAILED;
	}

	/* setup context for parameters */
	nctx = calloc(1, sizeof(struct login_ctx));
	if (nctx == NULL) {
		return NSERROR_NOMEM;
	}

	nctx->username = strdup(username);
	nctx->password = strdup(password);
	nctx->cb = cb;
	nctx->cbctx = cbctx;

	res = get_login_description(url, realm, &nctx->description);
	if (res != NSERROR_OK) {
		free_loginctx(nctx);
		return res;
	}

	/* create modeless dialog */
	hwndDlg = CreateDialogParam(NULL,
				    MAKEINTRESOURCE(IDD_LOGIN),
				    gw->main,
				    login_dialog_callback,
				    (LPARAM)nctx);

	nsw32_add_dialog(hwndDlg);

	return NSERROR_OK;
}

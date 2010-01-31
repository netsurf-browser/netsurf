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
#define _WIN32_IE (0x0501)
#include <commctrl.h>
#include "desktop/options.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "windows/gui.h"
#include "windows/prefs.h"
#include "windows/resourceid.h"

#define NSWS_PREFS_WINDOW_WIDTH 600
#define NSWS_PREFS_WINDOW_HEIGHT 400

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif
/* static HWND prefswindow = NULL; */

void nsws_prefs_window_create(HWND parent);
BOOL CALLBACK nsws_prefs_event_callback(HWND hwnd, UINT msg, WPARAM wparam,
		LPARAM lparam);
CHOOSEFONT *nsws_prefs_font_prepare(int fontfamily, HWND parent);
					     
void nsws_prefs_dialog_init(HWND parent)
{
	int ret = DialogBox(hinstance, MAKEINTRESOURCE(NSWS_ID_PREFS_DIALOG),
			parent, nsws_prefs_event_callback);
	if (ret == -1) {
		warn_user(messages_get("NoMemory"), 0);
		return;
	}
}


BOOL CALLBACK nsws_prefs_event_callback(HWND hwnd, UINT msg, WPARAM wparam,
		LPARAM lparam)
{
	HWND sub;
	char *temp, number[6];
	int len;
	switch(msg) {
	case WM_INITDIALOG: {
		if ((option_homepage_url != NULL) && 
				(option_homepage_url[0] != '\0')) {
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_HOMEPAGE);
			SendMessage(sub, WM_SETTEXT, 0, 
					(LPARAM)option_homepage_url);
		}
		sub = GetDlgItem(hwnd, NSWS_ID_PREFS_PROXYTYPE);
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"None");
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"Simple");
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"Basic Auth");
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"NTLM Auth");
		if (option_http_proxy)
			SendMessage(sub, CB_SETCURSEL, (WPARAM) 
					(option_http_proxy_auth + 1), 0);
		else
			SendMessage(sub, CB_SETCURSEL, 0, 0);
		sub = GetDlgItem(hwnd, NSWS_ID_PREFS_PROXYHOST);
		if ((option_http_proxy_host != NULL) &&
				(option_http_proxy_host[0] != '\0'))
			SendMessage(sub, WM_SETTEXT, 0,
					(LPARAM)option_http_proxy_host);
		sub = GetDlgItem(hwnd, NSWS_ID_PREFS_PROXYPORT);
		if (option_http_proxy_port != 0) {
			snprintf(number, 6, "%d", option_http_proxy_port);
			SendMessage(sub, WM_SETTEXT, 0,	(LPARAM)number);
		}
		sub = GetDlgItem(hwnd, NSWS_ID_PREFS_PROXYNAME);
		if ((option_http_proxy_auth_user != NULL) &&
				(option_http_proxy_auth_user[0] != '\0'))
			SendMessage(sub, WM_SETTEXT, 0,
					(LPARAM)option_http_proxy_auth_user);
		sub = GetDlgItem(hwnd, NSWS_ID_PREFS_PROXYPASS);
		if ((option_http_proxy_auth_pass != NULL) &&
				(option_http_proxy_auth_pass[0] != '\0'))
			SendMessage(sub, WM_SETTEXT, 0,
					(LPARAM)option_http_proxy_auth_pass);
		sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FONTDEF);
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"Sans serif");
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"Serif");
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"Monospace");
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"Cursive");
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"Fantasy");
		SendMessage(sub, CB_SETCURSEL,
				(WPARAM) (option_font_default - 1), 0);
		sub = GetDlgItem(hwnd, NSWS_ID_PREFS_ADVERTS);
		SendMessage(sub, BM_SETCHECK, (WPARAM) ((option_block_ads) ?
				BST_CHECKED : BST_UNCHECKED), 0);
		sub = GetDlgItem(hwnd, NSWS_ID_PREFS_REFERER);
		SendMessage(sub, BM_SETCHECK, (WPARAM)((option_send_referer) ? 
				BST_CHECKED : BST_UNCHECKED), 0);
		sub = GetDlgItem(hwnd, NSWS_ID_PREFS_NOANIMATION);
		SendMessage(sub, BM_SETCHECK, (WPARAM)((option_animate_images) 
				? BST_UNCHECKED : BST_CHECKED),	0);
		sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FETCHERS);
		snprintf(number, 6, "%d", option_max_fetchers);
		SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
		sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FETCH_HOST);
		snprintf(number, 6, "%d", option_max_fetchers_per_host);
		SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
		sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FETCH_HANDLES);
		snprintf(number, 6, "%d", option_max_cached_fetch_handles);
		SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
		
		if ((option_font_sans != NULL) && 
				(option_font_sans[0] != '\0')) {
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_SANS);
			SendMessage(sub, WM_SETTEXT, 0,
					(LPARAM)option_font_sans);
		}
		if ((option_font_serif != NULL) && 
				(option_font_serif[0] != '\0')) {
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_SERIF);
			SendMessage(sub, WM_SETTEXT, 0,
					(LPARAM)option_font_serif);
		}
		if ((option_font_mono != NULL) && 
				(option_font_mono[0] != '\0')) {
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_MONO);
			SendMessage(sub, WM_SETTEXT, 0,
					(LPARAM)option_font_mono);
		}
		if ((option_font_cursive != NULL) && 
				(option_font_cursive[0] != '\0')) {
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_CURSIVE);
			SendMessage(sub, WM_SETTEXT, 0,
					(LPARAM)option_font_cursive);
		}
		if ((option_font_fantasy != NULL) && 
				(option_font_fantasy[0] != '\0')) {
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FANTASY);
			SendMessage(sub, WM_SETTEXT, 0,
				     (LPARAM)option_font_fantasy);
		}
		if (option_font_min_size != 0) {
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FONT_MINSIZE);
			snprintf(number, 6, "%.1f", option_font_min_size /
					10.0);
			SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
		}
		if (option_font_size != 0) {
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FONT_SIZE);
			snprintf(number, 6, "%.1f", option_font_size / 10.0);
			SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
		}
		if (option_max_fetchers != 0) {
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FETCHERS);
			snprintf(number, 6, "%d", option_max_fetchers);
			SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
		}
		if (option_max_fetchers_per_host != 0) {
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FETCH_HOST);
			snprintf(number, 6, "%d", 
					option_max_fetchers_per_host);
			SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
		}
		if (option_max_cached_fetch_handles != 0) {
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FETCH_HANDLES);
			snprintf(number, 6, "%d",
					option_max_cached_fetch_handles);
			SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
		}
		if (option_minimum_gif_delay != 0) {
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_ANIMATIONDELAY);
			snprintf(number, 6, "%.1f", option_minimum_gif_delay /
					100.0);
			SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
		}
		return TRUE;
	}
	case WM_CREATE:
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wparam)) {
		case IDOK: {
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_HOMEPAGE);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			if (option_homepage_url != NULL)
				free(option_homepage_url);
			option_homepage_url = malloc(len + 1);
			if (option_homepage_url != NULL)
				SendMessage(sub, WM_GETTEXT, 
						(WPARAM) (len + 1),
						(LPARAM) option_homepage_url);
/*			seems to segfault at startup
			
			option_block_ads = (IsDlgButtonChecked(hwnd,
					NSWS_ID_PREFS_ADVERTS) == BST_CHECKED)
					? true : false;
*/
			option_send_referer = (IsDlgButtonChecked(hwnd,
					NSWS_ID_PREFS_REFERER) == BST_CHECKED)
					? true : false;
			option_animate_images = (IsDlgButtonChecked(hwnd,
					NSWS_ID_PREFS_NOANIMATION) ==
					BST_CHECKED) ? false : true;
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FETCHERS);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)(len + 1),
						(LPARAM)temp);
				option_max_fetchers = atoi(temp);
				free(temp);
			}
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FETCH_HOST);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)(len + 1),
					     (LPARAM)temp);
				option_max_fetchers_per_host = atoi(temp);
				free(temp);
			}
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FETCH_HANDLES);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)(len + 1),
					     (LPARAM)temp);
				option_max_cached_fetch_handles = atoi(temp);
				free(temp);
			}
			sub = GetDlgItem(hwnd,
					  NSWS_ID_PREFS_FONT_SIZE);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)
						(len + 1), (LPARAM) temp);
				option_font_size = (int) 
						(10 * strtod(temp, NULL));
				free(temp);
			}
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FONT_MINSIZE);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)
						(len + 1), (LPARAM) temp);
				option_font_min_size = (int) 
						(10 * strtod(temp, NULL));
				free(temp);
			}
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_ANIMATIONDELAY);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)
						(len + 1), (LPARAM) temp);
				option_minimum_gif_delay = (int) 
						(100 * strtod(temp, NULL));
				free(temp);
			}
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_PROXYHOST);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				if (option_http_proxy_host != NULL)
					free(option_http_proxy_host);
				SendMessage(sub, WM_GETTEXT, (WPARAM)(len + 1),
						(LPARAM)temp);
				option_http_proxy_host = strdup(temp);
				free(temp);
			}
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_PROXYPORT);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)(len + 1),
					     (LPARAM)temp);
				option_http_proxy_port = atoi(temp);
				free(temp);
			}
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_PROXYNAME);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				if (option_http_proxy_auth_user != NULL)
					free(option_http_proxy_auth_user);
				SendMessage(sub, WM_GETTEXT, (WPARAM)(len + 1),
					     (LPARAM)temp);
				option_http_proxy_auth_user = strdup(temp);
				free(temp);
			}
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_PROXYPASS);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				if (option_http_proxy_auth_pass != NULL)
					free(option_http_proxy_auth_pass);
				SendMessage(sub, WM_GETTEXT, (WPARAM)(len + 1),
					     (LPARAM)temp);
				option_http_proxy_auth_pass = strdup(temp);
				free(temp);
			}
			options_write(options_file_location);
		}
		case IDCANCEL:
			EndDialog(hwnd, IDOK);
			break;
		case NSWS_ID_PREFS_HOMEPAGE:
			break;
		case NSWS_ID_PREFS_ADVERTS:
			break;
		case NSWS_ID_PREFS_POPUPS:
			printf("wparam %d, lparam %ld hi %d lo %d\n", wparam,
					lparam, HIWORD(lparam),
					LOWORD(lparam));
			break;
		case NSWS_ID_PREFS_PLUGINS:
			printf("wparam %d, lparam %ld hi %d lo %d\n", wparam,
					lparam, HIWORD(lparam),
					LOWORD(lparam));
			break;
		case NSWS_ID_PREFS_REFERER:
			printf("wparam %d, lparam %ld hi %d lo %d\n", wparam,
					lparam, HIWORD(lparam),
					LOWORD(lparam));
			break;
		case NSWS_ID_PREFS_PROXYTYPE:
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_PROXYTYPE);
			option_http_proxy_auth = SendMessage(sub,
					CB_GETCURSEL, 0, 0) - 1;
			option_http_proxy = (option_http_proxy_auth != -1);
			option_http_proxy_auth += (option_http_proxy) ? 0 : 1;
			break;
		case NSWS_ID_PREFS_PROXYHOST:
			break;
		case NSWS_ID_PREFS_PROXYNAME:
			break;
		case NSWS_ID_PREFS_PROXYPASS:
			break;
		case NSWS_ID_PREFS_SANS: {
			CHOOSEFONT *cf = nsws_prefs_font_prepare(FF_SWISS,
					hwnd);
			if (cf == NULL)
				break;
			if (ChooseFont(cf) == TRUE) {
				if (option_font_sans != NULL)
					free(option_font_sans);
				option_font_sans = strdup(
						cf->lpLogFont->lfFaceName);
			}
			free(cf->lpLogFont);
			free(cf);
			if ((option_font_sans != NULL) && 
				(option_font_sans[0] != '\0')) {
				sub = GetDlgItem(hwnd, NSWS_ID_PREFS_SANS);
			SendMessage(sub, WM_SETTEXT, 0,
				     (LPARAM)option_font_sans);
			}
			break;
		}
		case NSWS_ID_PREFS_SERIF: {
			CHOOSEFONT *cf = nsws_prefs_font_prepare(FF_ROMAN,
					hwnd);
			if (cf == NULL)
				break;
			
			if (ChooseFont(cf) == TRUE) {
				if (option_font_serif != NULL)
					free(option_font_serif);
				option_font_serif = strdup(
						cf->lpLogFont->lfFaceName);
			}
			free(cf->lpLogFont);
			free(cf);
			if ((option_font_serif != NULL) && 
				(option_font_serif[0] != '\0')) {
				sub = GetDlgItem(hwnd, NSWS_ID_PREFS_SERIF);
			SendMessage(sub, WM_SETTEXT, 0,
				     (LPARAM)option_font_serif);
			}
			break;
		}
		case NSWS_ID_PREFS_MONO: {
			CHOOSEFONT *cf = nsws_prefs_font_prepare(FF_MODERN,
					hwnd);
			if (cf == NULL)
				break;

			if (ChooseFont(cf) == TRUE) {
				if (option_font_mono != NULL)
					free(option_font_mono);
				option_font_mono = strdup(
						cf->lpLogFont->lfFaceName);
			}
			free(cf->lpLogFont);
			free(cf);
			if ((option_font_mono != NULL) && 
				(option_font_mono[0] != '\0')) {
				sub = GetDlgItem(hwnd, NSWS_ID_PREFS_MONO);
			SendMessage(sub, WM_SETTEXT, 0,
				     (LPARAM)option_font_mono);
			}
			break;
		}
		case NSWS_ID_PREFS_CURSIVE: {
			CHOOSEFONT *cf = nsws_prefs_font_prepare(FF_SCRIPT, 
					hwnd);
			if (cf == NULL)
				break;
			
			if (ChooseFont(cf) == TRUE) {
				if (option_font_cursive != NULL)
					free(option_font_cursive);
				option_font_cursive = strdup(
						cf->lpLogFont->lfFaceName);
			}
			free(cf->lpLogFont);
			free(cf);
			if ((option_font_cursive != NULL) && 
				(option_font_cursive[0] != '\0')) {
				sub = GetDlgItem(hwnd, NSWS_ID_PREFS_CURSIVE);
			SendMessage(sub, WM_SETTEXT, 0,
				     (LPARAM)option_font_cursive);
			}
			break;
		}
		case NSWS_ID_PREFS_FANTASY: {
			CHOOSEFONT *cf = nsws_prefs_font_prepare(FF_DECORATIVE,
					hwnd);
			if (cf == NULL)
				break;
			if (ChooseFont(cf) == TRUE) {
				if (option_font_fantasy != NULL)
					free(option_font_fantasy);
				option_font_fantasy = strdup(
						cf->lpLogFont->lfFaceName);
			}
			free(cf->lpLogFont);
			free(cf);
			if ((option_font_fantasy != NULL) && 
				(option_font_fantasy[0] != '\0')) {
				sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FANTASY);
			SendMessage(sub, WM_SETTEXT, 0,
				     (LPARAM)option_font_fantasy);
			}
			break;
		}
		case NSWS_ID_PREFS_FONTDEF: {
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FONTDEF);
			option_font_default = SendMessage(sub, CB_GETCURSEL, 0, 0) + 1;
			break;
		}
		case NSWS_ID_PREFS_FETCHERS:
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FETCHERS);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp == NULL)
				return FALSE;
			SendMessage(sub, WM_GETTEXT, (WPARAM)
					(len + 1), (LPARAM) temp);
			option_max_fetchers = atoi(temp);
			free(temp);
			break;
		case NSWS_ID_PREFS_FETCH_HOST:
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FETCH_HOST);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp == NULL)
				return FALSE;
			SendMessage(sub, WM_GETTEXT, (WPARAM)
					(len + 1), (LPARAM) temp);
			option_max_fetchers_per_host = atoi(temp);
			free(temp);
			break;
		case NSWS_ID_PREFS_FETCH_HANDLES:
			sub = GetDlgItem(hwnd, NSWS_ID_PREFS_FETCH_HANDLES);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp == NULL)
				return FALSE;
			SendMessage(sub, WM_GETTEXT, (WPARAM)
					(len + 1), (LPARAM) temp);
			option_max_cached_fetch_handles = atoi(temp);
			free(temp);
			break;
		default:
			return FALSE;
		}
		break;
	case WM_NOTIFY: {
		NMHDR *nm = (NMHDR *)lparam;
		NMUPDOWN *ud = (NMUPDOWN *)lparam;
		if (nm->code == UDN_DELTAPOS)
			switch(nm->idFrom) {
			case NSWS_ID_PREFS_FONT_SIZE_SPIN: {
				double size = 0;
				sub = GetDlgItem(hwnd,
						NSWS_ID_PREFS_FONT_SIZE);
				len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
				temp = malloc(len + 1);
				if (temp == NULL)
					return FALSE;
				SendMessage(sub, WM_GETTEXT, (WPARAM)
						(len + 1), (LPARAM) temp);
				if (ud->iDelta == 1) {
					size = strtod(temp, NULL) + 0.1;
				}
				else if (ud->iDelta == -1) {
					size = strtod(temp, NULL) - 0.1;
				}
				free(temp);
				size = MAX(size, 0);
				snprintf(number, 6, "%.1f", size);
				SendMessage(sub, WM_SETTEXT, 0, 
						(LPARAM)number);
				return TRUE;
			}
			case NSWS_ID_PREFS_FONT_MINSIZE_SPIN: {
				double size = 0;
				sub = GetDlgItem(hwnd,
						NSWS_ID_PREFS_FONT_MINSIZE);
				len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
				temp = malloc(len + 1);
				if (temp == NULL)
					return FALSE;
				SendMessage(sub, WM_GETTEXT, (WPARAM)
						(len + 1), (LPARAM) temp);
				if (ud->iDelta == 1) {
					size = strtod(temp, NULL) + 0.1;
				}
				else if (ud->iDelta == -1) {
					size = strtod(temp, NULL) - 0.1;
				}
				free(temp);
				size = MAX(size, 0);
				snprintf(number, 6, "%.1f", size);
				SendMessage(sub, WM_SETTEXT, 0, 
						(LPARAM)number);
				return TRUE;
			}
			case NSWS_ID_PREFS_ANIMATIONDELAY_SPIN: {
				double animation=0;
				sub = GetDlgItem(hwnd,
						NSWS_ID_PREFS_ANIMATIONDELAY);
				len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
				temp = malloc(len + 1);
				if (temp == NULL)
					return FALSE;
				SendMessage(sub, WM_GETTEXT, (WPARAM)
						(len + 1), (LPARAM) temp);
				if (ud->iDelta == 1) {
					animation = strtod(temp, NULL) + 0.1;
				}
				else if (ud->iDelta == -1) {
					animation = strtod(temp, NULL) - 0.1;
				}
				free(temp);
				animation = MAX(animation, 0);
				snprintf(number, 6, "%.1f", animation);
				SendMessage(sub, WM_SETTEXT, 0, 
						(LPARAM)number);
				return TRUE;
			}
			}
		break;
	}
	default:
		return FALSE;
	}
	return TRUE;
}

CHOOSEFONT *nsws_prefs_font_prepare(int fontfamily, HWND parent)
{	
	CHOOSEFONT *cf = malloc(sizeof(CHOOSEFONT));
	if (cf == NULL) {
		warn_user(messages_get("NoMemory"),0);
		return NULL;
	}
	LOGFONT *lf = malloc(sizeof(LOGFONT));
	if (lf == NULL) {
		warn_user(messages_get("NoMemory"),0);
		free(cf);
		return NULL;
	}
	switch(fontfamily) {
		case FF_ROMAN:
			snprintf(lf->lfFaceName, LF_FACESIZE, "%s",
					option_font_serif);
			break;
		case FF_MODERN:
			snprintf(lf->lfFaceName, LF_FACESIZE, "%s",
					option_font_mono);
			break;
		case FF_SCRIPT:
			snprintf(lf->lfFaceName, LF_FACESIZE, "%s",
					option_font_cursive);
			break;
		case FF_DECORATIVE:
			snprintf(lf->lfFaceName, LF_FACESIZE, "%s",
					option_font_fantasy);
			break;
		case FF_SWISS:
		default:
			snprintf(lf->lfFaceName, LF_FACESIZE, "%s",
					option_font_sans);
			break;
	}
	cf->lStructSize = sizeof(CHOOSEFONT);
	cf->hwndOwner = parent;
	cf->lpLogFont = lf;
	cf->Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_LIMITSIZE;
	cf->nSizeMin = 16;
	cf->nSizeMax = 24;
	
	return cf;
}

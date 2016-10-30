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

#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <windows.h>

#include "utils/errors.h"
#include "utils/nsurl.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "utils/corestrings.h"
#include "utils/url.h"
#include "utils/file.h"
#include "utils/messages.h"
#include "netsurf/browser_window.h"
#include "netsurf/clipboard.h"

#include "windows/schedule.h"
#include "windows/window.h"
#include "windows/filetype.h"
#include "windows/gui.h"

/**
 * win32 application instance handle.
 *
 * This handle is set in the main windows entry point.
 */
HINSTANCE hinst;

static bool win32_quit = false;

void win32_set_quit(bool q)
{
	win32_quit = q;
}

/* exported interface documented in gui.h */
void win32_run(void)
{
	MSG Msg; /* message from system */
	BOOL bRet; /* message fetch result */
	int timeout; /* timeout in miliseconds */
	UINT timer_id = 0;

	LOG("Starting messgae dispatcher");

	while (!win32_quit) {
		/* run the scheduler and discover how long to wait for
		 * the next event.
		 */
		timeout = schedule_run();

		if (timeout == 0) {
			bRet = PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE);
		} else {
			if (timeout > 0) {
				/* set up a timer to ensure we get woken */
				timer_id = SetTimer(NULL, 0, timeout, NULL);
			}

			/* wait for a message */
			bRet = GetMessage(&Msg, NULL, 0, 0);

			/* if a timer was sucessfully created remove it */
			if (timer_id != 0) {
				KillTimer(NULL, timer_id);
				timer_id = 0;
			}
		}

		if (bRet > 0) {
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
	}
}


/* exported function documented in windows/gui.h */
nserror win32_warning(const char *warning, const char *detail)
{
	size_t len = 1 + ((warning != NULL) ? strlen(messages_get(warning)) :
			0) + ((detail != 0) ? strlen(detail) : 0);
	char message[len];
	snprintf(message, len, messages_get(warning), detail);
	MessageBox(NULL, message, "Warning", MB_ICONWARNING);

	return NSERROR_OK;
}


/**
 * Core asks front end for clipboard contents.
 *
 * \param  buffer  UTF-8 text, allocated by front end, ownership yeilded to core
 * \param  length  Byte length of UTF-8 text in buffer
 */
static void gui_get_clipboard(char **buffer, size_t *length)
{
	/* TODO: Implement this */
	HANDLE clipboard_handle;
	char *content;

	clipboard_handle = GetClipboardData(CF_TEXT);
	if (clipboard_handle != NULL) {
		content = GlobalLock(clipboard_handle);
		LOG("pasting %s", content);
		GlobalUnlock(clipboard_handle);
	}
}


/**
 * Core tells front end to put given text in clipboard
 *
 * \param  buffer    UTF-8 text, owned by core
 * \param  length    Byte length of UTF-8 text in buffer
 * \param  styles    Array of styles given to text runs, owned by core, or NULL
 * \param  n_styles  Number of text run styles in array
 */
static void gui_set_clipboard(const char *buffer, size_t length,
		nsclipboard_styles styles[], int n_styles)
{
	/* TODO: Implement this */
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
	snprintf(new, length + len, "%s%s", original, buffer);

	if (h != NULL) {
		GlobalUnlock(h);
		EmptyClipboard();
	}
	GlobalUnlock(hnew);
	SetClipboardData(CF_TEXT, hnew);
}



static struct gui_clipboard_table clipboard_table = {
	.get = gui_get_clipboard,
	.set = gui_set_clipboard,
};

struct gui_clipboard_table *win32_clipboard_table = &clipboard_table;



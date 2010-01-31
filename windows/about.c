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
#include <stdio.h>

#include "utils/utils.h"
#include "utils/messages.h"
#include "desktop/netsurf.h"

#include "windows/about.h"
#include "windows/resourceid.h"

const char *netsurf_authors[] = {
       "John-Mark Bell", "James Bursa", "Michael Drake",
       "Rob Kendrick", "Adrian Lees", "Vincent Sanders",
       "Daniel Silverstone", "Richard Wilson",
       "\nContributors:", "Kevin Bagust", "Stefaan Claes",
       "Matthew Hambley", "Rob Jackson", "Jeffrey Lee", "Phil Mellor",
       "Philip Pemberton", "Darren Salt", "Andrew Timmins",
       "John Tytgat", "Chris Williams",
       "\nGoogle Summer of Code Contributors:", "Mark Benjamin",
       "Adam Blokus", "Paul Blokus", "Sean Fox", "Michael Lester",
       "Andrew Sidwell", "Bo Yang", NULL
};
const char * const netsurf_translators = "Sebastian Barthel\n"
       "Bruno D'Arcangeli\nGerard van Katwijk\nJérôme Mathevet\n"
       "Simon Voortman.";
const char *netsurf_artists[] = {
       "Michael Drake", "\nContributors:", "Andrew Duffell",
       "John Duffell", "Richard Hallas", "Phil Mellor", NULL
};
const char *netsurf_documenters[] = {
       "John-Mark Bell", "James Bursa", "Michael Drake",
       "Richard Wilson", "\nContributors:", "James Shaw", NULL
};
const char * const netsurf_name = "NetSurf";
const char * const netsurf_description = 
       "Small as a mouse, fast as a cheetah, and available for free.\n"
       "NetSurf is a portable web browser for RISC OS, AmigaOS, BeOS, "
       "Windows and UNIX-like platforms.";
const char * const netsurf_url = "http://www.netsurf-browser.org/";
const char * const netsurf_url_label = "NetSurf Website";
const char * const netsurf_copyright =
       "Copyright © 2003 - 2009 The NetSurf Developers";

BOOL CALLBACK nsws_about_event_callback(HWND hwnd, UINT msg, WPARAM wparam,
		LPARAM lparam);

BOOL CALLBACK nsws_about_event_callback(HWND hwnd, UINT msg, WPARAM wparam,
		LPARAM lparam)
{
	switch(msg) {
	case WM_INITDIALOG: {
		HWND content = GetDlgItem(hwnd, NSWS_ID_ABOUT_CONTENT);
		/* modify label NSWS_ID_ABOUT_CONTENT */
		size_t len;
		char *newcontent, *authors, *artists, *documenters;
		int i;
		for (i = 0, len = 0; netsurf_authors[i] != NULL; i++) {
			len += strlen(netsurf_authors[i]) + 1;
		}
		authors = malloc(len + 1);
		if (authors == NULL) {
			warn_user(messages_get("NoMemory"), 0);
			return FALSE;
		}
		authors[0] = '\0';
		for (i = 0; netsurf_authors[i] != NULL; i++) {
			strcat(authors, netsurf_authors[i]);
			strcat(authors, " ");
		}
		for (i = 0, len = 0; netsurf_artists[i] != NULL; i++) {
			len += strlen(netsurf_artists[i]) + 1;
		}
		artists = malloc(len + 1);
		if (artists == NULL) {
			warn_user(messages_get("NoMemory"), 0);
			free(authors);
			return FALSE;
		}
		artists[0] = '\0';
		for (i = 0; netsurf_artists[i] != NULL; i++) {
			strcat(artists, netsurf_artists[i]);
			strcat(artists, " ");
		}
		for (i = 0, len = 0; netsurf_documenters[i] != NULL; i++) {
			len += strlen(netsurf_documenters[i]) + 1;
		}
		documenters = malloc(len + 1);
		if (documenters == NULL) {
			warn_user(messages_get("NoMemory"), 0);
			free(authors);
			free(artists);
			return FALSE;
		}
		documenters[0] = '\0';
		for (i = 0; netsurf_documenters[i] != NULL; i++) {
			strcat(documenters, netsurf_documenters[i]);
			strcat(documenters, " ");
		}
		len = strlen(netsurf_name) + 1 + strlen(netsurf_version) +
				2 + strlen(netsurf_description) + 2 + 
				strlen(netsurf_url) + 2 + 
				strlen(netsurf_copyright) + 2 +
				strlen(netsurf_translators) + 2 + 
				strlen(authors) + 2 + strlen(artists) + 2 +
				strlen(documenters) + 2 + SLEN("authors:") + 2 +
				SLEN("artists:") + 2 + SLEN("documenters:") + 2 +
				SLEN("translators:") + 2;
		newcontent = malloc(len + 1);
		if (newcontent == NULL) {
			warn_user(messages_get("NoMemory"), 0);
			free(authors);
			free(artists);
			free(documenters);
			return FALSE;
		}
		sprintf(newcontent, "%s %s\n\n%s\n\nauthors:\n\n%s\n\n"
				"artists:\n\n%s\n\ndocumenters:\n\n%s\n\n"
				"translators:\n\n%s\n\n%s\n\n%s\n",
				netsurf_name, netsurf_version,
				netsurf_description, authors, artists, 
				documenters, netsurf_translators, netsurf_url,
				netsurf_copyright);
		SendMessage(content, WM_SETTEXT, 0, (LPARAM)newcontent);
		free(authors);
		free(artists);
		free(documenters);
		free(newcontent);
		
		return TRUE;
	}
	case WM_COMMAND:
		switch(LOWORD(wparam)) {
		case IDOK:
			EndDialog(hwnd, IDOK);
			break;
		default:
			return FALSE;
		}
		break;
	case WM_CREATE:
		return TRUE;
	default:
		return FALSE;
	}
	return TRUE;
}

void nsws_about_dialog_init(HINSTANCE hinst, HWND parent)
{
	int ret = DialogBox(hinst, MAKEINTRESOURCE(NSWS_ID_ABOUT_DIALOG), parent,
			nsws_about_event_callback);
	if (ret == -1) {
		warn_user(messages_get("NoMemory"), 0);
		return;
	}
}

/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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

#define __STDBOOL_H__	1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern "C" {
#include "utils/log.h"
}
#include "beos/beos_about.h"
#include "beos/beos_scaffolding.h"
#include "beos/beos_window.h"

#include <Alert.h>
#include <ScrollView.h>
#include <String.h>
#include <TextView.h>

static const char *authors[] = {
		"John-Mark Bell", "James Bursa", "Michael Drake",
		"Rob Kendrick", "Adrian Lees", "Vincent Sanders",
		"Daniel Silverstone", "Richard Wilson",
		"\nContributors:", "Kevin Bagust", "Stefaan Claes",
		"Matthew Hambley", "Rob Jackson", "Jeffrey Lee", "Phil Mellor",
		"Philip Pemberton", "Darren Salt", "Andrew Timmins",
		"John Tytgat", "Chris Williams",
		"\nGoogle Summer of Code Contributors:", "Adam Blokus",
		"Sean Fox", "Michael Lester", "Andrew Sidwell", NULL
};

static const char *translators[] = { "Sebastian Barthel", "Bruno D'Arcangeli",
		"Gerard van Katwijk", "Jérôme Mathevet", "Simon Voortman.", NULL 
};
static const char *artists[] = {
		"Michael Drake", "\nContributors:", "Andrew Duffell",
		"John Duffell", "Richard Hallas", "Phil Mellor", NULL
};

static const char *documenters[] = {
		"John-Mark Bell", "James Bursa", "Michael Drake",
		"Richard Wilson", "\nContributors:", "James Shaw", NULL
};

static const char *name = "NetSurf";
static const char *description =
		"Small as a mouse, fast as a cheetah, and available for free.\n"
		"NetSurf is a web browser for RISC OS and UNIX-like platforms.";
static const char *url = "http://www.netsurf-browser.org/";
static const char *url_label = "NetSurf Website";
static const char *copyright =
		"Copyright © 2003 - 2008 The NetSurf Developers";

static void add_section(BTextView *textview, const char *header, 
	const char *text)
{
	BFont titleFont;
	titleFont.SetSize(titleFont.Size() + 10);
	BFont textFont;
	text_run_array titleRuns = { 1, { 0, titleFont, { 0, 0, 0, 255 } } };
	text_run_array textRuns = { 1, { 0, textFont, { 0, 0, 0, 255 } } };
	BString h(header);
	BString t(text);
	h << "\n";
	t << "\n\n";
	if (header)
		textview->Insert(h.String(), &titleRuns);
	if (text)
		textview->Insert(t.String(), &textRuns);
}

static void add_section(BTextView *textview, const char *header, 
	const char **texts)
{
	BString t;
	while (*texts) {
		t << *texts;
		t << ", ";
		texts++;
	}
	add_section(textview, header, t.String());
}

/**
 * Creates the about alert
 */
void nsbeos_about(struct gui_window *gui)
{
	BAlert *alert;
	alert = new BAlert("about", "", /*"HomePage",*/ "Ok");
	//XXX: i18n-ize
	BTextView *tv = alert->TextView();
	if (gui) {
		alert->SetFeel(B_MODAL_SUBSET_WINDOW_FEEL);
		nsbeos_scaffolding *s = nsbeos_get_scaffold(gui);
		if (s) {
			NSBrowserWindow *w = nsbeos_get_bwindow_for_scaffolding(s);
			if (w)
				alert->AddToSubset(w);
		}
	}
	tv->SetStylable(true);
	add_section(tv, name, description);
	add_section(tv, NULL, copyright);
	add_section(tv, "authors", authors);
	add_section(tv, "translators", translators);
	add_section(tv, "artists", artists);
	add_section(tv, "documenters", documenters);
	add_section(tv, url_label, url);
#if 0
	BView *p = tv->Parent();
	//tv->MakeSelectable(true);
	
	//tv->ResizeBy(-B_V_SCROLL_BAR_WIDTH, 0);
	//tv->ResizeBy(-B_V_SCROLL_BAR_WIDTH, 0);
	if (p && p->RemoveChild(tv)) {
		BScrollView *sv = new BScrollView("sv", tv, B_FOLLOW_ALL, 0, 
			false, true, B_NO_BORDER);
		p->AddChild(sv);
	}
	
	//tv->ResizeToPreferred();
#endif
	// make space for controls
	alert->ResizeBy(200, 500);
	alert->MoveTo(alert->AlertPosition(alert->Frame().Width() + 1, 
		alert->Frame().Height() + 1));

	alert->Go(NULL);
}

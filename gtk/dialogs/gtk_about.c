/*
 * Copyright 2008 Mike Lester <element3260@gmail.com>
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

#include "gtk/gtk_gui.h"
#include "gtk/dialogs/gtk_about.h"
#include "desktop/browser.h"

GtkAboutDialog* about_dialog;

static const gchar *authors[] = {
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

static const gchar *translators = "Sebastian Barthel\nBruno D'Arcangeli\n"
		"Gerard van Katwijk\nJérôme Mathevet\nSimon Voortman.";
static const gchar *artists[] = {
		"Michael Drake", "\nContributors:", "Andrew Duffell",
		"John Duffell", "Richard Hallas", "Phil Mellor", NULL
};

static const gchar *documenters[] = {
		"John-Mark Bell", "James Bursa", "Michael Drake",
		"Richard Wilson", "\nContributors:", "James Shaw", NULL
};

static const gchar *name = "NetSurf";
static const gchar *description =
		"Small as a mouse, fast as a cheetah, and available for free.\n"
		"NetSurf is a portable web browser for RISC OS, AmigaOS, BeOS "
		"and UNIX-like platforms.";
static const gchar *url = "http://www.netsurf-browser.org/";
static const gchar *url_label = "NetSurf Website";
static const gchar *copyright =
		"Copyright © 2003 - 2009 The NetSurf Developers";

static void launch_url (GtkAboutDialog *about_dialog, const gchar *url,
		gpointer data)
{
	struct browser_window *bw = data;
	browser_window_go(bw, url, 0, true);
}

void nsgtk_about_dialog_init(GtkWindow *parent, struct browser_window *bw,
		const char *version)
{
	gchar *licence_text;
  gchar *licence_location = g_strconcat(res_dir_location, "licence", NULL);
	
  g_file_get_contents(licence_location, &licence_text, NULL, NULL);
  free(licence_location);
	gtk_about_dialog_set_url_hook (launch_url, (gpointer) bw, NULL);

	gtk_show_about_dialog(parent, "artists", artists, "authors", authors,
			"comments", description,"copyright", copyright,
			"documenters", documenters, "license", licence_text,
			"program-name", name, "translator-credits", translators,
			"version", version, "website", url,
			"website-label", url_label,
			"wrap-license", FALSE, NULL);
}


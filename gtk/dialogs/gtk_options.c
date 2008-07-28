/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include "utils/log.h"
#include "desktop/options.h"
#include "gtk/options.h"
#include "gtk/gtk_gui.h"
#include "gtk/gtk_scaffolding.h"
#include "gtk/dialogs/gtk_options.h"
#include "gtk/gtk_window.h"

GtkDialog *wndPreferences;
GladeXML *gladeFile;
gboolean is_initialized = FALSE;
gchar *glade_location = "options.glade";
struct browser_window *current_browser;

char *proxy_port = NULL;
int proxy_type;
float animation_delay;

/* Declares both widget and callback */
#define DECLARE(x) GtkWidget *x; gboolean on_##x##_changed( \
	GtkWidget *widget, gpointer data)

DECLARE(entryHomePageURL);
DECLARE(setCurrentPage);
DECLARE(setDefaultPage);
DECLARE(checkHideAdverts);
DECLARE(checkDisablePopups);
DECLARE(checkDisablePlugins);
DECLARE(spinHistoryAge);
DECLARE(checkHoverURLs);
DECLARE(checkDisplayRecentURLs);
DECLARE(checkSendReferer);

DECLARE(comboProxyType);
DECLARE(entryProxyHost);
DECLARE(entryProxyPort);
DECLARE(entryProxyUser);
DECLARE(entryProxyPassword);
DECLARE(spinMaxFetchers);
DECLARE(spinFetchesPerHost);
DECLARE(spinCachedConnections);

DECLARE(checkResampleImages);
DECLARE(spinAnimationSpeed);
DECLARE(checkDisableAnimations);

DECLARE(fontSansSerif);
DECLARE(fontSerif);
DECLARE(fontMonospace);
DECLARE(fontCursive);
DECLARE(fontFantasy);
DECLARE(comboDefault);
DECLARE(spinDefaultSize);
DECLARE(spinMinimumSize);
DECLARE(fontPreview);

DECLARE(spinMemoryCacheSize);
DECLARE(spinDiscCacheAge);

DECLARE(checkClearDownloads);
DECLARE(checkRequestOverwrite);
DECLARE(fileChooserDownloads);

/* Used when the feature is not implemented yet */
#define FIND_WIDGET(x) (x) = glade_xml_get_widget(gladeFile, #x); \
	if ((x) == NULL) LOG(("Unable to find widget '%s'!", #x))
		
/* Assigns widget and connects it to its callback function */
#define CONNECT(x, y) g_signal_connect(G_OBJECT(x), y, \
	G_CALLBACK(on_##x##_changed), NULL)

GtkDialog* nsgtk_options_init(struct browser_window *bw, GtkWindow *parent) {
	glade_location = g_strconcat(res_dir_location, glade_location, NULL);
	LOG(("Using '%s' as Glade template file", glade_location));
	gladeFile = glade_xml_new(glade_location, NULL, NULL);
	
	current_browser = bw;
	wndPreferences = GTK_DIALOG(glade_xml_get_widget(gladeFile,
				"dlgPreferences"));
	gtk_window_set_transient_for (GTK_WINDOW(wndPreferences), parent);
		
	/* set the widgets to reflect the current options */
	nsgtk_options_load();
	
	/* Connect all widgets to their appropriate callbacks */
	CONNECT(entryHomePageURL, "focus-out-event");
	CONNECT(setCurrentPage, "clicked");
	CONNECT(setDefaultPage, "clicked");
	CONNECT(checkHideAdverts, "toggled");
	//CONNECT(checkDisablePopups, "toggled");
	//CONNECT(checkDisablePlugins, "toggled");
	//CONNECT(spinHistoryAge, "focus-out-event");
	//CONNECT(checkHoverURLs, "toggled");
	CONNECT(checkDisplayRecentURLs, "toggled");
	CONNECT(checkSendReferer, "toggled");

	CONNECT(comboProxyType, "changed");
	CONNECT(entryProxyHost, "focus-out-event");
	CONNECT(entryProxyPort, "focus-out-event");
	CONNECT(entryProxyUser, "focus-out-event");
	CONNECT(entryProxyPassword, "focus-out-event");
	CONNECT(spinMaxFetchers, "value-changed");
	CONNECT(spinFetchesPerHost, "value-changed");
	CONNECT(spinCachedConnections, "value-changed");

	CONNECT(checkResampleImages, "toggled");
	CONNECT(spinAnimationSpeed, "value-changed");
	CONNECT(checkDisableAnimations, "toggled");

	CONNECT(fontSansSerif, "font-set");
	CONNECT(fontSerif, "font-set");
	CONNECT(fontMonospace, "font-set");
	CONNECT(fontCursive, "font-set");
	CONNECT(fontFantasy, "font-set");
	CONNECT(comboDefault, "changed");
	CONNECT(spinDefaultSize, "value-changed");
	CONNECT(spinMinimumSize, "value-changed");
	CONNECT(fontPreview, "clicked");

	CONNECT(spinMemoryCacheSize, "value-changed");
	CONNECT(spinDiscCacheAge, "value-changed");
	
	CONNECT(checkClearDownloads, "toggled");
	CONNECT(checkRequestOverwrite, "toggled");
	CONNECT(fileChooserDownloads, "current-folder-changed");
	
	g_signal_connect(G_OBJECT(wndPreferences), "response",
		G_CALLBACK (dialog_response_handler), NULL);
	
	g_signal_connect(G_OBJECT(wndPreferences), "delete-event",
		G_CALLBACK (on_dialog_close), (gpointer)TRUE);
	
	g_signal_connect(G_OBJECT(wndPreferences), "destroy",
		G_CALLBACK (on_dialog_close), (gpointer)FALSE);
				
	gtk_widget_show(GTK_WIDGET(wndPreferences));
	
	return wndPreferences;
}

#define SET_ENTRY(x, y) (x) = glade_xml_get_widget(gladeFile, #x); \
	gtk_entry_set_text(GTK_ENTRY((x)), (y))
#define SET_SPIN(x, y) (x) = glade_xml_get_widget(gladeFile, #x); \
	gtk_spin_button_set_value(GTK_SPIN_BUTTON((x)), (y))
#define SET_CHECK(x, y) (x) = glade_xml_get_widget(gladeFile, #x); \
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON((x)), (y))
#define SET_COMBO(x, y) (x) = glade_xml_get_widget(gladeFile, #x); \
	gtk_combo_box_set_active(GTK_COMBO_BOX((x)), (y))
#define SET_FONT(x, y) (x) = glade_xml_get_widget(gladeFile, #x); \
	gtk_font_button_set_font_name(GTK_FONT_BUTTON((x)), (y))
#define SET_FILE_CHOOSER(x, y) (x) = glade_xml_get_widget(gladeFile, #x); \
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER((x)), (y))
#define SET_BUTTON(x) (x) = glade_xml_get_widget(gladeFile, #x);


void nsgtk_options_load(void) {
	char b[20];
	int proxytype = 0;

	SET_ENTRY(entryHomePageURL,
			option_homepage_url ? option_homepage_url : "");
	SET_BUTTON(setCurrentPage);
	SET_BUTTON(setDefaultPage);
	SET_CHECK(checkHideAdverts, option_block_ads);
	SET_CHECK(checkDisplayRecentURLs, option_url_suggestion);
	SET_CHECK(checkSendReferer, option_send_referer);
	
	if (option_http_proxy == false)
		proxytype = 0;
	else
		proxytype = option_http_proxy_auth + 1;

	SET_COMBO(comboProxyType, proxytype);
	SET_ENTRY(entryProxyHost,
			option_http_proxy_host ? option_http_proxy_host : "");
			gtk_widget_set_sensitive (entryProxyHost, !proxytype == 0);
	snprintf(b, 20, "%d", option_http_proxy_port);	
	SET_ENTRY(entryProxyPort, b);
			gtk_widget_set_sensitive (entryProxyPort, !proxytype == 0);
	SET_ENTRY(entryProxyUser, option_http_proxy_auth_user ?
			option_http_proxy_auth_user : "");
			gtk_widget_set_sensitive (entryProxyUser, !proxytype == 0);
	SET_ENTRY(entryProxyPassword, option_http_proxy_auth_pass ?
			option_http_proxy_auth_pass : "");
			gtk_widget_set_sensitive (entryProxyPassword, !proxytype == 0);

	SET_SPIN(spinMaxFetchers, option_max_fetchers);
	SET_SPIN(spinFetchesPerHost, option_max_fetchers_per_host);
	SET_SPIN(spinCachedConnections, option_max_cached_fetch_handles);

	SET_CHECK(checkResampleImages, option_render_resample);
	SET_SPIN(spinAnimationSpeed, option_minimum_gif_delay / 100.0);
	SET_CHECK(checkDisableAnimations, !option_animate_images);

	SET_FONT(fontSansSerif, option_font_sans);
	SET_FONT(fontSerif, option_font_serif);
	SET_FONT(fontMonospace, option_font_mono);
	SET_FONT(fontCursive, option_font_cursive);
	SET_FONT(fontFantasy, option_font_fantasy);
	SET_COMBO(comboDefault, option_font_default - 1);
	SET_SPIN(spinDefaultSize, option_font_size / 10);
	SET_SPIN(spinMinimumSize, option_font_min_size / 10);
	SET_BUTTON(fontPreview);

	SET_SPIN(spinMemoryCacheSize, option_memory_cache_size >> 20);
	SET_SPIN(spinDiscCacheAge, option_disc_cache_age);
	
	SET_CHECK(checkClearDownloads, option_downloads_clear);
	SET_CHECK(checkRequestOverwrite, option_request_overwrite);
	SET_FILE_CHOOSER(fileChooserDownloads, option_downloads_directory);
}

static void dialog_response_handler (GtkDialog *dlg, gint res_id){
	switch (res_id)
	{
		case GTK_RESPONSE_HELP:
			/* Ready to implement Help */
			break;

		case GTK_RESPONSE_CLOSE:
			on_dialog_close(dlg, TRUE);
	}
}

static gboolean on_dialog_close (GtkDialog *dlg, gboolean stay_alive){
	LOG(("Writing options to file"));
	options_write(options_file_location);
	if (stay_alive) gtk_widget_hide (GTK_WIDGET(wndPreferences));
	return stay_alive;
}
	

/* Defines the callback functions for all widgets and specifies
 * nsgtk_reflow_all_windows only where necessary */
#define ENTRY_CHANGED(x, y) gboolean on_##x##_changed(GtkWidget *widget, gpointer data) { \
		if (!g_str_equal(gtk_entry_get_text(GTK_ENTRY((x))), (y))) { \
			LOG(("Signal emitted on '%s'", #x)); \
			if ((y)) free((y)); \
			(y) = strdup(gtk_entry_get_text(GTK_ENTRY((x)))); 
#define CHECK_CHANGED(x, y) gboolean on_##x##_changed(GtkWidget *widget, gpointer data) { \
	LOG(("Signal emitted on '%s'", #x)); \
	(y) = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON((x)));
#define SPIN_CHANGED(x, y) gboolean on_##x##_changed(GtkWidget *widget, gpointer data) { \
		LOG(("Signal emitted on '%s'", #x)); \
		(y) = gtk_spin_button_get_value(GTK_SPIN_BUTTON((x)));
#define COMBO_CHANGED(x, y) gboolean on_##x##_changed(GtkWidget *widget, gpointer data) { \
	LOG(("Signal emitted on '%s'", #x)); \
	(y) = gtk_combo_box_get_active(GTK_COMBO_BOX((x)));
#define FONT_CHANGED(x, y) gboolean on_##x##_changed(GtkWidget *widget, gpointer data) { \
	LOG(("Signal emitted on '%s'", #x)); \
	if ((y)) free((y)); \
	(y) = strdup(gtk_font_button_get_font_name(GTK_FONT_BUTTON((x))));
#define FILE_CHOOSER_CHANGED(x, y) gboolean on_##x##_changed(GtkWidget *widget, gpointer data) { \
	LOG(("Signal emitted on '%s'", #x)); \
	(y) = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER((x)));
#define BUTTON_CLICKED(x) gboolean on_##x##_changed(GtkWidget *widget, gpointer data) { \
	LOG(("Signal emitted on '%s'", #x));

ENTRY_CHANGED(entryHomePageURL, option_homepage_url)}
	return FALSE;}
BUTTON_CLICKED(setCurrentPage)
	const gchar *url = current_browser->current_content->url;
	gtk_entry_set_text(GTK_ENTRY(entryHomePageURL), url);
	option_homepage_url = 
		strdup(gtk_entry_get_text(GTK_ENTRY(entryHomePageURL)));}
BUTTON_CLICKED(setDefaultPage)
	gtk_entry_set_text(GTK_ENTRY(entryHomePageURL),
	 				   "http://www.netsurf-browser.org/welcome/");
	option_homepage_url = 
		strdup(gtk_entry_get_text(GTK_ENTRY(entryHomePageURL)));}
CHECK_CHANGED(checkHideAdverts, option_block_ads)}
CHECK_CHANGED(checkDisplayRecentURLs, option_url_suggestion)}
CHECK_CHANGED(checkSendReferer, option_send_referer)}

COMBO_CHANGED(comboProxyType, proxy_type)
	LOG(("proxy type: %d", proxy_type));
	switch (proxy_type)
	{
		case 0:
			option_http_proxy = false;
			option_http_proxy_auth = OPTION_HTTP_PROXY_AUTH_NONE;
			break;
		case 1:
			option_http_proxy = true;
			option_http_proxy_auth = OPTION_HTTP_PROXY_AUTH_NONE;
			break;
		case 2:
			option_http_proxy = true;
			option_http_proxy_auth = OPTION_HTTP_PROXY_AUTH_BASIC;
			break;
		case 3:
			option_http_proxy = true;
			option_http_proxy_auth = OPTION_HTTP_PROXY_AUTH_NTLM;
			break;
	}
	gboolean sensitive = (option_http_proxy_auth);
	gtk_widget_set_sensitive (entryProxyHost, sensitive);
	gtk_widget_set_sensitive (entryProxyPort, sensitive);
	gtk_widget_set_sensitive (entryProxyUser, sensitive);
	gtk_widget_set_sensitive (entryProxyPassword, sensitive);
}

ENTRY_CHANGED(entryProxyHost, option_http_proxy_host)}
	return FALSE;}

gboolean on_entryProxyPort_changed(GtkWidget *widget, gpointer data)
{
	long port;

	errno = 0;
	port = strtol((char *)gtk_entry_get_text(GTK_ENTRY(entryProxyPort)),
			NULL, 10) & 0xffff;
	if (port != 0 && errno == 0) {
		option_http_proxy_port = port;
	} else {
		char buf[32];
		snprintf(buf, 31, "%d", option_http_proxy_port);
		SET_ENTRY(entryProxyPort, buf);
	}

	return FALSE;
}

ENTRY_CHANGED(entryProxyUser, option_http_proxy_auth_user)}
	return FALSE;}
ENTRY_CHANGED(entryProxyPassword, option_http_proxy_auth_pass)}
	return FALSE;}

SPIN_CHANGED(spinMaxFetchers, option_max_fetchers)}
SPIN_CHANGED(spinFetchesPerHost, option_max_fetchers_per_host)}
SPIN_CHANGED(spinCachedConnections, option_max_cached_fetch_handles)}

CHECK_CHANGED(checkResampleImages, option_render_resample)}
SPIN_CHANGED(spinAnimationSpeed, animation_delay)
	option_minimum_gif_delay = round(animation_delay * 100.0);}
CHECK_CHANGED(checkDisableAnimations, option_animate_images);
	option_animate_images = !option_animate_images;}

FONT_CHANGED(fontSansSerif, option_font_sans)}
FONT_CHANGED(fontSerif, option_font_serif)}
FONT_CHANGED(fontMonospace, option_font_mono)}
FONT_CHANGED(fontCursive, option_font_cursive)}
FONT_CHANGED(fontFantasy, option_font_fantasy)}
COMBO_CHANGED(comboDefault, option_font_default)
	option_font_default++;}

SPIN_CHANGED(spinDefaultSize, option_font_size)
	option_font_size *= 10;}
SPIN_CHANGED(spinMinimumSize, option_font_min_size)
	option_font_min_size *= 10;}
BUTTON_CLICKED(fontPreview)
	nsgtk_reflow_all_windows();}

SPIN_CHANGED(spinMemoryCacheSize, option_memory_cache_size)
	option_memory_cache_size <<= 20;}
SPIN_CHANGED(spinDiscCacheAge, option_disc_cache_age)}

CHECK_CHANGED(checkClearDownloads, option_downloads_clear)}
CHECK_CHANGED(checkRequestOverwrite, option_request_overwrite)}
FILE_CHOOSER_CHANGED(fileChooserDownloads, option_downloads_directory)}

/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 * Copyright 2008 Mike Lester <element3260@gmail.com>
 * Copyright 2009 Daniel Silverstone <dsilvers@netsurf-browser.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <gtk/gtk.h>

#include "desktop/options.h"
#include "desktop/print.h"
#include "desktop/searchweb.h"

#include "gtk/compat.h"
#include "gtk/gui.h"
#include "gtk/scaffolding.h"
#include "gtk/theme.h"
#include "gtk/dialogs/options.h"
#include "gtk/window.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "utils/messages.h"

GtkDialog *wndPreferences = NULL;
static GtkBuilder *gladeFile;

static struct browser_window *current_browser;

static int proxy_type;

static void dialog_response_handler (GtkDialog *dlg, gint res_id);
static gboolean on_dialog_close (GtkDialog *dlg, gboolean stay_alive);
static void nsgtk_options_theme_combo(void);

/* Declares both widget and callback */
#define DECLARE(x) \
  static GtkWidget *x; \
  static gboolean on_##x##_changed(GtkWidget *widget, gpointer data)

DECLARE(entryHomePageURL);
DECLARE(setCurrentPage);
DECLARE(setDefaultPage);
DECLARE(checkHideAdverts);
DECLARE(checkDisablePopups);
DECLARE(checkDisablePlugins);
DECLARE(spinHistoryAge);
DECLARE(checkHoverURLs);
DECLARE(checkDisplayRecentURLs);
DECLARE(comboLanguage);
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

DECLARE(comboButtonType);

DECLARE(spinMemoryCacheSize);
DECLARE(spinDiscCacheAge);

DECLARE(checkClearDownloads);
DECLARE(checkRequestOverwrite);
DECLARE(fileChooserDownloads);
/* Tabs */
DECLARE(checkShowSingleTab);
DECLARE(checkFocusNew);
DECLARE(checkNewBlank);
DECLARE(comboTabPosition);

DECLARE(checkUrlSearch);
DECLARE(comboSearch);
DECLARE(combotheme);
DECLARE(buttonaddtheme);
DECLARE(sourceButtonTab);
static GtkWidget *sourceButtonWindow;

DECLARE(spinMarginTop);
DECLARE(spinMarginBottom);
DECLARE(spinMarginLeft);
DECLARE(spinMarginRight);
DECLARE(spinExportScale);
DECLARE(checkSuppressImages);
DECLARE(checkRemoveBackgrounds);
DECLARE(checkFitPage);
DECLARE(checkCompressPDF);
DECLARE(checkPasswordPDF);
DECLARE(setDefaultExportOptions);

/* Used when the feature is not implemented yet */
#define FIND_WIDGET(wname)                                              \
        do {                                                            \
		(wname) = GTK_WIDGET(gtk_builder_get_object(gladeFile, #wname)); \
                if ((wname) == NULL)                                    \
                        LOG(("Unable to find widget '%s'!", #wname));   \
        } while (0)

/* Assigns widget and connects it to its callback function */
#define CONNECT(wname, event)                                           \
        g_signal_connect(G_OBJECT(wname), event,                        \
                         G_CALLBACK(on_##wname##_changed), NULL)

GtkDialog* nsgtk_options_init(struct browser_window *bw, GtkWindow *parent)
{
	GError* error = NULL;
	gladeFile = gtk_builder_new();
	if (!gtk_builder_add_from_file(gladeFile, glade_file_location->options, &error)) {
		g_warning("Couldn't load builder file: %s", error->message);
		g_error_free(error);
		return NULL;
	}
	
	current_browser = bw;
	wndPreferences = GTK_DIALOG(gtk_builder_get_object(gladeFile, "dlgPreferences"));
	gtk_window_set_transient_for(GTK_WINDOW(wndPreferences), parent);
	
	FIND_WIDGET(sourceButtonTab);
	FIND_WIDGET(sourceButtonWindow);
	GSList *group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(
			sourceButtonWindow));
	gtk_radio_button_set_group(GTK_RADIO_BUTTON(sourceButtonTab), group);
	
	/* set the widgets to reflect the current options */
	nsgtk_options_load();
	
	/* Connect all widgets to their appropriate callbacks */
	CONNECT(entryHomePageURL, "focus-out-event");
	CONNECT(setCurrentPage, "clicked");
	CONNECT(setDefaultPage, "clicked");
	CONNECT(checkHideAdverts, "toggled");

	CONNECT(checkDisablePopups, "toggled");
	CONNECT(checkDisablePlugins, "toggled");
	CONNECT(spinHistoryAge, "focus-out-event");
	CONNECT(checkHoverURLs, "toggled");
	
	CONNECT(comboLanguage, "changed");
	
	CONNECT(checkDisplayRecentURLs, "toggled");
	CONNECT(checkSendReferer, "toggled");
	CONNECT(checkShowSingleTab, "toggled");

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
	
	CONNECT(comboButtonType, "changed");

	CONNECT(comboTabPosition, "changed");

	CONNECT(spinMemoryCacheSize, "value-changed");
	CONNECT(spinDiscCacheAge, "value-changed");
	
	CONNECT(checkClearDownloads, "toggled");
	CONNECT(checkRequestOverwrite, "toggled");
	CONNECT(fileChooserDownloads, "current-folder-changed");
	
	CONNECT(checkFocusNew, "toggled");
	CONNECT(checkNewBlank, "toggled");
	CONNECT(checkUrlSearch, "toggled");
	CONNECT(comboSearch, "changed");
	
	CONNECT(combotheme, "changed");
	CONNECT(buttonaddtheme, "clicked");
	CONNECT(sourceButtonTab, "toggled");
	
	CONNECT(spinMarginTop, "value-changed");
	CONNECT(spinMarginBottom, "value-changed");
	CONNECT(spinMarginLeft, "value-changed");
	CONNECT(spinMarginRight, "value-changed");
	CONNECT(spinExportScale, "value-changed");
	CONNECT(checkSuppressImages, "toggled");
	CONNECT(checkRemoveBackgrounds, "toggled");
	CONNECT(checkFitPage, "toggled");
	CONNECT(checkCompressPDF, "toggled");
	CONNECT(checkPasswordPDF, "toggled");
	CONNECT(setDefaultExportOptions, "clicked");
		
	g_signal_connect(G_OBJECT(wndPreferences), "response",
		G_CALLBACK (dialog_response_handler), NULL);
	
	g_signal_connect(G_OBJECT(wndPreferences), "delete-event",
		G_CALLBACK (on_dialog_close), (gpointer)TRUE);
	
	g_signal_connect(G_OBJECT(wndPreferences), "destroy",
		G_CALLBACK (on_dialog_close), (gpointer)FALSE);
				
	gtk_widget_show(GTK_WIDGET(wndPreferences));
	
	return wndPreferences;
}

#define SET_ENTRY(widget, value)                                        \
        do {                                                            \
		(widget) = GTK_WIDGET(gtk_builder_get_object(gladeFile, #widget)); \
                gtk_entry_set_text(GTK_ENTRY((widget)), (value));       \
        } while (0)

#define SET_SPIN(widget, value)                                         \
        do {                                                            \
                (widget) = GTK_WIDGET(gtk_builder_get_object(gladeFile, #widget)); \
                gtk_spin_button_set_value(GTK_SPIN_BUTTON((widget)), (value)); \
        } while (0)

#define SET_CHECK(widget, value)                                        \
        do {                                                            \
                (widget) = GTK_WIDGET(gtk_builder_get_object(gladeFile, #widget)); \
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON((widget)), \
                                             (value));                  \
        } while (0)

#define SET_COMBO(widget, value)                                        \
        do {                                                            \
                (widget) = GTK_WIDGET(gtk_builder_get_object(gladeFile, #widget)); \
                gtk_combo_box_set_active(GTK_COMBO_BOX((widget)), (value)); \
        } while (0)

#define SET_FONT(widget, value)                                         \
        do {                                                            \
                (widget) = GTK_WIDGET(gtk_builder_get_object(gladeFile, #widget)); \
                gtk_font_button_set_font_name(GTK_FONT_BUTTON((widget)), \
                                              (value));                 \
        } while (0)

#define SET_FILE_CHOOSER(widget, value)                                  \
        do {                                                            \
                (widget) = GTK_WIDGET(gtk_builder_get_object(gladeFile, #widget)); \
                gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(\
                		(widget)), (value));			\
        } while (0)

#define SET_BUTTON(widget)                                              \
        do {                                                            \
                (widget) = GTK_WIDGET(gtk_builder_get_object(gladeFile, #widget)); \
        } while (0)


void nsgtk_options_load(void) 
{
	GtkBox *box;
	const char *default_accept_language = "en";
	const char *default_homepage_url = "";
	const char *default_http_proxy_host = "";
	const char *default_http_proxy_auth_user = "";
	const char *default_http_proxy_auth_pass = "";
	int combo_row_count = 0;
	int active_language = 0;
	int proxytype = 0;
	FILE *fp;
	char buf[50];

	/* get widget text */
	if (nsoption_charp(accept_language) != NULL) {
		default_accept_language = nsoption_charp(accept_language);
	}

	if (nsoption_charp(homepage_url) != NULL) {
		default_homepage_url = nsoption_charp(homepage_url);
	}

	if (nsoption_charp(http_proxy_host) != NULL) {
		default_http_proxy_host = nsoption_charp(http_proxy_host);
	}

	if (nsoption_charp(http_proxy_auth_user) != NULL) {
		default_http_proxy_auth_user = nsoption_charp(http_proxy_auth_user);
	}

	if (nsoption_charp(http_proxy_auth_pass) != NULL) {
		default_http_proxy_auth_pass = nsoption_charp(http_proxy_auth_pass);
	}

	if (nsoption_bool(http_proxy) == true) {
		proxytype = nsoption_int(http_proxy_auth) + 1;
	}

	/* Create combobox */
	box = GTK_BOX(gtk_builder_get_object(gladeFile, "combolanguagevbox"));
	comboLanguage = nsgtk_combo_box_text_new();

	/* Populate combobox from languages file */
	if ((languages_file_location != NULL) && 
	    ((fp = fopen(languages_file_location, "r")) != NULL)) {
		LOG(("Used %s for languages", languages_file_location));
		while (fgets(buf, sizeof(buf), fp)) {
			/* Ignore blank lines */
			if (buf[0] == '\0')
				continue;

			/* Remove trailing \n */
			buf[strlen(buf) - 1] = '\0';

			nsgtk_combo_box_text_append_text(comboLanguage, buf);

			if (strcmp(buf, default_accept_language) == 0)
				active_language = combo_row_count;

			combo_row_count++;
		}

		fclose(fp);
	} else {
		LOG(("Failed opening languages file"));
		warn_user("FileError", languages_file_location);
		nsgtk_combo_box_text_append_text(comboLanguage, "en");
	}


	gtk_combo_box_set_active(GTK_COMBO_BOX(comboLanguage), active_language);
	/** \todo localisation */
	gtk_widget_set_tooltip_text(GTK_WIDGET(comboLanguage), 
			"set preferred language for web pages");
	gtk_box_pack_start(box, comboLanguage, FALSE, FALSE, 0);
	gtk_widget_show(comboLanguage);
	
	nsgtk_options_theme_combo();
	
	SET_ENTRY(entryHomePageURL, default_homepage_url);
	SET_BUTTON(setCurrentPage);
	SET_BUTTON(setDefaultPage);
	SET_CHECK(checkHideAdverts, nsoption_bool(block_ads));
	
	SET_CHECK(checkDisablePopups, nsoption_bool(disable_popups));
	SET_CHECK(checkDisablePlugins, nsoption_bool(disable_plugins));
	SET_SPIN(spinHistoryAge, nsoption_int(history_age));
	SET_CHECK(checkHoverURLs, nsoption_bool(hover_urls));
	
	SET_CHECK(checkDisplayRecentURLs, nsoption_bool(url_suggestion));
	SET_CHECK(checkSendReferer, nsoption_bool(send_referer));
        SET_CHECK(checkShowSingleTab, nsoption_bool(show_single_tab));
	
	SET_COMBO(comboProxyType, proxytype);
	SET_ENTRY(entryProxyHost, default_http_proxy_host);

	gtk_widget_set_sensitive(entryProxyHost, proxytype != 0);

	snprintf(buf, sizeof(buf), "%d", nsoption_int(http_proxy_port));	

	SET_ENTRY(entryProxyPort, buf);
	gtk_widget_set_sensitive(entryProxyPort, proxytype != 0);

	SET_ENTRY(entryProxyUser, default_http_proxy_auth_user);

	gtk_widget_set_sensitive(entryProxyUser, proxytype != 0);

	SET_ENTRY(entryProxyPassword, default_http_proxy_auth_pass);

	gtk_widget_set_sensitive(entryProxyPassword, proxytype != 0);

	SET_SPIN(spinMaxFetchers, nsoption_int(max_fetchers));
	SET_SPIN(spinFetchesPerHost, nsoption_int(max_fetchers_per_host));
	SET_SPIN(spinCachedConnections, nsoption_int(max_cached_fetch_handles));

	SET_CHECK(checkResampleImages, nsoption_bool(render_resample));
	SET_SPIN(spinAnimationSpeed, nsoption_int(minimum_gif_delay) / 100.0);
	SET_CHECK(checkDisableAnimations, !nsoption_bool(animate_images));

	SET_FONT(fontSansSerif, nsoption_charp(font_sans));
	SET_FONT(fontSerif, nsoption_charp(font_serif));
	SET_FONT(fontMonospace, nsoption_charp(font_mono));
	SET_FONT(fontCursive, nsoption_charp(font_cursive));
	SET_FONT(fontFantasy, nsoption_charp(font_fantasy));
	SET_COMBO(comboDefault, nsoption_int(font_default));
	SET_SPIN(spinDefaultSize, nsoption_int(font_size) / 10);
	SET_SPIN(spinMinimumSize, nsoption_bool(font_min_size) / 10);
	SET_BUTTON(fontPreview);
	
	SET_COMBO(comboButtonType, nsoption_int(button_type) -1);

	SET_COMBO(comboTabPosition, nsoption_int(position_tab));

	SET_SPIN(spinMemoryCacheSize, nsoption_int(memory_cache_size) >> 20);
	SET_SPIN(spinDiscCacheAge, nsoption_int(disc_cache_age));
	
	SET_CHECK(checkClearDownloads, nsoption_bool(downloads_clear));
	SET_CHECK(checkRequestOverwrite, nsoption_bool(request_overwrite));
	SET_FILE_CHOOSER(fileChooserDownloads, nsoption_charp(downloads_directory));
	
	SET_CHECK(checkFocusNew, nsoption_bool(focus_new));
	SET_CHECK(checkNewBlank, nsoption_bool(new_blank));
	SET_CHECK(checkUrlSearch, nsoption_bool(search_url_bar));
	SET_COMBO(comboSearch, nsoption_int(search_provider));
	
	SET_BUTTON(buttonaddtheme);
	SET_CHECK(sourceButtonTab, nsoption_bool(source_tab));
		
	SET_SPIN(spinMarginTop, nsoption_int(margin_top));
	SET_SPIN(spinMarginBottom, nsoption_int(margin_bottom));
	SET_SPIN(spinMarginLeft, nsoption_int(margin_left));
	SET_SPIN(spinMarginRight, nsoption_int(margin_right));
	SET_SPIN(spinExportScale, nsoption_int(export_scale));
	SET_CHECK(checkSuppressImages, nsoption_bool(suppress_images));
	SET_CHECK(checkRemoveBackgrounds, nsoption_bool(remove_backgrounds));
	SET_CHECK(checkFitPage, nsoption_bool(enable_loosening));
	SET_CHECK(checkCompressPDF, nsoption_bool(enable_PDF_compression));
	SET_CHECK(checkPasswordPDF, nsoption_bool(enable_PDF_password));
	SET_BUTTON(setDefaultExportOptions);
}

static void dialog_response_handler (GtkDialog *dlg, gint res_id)
{
	switch (res_id)	{
	case GTK_RESPONSE_HELP:
		/* Ready to implement Help */
		break;

	case GTK_RESPONSE_CLOSE:
		on_dialog_close(dlg, TRUE);
	}
}

static gboolean on_dialog_close (GtkDialog *dlg, gboolean stay_alive)
{
	LOG(("Writing options to file"));
	nsoption_write(options_file_location);
	if ((stay_alive) && GTK_IS_WIDGET(dlg))
		gtk_widget_hide(GTK_WIDGET(dlg));
	else {
		stay_alive = FALSE;
	}
	return stay_alive;
}

static void nsgtk_options_theme_combo(void) {
/* populate theme combo from themelist file */
	GtkBox *box = GTK_BOX(gtk_builder_get_object(gladeFile, "themehbox"));
	char buf[50];
	size_t len = SLEN("themelist") + strlen(res_dir_location) + 1;
	char themefile[len];

	combotheme = nsgtk_combo_box_text_new();

	if ((combotheme == NULL) || (box == NULL)) {
		warn_user(messages_get("NoMemory"), 0);
		return;
	}
	snprintf(themefile, len, "%sthemelist", res_dir_location);
	FILE *fp = fopen((const char *)themefile, "r");
	if (fp == NULL) {
		LOG(("Failed opening themes file"));
		warn_user("FileError", (const char *) themefile);
		return;
	}
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		/* Ignore blank lines */
		if (buf[0] == '\0')
			continue;
		
		/* Remove trailing \n */
		buf[strlen(buf) - 1] = '\0';
		
		nsgtk_combo_box_text_append_text(combotheme, buf);
	}
	fclose(fp);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combotheme), 
				 nsoption_int(current_theme));
	gtk_box_pack_start(box, combotheme, FALSE, TRUE, 0);
	gtk_widget_show(combotheme);		
}

bool nsgtk_options_combo_theme_add(const char *themename)
{
	if (wndPreferences == NULL)
		return false;
	nsgtk_combo_box_text_append_text(combotheme, themename);
	return true;
}
	

/* Defines the callback functions for all widgets and specifies
 * nsgtk_reflow_all_windows only where necessary */

#define ENTRY_CHANGED(widget, option)                                   \
static gboolean on_##widget##_changed(GtkWidget *widget, gpointer data) \
{									\
        if (!g_str_equal(gtk_entry_get_text(GTK_ENTRY((widget))),	\
			 nsoption_charp(option) ? nsoption_charp(option) : "")) { \
                LOG(("Signal emitted on '%s'", #widget));               \
		nsoption_set_charp(option, strdup(gtk_entry_get_text(GTK_ENTRY((widget))))); \
	}								\
        return FALSE;							\
}
                
#define CHECK_CHANGED(widget, option)                                   \
        static gboolean on_##widget##_changed(GtkWidget *widget, gpointer data) { \
	LOG(("Signal emitted on '%s'", #widget));                       \
	nsoption_set_bool(option, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON((widget)))); \
        do 

#define SPIN_CHANGED(widget, option)                                    \
        static gboolean on_##widget##_changed(GtkWidget *widget, gpointer data) { \
        LOG(("Signal emitted on '%s'", #widget));                       \
        nsoption_set_int(option, gtk_spin_button_get_value(GTK_SPIN_BUTTON((widget)))); \
        do

#define COMBO_CHANGED(widget, option)                                   \
        static gboolean on_##widget##_changed(GtkWidget *widget, gpointer data) { \
	LOG(("Signal emitted on '%s'", #widget));                       \
	nsoption_set_int(option, gtk_combo_box_get_active(GTK_COMBO_BOX((widget)))); \
        do 

#define FONT_CHANGED(widget, option)                                    \
        static gboolean on_##widget##_changed(GtkWidget *widget, gpointer data) { \
	LOG(("Signal emitted on '%s'", #widget));                       \
	nsoption_set_charp(option, strdup(gtk_font_button_get_font_name(GTK_FONT_BUTTON((widget))))); \
        do 

#define BUTTON_CLICKED(widget)                                          \
        static gboolean on_##widget##_changed(GtkWidget *widget, gpointer data) { \
	LOG(("Signal emitted on '%s'", #widget));                       \
        do 

#define END_HANDLER                             \
        while (0);                            \
        return FALSE;                           \
        }

static gboolean on_comboLanguage_changed(GtkWidget *widget, gpointer data)
{
	gchar *lang; 

	lang = nsgtk_combo_box_text_get_active_text(comboLanguage);
	if (lang == NULL)
		return FALSE;

	nsoption_set_charp(accept_language, strdup(lang));

	g_free(lang);
	
	return FALSE;
}

ENTRY_CHANGED(entryHomePageURL, homepage_url)

BUTTON_CLICKED(setCurrentPage)
{
	const gchar *url;
	url = nsurl_access(hlcache_handle_get_url(current_browser->current_content));
	gtk_entry_set_text(GTK_ENTRY(entryHomePageURL), url);
	nsoption_set_charp(homepage_url, 
			   strdup(gtk_entry_get_text(GTK_ENTRY(entryHomePageURL))));
}
END_HANDLER

BUTTON_CLICKED(setDefaultPage)
{
	gtk_entry_set_text(GTK_ENTRY(entryHomePageURL), NETSURF_HOMEPAGE);
	nsoption_set_charp(homepage_url, 
			   strdup(gtk_entry_get_text(GTK_ENTRY(entryHomePageURL))));
}
END_HANDLER

CHECK_CHANGED(checkHideAdverts, block_ads)
{
}
END_HANDLER

CHECK_CHANGED(checkDisplayRecentURLs, url_suggestion)
{
}
END_HANDLER

CHECK_CHANGED(checkSendReferer, send_referer)
{
}
END_HANDLER
                
CHECK_CHANGED(checkShowSingleTab, show_single_tab)
{
	nsgtk_reflow_all_windows();
}
END_HANDLER

COMBO_CHANGED(comboProxyType, http_proxy_auth)
{
	LOG(("proxy auth: %d", nsoption_int(http_proxy_auth)));
	switch (nsoption_int(http_proxy_auth)) {
	case 0:
		nsoption_set_bool(http_proxy, false);
		nsoption_set_int(http_proxy_auth, OPTION_HTTP_PROXY_AUTH_NONE);
		break;
	case 1:
		nsoption_set_bool(http_proxy, true);
		nsoption_set_int(http_proxy_auth, OPTION_HTTP_PROXY_AUTH_NONE);
		break;
	case 2:
		nsoption_set_bool(http_proxy, true);
		nsoption_set_int(http_proxy_auth, OPTION_HTTP_PROXY_AUTH_BASIC);
		break;
	case 3:
		nsoption_set_bool(http_proxy, true);
		nsoption_set_int(http_proxy_auth, OPTION_HTTP_PROXY_AUTH_NTLM);
		break;
	}
	gboolean sensitive = (!proxy_type == 0);
	gtk_widget_set_sensitive(entryProxyHost, sensitive);
	gtk_widget_set_sensitive(entryProxyPort, sensitive);
	gtk_widget_set_sensitive(entryProxyUser, sensitive);
	gtk_widget_set_sensitive(entryProxyPassword, sensitive);
}	
END_HANDLER

ENTRY_CHANGED(entryProxyHost, http_proxy_host)

gboolean on_entryProxyPort_changed(GtkWidget *widget, gpointer data)
{
	long port;

	errno = 0;
	port = strtol((char *)gtk_entry_get_text(GTK_ENTRY(entryProxyPort)),
			NULL, 10) & 0xffff;
	if ((port != 0) && (errno == 0)) {
		nsoption_set_int(http_proxy_port, port);
	} else {
		char buf[32];
		snprintf(buf, sizeof(buf), "%d", nsoption_int(http_proxy_port));
		SET_ENTRY(entryProxyPort, buf);
	}

	return FALSE;
}

ENTRY_CHANGED(entryProxyUser, http_proxy_auth_user)

ENTRY_CHANGED(entryProxyPassword, http_proxy_auth_pass)

SPIN_CHANGED(spinMaxFetchers, max_fetchers)
{
}
END_HANDLER

SPIN_CHANGED(spinFetchesPerHost, max_fetchers_per_host)
{
}
END_HANDLER

SPIN_CHANGED(spinCachedConnections, max_cached_fetch_handles)
{
}
END_HANDLER

CHECK_CHANGED(checkResampleImages, render_resample)
{
}
END_HANDLER

static gboolean on_spinAnimationSpeed_changed(GtkWidget *widget, gpointer data)
{ 
        LOG(("Signal emitted on '%s'", "spinAnimationSpeed"));
	nsoption_set_int(minimum_gif_delay, 
			 round(gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)) * 100.0));
	return FALSE;
}

CHECK_CHANGED(checkDisableAnimations, animate_images)
{
	nsoption_set_bool(animate_images, !nsoption_bool(animate_images));
}
END_HANDLER

CHECK_CHANGED(checkDisablePopups, disable_popups)
{
}
END_HANDLER

CHECK_CHANGED(checkDisablePlugins, disable_plugins)
{
}
END_HANDLER

SPIN_CHANGED(spinHistoryAge, history_age)
{
}
END_HANDLER

CHECK_CHANGED(checkHoverURLs, hover_urls)
{
}
END_HANDLER

FONT_CHANGED(fontSansSerif, font_sans)
{
}
END_HANDLER

FONT_CHANGED(fontSerif, font_serif)
{
}
END_HANDLER

FONT_CHANGED(fontMonospace, font_mono)
{
}
END_HANDLER

FONT_CHANGED(fontCursive, font_cursive)
{
}
END_HANDLER

FONT_CHANGED(fontFantasy, font_fantasy)
{
}
END_HANDLER

COMBO_CHANGED(comboDefault, font_default)
{
}
END_HANDLER

SPIN_CHANGED(spinDefaultSize, font_size)
{
	nsoption_set_int(font_size, nsoption_int(font_size) * 10);
}
END_HANDLER

SPIN_CHANGED(spinMinimumSize, font_min_size)
{
	nsoption_set_int(font_min_size, nsoption_int(font_min_size) * 10);
}
END_HANDLER

BUTTON_CLICKED(fontPreview)
{
	nsgtk_reflow_all_windows();
}
END_HANDLER

COMBO_CHANGED(comboButtonType, button_type)
{
	nsgtk_scaffolding *current = scaf_list;
	nsoption_set_int(button_type, nsoption_int(button_type) + 1);

	/* value of 0 is reserved for 'unset' */
	while (current)	{
		nsgtk_scaffolding_reset_offset(current);
		switch(nsoption_int(button_type)) {
		case 1:
			gtk_toolbar_set_style(
				GTK_TOOLBAR(nsgtk_scaffolding_toolbar(current)),
				GTK_TOOLBAR_ICONS);
			gtk_toolbar_set_icon_size(
				GTK_TOOLBAR(nsgtk_scaffolding_toolbar(current)),
				GTK_ICON_SIZE_SMALL_TOOLBAR);
			break;
		case 2:
			gtk_toolbar_set_style(
				GTK_TOOLBAR(nsgtk_scaffolding_toolbar(current)),
				GTK_TOOLBAR_ICONS);
			gtk_toolbar_set_icon_size(
				GTK_TOOLBAR(nsgtk_scaffolding_toolbar(current)),
				GTK_ICON_SIZE_LARGE_TOOLBAR);
			break;
		case 3:
			gtk_toolbar_set_style(
				GTK_TOOLBAR(nsgtk_scaffolding_toolbar(current)),
				GTK_TOOLBAR_BOTH);
			gtk_toolbar_set_icon_size(
				GTK_TOOLBAR(nsgtk_scaffolding_toolbar(current)),
				GTK_ICON_SIZE_LARGE_TOOLBAR);
			break;
		case 4:
			gtk_toolbar_set_style(
				GTK_TOOLBAR(nsgtk_scaffolding_toolbar(current)),
				GTK_TOOLBAR_TEXT);
		default:
			break;
		}
		current = nsgtk_scaffolding_iterate(current);
	}
}
END_HANDLER

COMBO_CHANGED(comboTabPosition, position_tab)
{
	nsgtk_scaffolding *current = scaf_list;
	nsoption_set_int(button_type, nsoption_int(button_type) + 1);

	/* value of 0 is reserved for 'unset' */
	while (current)	{
		nsgtk_scaffolding_reset_offset(current);

		nsgtk_reflow_all_windows();

		current = nsgtk_scaffolding_iterate(current);
	}
}
END_HANDLER

SPIN_CHANGED(spinMemoryCacheSize, memory_cache_size)
{
	nsoption_set_int(memory_cache_size, nsoption_int(memory_cache_size) << 20);
}
END_HANDLER

SPIN_CHANGED(spinDiscCacheAge, disc_cache_age)
{
}
END_HANDLER

CHECK_CHANGED(checkClearDownloads, downloads_clear)
{
}
END_HANDLER

CHECK_CHANGED(checkRequestOverwrite, request_overwrite)
{
}
END_HANDLER

static gboolean on_fileChooserDownloads_changed(GtkWidget *widget, gpointer data) 
{
	gchar *dir;
	LOG(("Signal emitted on '%s'", "fileChooserDownloads"));

	dir = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER((widget)));
	nsoption_set_charp(downloads_directory, strdup(dir));
	g_free(dir);
	return FALSE;
} 

CHECK_CHANGED(checkFocusNew, focus_new)
{
}
END_HANDLER

CHECK_CHANGED(checkNewBlank, new_blank)
{
}
END_HANDLER

CHECK_CHANGED(checkUrlSearch, search_url_bar)
{
}
END_HANDLER

COMBO_CHANGED(comboSearch, search_provider)	
{
	nsgtk_scaffolding *current = scaf_list;
	char *name;

	/* refresh web search prefs from file */
	search_web_provider_details(nsoption_charp(search_provider));

	/* retrieve ico */
	search_web_retrieve_ico(false);

	/* callback may handle changing gui */
	if (search_web_ico() != NULL)
		gui_window_set_search_ico(search_web_ico());

	/* set entry */
	name = search_web_provider_name();
	if (name == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		continue;
	}
	char content[strlen(name) + SLEN("Search ") + 1];
	sprintf(content, "Search %s", name);
	free(name);
	while (current) {
		nsgtk_scaffolding_set_websearch(current, content);
		current = nsgtk_scaffolding_iterate(current);
	}
}
END_HANDLER

COMBO_CHANGED(combotheme, current_theme)
{
	nsgtk_scaffolding *current = scaf_list;
	char *name;
	if (nsoption_int(current_theme) != 0) {
		if (nsgtk_theme_name() != NULL)
			free(nsgtk_theme_name());
		name = nsgtk_combo_box_text_get_active_text(combotheme);
		if (name != NULL) {
			nsgtk_theme_set_name(name);
			nsgtk_theme_prepare();
			/* possible name leak */
		}
	} else if (nsgtk_theme_name() != NULL) {
		free(nsgtk_theme_name());
		nsgtk_theme_set_name(NULL);
	}

	while (current)	{
		nsgtk_theme_implement(current);
		current = nsgtk_scaffolding_iterate(current);
	}		
}
END_HANDLER

BUTTON_CLICKED(buttonaddtheme)
{
	char *filename, *directory;
	size_t len;
	GtkWidget *fc = gtk_file_chooser_dialog_new(
			messages_get("gtkAddThemeTitle"),
			GTK_WINDOW(wndPreferences),
			GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
			GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
	len = SLEN("themes") + strlen(res_dir_location) + 1;
	char themesfolder[len];
	snprintf(themesfolder, len, "%sthemes", res_dir_location);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc), 
			themesfolder);
	gint res = gtk_dialog_run(GTK_DIALOG(fc));
	if (res == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_current_folder(
				GTK_FILE_CHOOSER(fc));
		if (strcmp(filename, themesfolder) != 0) {
			directory = strrchr(filename, '/');
			*directory = '\0';
			if (strcmp(filename, themesfolder) != 0) {
				warn_user(messages_get(
						"gtkThemeFolderInstructions"), 
						0);
				gtk_widget_destroy(GTK_WIDGET(fc));
				if (filename != NULL)
					g_free(filename);
				return FALSE;
			} else {
				directory++;
			}
		} else {
			if (filename != NULL)
				g_free(filename);
			filename = gtk_file_chooser_get_filename(
					GTK_FILE_CHOOSER(fc));
			if (strcmp(filename, themesfolder) == 0) {
				warn_user(messages_get("gtkThemeFolderSub"),
						0);
				gtk_widget_destroy(GTK_WIDGET(fc));
				g_free(filename);
				return FALSE;
			}
			directory = strrchr(filename, '/') + 1;
		}
		gtk_widget_destroy(GTK_WIDGET(fc));
		nsgtk_theme_add(directory);
		if (filename != NULL)
			g_free(filename);
	}
}
END_HANDLER

CHECK_CHANGED(sourceButtonTab, source_tab)
{
}
END_HANDLER

SPIN_CHANGED(spinMarginTop, margin_top)
{
}
END_HANDLER

SPIN_CHANGED(spinMarginBottom, margin_bottom)
{
}
END_HANDLER

SPIN_CHANGED(spinMarginLeft, margin_left)
{
}
END_HANDLER

SPIN_CHANGED(spinMarginRight, margin_right)
{
}
END_HANDLER

SPIN_CHANGED(spinExportScale, export_scale)
{
}
END_HANDLER

CHECK_CHANGED(checkSuppressImages, suppress_images)
{
}
END_HANDLER

CHECK_CHANGED(checkRemoveBackgrounds, remove_backgrounds)
{
}
END_HANDLER

CHECK_CHANGED(checkFitPage, enable_loosening)
{
}
END_HANDLER

CHECK_CHANGED(checkCompressPDF, enable_PDF_compression)
{
}
END_HANDLER

CHECK_CHANGED(checkPasswordPDF, enable_PDF_password)
{
}
END_HANDLER

BUTTON_CLICKED(setDefaultExportOptions)
{
	nsoption_set_int(margin_top, DEFAULT_MARGIN_TOP_MM);
	nsoption_set_int(margin_bottom, DEFAULT_MARGIN_BOTTOM_MM);
	nsoption_set_int(margin_left, DEFAULT_MARGIN_LEFT_MM);
	nsoption_set_int(margin_right, DEFAULT_MARGIN_RIGHT_MM);
	nsoption_set_int(export_scale, DEFAULT_EXPORT_SCALE * 100);		
	nsoption_set_bool(suppress_images, false);
	nsoption_set_bool(remove_backgrounds, false);
	nsoption_set_bool(enable_loosening, true);
	nsoption_set_bool(enable_PDF_compression, true);
	nsoption_set_bool(enable_PDF_password, false);
			
	SET_SPIN(spinMarginTop, nsoption_int(margin_top));
	SET_SPIN(spinMarginBottom, nsoption_int(margin_bottom));
	SET_SPIN(spinMarginLeft, nsoption_int(margin_left));
	SET_SPIN(spinMarginRight, nsoption_int(margin_right));
	SET_SPIN(spinExportScale, nsoption_int(export_scale));
	SET_CHECK(checkSuppressImages, nsoption_bool(suppress_images));
	SET_CHECK(checkRemoveBackgrounds, nsoption_bool(remove_backgrounds));
	SET_CHECK(checkCompressPDF, nsoption_bool(enable_PDF_compression));
	SET_CHECK(checkPasswordPDF, nsoption_bool(enable_PDF_password));
	SET_CHECK(checkFitPage, nsoption_bool(enable_loosening));
}
END_HANDLER

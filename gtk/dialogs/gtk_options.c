/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 * Copyright 2008 Mike Lester <element3260@gmail.com>
 * Copyright 2009 Daniel Silverstone <dsilvers@netsurf-browser.org>
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
#include "desktop/options.h"
#include "desktop/print.h"
#include "gtk/options.h"
#include "gtk/gtk_gui.h"
#include "gtk/gtk_scaffolding.h"
#include "gtk/dialogs/gtk_options.h"
#include "gtk/gtk_window.h"
#include "utils/log.h"
#include "utils/utils.h"

GtkDialog *wndPreferences;
static GladeXML *gladeFile;
static gchar *glade_location;
static struct browser_window *current_browser;

static int proxy_type;
static float animation_delay;

static void dialog_response_handler (GtkDialog *dlg, gint res_id);
static gboolean on_dialog_close (GtkDialog *dlg, gboolean stay_alive);

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
DECLARE(checkShowSingleTab);

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
DECLARE(checkFocusNew);
DECLARE(checkNewBlank);

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
                (wname) = glade_xml_get_widget(gladeFile, #wname);      \
                if ((wname) == NULL)                                    \
                        LOG(("Unable to find widget '%s'!", #wname));   \
        } while (0)

/* Assigns widget and connects it to its callback function */
#define CONNECT(wname, event)                                           \
        g_signal_connect(G_OBJECT(wname), event,                        \
                         G_CALLBACK(on_##wname##_changed), NULL)

GtkDialog* nsgtk_options_init(struct browser_window *bw, GtkWindow *parent)
{
	glade_location = g_strconcat(res_dir_location, "options.glade", NULL);
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

	CONNECT(spinMemoryCacheSize, "value-changed");
	CONNECT(spinDiscCacheAge, "value-changed");
	
	CONNECT(checkClearDownloads, "toggled");
	CONNECT(checkRequestOverwrite, "toggled");
	CONNECT(fileChooserDownloads, "current-folder-changed");
	
	CONNECT(checkFocusNew, "toggled");
	CONNECT(checkNewBlank, "toggled");
	
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
                (widget) = glade_xml_get_widget(gladeFile, #widget);    \
                gtk_entry_set_text(GTK_ENTRY((widget)), (value));       \
        } while (0)

#define SET_SPIN(widget, value)                                         \
        do {                                                            \
                (widget) = glade_xml_get_widget(gladeFile, #widget);    \
                gtk_spin_button_set_value(GTK_SPIN_BUTTON((widget)), (value)); \
        } while (0)

#define SET_CHECK(widget, value)                                        \
        do {                                                            \
                (widget) = glade_xml_get_widget(gladeFile, #widget);    \
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON((widget)), \
                                             (value));                  \
        } while (0)

#define SET_COMBO(widget, value)                                        \
        do {                                                            \
                (widget) = glade_xml_get_widget(gladeFile, #widget);    \
                gtk_combo_box_set_active(GTK_COMBO_BOX((widget)), (value)); \
        } while (0)

#define SET_FONT(widget, value)                                         \
        do {                                                            \
                (widget) = glade_xml_get_widget(gladeFile, #widget);    \
                gtk_font_button_set_font_name(GTK_FONT_BUTTON((widget)), \
                                              (value));                 \
        } while (0)

#define SET_FILE_CHOOSER(widgt, value)                                  \
        do {                                                            \
                (widgt) = glade_xml_get_widget(gladeFile, #widgt);      \
                gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER((widgt)), \
                                                    (value));           \
        } while (0)

#define SET_BUTTON(widget)                                              \
        do {                                                            \
                (widget) = glade_xml_get_widget(gladeFile, #widget);    \
        } while (0)


void nsgtk_options_load(void) 
{
	GtkVBox *combolanguagevbox;
	gchar *languagefile;
	const char *default_accept_language = 
			option_accept_language ? option_accept_language : "en";
	int combo_row_count = 0;
	int active_language = 0;
	int proxytype = 0;
	FILE *fp;
	char buf[20];

	/* Create combobox */
	combolanguagevbox = 
		GTK_VBOX(glade_xml_get_widget(gladeFile, "combolanguagevbox"));
	comboLanguage = gtk_combo_box_new_text();

	languagefile = g_strconcat(res_dir_location, "languages", NULL);

	/* Populate combobox from languages file */
	fp = fopen((const char *) languagefile, "r");
	if (fp == NULL) {
		LOG(("Failed opening languages file"));
		warn_user("FileError", (const char *) languagefile);
		return;
	}

	while (fgets(buf, sizeof(buf), fp)) {
		/* Ignore blank lines */
		if (buf[0] == '\0')
			continue;

		/* Remove trailing \n */
		buf[strlen(buf) - 1] = '\0';

		gtk_combo_box_append_text(GTK_COMBO_BOX(comboLanguage), buf);

		if (strcmp(buf, default_accept_language) == 0)
			active_language = combo_row_count;

		combo_row_count++;
	}

	fclose(fp);

	gtk_combo_box_set_active(GTK_COMBO_BOX(comboLanguage), active_language);
	/** \todo localisation */
	gtk_widget_set_tooltip_text(GTK_WIDGET(comboLanguage), 
			"set preferred language for web pages");
	gtk_box_pack_start(GTK_BOX(combolanguagevbox), 
			comboLanguage, FALSE, FALSE, 0);
	gtk_widget_show(comboLanguage);

	SET_ENTRY(entryHomePageURL,
			option_homepage_url ? option_homepage_url : "");
	SET_BUTTON(setCurrentPage);
	SET_BUTTON(setDefaultPage);
	SET_CHECK(checkHideAdverts, option_block_ads);
	
	SET_CHECK(checkDisablePopups, option_disable_popups);
	SET_CHECK(checkDisablePlugins, option_disable_plugins);
	SET_SPIN(spinHistoryAge, option_history_age);
	SET_CHECK(checkHoverURLs, option_hover_urls);
	
	SET_CHECK(checkDisplayRecentURLs, option_url_suggestion);
	SET_CHECK(checkSendReferer, option_send_referer);
        SET_CHECK(checkShowSingleTab, option_show_single_tab);
	
	if (option_http_proxy == false)
		proxytype = 0;
	else
		proxytype = option_http_proxy_auth + 1;

	SET_COMBO(comboProxyType, proxytype);
	SET_ENTRY(entryProxyHost,
			option_http_proxy_host ? option_http_proxy_host : "");
	gtk_widget_set_sensitive(entryProxyHost, proxytype != 0);

	snprintf(buf, sizeof(buf), "%d", option_http_proxy_port);	

	SET_ENTRY(entryProxyPort, buf);
	gtk_widget_set_sensitive(entryProxyPort, proxytype != 0);

	SET_ENTRY(entryProxyUser, option_http_proxy_auth_user ?
			option_http_proxy_auth_user : "");
	gtk_widget_set_sensitive(entryProxyUser, proxytype != 0);

	SET_ENTRY(entryProxyPassword, option_http_proxy_auth_pass ?
			option_http_proxy_auth_pass : "");
	gtk_widget_set_sensitive(entryProxyPassword, proxytype != 0);

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
	SET_COMBO(comboDefault, option_font_default);
	SET_SPIN(spinDefaultSize, option_font_size / 10);
	SET_SPIN(spinMinimumSize, option_font_min_size / 10);
	SET_BUTTON(fontPreview);
	
	SET_COMBO(comboButtonType, option_button_type -1);

	SET_SPIN(spinMemoryCacheSize, option_memory_cache_size >> 20);
	SET_SPIN(spinDiscCacheAge, option_disc_cache_age);
	
	SET_CHECK(checkClearDownloads, option_downloads_clear);
	SET_CHECK(checkRequestOverwrite, option_request_overwrite);
	SET_FILE_CHOOSER(fileChooserDownloads, option_downloads_directory);
	
	SET_CHECK(checkFocusNew, option_focus_new);
	SET_CHECK(checkNewBlank, option_new_blank);
	
	SET_SPIN(spinMarginTop, option_margin_top);
	SET_SPIN(spinMarginBottom, option_margin_bottom);
	SET_SPIN(spinMarginLeft, option_margin_left);
	SET_SPIN(spinMarginRight, option_margin_right);
	SET_SPIN(spinExportScale, option_export_scale);
	SET_CHECK(checkSuppressImages, option_suppress_images);
	SET_CHECK(checkRemoveBackgrounds, option_remove_backgrounds);
	SET_CHECK(checkFitPage, option_enable_loosening);
	SET_CHECK(checkCompressPDF, option_enable_PDF_compression);
	SET_CHECK(checkPasswordPDF, option_enable_PDF_password);
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
	options_write(options_file_location);
	if ((stay_alive) && GTK_IS_WIDGET(dlg))
		gtk_widget_hide(GTK_WIDGET(dlg));
	else {
		stay_alive = FALSE;
	}
	return stay_alive;
}
	

/* Defines the callback functions for all widgets and specifies
 * nsgtk_reflow_all_windows only where necessary */

#define ENTRY_CHANGED(widget, option)                                   \
        static gboolean on_##widget##_changed(GtkWidget *widget, gpointer data) { \
        if (!g_str_equal(gtk_entry_get_text(GTK_ENTRY((widget))), (option) ? (option) : "")) { \
                LOG(("Signal emitted on '%s'", #widget));               \
                if ((option))                                           \
                        free((option));                                 \
                (option) = strdup(gtk_entry_get_text(GTK_ENTRY((widget)))); \
                }                                                       \
        do {                                                            \
                
#define CHECK_CHANGED(widget, option)                                   \
        static gboolean on_##widget##_changed(GtkWidget *widget, gpointer data) { \
	LOG(("Signal emitted on '%s'", #widget));                       \
	(option) = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON((widget))); \
        do {                                                            \

#define SPIN_CHANGED(widget, option)                                    \
        static gboolean on_##widget##_changed(GtkWidget *widget, gpointer data) { \
        LOG(("Signal emitted on '%s'", #widget));                       \
        (option) = gtk_spin_button_get_value(GTK_SPIN_BUTTON((widget))); \
        do {                                                            \

#define COMBO_CHANGED(widget, option)                                   \
        static gboolean on_##widget##_changed(GtkWidget *widget, gpointer data) { \
	LOG(("Signal emitted on '%s'", #widget));                       \
	(option) = gtk_combo_box_get_active(GTK_COMBO_BOX((widget)));   \
        do {

#define FONT_CHANGED(widget, option)                                    \
        static gboolean on_##widget##_changed(GtkWidget *widget, gpointer data) { \
	LOG(("Signal emitted on '%s'", #widget));                       \
	if ((option))                                                   \
                free((option));                                         \
	(option) = strdup(gtk_font_button_get_font_name(GTK_FONT_BUTTON((widget)))); \
        do {

#define FILE_CHOOSER_CHANGED(widget, option)                            \
        static gboolean on_##widget##_changed(GtkWidget *widget, gpointer data) { \
	LOG(("Signal emitted on '%s'", #widget));                       \
	(option) = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER((widget))); \
        do {

#define BUTTON_CLICKED(widget)                                          \
        static gboolean on_##widget##_changed(GtkWidget *widget, gpointer data) { \
	LOG(("Signal emitted on '%s'", #widget));                       \
        do {

#define END_HANDLER                             \
        } while (0);                            \
        return FALSE;                           \
        }

static gboolean on_comboLanguage_changed(GtkWidget *widget, gpointer data)
{
	char *old_lang = option_accept_language;
	gchar *lang = 
		gtk_combo_box_get_active_text(GTK_COMBO_BOX(comboLanguage));
	if (lang == NULL)
		return FALSE;

	option_accept_language = strdup((const char *) lang);
	if (option_accept_language == NULL)
		option_accept_language = old_lang;
	else
		free(old_lang);

	g_free(lang);
	
	return FALSE;
}

ENTRY_CHANGED(entryHomePageURL, option_homepage_url)
END_HANDLER

BUTTON_CLICKED(setCurrentPage)
	const gchar *url = current_browser->current_content->url;
	gtk_entry_set_text(GTK_ENTRY(entryHomePageURL), url);
	option_homepage_url = 
		strdup(gtk_entry_get_text(GTK_ENTRY(entryHomePageURL)));
END_HANDLER

BUTTON_CLICKED(setDefaultPage)
	gtk_entry_set_text(GTK_ENTRY(entryHomePageURL),
                           "http://www.netsurf-browser.org/welcome/");
	option_homepage_url = 
		strdup(gtk_entry_get_text(GTK_ENTRY(entryHomePageURL)));
END_HANDLER

CHECK_CHANGED(checkHideAdverts, option_block_ads)
END_HANDLER

CHECK_CHANGED(checkDisplayRecentURLs, option_url_suggestion)
END_HANDLER

CHECK_CHANGED(checkSendReferer, option_send_referer)
END_HANDLER
                
CHECK_CHANGED(checkShowSingleTab, option_show_single_tab)
	nsgtk_reflow_all_windows();
END_HANDLER

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
	gboolean sensitive = (!proxy_type == 0);
	gtk_widget_set_sensitive (entryProxyHost, sensitive);
	gtk_widget_set_sensitive (entryProxyPort, sensitive);
	gtk_widget_set_sensitive (entryProxyUser, sensitive);
	gtk_widget_set_sensitive (entryProxyPassword, sensitive);
	
END_HANDLER

ENTRY_CHANGED(entryProxyHost, option_http_proxy_host)
END_HANDLER

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
		snprintf(buf, sizeof(buf), "%d", option_http_proxy_port);
		SET_ENTRY(entryProxyPort, buf);
	}

	return FALSE;
}

ENTRY_CHANGED(entryProxyUser, option_http_proxy_auth_user)
END_HANDLER

ENTRY_CHANGED(entryProxyPassword, option_http_proxy_auth_pass)
END_HANDLER

SPIN_CHANGED(spinMaxFetchers, option_max_fetchers)
END_HANDLER

SPIN_CHANGED(spinFetchesPerHost, option_max_fetchers_per_host)
END_HANDLER

SPIN_CHANGED(spinCachedConnections, option_max_cached_fetch_handles)
END_HANDLER

CHECK_CHANGED(checkResampleImages, option_render_resample)
END_HANDLER

SPIN_CHANGED(spinAnimationSpeed, animation_delay)
	option_minimum_gif_delay = round(animation_delay * 100.0);
END_HANDLER

CHECK_CHANGED(checkDisableAnimations, option_animate_images);
	option_animate_images = !option_animate_images;
END_HANDLER

CHECK_CHANGED(checkDisablePopups, option_disable_popups)
END_HANDLER

CHECK_CHANGED(checkDisablePlugins, option_disable_plugins)
END_HANDLER

SPIN_CHANGED(spinHistoryAge, option_history_age)
END_HANDLER

CHECK_CHANGED(checkHoverURLs, option_hover_urls)
END_HANDLER

FONT_CHANGED(fontSansSerif, option_font_sans)
END_HANDLER

FONT_CHANGED(fontSerif, option_font_serif)
END_HANDLER

FONT_CHANGED(fontMonospace, option_font_mono)
END_HANDLER

FONT_CHANGED(fontCursive, option_font_cursive)
END_HANDLER

FONT_CHANGED(fontFantasy, option_font_fantasy)
END_HANDLER

COMBO_CHANGED(comboDefault, option_font_default)
END_HANDLER

SPIN_CHANGED(spinDefaultSize, option_font_size)
	option_font_size *= 10;
END_HANDLER

SPIN_CHANGED(spinMinimumSize, option_font_min_size)
	option_font_min_size *= 10;
END_HANDLER

BUTTON_CLICKED(fontPreview)
	nsgtk_reflow_all_windows();
END_HANDLER

COMBO_CHANGED(comboButtonType, option_button_type)
	struct gui_window *current = window_list;

	while (current)	{
		switch(option_button_type) {
		case 0:
			gtk_toolbar_set_style(
				GTK_TOOLBAR(current->scaffold->tool_bar),
				GTK_TOOLBAR_ICONS);
			gtk_toolbar_set_icon_size(
				GTK_TOOLBAR(current->scaffold->tool_bar),
				GTK_ICON_SIZE_SMALL_TOOLBAR);
			break;
		case 1:
			gtk_toolbar_set_style(
				GTK_TOOLBAR(current->scaffold->tool_bar),
				GTK_TOOLBAR_ICONS);
			gtk_toolbar_set_icon_size(
				GTK_TOOLBAR(current->scaffold->tool_bar),
				GTK_ICON_SIZE_LARGE_TOOLBAR);
			break;
		case 2:
			gtk_toolbar_set_style(
				GTK_TOOLBAR(current->scaffold->tool_bar),
				GTK_TOOLBAR_BOTH);
			gtk_toolbar_set_icon_size(
				GTK_TOOLBAR(current->scaffold->tool_bar),
				GTK_ICON_SIZE_LARGE_TOOLBAR);
			break;
		case 3:
			gtk_toolbar_set_style(
				GTK_TOOLBAR(current->scaffold->tool_bar),
				GTK_TOOLBAR_TEXT);
		default:
			break;
		}
		current = current->next;
	}
END_HANDLER

SPIN_CHANGED(spinMemoryCacheSize, option_memory_cache_size)
	option_memory_cache_size <<= 20;
END_HANDLER

SPIN_CHANGED(spinDiscCacheAge, option_disc_cache_age)
END_HANDLER

CHECK_CHANGED(checkClearDownloads, option_downloads_clear)
END_HANDLER

CHECK_CHANGED(checkRequestOverwrite, option_request_overwrite)
END_HANDLER

FILE_CHOOSER_CHANGED(fileChooserDownloads, option_downloads_directory)
END_HANDLER

CHECK_CHANGED(checkFocusNew, option_focus_new)
END_HANDLER

CHECK_CHANGED(checkNewBlank, option_new_blank)
END_HANDLER

SPIN_CHANGED(spinMarginTop, option_margin_top)
END_HANDLER

SPIN_CHANGED(spinMarginBottom, option_margin_bottom)
END_HANDLER

SPIN_CHANGED(spinMarginLeft, option_margin_left)
END_HANDLER

SPIN_CHANGED(spinMarginRight, option_margin_right)
END_HANDLER

SPIN_CHANGED(spinExportScale, option_export_scale)
END_HANDLER

CHECK_CHANGED(checkSuppressImages, option_suppress_images)
END_HANDLER

CHECK_CHANGED(checkRemoveBackgrounds, option_remove_backgrounds)
END_HANDLER

CHECK_CHANGED(checkFitPage, option_enable_loosening)
END_HANDLER

CHECK_CHANGED(checkCompressPDF, option_enable_PDF_compression)
END_HANDLER

CHECK_CHANGED(checkPasswordPDF, option_enable_PDF_password)
END_HANDLER

BUTTON_CLICKED(setDefaultExportOptions)
	option_margin_top = DEFAULT_MARGIN_TOP_MM;
	option_margin_bottom = DEFAULT_MARGIN_BOTTOM_MM;
	option_margin_left = DEFAULT_MARGIN_LEFT_MM;
	option_margin_right = DEFAULT_MARGIN_RIGHT_MM;
	option_export_scale = DEFAULT_EXPORT_SCALE * 100;		
	option_suppress_images = false;
	option_remove_backgrounds = false;
	option_enable_loosening = true;
	option_enable_PDF_compression = true;
	option_enable_PDF_password = false;
			
	SET_SPIN(spinMarginTop, option_margin_top);
	SET_SPIN(spinMarginBottom, option_margin_bottom);
	SET_SPIN(spinMarginLeft, option_margin_left);
	SET_SPIN(spinMarginRight, option_margin_right);
	SET_SPIN(spinExportScale, option_export_scale);
	SET_CHECK(checkSuppressImages, option_suppress_images);
	SET_CHECK(checkRemoveBackgrounds, option_remove_backgrounds);
	SET_CHECK(checkCompressPDF, option_enable_PDF_compression);
	SET_CHECK(checkPasswordPDF, option_enable_PDF_password);
	SET_CHECK(checkFitPage, option_enable_loosening);
END_HANDLER

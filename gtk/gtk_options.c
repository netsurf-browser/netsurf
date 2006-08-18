/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include "netsurf/utils/log.h"
#include "netsurf/desktop/options.h"
#include "netsurf/gtk/options.h"
#include "netsurf/gtk/gtk_gui.h"
#include "netsurf/gtk/gtk_window.h"
#include "netsurf/gtk/gtk_options.h"

GtkWindow *wndChoices;

static GtkWidget 	*entryHomePageURL,
			*checkHideAdverts,
			*checkDisablePopups,
			*checkDisablePlugins,
			*spinHistoryAge,
			*checkHoverURLs,
			*checkRequestOverwrite,
			*checkDisplayRecentURLs,
			*checkSendReferer,
			
			*comboProxyType,
			*entryProxyHost,
			*entryProxyPort,
			*entryProxyUser,
			*entryProxyPassword,
			*spinMaxFetchers,
			*spinFetchesPerHost,
			*spinCachedConnections,
			
			*checkUseCairo,
			*checkResampleImages,
			*spinAnimationSpeed,
			*checkDisableAnimations,

			*fontSansSerif,
			*fontSerif,
			*fontMonospace,
			*fontCursive,
			*fontFantasy,
			*comboDefault,
			*spinDefaultSize,
			*spinMinimumSize,

			*spinMemoryCacheSize,
			*spinDiscCacheAge;

#define FIND_WIDGET(x) (x) = glade_xml_get_widget(gladeWindows, #x); if ((x) == NULL) LOG(("Unable to find widget '%s'!", #x))

void nsgtk_options_init(void) {
	wndChoices = GTK_WINDOW(glade_xml_get_widget(gladeWindows,
				"wndChoices"));

	/* get widget objects */
	FIND_WIDGET(entryHomePageURL);
	FIND_WIDGET(checkHideAdverts);
	FIND_WIDGET(checkDisablePopups);
	FIND_WIDGET(checkDisablePlugins);
	FIND_WIDGET(spinHistoryAge);
	FIND_WIDGET(checkHoverURLs);
	FIND_WIDGET(checkRequestOverwrite);
	FIND_WIDGET(checkDisplayRecentURLs);
	FIND_WIDGET(checkSendReferer);

	FIND_WIDGET(comboProxyType);
	FIND_WIDGET(entryProxyHost);
	FIND_WIDGET(entryProxyPort);
	FIND_WIDGET(entryProxyUser);
	FIND_WIDGET(entryProxyPassword);
	FIND_WIDGET(spinMaxFetchers);
	FIND_WIDGET(spinFetchesPerHost);
	FIND_WIDGET(spinCachedConnections);

	FIND_WIDGET(checkUseCairo);
	FIND_WIDGET(checkResampleImages);
	FIND_WIDGET(spinAnimationSpeed);
	FIND_WIDGET(checkDisableAnimations);

	FIND_WIDGET(fontSansSerif);
	FIND_WIDGET(fontSerif);
	FIND_WIDGET(fontMonospace);
	FIND_WIDGET(fontCursive);
	FIND_WIDGET(fontFantasy);
	FIND_WIDGET(comboDefault);
	FIND_WIDGET(spinDefaultSize);
	FIND_WIDGET(spinMinimumSize);

	FIND_WIDGET(spinMemoryCacheSize);
	FIND_WIDGET(spinDiscCacheAge);

	/* set the widgets to reflect the current options */
	nsgtk_options_load();
}

#define SET_ENTRY(x, y) gtk_entry_set_text(GTK_ENTRY((x)), (y))
#define SET_SPIN(x, y) gtk_spin_button_set_value(GTK_SPIN_BUTTON((x)), (y))
#define SET_CHECK(x, y) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON((x)), (y))
#define SET_COMBO(x, y) gtk_combo_box_set_active(GTK_COMBO_BOX((x)), (y))
#define SET_FONT(x, y) gtk_font_button_set_font_name(GTK_FONT_BUTTON((x)), (y))

void nsgtk_options_load(void) {
	char b[20];
	
	SET_ENTRY(entryHomePageURL, option_homepage_url);
	SET_CHECK(checkHideAdverts, option_block_ads);
	SET_CHECK(checkDisplayRecentURLs, option_url_suggestion);
	SET_CHECK(checkSendReferer, option_send_referer);

	SET_COMBO(comboProxyType, option_http_proxy_auth);
	SET_ENTRY(entryProxyHost, option_http_proxy_host);
	snprintf(b, 20, "%d", option_http_proxy_port);
	SET_ENTRY(entryProxyPort, b);
	SET_ENTRY(entryProxyUser, option_http_proxy_auth_user);
	SET_ENTRY(entryProxyPassword, option_http_proxy_auth_pass);
	
	SET_SPIN(spinMaxFetchers, option_max_fetchers);
	SET_SPIN(spinFetchesPerHost, option_max_fetchers_per_host);
	SET_SPIN(spinCachedConnections, option_max_cached_fetch_handles);

	SET_CHECK(checkUseCairo, option_render_cairo);
	SET_CHECK(checkResampleImages, option_render_resample);
	SET_SPIN(spinAnimationSpeed, option_minimum_gif_delay);
	SET_CHECK(checkDisableAnimations, !option_animate_images);

	SET_FONT(fontSansSerif, option_font_sans);
	SET_FONT(fontSerif, option_font_serif);
	SET_FONT(fontMonospace, option_font_mono);
	SET_FONT(fontCursive, option_font_cursive);
	SET_FONT(fontFantasy, option_font_fantasy);
	SET_COMBO(comboDefault, option_font_default - 1);
	SET_SPIN(spinDefaultSize, option_font_size / 10);
	SET_SPIN(spinMinimumSize, option_font_min_size / 10);

	SET_SPIN(spinMemoryCacheSize, option_memory_cache_size);
	SET_SPIN(spinDiscCacheAge, option_disc_cache_age);
}

#define GET_ENTRY(x, y) if ((y)) free((y)); \
	(y) = strdup(gtk_entry_get_text(GTK_ENTRY((x))))
#define GET_CHECK(x, y) (y) = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON((x)))
#define GET_SPIN(x, y) (y) = gtk_spin_button_get_value(GTK_SPIN_BUTTON((x)))
#define GET_COMBO(x, y) (y) = gtk_combo_box_get_active(GTK_COMBO_BOX((x)))
#define GET_FONT(x, y) if ((y)) free((y)); \
	(y) = strdup(gtk_font_button_get_font_name(GTK_FONT_BUTTON((x))))

void nsgtk_options_save(void) {
	char *b = NULL;
	int i;
	
	GET_ENTRY(entryHomePageURL, option_homepage_url);
	GET_CHECK(checkDisplayRecentURLs, option_url_suggestion);

	GET_COMBO(comboProxyType, i);
	option_http_proxy = (i > 0) ? true : false;
	switch (i)
	{
		case 0:
		case 1:
			option_http_proxy_auth = OPTION_HTTP_PROXY_AUTH_NONE;
			break;
		case 2:
			option_http_proxy_auth = OPTION_HTTP_PROXY_AUTH_BASIC;
			break;
		case 3:
			option_http_proxy_auth = OPTION_HTTP_PROXY_AUTH_NTLM;
			break;
		default:
			option_http_proxy_auth = OPTION_HTTP_PROXY_AUTH_NONE;
			break;
	}
	
	GET_ENTRY(entryProxyHost, option_http_proxy_host);
	GET_ENTRY(entryProxyPort, b);
	option_http_proxy_port = atoi(b);
	free(b);
	GET_ENTRY(entryProxyUser, option_http_proxy_auth_user);
	GET_ENTRY(entryProxyPassword, option_http_proxy_auth_pass);
	
	GET_SPIN(spinMaxFetchers, option_max_fetchers);
	GET_SPIN(spinFetchesPerHost, option_max_fetchers_per_host);
	GET_SPIN(spinCachedConnections, option_max_cached_fetch_handles);	

	GET_CHECK(checkUseCairo, option_render_cairo);
	GET_CHECK(checkResampleImages, option_render_resample);
	GET_SPIN(spinAnimationSpeed, option_minimum_gif_delay);
	option_minimum_gif_delay *= 10;
	
	GET_FONT(fontSansSerif, option_font_sans);
	GET_FONT(fontSerif, option_font_serif);
	GET_FONT(fontMonospace, option_font_mono);
	GET_FONT(fontCursive, option_font_cursive);
	GET_FONT(fontFantasy, option_font_fantasy);
	GET_COMBO(comboDefault, option_font_default);
	option_font_default++;
	
	GET_SPIN(spinDefaultSize, option_font_size);
	option_font_size *= 10;
	GET_SPIN(spinMinimumSize, option_font_min_size);
	option_font_min_size *= 10;

	options_write(options_file_location);
	nsgtk_reflow_all_windows();
}


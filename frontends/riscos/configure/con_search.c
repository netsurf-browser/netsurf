/*
 * Copyright 2025 Vincent Sanders <vince@netsurf-browser.org>
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

#include "utils/nsoption.h"
#include "utils/log.h"
#include "utils/messages.h"

#include "desktop/searchweb.h"

#include "riscos/gui.h"
#include "riscos/menus.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/configure.h"
#include "riscos/configure/configure.h"
#include "riscos/dialog.h"


#define SEARCH_WEB_URLBAR 2
#define SEARCH_WEB_PROVIDER_FIELD 4
#define SEARCH_WEB_PROVIDER_GRIGHT 5
#define SEARCH_DEFAULT_BUTTON 6
#define SEARCH_CANCEL_BUTTON 7
#define SEARCH_OK_BUTTON 8


static void ro_gui_options_search_default(wimp_pointer *pointer)
{
	const char *defprovider;
	search_web_iterate_providers(-1, &defprovider);
	if (defprovider == NULL) {
		defprovider = "DuckDuckGo";
	}

	ro_gui_set_icon_selected_state(pointer->w, SEARCH_WEB_URLBAR,
				       nsoption_bool(search_url_bar));

	ro_gui_set_icon_string(pointer->w, SEARCH_WEB_PROVIDER_FIELD,
			       defprovider, true);
}


static bool ro_gui_options_search_ok(wimp_w w)
{
	char *provider;
	const char* defprovider;

	nsoption_set_bool(search_url_bar,
			  ro_gui_get_icon_selected_state(w, SEARCH_WEB_URLBAR));

	provider = strdup(ro_gui_get_icon_string(w, SEARCH_WEB_PROVIDER_FIELD));
	if (provider) {
		/* set search provider */
		search_web_select_provider(provider);

		/* set to default option if the default provider is selected */
		if ((search_web_iterate_providers(-1, &defprovider) != -1) &&
		    (strcmp(provider, defprovider) == 0)) {
			free(provider);
			/* use default option */
			provider = NULL;
		}

		/* set the option which takes owership of the provider allocation */
		nsoption_set_charp(search_web_provider, provider);

	} else {
		NSLOG(netsurf, INFO, "No memory to duplicate search code");
		ro_warn_user("NoMemory", 0);
	}

	ro_gui_save_options();
	return true;
}


bool ro_gui_options_search_initialise(wimp_w w)
{
	const char* defprovider;
	
	/* set the current values */
	ro_gui_set_icon_selected_state(w, SEARCH_WEB_URLBAR,
                                       nsoption_bool(search_url_bar));

	search_web_iterate_providers(-1, &defprovider);
	if (defprovider == NULL) {
		defprovider = "DuckDuckGo";
	}
	
	ro_gui_set_icon_string(w, SEARCH_WEB_PROVIDER_FIELD,
			       nsoption_charp(search_web_provider) ?
			       nsoption_charp(search_web_provider) :
			       defprovider, true);

	/* initialise all functions for a newly created window */
	ro_gui_wimp_event_register_checkbox(w, SEARCH_WEB_URLBAR);
	ro_gui_wimp_event_register_menu_gright(w, SEARCH_WEB_PROVIDER_FIELD,
			SEARCH_WEB_PROVIDER_GRIGHT, search_provider_menu);
	ro_gui_wimp_event_register_button(w, SEARCH_DEFAULT_BUTTON,
			ro_gui_options_search_default);
	ro_gui_wimp_event_register_cancel(w, SEARCH_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, SEARCH_OK_BUTTON,
			ro_gui_options_search_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpSearchConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}

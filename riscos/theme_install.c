/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Theme auto-installing.
 */

#include <assert.h>
#include <stdbool.h>
#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


static bool theme_install_active = false;
wimp_w dialog_theme_install;


void theme_install_callback(content_msg msg, struct content *c,
		void *p1, void *p2, union content_msg_data data);


/**
 * Handle a CONTENT_THEME that has started loading.
 */

void theme_install_start(struct content *c)
{
	assert(c);
	assert(c->type == CONTENT_THEME);

	if (theme_install_active) {
		warn_user("ThemeInstActive", 0);
		/* raise & centre dialog */
		return;
	}

	if (!content_add_user(c, theme_install_callback, 0, 0)) {
		warn_user("NoMemory", 0);
		return;
	}

	theme_install_active = true;

	ro_gui_set_icon_string(dialog_theme_install, ICON_THEME_INSTALL_MESSAGE,
			messages_get("ThemeInstDown"));
	ro_gui_set_icon_shaded_state(dialog_theme_install,
			ICON_THEME_INSTALL_INSTALL, true);
	ro_gui_dialog_open(dialog_theme_install);
}


/**
 * Callback for fetchcache() for theme install fetches.
 */

void theme_install_callback(content_msg msg, struct content *c,
		void *p1, void *p2, union content_msg_data data)
{
	switch (msg) {
		case CONTENT_MSG_READY:
			break;

		case CONTENT_MSG_DONE:
			/** \todo: parse the theme data, extract name & author,
			 * and ask the user if they want to install */
			ro_gui_set_icon_string(dialog_theme_install,
					ICON_THEME_INSTALL_MESSAGE,
					"Would you like to install the theme "
					"\"x\" by y?");
			ro_gui_set_icon_shaded_state(dialog_theme_install,
					ICON_THEME_INSTALL_INSTALL, false);
			break;

		case CONTENT_MSG_ERROR:
			ro_gui_dialog_close(dialog_theme_install);
			warn_user(data.error, 0);
			theme_install_active = false;
			break;

		case CONTENT_MSG_STATUS:
			break;

		case CONTENT_MSG_LOADING:
		case CONTENT_MSG_REDIRECT:
		case CONTENT_MSG_REFORMAT:
		case CONTENT_MSG_REDRAW:
		case CONTENT_MSG_NEWPTR:
		case CONTENT_MSG_AUTH:
		default:
			assert(0);
			break;
	}
}


/**
 * Create theme install window.
 */

void ro_gui_theme_install_init(void)
{
	dialog_theme_install = ro_gui_dialog_create("theme_inst");
}

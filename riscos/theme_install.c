/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Theme auto-installing.
 */

#include <assert.h>
#include <stdbool.h>
#include "oslib/osfile.h"
#include "content/content.h"
#include "desktop/browser.h"
#include "riscos/dialog.h"
#include "riscos/gui.h"
#include "riscos/options.h"
#include "riscos/theme.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"


static struct content *theme_install_content = NULL;
static struct theme_descriptor theme_install_descriptor;
wimp_w dialog_theme_install;


static void theme_install_close(wimp_w w);
static void theme_install_callback(content_msg msg, struct content *c,
		intptr_t p1, intptr_t p2, union content_msg_data data);
static bool theme_install_read(char *source_data, unsigned long source_size);


/**
 * Handle a CONTENT_THEME that has started loading.
 */

void theme_install_start(struct content *c)
{
	assert(c);
	assert(c->type == CONTENT_THEME);

	if (ro_gui_dialog_open_top(dialog_theme_install, NULL, 0, 0)) {
		warn_user("ThemeInstActive", 0);
		return;
	}

	/* stop theme sitting in memory cache */
	c->fresh = false;
	if (!content_add_user(c, theme_install_callback, 0, 0)) {
		warn_user("NoMemory", 0);
		return;
	}

	ro_gui_set_icon_string(dialog_theme_install, ICON_THEME_INSTALL_MESSAGE,
			messages_get("ThemeInstDown"));
	ro_gui_set_icon_shaded_state(dialog_theme_install,
			ICON_THEME_INSTALL_INSTALL, true);
	ro_gui_wimp_event_register_close_window(dialog_theme_install,
			theme_install_close);
}


/**
 * Callback for fetchcache() for theme install fetches.
 */

void theme_install_callback(content_msg msg, struct content *c,
		intptr_t p1, intptr_t p2, union content_msg_data data)
{
	char buffer[256];
	int author_indent = 0;

	switch (msg) {
	case CONTENT_MSG_READY:
		break;

	case CONTENT_MSG_DONE:
		theme_install_content = c;
		if (!theme_install_read(c->source_data, c->source_size)) {
			warn_user("ThemeInvalid", 0);
			theme_install_close(dialog_theme_install);
			break;
		}

		/* remove '© ' from the start of the data */
		if (theme_install_descriptor.author[0] == '©')
			author_indent++;
		while (theme_install_descriptor.author[author_indent] == ' ')
			author_indent++;
		snprintf(buffer, sizeof buffer, messages_get("ThemeInstall"),
				theme_install_descriptor.name,
				&theme_install_descriptor.author[author_indent]);
		buffer[sizeof buffer - 1] = '\0';
		ro_gui_set_icon_string(dialog_theme_install,
				ICON_THEME_INSTALL_MESSAGE,
				buffer);
		ro_gui_set_icon_shaded_state(dialog_theme_install,
				ICON_THEME_INSTALL_INSTALL, false);
		break;

	case CONTENT_MSG_ERROR:
		theme_install_close(dialog_theme_install);
		warn_user(data.error, 0);
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
 * Fill in theme_install_descriptor from received theme data.
 *
 * \param  source_data  received data
 * \param  source_size  size of data
 * \return  true if data is a correct theme, false on error
 *
 * If the data is a correct theme, theme_install_descriptor is filled in.
 */

bool theme_install_read(char *source_data, unsigned long source_size)
{
	if (source_size < sizeof(struct theme_file_header))
		return false;
	if (!ro_gui_theme_read_file_header(&theme_install_descriptor,
			(struct theme_file_header *) source_data))
		return false;
	if (source_size - sizeof(struct theme_file_header) !=
			theme_install_descriptor.compressed_size)
		return false;
	return true;
}


/**
 * Install the downloaded theme.
 *
 * \param  w	the theme install window handle
 */

bool ro_gui_theme_install_apply(wimp_w w)
{
	char theme_save[256];
	char *theme_file;
	struct theme_descriptor *theme_install;
	os_error *error;
	char *fix;

	assert(theme_install_content);

	/* convert spaces to hard spaces */
	theme_file = strdup(theme_install_descriptor.name);
	if (!theme_file) {
	  	LOG(("malloc failed"));
	  	warn_user("NoMemory", 0);
	}
	for (fix = theme_file; *fix != '\0'; *fix++)
		if (*fix == ' ')
			*fix = 160;	/* hard space */

	/* simply overwrite previous theme versions */
	snprintf(theme_save, sizeof theme_save, "%s.%s",
			option_theme_save, theme_file);

	theme_save[sizeof theme_save - 1] = '\0';
	error = xosfile_save_stamped(theme_save, 0xffd,
			theme_install_content->source_data,
			theme_install_content->source_data +
			theme_install_content->source_size);
	if (error) {
		LOG(("xosfile_save_stamped: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("ThemeInstallErr", 0);
		free(theme_file);
		return false;
	}

	/* apply the new theme */
	ro_gui_theme_get_available();
	theme_install = ro_gui_theme_find(theme_file);
	if (!theme_install || !ro_gui_theme_apply(theme_install)) {
		warn_user("ThemeApplyErr", 0);
	} else {
		free(option_theme);
		option_theme = strdup(theme_install->leafname);
	}
	free(theme_file);
	ro_gui_save_options();
	return true;
}


/**
 * Close the theme installer and free resources.
 */

void theme_install_close(wimp_w w)
{
	if (theme_install_content)
		content_remove_user(theme_install_content,
				theme_install_callback, 0, 0);
	theme_install_content = NULL;
}

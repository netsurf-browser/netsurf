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
#include "oslib/osfile.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"


static bool theme_install_active;
static struct content *theme_install_content = NULL;
static struct theme_descriptor theme_install_descriptor;
wimp_w dialog_theme_install;

static void theme_install_close(void);
void theme_install_callback(content_msg msg, struct content *c,
		void *p1, void *p2, union content_msg_data data);

#ifndef NCOS
#define THEME_LEAFNAME "WWW.NetSurf.Themes"
#define THEME_PATHNAME "<Choices$Write>"
#else
#define THEME_LEAFNAME "NetSurf.Choices.Themes"
#define THEME_PATHNAME "<User$Path>.Choices"
#endif

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
	char txt_buffer[256];
	bool error = false;
	int author_indent = 0;

	switch (msg) {
		case CONTENT_MSG_READY:
			break;

		case CONTENT_MSG_DONE:
			theme_install_content = c;
			if ((c->source_size < sizeof(struct theme_file_header)) ||
					(!ro_gui_theme_read_file_header(&theme_install_descriptor,
					(struct theme_file_header *)c->source_data)))
				error = true;
			else if (c->source_size - sizeof(struct theme_file_header) !=
					theme_install_descriptor.compressed_size)
				error = true;
			
			if (error) {
				warn_user("ThemeInvalid", 0);
				theme_install_close();
				break;
			}

			/* remove '© ' from the start of the data */
			if (theme_install_descriptor.author[0] == '©')
				author_indent++;
			while (theme_install_descriptor.author[author_indent] == ' ')
				author_indent++;
			snprintf(txt_buffer, 256, messages_get("ThemeInstall"),
					theme_install_descriptor.name,
					&theme_install_descriptor.author[author_indent]);
			txt_buffer[255] = '\0';
			ro_gui_set_icon_string(dialog_theme_install,
					ICON_THEME_INSTALL_MESSAGE,
					txt_buffer);
			ro_gui_set_icon_shaded_state(dialog_theme_install,
					ICON_THEME_INSTALL_INSTALL, false);
			break;

		case CONTENT_MSG_ERROR:
			theme_install_close();
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
 * Handle clicks in the theme install window
 */
void ro_gui_theme_install_click(wimp_pointer *pointer) {
	os_error *error;
  	fileswitch_object_type obj_type;
	char theme_save[256];
	char theme_leaf[256];
	char *theme_file;
	int theme_number = 1;
	bool theme_found;
	struct theme_descriptor *theme_install;

	switch (pointer->i) {
		case ICON_THEME_INSTALL_INSTALL:
			if (theme_install_content) {
				if (url_nice(theme_install_descriptor.name, &theme_file) != URL_FUNC_OK) {
					warn_user("ThemeInstallErr", 0);
					theme_install_close();
					return;
				}
				theme_found = false;
				while (!theme_found) {
				  	if (theme_number == 1)
						snprintf(theme_leaf, 256,
								"%s.%s", THEME_LEAFNAME, theme_file);
				  	else
						snprintf(theme_leaf, 256,
								"%s.%s%i", THEME_LEAFNAME, theme_file,
								theme_number);
					theme_leaf[255] = '\0';
					theme_number++;
					snprintf(theme_save, 256,
							"%s.%s", THEME_PATHNAME, theme_leaf);
					theme_save[255] = '\0';
					error = xosfile_read_stamped(theme_save,
							&obj_type, 0, 0, 0, 0, 0);
					if (error) {
					  	warn_user("ThemeInstallErr", 0);
						theme_install_close();
						free(theme_file);
						return;
					}
					theme_found = (obj_type == osfile_NOT_FOUND);
				}
				free(theme_file);
				error = xosfile_save_stamped(theme_save, 0xffd,
						theme_install_content->source_data,
						theme_install_content->source_data +
						theme_install_content->source_size);
				if (error) {
					warn_user("ThemeInstallErr", 0);
					theme_install_close();
					return;
				}
				/* apply theme only on Select clicks */
				if (pointer->buttons == wimp_CLICK_SELECT) {
					ro_gui_theme_get_available();
#ifndef NCOS
					snprintf(theme_save, 256, "Choices:%s", theme_leaf);
#else
					snprintf(theme_save, 256, "%s.%s", THEME_PATHNAME, theme_leaf);
#endif
					theme_save[255] = '\0';
					theme_install = ro_gui_theme_find(theme_save);
					if ((!theme_install) ||
							(!ro_gui_theme_apply(theme_install))) {
						warn_user("ThemeApplyErr", 0);
					} else {
					  	theme_file = strdup(theme_save);
					  	if (!theme_file) {
					  	  	warn_user("NoMemory", 0);
					  	} else {
							free(option_theme);
							option_theme = theme_file;
						}
					}
				}
				theme_install_close();
			}
			break;
		case ICON_THEME_INSTALL_CANCEL:
			if (pointer->buttons == wimp_CLICK_ADJUST)
				break;
			theme_install_close();
			break;
	}
}

static void theme_install_close(void) {
  	theme_install_active = false;
	if (theme_install_content)
		content_close(theme_install_content);
	theme_install_content = NULL;
	ro_gui_dialog_close(dialog_theme_install);
}

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Toolbar themes (implementation).
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "oslib/osfile.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/utils/utils.h"

static wimp_window *theme_toolbar_template = 0;
static osspriteop_area *theme_sprite_area = 0;
unsigned int theme_throbs;


/**
 * Load a theme from a directory.
 *
 * The directory must contain a Templates file containing the toolbar template,
 * and a Sprites file containing icons.
 */

void ro_theme_load(char *pathname)
{
	char name[] = "toolbar";
	int context, window_size, data_size, size, i;
	static char *data = 0;
	char filename[strlen(pathname) + 12];
	fileswitch_object_type obj_type;

	/* free old theme data */
	free(theme_toolbar_template);
	free(data);
	free(theme_sprite_area);

	/* load template */
	sprintf(filename, "%s.Templates", pathname);
	wimp_open_template(filename);

	/* find required buffer sizes */
	context = wimp_load_template(wimp_GET_SIZE, 0, 0, wimp_NO_FONTS,
			name, 0, &window_size, &data_size);
	assert(context != 0);

	theme_toolbar_template = xcalloc((unsigned int) window_size, 1);
	data = xcalloc((unsigned int) data_size, 1);

	/* load */
	wimp_load_template(theme_toolbar_template, data, data + data_size,
			wimp_NO_FONTS, name, 0, 0, 0);

	wimp_close_template();

	assert(ICON_TOOLBAR_RELOAD < theme_toolbar_template->icon_count);
	theme_toolbar_template->flags |= wimp_WINDOW_FURNITURE_WINDOW;
	theme_toolbar_template->icons[ICON_TOOLBAR_URL].data.indirected_text.size = 256;
	theme_toolbar_template->icons[ICON_TOOLBAR_STATUS].data.indirected_text.size = 256;

	/* load sprites */
	sprintf(filename, "%s.Sprites", pathname);
	obj_type = osfile_read_no_path(filename, 0, 0, &size, 0);
	assert(obj_type & fileswitch_IS_FILE);

	theme_sprite_area = xcalloc((unsigned int)(size + 16), 1);
	theme_sprite_area->size = size + 16;
	theme_sprite_area->sprite_count = 0;
	theme_sprite_area->first = 16;
	theme_sprite_area->used = 16;
	osspriteop_clear_sprites(osspriteop_USER_AREA, theme_sprite_area);
	osspriteop_load_sprite_file(osspriteop_USER_AREA, theme_sprite_area,
			filename);

	theme_toolbar_template->sprite_area = theme_sprite_area;
	theme_toolbar_template->icons[ICON_TOOLBAR_THROBBER].data.indirected_sprite.area =
			theme_sprite_area;

	/* find the highest sprite called throbber%i */
	theme_throbs = 0;
	for (i = 1; i <= theme_sprite_area->sprite_count; i++) {
		char name[32];
		osspriteop_return_name(osspriteop_USER_AREA,
				theme_sprite_area, name, 32, i);
		if (strncmp(name, "throbber", 8) == 0) {
			unsigned int n = atoi(name + 8);
			if (theme_throbs < n)
				theme_throbs = n;
		}
	}
}


/**
 * Create a toolbar from the current theme.
 *
 * The buffers url_buffer and status_buffer must be at least 256 bytes each,
 * throbber_buffer at least 12 bytes;
 */

wimp_w ro_theme_create_toolbar(char *url_buffer, char *status_buffer,
		char *throbber_buffer)
{
	wimp_w w;

	theme_toolbar_template->icons[ICON_TOOLBAR_URL].data.indirected_text.text = url_buffer;
	theme_toolbar_template->icons[ICON_TOOLBAR_STATUS].data.indirected_text.text = status_buffer;
	theme_toolbar_template->icons[ICON_TOOLBAR_THROBBER].data.indirected_sprite.id =
			(osspriteop_id) throbber_buffer;

	w = wimp_create_window(theme_toolbar_template);
	return w;
}


/**
 * Return the height of the current toolbar.
 */

int ro_theme_toolbar_height(void)
{
	return abs(theme_toolbar_template->extent.y1 - theme_toolbar_template->extent.y0);
}


/**
 * Resize the URL icon in a toolbar.
 */

void ro_theme_resize_toolbar(wimp_w w, int width, int height)
{
	wimp_icon_state ic;
	ic.w = w;
	ic.i = ICON_TOOLBAR_URL;
	wimp_get_icon_state(&ic);

	wimp_resize_icon(w, ICON_TOOLBAR_URL, ic.icon.extent.x0, ic.icon.extent.y0,
			width - 8, ic.icon.extent.y1);
	wimp_force_redraw(w, ic.icon.extent.x0, ic.icon.extent.y0,
			width, ic.icon.extent.y1);
}

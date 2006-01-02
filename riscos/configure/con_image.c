/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

#include <stdbool.h>
#include <swis.h>
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "netsurf/desktop/options.h"
#include "netsurf/riscos/configure/configure.h"
#include "netsurf/riscos/dialog.h"
#include "netsurf/riscos/menus.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/riscos/wimp_event.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


#define IMAGE_FOREGROUND_FIELD 3
#define IMAGE_FOREGROUND_MENU 4
#define IMAGE_BACKGROUND_FIELD 6
#define IMAGE_BACKGROUND_MENU 7
#define IMAGE_CURRENT_DISPLAY 8
#define IMAGE_DEFAULT_BUTTON 9
#define IMAGE_CANCEL_BUTTON 10
#define IMAGE_OK_BUTTON 11

static bool ro_gui_options_image_click(wimp_pointer *pointer);
static bool ro_gui_options_image_ok(wimp_w w);
static void ro_gui_options_image_redraw(wimp_draw *redraw);
static void ro_gui_options_image_update(wimp_w w, wimp_i i);
static void ro_gui_options_image_read(wimp_w w,unsigned int *bg, unsigned int *fg);

static osspriteop_area *example_images;
int example_users = 0;
unsigned int tinct_options[] = {tinct_USE_OS_SPRITE_OP, 0, tinct_DITHER,
		tinct_ERROR_DIFFUSE};

bool ro_gui_options_image_initialise(wimp_w w) {
	char pathname[256];
	int i;

	/* load the sprite file */
	if (example_users == 0) {
		snprintf(pathname, 256, "%s.Resources.Image", NETSURF_DIR);
		pathname[255] = '\0';
		example_images = ro_gui_load_sprite_file(pathname);
		if (!example_images)
			return false;
	}
	example_users++;

	/* set the current values */
	for (i = 0; (i < 4); i++) {
		if ((unsigned int)option_fg_plot_style == tinct_options[i])
			ro_gui_set_icon_string(w, IMAGE_FOREGROUND_FIELD,
					image_quality_menu->entries[i].
						data.indirected_text.text);
		if ((unsigned int)option_bg_plot_style == tinct_options[i])
			ro_gui_set_icon_string(w, IMAGE_BACKGROUND_FIELD,
					image_quality_menu->entries[i].
						data.indirected_text.text);
	}

	/* register icons */
	ro_gui_wimp_event_register_menu_gright(w, IMAGE_FOREGROUND_FIELD,
			IMAGE_FOREGROUND_MENU, image_quality_menu);
	ro_gui_wimp_event_register_menu_gright(w, IMAGE_BACKGROUND_FIELD,
			IMAGE_BACKGROUND_MENU, image_quality_menu);
	ro_gui_wimp_event_register_redraw_window(w,
			ro_gui_options_image_redraw);
	ro_gui_wimp_event_register_mouse_click(w,
			ro_gui_options_image_click);
	ro_gui_wimp_event_register_menu_selection(w,
			ro_gui_options_image_update);
	ro_gui_wimp_event_register_cancel(w, IMAGE_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, IMAGE_OK_BUTTON,
			ro_gui_options_image_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpImageConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}

void ro_gui_options_image_finalise(wimp_w w) {
	example_users--;
	if (example_users == 0) {
	  	free(example_images);
	  	example_images = NULL;
	}
	ro_gui_wimp_event_finalise(w);
}

void ro_gui_options_image_update(wimp_w w, wimp_i i) {
	ro_gui_redraw_icon(w, IMAGE_CURRENT_DISPLAY);
}

void ro_gui_options_image_redraw(wimp_draw *redraw) {
	osbool more;
	int origin_x, origin_y;
	os_error *error;
	wimp_icon_state icon_state;
	osspriteop_header *bg = NULL, *fg = NULL;
	unsigned int bg_tinct = 0, fg_tinct = 0;

	/* get the icon location */
	icon_state.w = redraw->w;
	icon_state.i = IMAGE_CURRENT_DISPLAY;
	error = xwimp_get_icon_state(&icon_state);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
		return;
	}
	
	/* find the sprites */
	if (example_images) {
		ro_gui_options_image_read(redraw->w, &bg_tinct, &fg_tinct);
		xosspriteop_select_sprite(osspriteop_USER_AREA,
				example_images, (osspriteop_id)"img_bg", &bg);
		xosspriteop_select_sprite(osspriteop_USER_AREA,
				example_images, (osspriteop_id)"img_fg", &fg);
	}

	/* perform the redraw */
	more = wimp_redraw_window(redraw);
	while (more) {
		origin_x = redraw->box.x0 - redraw->xscroll +
				icon_state.icon.extent.x0 + 2;
		origin_y = redraw->box.y1 - redraw->yscroll +
				icon_state.icon.extent.y0 + 2;
		if (bg)
			_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
					bg, origin_x, origin_y, bg_tinct);
		if (fg)
			_swix(Tinct_PlotAlpha, _IN(2) | _IN(3) | _IN(4) | _IN(7),
					fg, origin_x, origin_y, fg_tinct);
		more = wimp_get_rectangle(redraw);
	}
}

void ro_gui_options_image_read(wimp_w w, unsigned int *bg, unsigned int *fg) {
	char *text;
	int i;
	
	text = ro_gui_get_icon_string(w, IMAGE_FOREGROUND_FIELD);
	for (i = 0; (i < 4); i++)
		if (!strcmp(text, image_quality_menu->entries[i].
				data.indirected_text.text))
			*fg = tinct_options[i];
	text = ro_gui_get_icon_string(w, IMAGE_BACKGROUND_FIELD);
	for (i = 0; (i < 4); i++)
		if (!strcmp(text, image_quality_menu->entries[i].
				data.indirected_text.text))
			*bg = tinct_options[i];
}

bool ro_gui_options_image_click(wimp_pointer *pointer) {
	unsigned int old_fg, old_bg, bg, fg;
	
	ro_gui_options_image_read(pointer->w, &old_bg, &old_fg);
	switch (pointer->i) {
		case IMAGE_DEFAULT_BUTTON:
			ro_gui_set_icon_string(pointer->w, IMAGE_FOREGROUND_FIELD,
					image_quality_menu->entries[3].
						data.indirected_text.text);
  			ro_gui_set_icon_string(pointer->w, IMAGE_BACKGROUND_FIELD,
					image_quality_menu->entries[2].
						data.indirected_text.text);
			break;
		case IMAGE_CANCEL_BUTTON:
			ro_gui_wimp_event_restore(pointer->w);
			break;
		default:
			return false;
	}
	ro_gui_options_image_read(pointer->w, &bg, &fg);
	if ((bg != old_bg) || (fg != old_fg))
		ro_gui_options_image_update(pointer->w, pointer->i);
	return false;
}

bool ro_gui_options_image_ok(wimp_w w) {
	ro_gui_options_image_read(w, &option_bg_plot_style, &option_fg_plot_style);
	ro_gui_save_options();
	return true;
}

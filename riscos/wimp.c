/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * General RISC OS WIMP/OS library functions (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "oslib/colourtrans.h"
#include "oslib/os.h"
#include "oslib/osfile.h"
#include "oslib/wimp.h"
#include "oslib/wimpextend.h"
#include "oslib/wimpreadsysinfo.h"
#include "oslib/wimpspriteop.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utf8.h"
#include "netsurf/utils/utils.h"

static void ro_gui_wimp_cache_furniture_sizes(wimp_w w);
static bool ro_gui_wimp_read_eig_factors(os_mode mode, int *xeig, int *yeig);

static wimpextend_furniture_sizes furniture_sizes;
static wimp_w furniture_window = NULL;

/**
 * Gets the horzontal scrollbar height
 *
 * \param  w  the window to read (or NULL to read a cached value)
 */
int ro_get_hscroll_height(wimp_w w) {
	ro_gui_wimp_cache_furniture_sizes(w);
	return furniture_sizes.border_widths.y0;
}


/**
 * Gets the vertical scrollbar width
 *
 * \param  w  the window to read (or NULL to read a cached value)
 */
int ro_get_vscroll_width(wimp_w w) {
	ro_gui_wimp_cache_furniture_sizes(w);
	return furniture_sizes.border_widths.x1;
}

/**
 * Caches window furniture information
 *
 * \param  w  the window to cache information from
 * \return true on success, false on error (default values cached)
 */
void ro_gui_wimp_cache_furniture_sizes(wimp_w w) {
	os_error *error;

	if (!w)
		w = dialog_debug;
	if (furniture_window == w)
		return;
	furniture_window = w;
	furniture_sizes.w = w;
	furniture_sizes.border_widths.y0 = 40;
	furniture_sizes.border_widths.x1 = 40;
	error = xwimpextend_get_furniture_sizes(&furniture_sizes);
	if (error) {
		LOG(("xwimpextend_get_furniture_sizes: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Reads a modes EIG factors.
 *
 * \param  mode  mode to read EIG factors for, or -1 for current
 */
bool ro_gui_wimp_read_eig_factors(os_mode mode, int *xeig, int *yeig) {
	os_error *error;

	error = xos_read_mode_variable(mode, os_MODEVAR_XEIG_FACTOR, xeig, 0);
	if (error) {
		LOG(("xos_read_mode_variable: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		return false;
	}
	error = xos_read_mode_variable(mode, os_MODEVAR_YEIG_FACTOR, yeig, 0);
	if (error) {
		LOG(("xos_read_mode_variable: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		return false;
	}
	return true;
}


/**
 * Converts the supplied os_coord from OS units to pixels.
 *
 * \param  os_units  values to convert
 * \param  mode	     mode to use EIG factors for, or -1 for current
 */
void ro_convert_os_units_to_pixels(os_coord *os_units, os_mode mode) {
	int xeig = 1, yeig = 1;

	ro_gui_wimp_read_eig_factors(mode, &xeig, &yeig);
	os_units->x = ((os_units->x + (1 << xeig) - 1) >> xeig);
	os_units->y = ((os_units->y + (1 << yeig) - 1) >> yeig);
}


/**
 * Converts the supplied os_coord from pixels to OS units.
 *
 * \param  pixels  values to convert
 * \param  mode	   mode to use EIG factors for, or -1 for current
 */
void ro_convert_pixels_to_os_units(os_coord *pixels, os_mode mode) {
	int xeig = 1, yeig = 1;

	ro_gui_wimp_read_eig_factors(mode, &xeig, &yeig);
	pixels->x = (pixels->x << xeig);
	pixels->y = (pixels->y << yeig);
}


/**
 * Redraws an icon
 *
 * \param  w  window handle
 * \param  i  icon handle
 */

#define ro_gui_redraw_icon(w, i) xwimp_set_icon_state(w, i, 0, 0)


/**
 * Forces an icon to be redrawn entirely (ie not just updated).
 *
 * \param  w  window handle
 * \param  i  icon handle
 */
void ro_gui_force_redraw_icon(wimp_w w, wimp_i i) {
	wimp_icon_state ic;
	os_error *error;

	/*	Get the icon data
	*/
	ic.w = w;
	ic.i = i;
	error = xwimp_get_icon_state(&ic);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	error = xwimp_force_redraw(w, ic.icon.extent.x0, ic.icon.extent.y0,
			ic.icon.extent.x1, ic.icon.extent.y1);
	if (error) {
		LOG(("xwimp_force_redraw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Read the contents of an icon.
 *
 * \param  w  window handle
 * \param  i  icon handle
 * \return string in icon
 */
char *ro_gui_get_icon_string(wimp_w w, wimp_i i) {
	wimp_icon_state ic;
	os_error *error;

	ic.w = w;
	ic.i = i;
	error = xwimp_get_icon_state(&ic);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return NULL;
	}
	return ic.icon.data.indirected_text.text;
}


/**
 * Set the contents of an icon to a string.
 *
 * \param  w	 window handle
 * \param  i	 icon handle
 * \param  text  string (UTF-8 encoded) (copied)
 */
void ro_gui_set_icon_string(wimp_w w, wimp_i i, const char *text) {
	wimp_caret caret;
	wimp_icon_state ic;
	os_error *error;
	int old_len, len;
	char *local_text;
	utf8_convert_ret err;

	/* get the icon data */
	ic.w = w;
	ic.i = i;
	error = xwimp_get_icon_state(&ic);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* convert text to local encoding */
	err = utf8_to_local_encoding(text, 0, &local_text);
	if (err != UTF8_CONVERT_OK) {
		/* A bad encoding should never happen, so assert this */
		assert(err != UTF8_CONVERT_BADENC);
		LOG(("utf8_to_enc failed"));
		return;
	}

	/* check that the existing text is not the same as the updated text
	 * to stop flicker */
	if (ic.icon.data.indirected_text.size &&
			!strncmp(ic.icon.data.indirected_text.text,
			local_text,
			(unsigned int)ic.icon.data.indirected_text.size - 1))
		return;

	/* copy the text across */
	old_len = strlen(ic.icon.data.indirected_text.text);
	if (ic.icon.data.indirected_text.size) {
		strncpy(ic.icon.data.indirected_text.text, local_text,
			(unsigned int)ic.icon.data.indirected_text.size - 1);
		ic.icon.data.indirected_text.text[
				ic.icon.data.indirected_text.size - 1] = '\0';
	}

	/* handle the caret being in the icon */
	error = xwimp_get_caret_position(&caret);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if ((caret.w == w) && (caret.i == i)) {
		len = strlen(local_text);
		if ((caret.index > len) || (caret.index == old_len))
				caret.index = len;
		error = xwimp_set_caret_position(w, i, caret.pos.x, caret.pos.y,
				-1, caret.index);
		if (error) {
			LOG(("xwimp_set_caret_position: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
	ro_gui_redraw_icon(w, i);

	free(local_text);
}


/**
 * Set the contents of an icon to a number.
 *
 * \param  w	  window handle
 * \param  i	  icon handle
 * \param  value  value
 */
void ro_gui_set_icon_integer(wimp_w w, wimp_i i, int value) {
	char buffer[20]; // Big enough for 64-bit int
	sprintf(buffer, "%d", value);
	ro_gui_set_icon_string(w, i, buffer);
}


/**
 * Set the selected state of an icon.
 *
 * \param  w	 window handle
 * \param  i	 icon handle
 * \param  state selected state
 */
void ro_gui_set_icon_selected_state(wimp_w w, wimp_i i, bool state) {
	os_error *error;
	if (ro_gui_get_icon_selected_state(w, i) == state) return;
	error = xwimp_set_icon_state(w, i,
			(state ? wimp_ICON_SELECTED : 0), wimp_ICON_SELECTED);
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}

/**
 * Gets the selected state of an icon.
 *
 * \param  w	 window handle
 * \param  i	 icon handle
 */
bool ro_gui_get_icon_selected_state(wimp_w w, wimp_i i) {
	os_error *error;
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	error = xwimp_get_icon_state(&ic);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	return ((ic.icon.flags & wimp_ICON_SELECTED) != 0);
}


/**
 * Set the shaded state of an icon.
 *
 * \param  w	 window handle
 * \param  i	 icon handle
 * \param  state selected state
 */
void ro_gui_set_icon_shaded_state(wimp_w w, wimp_i i, bool state) {
	os_error *error;
	if (ro_gui_get_icon_shaded_state(w, i) == state) return;
	error = xwimp_set_icon_state(w, i,
			(state ? wimp_ICON_SHADED : 0), wimp_ICON_SHADED);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Gets the shaded state of an icon.
 *
 * \param  w	 window handle
 * \param  i	 icon handle
 */
bool ro_gui_get_icon_shaded_state(wimp_w w, wimp_i i) {
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	xwimp_get_icon_state(&ic);
	return (ic.icon.flags & wimp_ICON_SHADED) != 0;
}


/**
 * Set the button type of an icon.
 *
 * \param  w	window handle
 * \param  i	icon handle
 * \param  type button type
 */
void ro_gui_set_icon_button_type(wimp_w w, wimp_i i, int type) {
	os_error *error;
	error = xwimp_set_icon_state(w, i, wimp_ICON_BUTTON_TYPE,
			(type << wimp_ICON_BUTTON_TYPE_SHIFT));
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Set an icon's sprite
 *
 * \param  w	window handle
 * \param  i	icon handle
 * \param  area sprite area containing sprite
 * \param  name name of sprite in area (in local encoding)
 */
void ro_gui_set_icon_sprite(wimp_w w, wimp_i i, osspriteop_area *area,
		const char *name)
{
	wimp_icon_state ic;
	os_error *error;

	/* get the icon data */
	ic.w = w;
	ic.i = i;
	error = xwimp_get_icon_state(&ic);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* copy the name across */
	if (ic.icon.data.indirected_text.size) {
		strncpy(ic.icon.data.indirected_text.text, name,
			(unsigned int)ic.icon.data.indirected_text.size - 1);
		ic.icon.data.indirected_text.text[
				ic.icon.data.indirected_text.size - 1] = '\0';
	}

	ic.icon.data.indirected_sprite.area = area;

	ro_gui_redraw_icon(w, i);
}


/**
 * Set a window title
 *
 * \param  w	 window handle
 * \param  text  new title (copied)
 */
void ro_gui_set_window_title(wimp_w w, const char *text) {
	wimp_window_info_base window;
	os_error *error;
	char *title_local_enc;
	utf8_convert_ret err;

	/*	Get the window details
	*/
	window.w = w;
	error = xwimp_get_window_info_header_only((wimp_window_info *)&window);
	if (error) {
		LOG(("xwimp_get_window_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* convert text to local encoding */
	err = utf8_to_local_encoding(text, 0, &title_local_enc);
	if (err != UTF8_CONVERT_OK) {
		/* A bad encoding should never happen,
		 * so assert this */
		assert(err != UTF8_CONVERT_BADENC);
		LOG(("utf8_to_enc failed"));
		return;
	}

	/*	Set the title string
	*/
	strncpy(window.title_data.indirected_text.text, title_local_enc,
			(unsigned int)window.title_data.indirected_text.size
					- 1);
	window.title_data.indirected_text.text[
			window.title_data.indirected_text.size - 1] = '\0';

	/*	Redraw accordingly
	*/
	error = xwimp_force_redraw_title(w);
	if (error) {
		LOG(("xwimp_force_redraw_title: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	free(title_local_enc);
}


/**
 * Places the caret in the first available icon
 */
void ro_gui_set_caret_first(wimp_w w) {
	int icon, b;
	wimp_window_state win_state;
	wimp_window_info_base window;
	wimp_icon_state state;
	os_error *error;

	/* check the window is open */
	win_state.w = w;
	error = xwimp_get_window_state(&win_state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if (!(win_state.flags & wimp_WINDOW_OPEN))
		return;

	/* get the window details for the icon count */
	window.w = w;
	error = xwimp_get_window_info_header_only((wimp_window_info *)&window);
	if (error) {
		LOG(("xwimp_get_window_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* work through all the icons */
	state.w = w;
	for (icon = 0; icon < window.icon_count; icon++) {
		state.i = icon;
		error = xwimp_get_icon_state(&state);
		if (error) {
			LOG(("xwimp_get_icon_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}

		/* ignore if it's shaded or not writable */
		if (state.icon.flags & wimp_ICON_SHADED)
			continue;
		b = (state.icon.flags >> wimp_ICON_BUTTON_TYPE_SHIFT) & 0xf;
		if ((b != wimp_BUTTON_WRITE_CLICK_DRAG) &&
				(b != wimp_BUTTON_WRITABLE))
			continue;

		/* move the caret */
		error = xwimp_set_caret_position(w, icon, 0, 0, -1,
				strlen(state.icon.data.indirected_text.text));
		if (error) {
			LOG(("xwimp_set_caret_position: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		return;
	}
}


/**
 * Opens a window at the centre of either another window or the screen
 *
 * /param parent the parent window (NULL for centre of screen)
 * /param child the child window
 */
void ro_gui_open_window_centre(wimp_w parent, wimp_w child) {
	os_error *error;
	wimp_window_state state;
	int mid_x, mid_y;
	int dimension, scroll_width;

	/* get the parent window state */
	if (parent) {
		state.w = parent;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
		scroll_width = ro_get_vscroll_width(parent);
		mid_x = (state.visible.x0 + state.visible.x1 + scroll_width);
		mid_y = (state.visible.y0 + state.visible.y1);
	} else {
		ro_gui_screen_size(&mid_x, &mid_y);
	}
	mid_x /= 2;
	mid_y /= 2;

	/* get the child window state */
	state.w = child;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* move to the centre of the parent at the top of the stack */
	dimension = state.visible.x1 - state.visible.x0;
	scroll_width = ro_get_vscroll_width(history_window);
	state.visible.x0 = mid_x - (dimension + scroll_width) / 2;
	state.visible.x1 = state.visible.x0 + dimension;
	dimension = state.visible.y1 - state.visible.y0;
	state.visible.y0 = mid_y - dimension / 2;
	state.visible.y1 = state.visible.y0 + dimension;
	state.next = wimp_TOP;
	error = xwimp_open_window((wimp_open *) &state);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
}


/**
 * Load a sprite file into memory.
 *
 * \param  pathname  file to load
 * \return  sprite area, or 0 on memory exhaustion or error and error reported
 */

osspriteop_area *ro_gui_load_sprite_file(const char *pathname)
{
	int len;
	fileswitch_object_type obj_type;
	osspriteop_area *area;
	os_error *error;

	error = xosfile_read_stamped_no_path(pathname,
			&obj_type, 0, 0, &len, 0, 0);
	if (error) {
		LOG(("xosfile_read_stamped_no_path: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		return 0;
	}
	if (obj_type != fileswitch_IS_FILE) {
		warn_user("FileError", pathname);
		return 0;
	}

	area = malloc(len + 4);
	if (!area) {
		warn_user("NoMemory", 0);
		return 0;
	}

	area->size = len + 4;
	area->sprite_count = 0;
	area->first = 16;
	area->used = 16;

	error = xosspriteop_load_sprite_file(osspriteop_USER_AREA,
			area, pathname);
	if (error) {
		LOG(("xosspriteop_load_sprite_file: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		free(area);
		return 0;
	}

	return area;
}


/**
 * Check if a sprite is present in the Wimp sprite pool.
 *
 * \param  sprite  name of sprite
 * \return  true if the sprite is present
 */

bool ro_gui_wimp_sprite_exists(const char *sprite)
{
	os_error *error;

	error = xwimpspriteop_select_sprite(sprite, 0);
	if (error) {
		if (error->errnum != error_SPRITE_OP_DOESNT_EXIST) {
			LOG(("xwimpspriteop_select_sprite: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
		}
		return false;
	}
	return true;
}


/**
 * Locate a sprite in the Wimp sprite pool, returning a pointer to it.
 *
 * \param  name    sprite name
 * \param  sprite  receives pointer to sprite if found
 * \return error ptr iff not found
 */

os_error *ro_gui_wimp_get_sprite(const char *name, osspriteop_header **sprite)
{
	osspriteop_area *rom_base, *ram_base;
	os_error *error;

	error = xwimp_base_of_sprites(&rom_base, &ram_base);
	if (error) return error;

	error = xosspriteop_select_sprite(osspriteop_USER_AREA,
			rom_base, (osspriteop_id)name, sprite);

	if (error && error->errnum == error_SPRITE_OP_DOESNT_EXIST)
		error = xosspriteop_select_sprite(osspriteop_USER_AREA,
				ram_base, (osspriteop_id)name, sprite);

	return error;
}


/**
 * Open a window as a pane in another window.
 *
 * \param  parent  parent window
 * \param  pane	   pane to open in parent window
 * \param  offset  inset of pane from parent
 */

void ro_gui_open_pane(wimp_w parent, wimp_w pane, int offset)
{
	wimp_window_state state;
	os_error *error;

	state.w = parent;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	state.w = pane;
	state.visible.x0 += offset;
	state.visible.x1 -= offset;
	state.visible.y0 += offset;
	state.visible.y1 -= offset;
	state.xscroll = 0;
	state.yscroll = 0;
	state.next = wimp_TOP;
	error = xwimp_open_window_nested((wimp_open *) &state, parent,
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_XORIGIN_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_YORIGIN_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_LS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_BS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_RS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_TS_EDGE_SHIFT);
	if (error) {
		LOG(("xwimp_open_window_nested: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
}


/**
 * Performs simple user redraw for a window.
 *
 * \param  user_fill	whether to fill the redraw area
 * \param  user_colour  the colour to use when filling
 */

void ro_gui_user_redraw(wimp_draw *redraw, bool user_fill,
		os_colour user_colour) {
	os_error *error;
	osbool more;

	error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	while (more) {
		if (user_fill) {
			error = xcolourtrans_set_gcol(user_colour,
					colourtrans_SET_BG,
					os_ACTION_OVERWRITE, 0, 0);
			if (error) {
				LOG(("xcolourtrans_set_gcol: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("MiscError", error->errmess);
			}
			os_clg();
		}
		error = xwimp_get_rectangle(redraw, &more);
		if (error) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
	}
}


/**
 * Sets whether a piece of window furniture is present for a window.
 *
 * \param  w	     the window to modify
 * \param  bic_mask  the furniture flags to clear
 * \param  xor_mask  the furniture flags to toggle
 */
void ro_gui_wimp_update_window_furniture(wimp_w w, wimp_window_flags bic_mask,
		wimp_window_flags xor_mask) {
	wimp_window_state state;
	os_error *error;
	bool open;

	state.w = w;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	open = state.flags & wimp_WINDOW_OPEN;
	state.flags &= ~(63 << 16); /* clear bits 16-21 */
	state.flags &= ~bic_mask;
	state.flags ^= xor_mask;
	if (!open)
		state.next = wimp_HIDDEN;
	error = xwimp_open_window_nested_with_flags(&state, (wimp_w)-1, 0);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (!open) {
		error = xwimp_close_window(w);
		if (error) {
			LOG(("xwimp_close_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
	}
}


/**
 * Checks whether a piece of window furniture is present for a window.
 *
 * \param  w	     the window to modify
 * \param  mask      the furniture flags to check
 */
bool ro_gui_wimp_check_window_furniture(wimp_w w, wimp_window_flags mask) {
	wimp_window_state state;
	os_error *error;

	state.w = w;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	return state.flags & mask;
}

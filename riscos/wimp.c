/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * General RISC OS WIMP/OS library functions (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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
#include "netsurf/utils/utils.h"

/*	Wimp_Extend,11 block
*/
static wimpextend_furniture_sizes furniture_sizes;


/**
 * Gets the horzontal scrollbar height
 */
int ro_get_hscroll_height(wimp_w w) {
	wimp_version_no version;

  	/*	Read the hscroll height
  	*/
  	if (!w) w = dialog_debug;
  	furniture_sizes.w = w;
	furniture_sizes.border_widths.y0 = 38;
	xwimpextend_get_furniture_sizes(&furniture_sizes);

	/*	There is a quirk with the returned size as it differs between versions of the
		WindowManager module. The incorrect height is returned by the version distributed
		with the universal boot sequence (3.98) and presumably any previous version.
	*/
	if (!xwimpreadsysinfo_version(&version)) {
		if ((int)version <= 398) {
			return furniture_sizes.border_widths.y0 + 2;
		}
	}

	/*	Return the standard (unhacked) size
	*/
	return furniture_sizes.border_widths.y0;
}


/**
 * Gets the vertical scrollbar width
 */
int ro_get_vscroll_width(wimp_w w) {

  	/*	Read the hscroll height
  	*/
  	if (!w) w = dialog_debug;
  	furniture_sizes.w = w;
	furniture_sizes.border_widths.x1 = 38;
	xwimpextend_get_furniture_sizes(&furniture_sizes);

	/*	Return the standard (unhacked) size
	*/
	return furniture_sizes.border_widths.x1;
}


/**
 * Reads a modes EIG factors.
 *
 * \param  mode  mode to read EIG factors for, or -1 for current
 */
struct eig_factors ro_read_eig_factors(os_mode mode) {
	bits psr;
	struct eig_factors factors;
	xos_read_mode_variable(mode, os_MODEVAR_XEIG_FACTOR, &factors.xeig, &psr);
	xos_read_mode_variable(mode, os_MODEVAR_YEIG_FACTOR, &factors.yeig, &psr);
	return factors;
}


/**
 * Converts the supplied os_coord from OS units to pixels.
 *
 * \param  os_units  values to convert
 * \param  mode      mode to use EIG factors for, or -1 for current
 */
void ro_convert_os_units_to_pixels(os_coord *os_units, os_mode mode) {
	struct eig_factors factors = ro_read_eig_factors(mode);
	os_units->x = (os_units->x >> factors.xeig);
	os_units->y = (os_units->y >> factors.yeig);
}


/**
 * Converts the supplied os_coord from pixels to OS units.
 *
 * \param  pixels  values to convert
 * \param  mode    mode to use EIG factors for, or -1 for current
 */
void ro_convert_pixels_to_os_units(os_coord *pixels, os_mode mode) {
	struct eig_factors factors = ro_read_eig_factors(mode);
	pixels->x = (pixels->x << factors.xeig);
	pixels->y = (pixels->y << factors.yeig);
}


/**
 * Redraws an icon
 *
 * \param  w  window handle
 * \param  i  icon handle
 */
#define ro_gui_redraw_icon(w, i) xwimp_set_icon_state(w, i, 0, 0)


/**
 * Read the contents of an icon.
 *
 * \param  w  window handle
 * \param  i  icon handle
 * \return string in icon
 */
char *ro_gui_get_icon_string(wimp_w w, wimp_i i) {
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	if (xwimp_get_icon_state(&ic)) return NULL;
	return ic.icon.data.indirected_text.text;
}


/**
 * Set the contents of an icon to a string.
 *
 * \param  w     window handle
 * \param  i     icon handle
 * \param  text  string (copied)
 */
void ro_gui_set_icon_string(wimp_w w, wimp_i i, const char *text) {
	wimp_caret caret;
	wimp_icon_state ic;
	int old_len, len;

	/*	Get the icon data
	*/
	ic.w = w;
	ic.i = i;
	if (xwimp_get_icon_state(&ic))
		return;

	/*	Check that the existing text is not the same as the updated text
		to stop flicker
	*/
	if (ic.icon.data.indirected_text.size
	    && !strncmp(ic.icon.data.indirected_text.text, text,
			(unsigned int)ic.icon.data.indirected_text.size - 1))
		return;

	/*	Copy the text across
	*/
	old_len = strlen(ic.icon.data.indirected_text.text);
	if (ic.icon.data.indirected_text.size) {
		strncpy(ic.icon.data.indirected_text.text, text,
			(unsigned int)ic.icon.data.indirected_text.size - 1);
		ic.icon.data.indirected_text.text[ic.icon.data.indirected_text.size - 1] = '\0';
	}

	/*	Handle the caret being in the icon
	*/
	if (!xwimp_get_caret_position(&caret)) {
		if ((caret.w == w) && (caret.i == i)) {
		  	len = strlen(text);
		  	if ((caret.index > len) || (caret.index == old_len)) caret.index = len;
			xwimp_set_caret_position(w, i, caret.pos.x, caret.pos.y, -1, caret.index);
		}
	}

	/*	Redraw the icon
	*/
	ro_gui_redraw_icon(w, i);
}


/**
 * Set the contents of an icon to a number.
 *
 * \param  w      window handle
 * \param  i      icon handle
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
 * \param  w     window handle
 * \param  i     icon handle
 * \param  state selected state
 */
#define ro_gui_set_icon_selected_state(w, i, state) \
		xwimp_set_icon_state(w, i, (state ? wimp_ICON_SELECTED : 0), wimp_ICON_SELECTED)


/**
 * Gets the selected state of an icon.
 *
 * \param  w     window handle
 * \param  i     icon handle
 */
bool ro_gui_get_icon_selected_state(wimp_w w, wimp_i i) {
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	xwimp_get_icon_state(&ic);
	return (ic.icon.flags & wimp_ICON_SELECTED) != 0;
}


/**
 * Set the selected state of an icon.
 *
 * \param  w     window handle
 * \param  i     icon handle
 * \param  state selected state
 */
#define ro_gui_set_icon_shaded_state(w, i, state) \
		xwimp_set_icon_state(w, i, (state ? wimp_ICON_SHADED : 0), wimp_ICON_SHADED)


/**
 * Gets the shaded state of an icon.
 *
 * \param  w     window handle
 * \param  i     icon handle
 */
bool ro_gui_get_icon_shaded_state(wimp_w w, wimp_i i) {
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	xwimp_get_icon_state(&ic);
	return (ic.icon.flags & wimp_ICON_SHADED) != 0;
}


/**
 * Set a window title (does *not* redraw the title)
 *
 * \param  w     window handle
 * \param  text  new title (copied)
 */
void ro_gui_set_window_title(wimp_w w, const char *text) {
	wimp_window_info_base window;
	os_error *error;

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

	/*	Set the title string
	*/
	strncpy(window.title_data.indirected_text.text, text,
			(unsigned int)window.title_data.indirected_text.size - 1);
	window.title_data.indirected_text.text[window.title_data.indirected_text.size - 1] = '\0';
}


/**
 * Places the caret in the first available icon
 */
void ro_gui_set_caret_first(wimp_w w) {
  	int icon, button;
	wimp_window_state win_state;
	wimp_window_info_base window;
	wimp_icon_state state;
	os_error *error;

	/*	Check the window is open
	*/
	win_state.w = w;
	error = xwimp_get_window_state(&win_state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if (!(win_state.flags & wimp_WINDOW_OPEN)) return;
	
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

	/*	Work through our icons
	*/
	state.w = w;
	for (icon = 0; icon < window.icon_count; icon++) {
	  	/*	Get the icon state
	  	*/
		state.i = icon;
		error = xwimp_get_icon_state(&state);
		if (error) {
			LOG(("xwimp_get_icon_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}

		/*	Ignore if it's shaded
		*/
		if (state.icon.flags & wimp_ICON_SHADED)
			continue;

		/*	Check if it's writable
		*/
		button = (state.icon.flags >> wimp_ICON_BUTTON_TYPE_SHIFT) & 0xf;
		if ((button == wimp_BUTTON_WRITE_CLICK_DRAG) ||
				(button == wimp_BUTTON_WRITABLE)) {
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

	/*	Get the parent window state
	*/
	if (parent) {
		state.w = parent;
		error = xwimp_get_window_state(&state);
		if (error) {
			warn_user("WimpError", error->errmess);
			return;
		}
		scroll_width = ro_get_vscroll_width(parent);

		/*	Get the centre of the parent
		*/
		mid_x = (state.visible.x0 + state.visible.x1 + scroll_width) / 2;
 		mid_y = (state.visible.y0 + state.visible.y1) / 2;
 	} else {
		ro_gui_screen_size(&mid_x, &mid_y);
		mid_x /= 2;
		mid_y /= 2;
 	}

	/*	Get the child window state
	*/
	state.w = child;
	error = xwimp_get_window_state(&state);
	if (error) {
		warn_user("WimpError", error->errmess);
		return;
	}

	/*	Move to the centre of the parent at the top of the stack
	*/
	dimension = state.visible.x1 - state.visible.x0;
	scroll_width = ro_get_vscroll_width(hotlist_window);
	state.visible.x0 = mid_x - (dimension + scroll_width) / 2;
	state.visible.x1 = state.visible.x0 + dimension;
	dimension = state.visible.y1 - state.visible.y0;
	state.visible.y0 = mid_y - dimension / 2;
	state.visible.y1 = state.visible.y0 + dimension;
	state.next = wimp_TOP;
	wimp_open_window((wimp_open *) &state);
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
 * Open a window as a pane in another window.
 *
 * \param  parent  parent window
 * \param  pane    pane to open in parent window
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

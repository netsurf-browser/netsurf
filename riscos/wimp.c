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
#define ro_gui_set_icon_selected_state(w, i, state) xwimp_set_icon_state(w, i, (state ? wimp_ICON_SELECTED : 0), wimp_ICON_SELECTED)


/**
 * Gets the selected state of an icon.
 *
 * \param  w     window handle
 * \param  i     icon handle
 */
int ro_gui_get_icon_selected_state(wimp_w w, wimp_i i) {
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
#define ro_gui_set_icon_shaded_state(w, i, state) xwimp_set_icon_state(w, i, (state ? wimp_ICON_SHADED : 0), wimp_ICON_SHADED)


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
	wimp_window_info_base window;
	wimp_icon_state state;
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

	/*	Work through our icons
	*/
	state.w = w;
	for (icon = 0; icon < window.icon_count; icon++) {
	  	/*	Get the icon state
	  	*/
		state.i = icon;
		error = xwimp_get_icon_state(&state);
		if (error) {
			LOG(("xwimp_get_window_info: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
		
		/*	Check if it's writable
		*/
		button = (state.icon.flags >> wimp_ICON_BUTTON_TYPE_SHIFT) & 0xf;
		if ((button == wimp_BUTTON_WRITE_CLICK_DRAG) || 
				(button == wimp_BUTTON_WRITABLE)) {
			xwimp_set_caret_position(w, icon, 0, 0, -1, strlen(state.icon.data.indirected_text.text));
			return;		  
		}
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

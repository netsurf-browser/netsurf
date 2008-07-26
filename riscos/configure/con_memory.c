/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
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
#include "desktop/options.h"
#include "riscos/bitmap.h"
#include "riscos/options.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/dialog.h"
#include "riscos/configure/configure.h"


#define MEMORY_DIRECT_FIELD 3
#define MEMORY_DIRECT_DEC 4
#define MEMORY_DIRECT_INC 5
#define MEMORY_DIRECT_TEXT 6
#define MEMORY_DIRECT_AUTO 7
#define MEMORY_COMPRESSED_FIELD 9
#define MEMORY_COMPRESSED_DEC 10
#define MEMORY_COMPRESSED_INC 11
#define MEMORY_COMPRESSED_TEXT 12
#define MEMORY_COMPRESSED_AUTO 13
#define MEMORY_DEFAULT_BUTTON 14
#define MEMORY_CANCEL_BUTTON 15
#define MEMORY_OK_BUTTON 16

static bool ro_gui_options_memory_click(wimp_pointer *pointer);
static bool ro_gui_options_memory_ok(wimp_w w);
static void ro_gui_options_update_shading(wimp_w w);

bool ro_gui_options_memory_initialise(wimp_w w)
{
	/* set the current values */
	ro_gui_set_icon_decimal(w, MEMORY_DIRECT_FIELD,
			(bitmap_direct_size * 10) >> 20, 1);
	ro_gui_set_icon_decimal(w, MEMORY_COMPRESSED_FIELD,
			(bitmap_compressed_size * 10) >> 20, 1);
	ro_gui_set_icon_selected_state(w, MEMORY_DIRECT_AUTO,
			(option_image_memory_direct == -1));
	ro_gui_set_icon_selected_state(w, MEMORY_COMPRESSED_AUTO,
			(option_image_memory_compressed == -1));
	ro_gui_options_update_shading(w);

	/* register icons */
	ro_gui_wimp_event_register_checkbox(w, MEMORY_DIRECT_AUTO);
	ro_gui_wimp_event_register_checkbox(w, MEMORY_COMPRESSED_AUTO);
	ro_gui_wimp_event_register_text_field(w, MEMORY_DIRECT_TEXT);
	ro_gui_wimp_event_register_text_field(w, MEMORY_COMPRESSED_TEXT);
	ro_gui_wimp_event_register_numeric_field(w, MEMORY_DIRECT_FIELD,
			MEMORY_DIRECT_INC, MEMORY_DIRECT_DEC,
			10, 5120, 10, 1);
	ro_gui_wimp_event_register_numeric_field(w, MEMORY_COMPRESSED_FIELD,
			MEMORY_COMPRESSED_INC, MEMORY_COMPRESSED_DEC,
			10, 5120, 10, 1);
	ro_gui_wimp_event_register_mouse_click(w,
			ro_gui_options_memory_click);
	ro_gui_wimp_event_register_cancel(w, MEMORY_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, MEMORY_OK_BUTTON,
			ro_gui_options_memory_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpMemoryConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}

bool ro_gui_options_memory_click(wimp_pointer *pointer)
{
	switch (pointer->i) {
		case MEMORY_DIRECT_AUTO:
			ro_gui_options_update_shading(pointer->w);
			return false;
		case MEMORY_COMPRESSED_AUTO:
			ro_gui_options_update_shading(pointer->w);
			return false;
		case MEMORY_DEFAULT_BUTTON:
			ro_gui_set_icon_decimal(pointer->w, MEMORY_DIRECT_FIELD,
					(bitmap_direct_size * 10) >> 20, 1);
			ro_gui_set_icon_decimal(pointer->w, MEMORY_COMPRESSED_FIELD,
					(bitmap_compressed_size * 10) >> 20, 1);
			ro_gui_set_icon_selected_state(pointer->w,
					MEMORY_DIRECT_AUTO, true);
			ro_gui_set_icon_selected_state(pointer->w,
					MEMORY_COMPRESSED_AUTO, true);
			ro_gui_options_update_shading(pointer->w);
			return true;
	}
	return false;
}

void ro_gui_options_update_shading(wimp_w w)
{
	bool shaded;

	shaded = ro_gui_get_icon_selected_state(w, MEMORY_DIRECT_AUTO);
	ro_gui_set_icon_shaded_state(w, MEMORY_DIRECT_FIELD, shaded);
	ro_gui_set_icon_shaded_state(w, MEMORY_DIRECT_INC, shaded);
	ro_gui_set_icon_shaded_state(w, MEMORY_DIRECT_DEC, shaded);
	ro_gui_set_icon_shaded_state(w, MEMORY_DIRECT_TEXT, shaded);
	shaded = ro_gui_get_icon_selected_state(w, MEMORY_COMPRESSED_AUTO);
	ro_gui_set_icon_shaded_state(w, MEMORY_COMPRESSED_FIELD, shaded);
	ro_gui_set_icon_shaded_state(w, MEMORY_COMPRESSED_INC, shaded);
	ro_gui_set_icon_shaded_state(w, MEMORY_COMPRESSED_DEC, shaded);
	ro_gui_set_icon_shaded_state(w, MEMORY_COMPRESSED_TEXT, shaded);
}

bool ro_gui_options_memory_ok(wimp_w w)
{
	/* set the option values */
	if (ro_gui_get_icon_selected_state(w, MEMORY_DIRECT_AUTO))
		option_image_memory_direct = -1;
	else
		option_image_memory_direct =
			(((ro_gui_get_icon_decimal(w, MEMORY_DIRECT_FIELD, 1)
				<< 10) + 1023) / 10);
	if (ro_gui_get_icon_selected_state(w, MEMORY_COMPRESSED_AUTO))
		option_image_memory_compressed = -1;
	else
		option_image_memory_compressed =
			(((ro_gui_get_icon_decimal(w, MEMORY_COMPRESSED_FIELD, 1)
				<< 10) + 1023) / 10);
	/* update the memory usage */
	bitmap_initialise_memory();
	ro_gui_set_icon_decimal(w, MEMORY_DIRECT_FIELD,
			(bitmap_direct_size * 10) >> 20, 1);
	ro_gui_set_icon_decimal(w, MEMORY_COMPRESSED_FIELD,
			(bitmap_compressed_size * 10) >> 20, 1);
	/* save the options */
	ro_gui_save_options();
	return true;
}

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/colourtrans.h"
#include "oslib/os.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "netsurf/riscos/buffer.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"

/*	SCREEN BUFFERING
	================

	Because RISC OS provides no native way for windows to be buffered (ie
	the contents is only updated when the task has finished doing any
	drawing) certain situation cause the window contents to flicker in an
	undesirable manner. Examples of this are GIF and MNG animations, and
	web pages with fixed backgrounds.

	To overcome this, a very simple, transparent, interface is provided here
	to allow for output to be buffered. It should be noted that screen
	buffering can lower the perceived client response time as the user is
	unable to see that the application is doing anything.

	[rjw] - Mon 19th July 2004
*/


/** The current buffer
*/
static osspriteop_area *buffer = NULL;

/** The sprite name
*/
static char name[12];

/** The current clip area
*/
static os_box clipping;

/** The current save area
*/
static osspriteop_save_area *save_area;
static osspriteop_area *context1;
static osspriteop_id context2;
static osspriteop_save_area *context3;


/**
 * Opens a buffer for writing to.
 *
 * \param redraw the current WIMP redraw area to buffer
 */
void ro_gui_buffer_open(wimp_draw *redraw) {
	int size;
	int orig_x0, orig_y0;
	int buffer_size;
	os_coord sprite_size;
	int bpp, word_size;
	osbool palette;
	os_error *error;

	/*	Close any open buffer
	*/
	if (buffer) ro_gui_buffer_close();

	/*	Store our clipping region
	*/
	clipping = redraw->clip;

	/*	Work out how much buffer we need
	*/
	sprite_size.x = clipping.x1 - clipping.x0 + 1;
	sprite_size.y = clipping.y1 - clipping.y0 + 1;
	ro_convert_os_units_to_pixels(&sprite_size, (os_mode)-1);

	/*	Get the screen depth as we can't use palettes for >8bpp
	*/
	xos_read_mode_variable((os_mode)-1, os_MODEVAR_LOG2_BPP, &bpp, 0);
	palette = (bpp < 4) ? 1 : 0;

	/*	Create our buffer
	*/
	word_size = (((sprite_size.x << bpp) >> 3) + 32) & ~3;
	buffer_size = sizeof(osspriteop_area) + sizeof(osspriteop_header) +
			(word_size * sprite_size.y);
	if (palette) buffer_size += ((1 << (1 << bpp)) << 3);
	buffer = malloc(buffer_size);
	if (!buffer) {
	  	LOG(("Buffer memory allocation failed."));
		return;
	}
	/*	Fill in the sprite area details
	*/
	buffer->size = buffer_size - sizeof(osspriteop_area);
	buffer->sprite_count = 0;
	buffer->first = 16;
	buffer->used = 16;

	/*	Fill in the sprite header details
	*/
	sprintf(name, "buffer");
	if ((error = xosspriteop_get_sprite_user_coords(osspriteop_NAME,
			buffer, name, palette,
			clipping.x0, clipping.y0,
			clipping.x1, clipping.y1)) != NULL) {
//		LOG(("Grab error '%s'", error->errmess));
		free(buffer);
		buffer = NULL;
		return;
	}

	/*	Allocate OS_SpriteOp save area
	*/
	if ((error = xosspriteop_read_save_area_size(osspriteop_NAME,
			buffer, (osspriteop_id)name, &size)) != NULL) {
//		LOG(("Save area error '%s'", error->errmess));
		free(buffer);
		buffer = NULL;
		return;
	}
	if ((save_area = malloc((size_t)size)) == NULL) {
		free(buffer);
		buffer = NULL;
		return;
	}
	save_area->a[0] = 0;

	/*	Switch output to sprite
	*/
	if ((error = xosspriteop_switch_output_to_sprite(osspriteop_NAME,
			buffer, (osspriteop_id)name, save_area, 0,
			(int *)&context1, (int *)&context2,
			(int *)&context3)) != NULL) {
//		LOG(("Switching error '%s'", error->errmess));
		free(save_area);
		free(buffer);
		buffer = NULL;
		return;
	}

	/*	Move the origin such that (x0, y0) becomes (0, 0). To do this
		we use VDU 29,(1 << 16) - x0; (1 << 16) - y0; because RISC OS
		is so insanely legacy driven.
	*/
	orig_x0 = (1 << 16) - clipping.x0;
	orig_y0 = (1 << 16) - clipping.y0;
	os_writec((char)29);
	os_writec(orig_x0 & 0xff); os_writec(orig_x0 >> 8);
	os_writec(orig_y0 & 0xff); os_writec(orig_y0 >> 8);
}


/**
 * Closes any open buffer and flushes the contents to screen
 */
void ro_gui_buffer_close(void) {

	/*	Check we have an open buffer
	*/
	if (!buffer) return;

	/*	Remove any redirection and origin hacking
	*/
	xosspriteop_switch_output_to_sprite(osspriteop_PTR,
			context1, context2, context3,
			0, 0, 0, 0);
	free(save_area);

	/*	Plot the contents to screen
	*/
	xosspriteop_put_sprite_user_coords(osspriteop_NAME,
		buffer, (osspriteop_id)name,
		clipping.x0, clipping.y0, (os_action)0);

	/*	Free our memory
	*/
	free(buffer);
	buffer = NULL;
}

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Page thumbnail creation (implementation).
 *
 * Thumbnails are created by redirecting output to a sprite and rendering the
 * page at a small scale.
 */

#include <string.h>
#include <swis.h>
#include "oslib/colourtrans.h"
#include "oslib/osfile.h"
#include "oslib/osspriteop.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/render/font.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/thumbnail.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/utils/log.h"


/*	Whether we can use 32bpp sprites
*/
static int thumbnail_32bpp_available = -1;


/*	Sprite output context saving
*/
struct thumbnail_save_area {
	osspriteop_save_area *save_area;
	int context1;
	int context2;
	int context3;
};


/*	Internal prototypes
*/
static void thumbnail_test(void);
static struct thumbnail_save_area* thumbnail_switch_output(osspriteop_area *sprite_area,
					osspriteop_header *sprite_header);
static void thumbnail_restore_output(struct thumbnail_save_area *save_area);


/**
 * Create a thumbnail of a page.
 *
 * \param  content  content structure to thumbnail
 * \param  area	    sprite area containing thumbnail sprite
 * \param  sprite   pointer to sprite
 * \param  width    sprite width / pixels
 * \param  height   sprite height / pixels
 *
 * The thumbnail is rendered in the given sprite.
 */
void thumbnail_create(struct content *content, osspriteop_area *area,
		osspriteop_header *sprite, int width, int height) {
	float scale = 1.0;
	osspriteop_area *temp_area = NULL;
	struct thumbnail_save_area *save_area;
	osspriteop_area *render_area = NULL;

	/*	Check for 32bpp support in case we've been called for a sprite
		we didn't set up.
	*/
	if (thumbnail_32bpp_available == -1) thumbnail_test();

	/*	Get a secondary holder for non-32bpp sprites as we get a better quality by
		going to a 32bpp sprite and then down to an [n]bpp one.
	*/
	if ((thumbnail_32bpp_available == 1) &&
			(sprite->mode != (os_mode)tinct_SPRITE_MODE)) {
		temp_area = thumbnail_initialise(
				width, height,
				(os_mode)0x301680b5);
		render_area = temp_area;
	}
	if (temp_area == NULL) render_area = area;

	/*	Calculate the scale
	*/
	if (content->width) scale = (float) width / (float) content->width;

	/*	Set up plotters
	*/
	plot = ro_plotters;
	ro_plot_origin_x = 0;
	ro_plot_origin_y = height * 2;
	ro_plot_set_scale(scale);

	/*	Switch output and redraw
	*/
	save_area = thumbnail_switch_output(render_area, sprite);
	if (save_area == NULL) {
		if (temp_area) free(temp_area);
		return;
	}
	colourtrans_set_gcol(os_COLOUR_WHITE, colourtrans_SET_BG,
			os_ACTION_OVERWRITE, 0);
	os_clg();
	if ((content->type == CONTENT_HTML) &&
			(content->data.html.fonts))
		nsfont_reopen_set(content->data.html.fonts);
	content_redraw(content, 0, 0, width, height,
			0, 0, width, height, scale, 0xFFFFFF);
	thumbnail_restore_output(save_area);
	if ((content->type == CONTENT_HTML) &&
			(content->data.html.fonts))
		nsfont_reopen_set(content->data.html.fonts);

	/*	Go back from 32bpp to [n]bpp if we should.
	*/
	if (temp_area != NULL) {
		save_area = thumbnail_switch_output(area, sprite);
		if (save_area != NULL) {
			_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
					(char *)(temp_area + 1), 0, 0,
					tinct_ERROR_DIFFUSE);
			thumbnail_restore_output(save_area);
		}
		free(temp_area);
	}
}


/**
 *	Initialises a sprite.
 *
 *	The sprite background cleared to white.
 *	Any necessary palette data is set up to the default palette.
 *	The sprite name is set to "thumbnail".
 *
 *	@param	width	The sprite width
 *	@param	height	The sprite height
 *	@param	mode	The preferred mode (0x301680b5 or os_MODE8BPP90X90)
 *	@return
 */
osspriteop_area* thumbnail_initialise(int width, int height, os_mode mode) {
	unsigned int area_size;
	unsigned int remaining_bytes;
	osspriteop_area *sprite_area;
	osspriteop_header *sprite_header;
	char *sprite_image;

	/*	Check if we can use 32bpp sprites if we haven't already. By
		doing it this way we don't need to allocate lot of memory
		first which will probably not be available on machines that
		can't handle such sprites..
	*/
	if (thumbnail_32bpp_available == -1) thumbnail_test();

	/*	If we can't handle 32bpp then we get 8bpp.
	*/
	if (thumbnail_32bpp_available != 1) mode = os_MODE8BPP90X90;

	/*	Calculate our required memory
	*/
	area_size = sizeof(osspriteop_area) + sizeof(osspriteop_header);
	if (mode == (os_mode)0x301680b5) {
		area_size += width * height * 4;
	} else {
		area_size += ((width + 3) & ~3) * height + 2048;
	}

	/*	Try to get enough memory
	*/
	if ((sprite_area = (osspriteop_area *)malloc(area_size)) == NULL) {
		LOG(("Insufficient memory to create thumbnail."));
		return NULL;
	}

	/*	Initialise the sprite area
	*/
	sprite_area->size = area_size;
	sprite_area->sprite_count = 1;
	sprite_area->first = 16;
	sprite_area->used = area_size;

	/*	Initialise the sprite header. We can't trust OS_SpriteOp to
		set up our palette properly due to insane legacy 8bpp palettes,
		so we do it all manually.
	*/
	sprite_header = (osspriteop_header *)(sprite_area + 1);
	sprite_header->size = area_size - sizeof(osspriteop_area);
	memset(sprite_header->name, 0x00, 12);
	strcpy(sprite_header->name, "thumbnail");
	sprite_header->left_bit = 0;
	sprite_header->height = height - 1;
	sprite_header->mode = mode;
	if (mode == (os_mode)0x301680b5) {
		sprite_header->right_bit = 31;
		sprite_header->width = width - 1;
		sprite_header->image = sizeof(osspriteop_header);
		sprite_header->mask = sizeof(osspriteop_header);

		/*	Clear to white, full opacity
		*/
		sprite_image = ((char *)sprite_header) + sprite_header->image;
		memset(sprite_image, 0xff, area_size - sizeof(osspriteop_area) -
						sizeof(osspriteop_header));
	} else {
		sprite_header->right_bit = ((width << 3) - 1) & 31;
		sprite_header->width = ((width + 3) >> 2) - 1;
		sprite_header->image = sizeof(osspriteop_header) + 2048;
		sprite_header->mask = sizeof(osspriteop_header) + 2048;

		/*	Create the palette. We don't read the necessary size
			like we really should as we know it's going to have
			256 entries of 8 bytes = 2048.
		*/
		xcolourtrans_read_palette((osspriteop_area *)mode, (osspriteop_id)0,
			(os_palette *)(sprite_header + 1), 2048,
			(colourtrans_palette_flags)(1 << 1), &remaining_bytes);

		/*	Clear to white
		*/
		sprite_image = ((char *)sprite_header) + sprite_header->image;
		memset(sprite_image, 0xff, area_size - sizeof(osspriteop_area) -
						sizeof(osspriteop_header) - 2048);
	}

	/*	Return our sprite area
	*/
	return sprite_area;
}


/*
 *	Checks to see whether 32bpp sprites are available. Rather than
 *	using Wimp_ReadSysInfo we test if 32bpp sprites are available in
 *	case the user has a 3rd party patch to enable them.
 */
static void thumbnail_test(void) {
	unsigned int area_size;
	osspriteop_area *sprite_area;

	/*	If we're configured not to use 32bpp then we don't
	*/
	if (!option_thumbnail_32bpp) {
		thumbnail_32bpp_available = 0;
		return;
	}

	/*	Get enough memory for a 1x1 32bpp sprite
	*/
	area_size = sizeof(osspriteop_area) +
			sizeof(osspriteop_header) + sizeof(int);
	if ((sprite_area = (osspriteop_area *)malloc(area_size)) == NULL) {
		LOG(("Insufficient memory to perform sprite test."));
		return;
	}

	/*	Initialise the sprite area
	*/
	sprite_area->size = area_size + 1;
	sprite_area->sprite_count = 0;
	sprite_area->first = 16;
	sprite_area->used = 16;

	/*	Try to create a 32bpp sprite
	*/
	if (xosspriteop_create_sprite(osspriteop_NAME, sprite_area,
			"test",	false, 1, 1, (os_mode)tinct_SPRITE_MODE)) {
		thumbnail_32bpp_available = 0;
	} else {
		thumbnail_32bpp_available = 1;
	}

	/*	Free our memory
	*/
	free(sprite_area);
}


/*	Switches output to the specified sprite and returns the previous context.
*/
static struct thumbnail_save_area* thumbnail_switch_output(osspriteop_area *sprite_area,
					osspriteop_header *sprite_header) {
	struct thumbnail_save_area *save_area;
	int size;

	/*	Create a save area
	*/
	save_area = calloc(sizeof(struct thumbnail_save_area), 1);
	if (save_area == NULL) return NULL;

	/*	Allocate OS_SpriteOp save area
	*/
	if (xosspriteop_read_save_area_size(osspriteop_PTR, sprite_area,
			(osspriteop_id)sprite_header, &size)) {
		free(save_area);
		return NULL;
	}

	/*	Create the save area
	*/
	save_area->save_area = malloc((unsigned)size);
	if (save_area->save_area == NULL) {
		free(save_area);
		return NULL;
	}
	save_area->save_area->a[0] = 0;

	/*	Switch output to sprite
	*/
	if (xosspriteop_switch_output_to_sprite(osspriteop_PTR, sprite_area,
			(osspriteop_id)sprite_header, save_area->save_area,
			0, &save_area->context1, &save_area->context2,
			&save_area->context3)) {
		free(save_area->save_area);
		free(save_area);
		return NULL;
	}
	return save_area;
}


/*	Restores output to the specified context, and destroys it.
*/
static void thumbnail_restore_output(struct thumbnail_save_area *save_area) {

	/*	We don't care if we err, as there's nothing we can do about it
	*/
	xosspriteop_switch_output_to_sprite(osspriteop_PTR,
			(osspriteop_area *)save_area->context1,
			(osspriteop_id)save_area->context2,
			(osspriteop_save_area *)save_area->context3,
			0, 0, 0, 0);

	/*	Free our workspace
	*/
	free(save_area->save_area);
	free(save_area);
}

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <stdbool.h>
#include <swis.h>

#include "oslib/colourtrans.h"
#include "oslib/osspriteop.h"

#include "netsurf/riscos/image.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


static bool image_redraw_tinct_alpha(osspriteop_area *area, int x, int y,
		int req_width, int req_height, int width, int height,
		unsigned long background_colour, bool repeatx, bool repeaty);
static bool image_redraw_tinct_opaque(osspriteop_area *area, int x, int y,
		int req_width, int req_height, int width, int height,
		unsigned long background_colour, bool repeatx, bool repeaty);
static bool image_redraw_os(osspriteop_area *area, int x, int y,
		int req_width, int req_height, int width, int height);

/**
 * Plot an image at the given coordinates using the method specified
 *
 * \param area              The sprite area containing the sprite
 * \param x                 Left edge of sprite
 * \param y                 Top edge of sprite
 * \param req_width         The requested width of the sprite
 * \param req_height        The requested height of the sprite
 * \param width             The actual width of the sprite
 * \param height            The actual height of the sprite
 * \param background_colour The background colour to blend to
 * \param repeatx           Repeat the image in the x direction
 * \param repeaty           Repeat the image in the y direction
 * \param type              The plot method to use
 * \return true on success, false otherwise
 */
bool image_redraw(osspriteop_area *area, int x, int y, int req_width,
		int req_height, int width, int height,
		unsigned long background_colour,
		bool repeatx, bool repeaty,image_type type)
{
	switch (type) {
		case IMAGE_PLOT_TINCT_ALPHA:
			return image_redraw_tinct_alpha(area, x, y,
						req_width, req_height,
						width, height,
						background_colour,
						repeatx, repeaty);
		case IMAGE_PLOT_TINCT_OPAQUE:
			return image_redraw_tinct_opaque(area, x, y,
						req_width, req_height,
						width, height,
						background_colour,
						repeatx, repeaty);
		case IMAGE_PLOT_OS:
			return image_redraw_os(area, x, y, req_width,
						req_height, width, height);
		default:
			break;
	}

	return false;
}

/**
 * Plot an alpha channel image at the given coordinates using tinct
 *
 * \param area              The sprite area containing the sprite
 * \param x                 Left edge of sprite
 * \param y                 Top edge of sprite
 * \param req_width         The requested width of the sprite
 * \param req_height        The requested height of the sprite
 * \param width             The actual width of the sprite
 * \param height            The actual height of the sprite
 * \param background_colour The background colour to blend to
 * \return true on success, false otherwise
 */
bool image_redraw_tinct_alpha(osspriteop_area *area, int x, int y,
		int req_width, int req_height, int width, int height,
		unsigned long background_colour, bool repeatx, bool repeaty)
{
	unsigned int tinct_options;
	_kernel_oserror *error;

	if (ro_gui_current_redraw_gui) {
		tinct_options =
			(ro_gui_current_redraw_gui->option.filter_sprites ?
						tinct_BILINEAR_FILTER : 0)
			|
			(ro_gui_current_redraw_gui->option.dither_sprites ?
						tinct_DITHER : 0);
	} else {
		tinct_options =
			(option_filter_sprites ? tinct_BILINEAR_FILTER : 0)
			|
			(option_dither_sprites ? tinct_DITHER : 0);
	}

	if (print_active) {
		tinct_options |= tinct_USE_OS_SPRITE_OP |
				background_colour << tinct_BACKGROUND_SHIFT;
	}

	if (repeatx)
		tinct_options |= tinct_FILL_HORIZONTALLY;

	if (repeaty)
		tinct_options |= tinct_FILL_VERTICALLY;

	error = _swix(Tinct_PlotScaledAlpha, _INR(2,7),
			(char*)area + area->first, x, y - req_height,
			req_width, req_height, tinct_options);
	if (error) {
		LOG(("xtinct_plotscaledalpha: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	return true;
}

/**
 * Plot an opaque image at the given coordinates using tinct
 *
 * \param area              The sprite area containing the sprite
 * \param x                 Left edge of sprite
 * \param y                 Top edge of sprite
 * \param req_width         The requested width of the sprite
 * \param req_height        The requested height of the sprite
 * \param width             The actual width of the sprite
 * \param height            The actual height of the sprite
 * \param background_colour The background colour to blend to
 * \return true on success, false otherwise
 */
bool image_redraw_tinct_opaque(osspriteop_area *area, int x, int y,
		int req_width, int req_height, int width, int height,
		unsigned long background_colour, bool repeatx, bool repeaty)
{
	unsigned int tinct_options;
	_kernel_oserror *error;

	if (ro_gui_current_redraw_gui) {
		tinct_options =
			(ro_gui_current_redraw_gui->option.filter_sprites ?
						tinct_BILINEAR_FILTER : 0)
			|
			(ro_gui_current_redraw_gui->option.dither_sprites ?
						tinct_DITHER : 0);
	} else {
		tinct_options =
			(option_filter_sprites ? tinct_BILINEAR_FILTER : 0)
			|
			(option_dither_sprites ? tinct_DITHER : 0);
	}

	if (print_active) {
		tinct_options |= tinct_USE_OS_SPRITE_OP |
				background_colour << tinct_BACKGROUND_SHIFT;
	}

	if (repeatx)
		tinct_options |= tinct_FILL_HORIZONTALLY;

	if (repeaty)
		tinct_options |= tinct_FILL_VERTICALLY;

	error = _swix(Tinct_PlotScaled, _INR(2,7),
			(char*)area + area->first, x, y - req_height,
			req_width, req_height, tinct_options);
	if (error) {
		LOG(("xtinct_plotscaled: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	return true;
}

/**
 * Plot an image at the given coordinates using os_spriteop
 *
 * \param area       The sprite area containing the sprite
 * \param x          Left edge of sprite
 * \param y          Top edge of sprite
 * \param req_width  The requested width of the sprite
 * \param req_height The requested height of the sprite
 * \param width      The actual width of the sprite
 * \param height     The actual height of the sprite
 * \return true on success, false otherwise
 */
bool image_redraw_os(osspriteop_area *area, int x, int y, int req_width,
		int req_height, int width, int height)
{
	unsigned int size;
	os_factors f;
	osspriteop_trans_tab *table;
	os_error *error;

	error = xcolourtrans_generate_table_for_sprite(
			area, (osspriteop_id)((char*) area + area->first),
			colourtrans_CURRENT_MODE,
			colourtrans_CURRENT_PALETTE,
			0, colourtrans_GIVEN_SPRITE, 0, 0, &size);
	if (error) {
		LOG(("xcolourtrans_generate_table_for_sprite: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}

	table = calloc(size, sizeof(char));
	if (!table) {
		LOG(("malloc failed"));
		warn_user("NoMemory", 0);
		return false;
	}

	error = xcolourtrans_generate_table_for_sprite(
			area, (osspriteop_id)((char*) area + area->first),
			colourtrans_CURRENT_MODE,
			colourtrans_CURRENT_PALETTE,
			table, colourtrans_GIVEN_SPRITE, 0, 0, 0);
	if (error) {
		LOG(("xcolourtrans_generate_table_for_sprite: 0x%x: %s", error->errnum, error->errmess));
		free(table);
		return false;
	}

	f.xmul = req_width;
	f.ymul = req_height;
	f.xdiv = width * 2;
	f.ydiv = height * 2;

	error = xosspriteop_put_sprite_scaled(osspriteop_PTR,
			area, (osspriteop_id)((char*) area + area->first),
			x, (int)(y - req_height),
			0, &f, table);
	if (error) {
		LOG(("xosspriteop_put_sprite_scaled: 0x%x: %s", error->errnum, error->errmess));
		free(table);
		return false;
	}

	free(table);

	return true;
}


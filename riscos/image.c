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


static bool image_redraw_tinct(osspriteop_area *area, int x, int y,
		int req_width, int req_height, int width, int height,
		unsigned long background_colour, bool repeatx, bool repeaty,
		bool alpha, unsigned int tinct_options);
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
 * \param background	    Use background image settings (otherwise foreground)
 * \param type              The plot method to use
 * \return true on success, false otherwise
 */
bool image_redraw(osspriteop_area *area, int x, int y, int req_width,
		int req_height, int width, int height,
		unsigned long background_colour,
		bool repeatx, bool repeaty, bool background, image_type type)
{
	unsigned int tinct_options;
	req_width *= 2;
	req_height *= 2;
	width *= 2;
	height *= 2;
	tinct_options = background ? option_bg_plot_style : option_fg_plot_style;
	switch (type) {
		case IMAGE_PLOT_TINCT_ALPHA:
			return image_redraw_tinct(area, x, y,
						req_width, req_height,
						width, height,
						background_colour,
						repeatx, repeaty, true,
						tinct_options);
		case IMAGE_PLOT_TINCT_OPAQUE:
			return image_redraw_tinct(area, x, y,
						req_width, req_height,
						width, height,
						background_colour,
						repeatx, repeaty, false,
						tinct_options);
		case IMAGE_PLOT_OS:
			return image_redraw_os(area, x, y, req_width,
						req_height, width, height);
		default:
			break;
	}

	return false;
}

/**
 * Plot an image at the given coordinates using tinct
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
 * \param alpha             Use the alpha channel
 * \param tinct_options	    The base option set to use
 * \return true on success, false otherwise
 */
bool image_redraw_tinct(osspriteop_area *area, int x, int y,
		int req_width, int req_height, int width, int height,
		unsigned long background_colour, bool repeatx, bool repeaty,
		bool alpha, unsigned int tinct_options)
{
	_kernel_oserror *error;

	/*	Set up our flagword
	*/
	tinct_options |= background_colour << tinct_BACKGROUND_SHIFT;
	if (print_active) 
		tinct_options |= tinct_USE_OS_SPRITE_OP;
	if (repeatx)
		tinct_options |= tinct_FILL_HORIZONTALLY;
	if (repeaty)
		tinct_options |= tinct_FILL_VERTICALLY;

	if (alpha) {
		error = _swix(Tinct_PlotScaledAlpha, _INR(2,7),
				(char*)area + area->first, x, y - req_height,
				req_width, req_height, tinct_options);
	} else {
		error = _swix(Tinct_PlotScaled, _INR(2,7),
				(char*)area + area->first, x, y - req_height,
				req_width, req_height, tinct_options);
	}

	if (error) {
		LOG(("xtinct_plotscaled%s: 0x%x: %s", (alpha ? "alpha" : ""),
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
			(osspriteop_area *)0x100,
			(osspriteop_id)((char*) area + area->first),
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
			(osspriteop_area *)0x100,
			(osspriteop_id)((char*) area + area->first),
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
	f.xdiv = width;
	f.ydiv = height;

	error = xosspriteop_put_sprite_scaled(osspriteop_PTR,
			(osspriteop_area *)0x100,
			(osspriteop_id)((char*) area + area->first),
			x, (int)(y - req_height),
			8, &f, table);
	if (error) {
		LOG(("xosspriteop_put_sprite_scaled: 0x%x: %s", error->errnum, error->errmess));
		free(table);
		return false;
	}

	free(table);

	return true;
}


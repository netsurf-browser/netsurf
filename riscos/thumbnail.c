/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Page thumbnail creation (implementation).
 *
 * Thumbnails are created by redirecting output to a sprite and rendering the
 * page at a small scale.
 */

#include "oslib/colourtrans.h"
#include "oslib/osspriteop.h"
#include "netsurf/content/content.h"
#include "netsurf/riscos/thumbnail.h"
#include "netsurf/utils/log.h"


/**
 * Create a thumbnail of a page.
 *
 * \param  content  content structure to thumbnail
 * \param  area     sprite area containing thumbnail sprite
 * \param  sprite   pointer to sprite
 * \param  width    sprite width / pixels
 * \param  height   sprite height / pixels
 *
 * The thumbnail is rendered in the given sprite.
 */

void thumbnail_create(struct content *content, osspriteop_area *area,
		osspriteop_header *sprite, int width, int height)
{
	float scale;
	os_error *error;

	scale = (float) width / (float) content->width;

	/* switch output to sprite */
	error = xosspriteop_switch_output_to_sprite(osspriteop_PTR, area,
			(osspriteop_id) sprite, 0, 0, 0, 0, 0);
	if (error) {
		LOG(("xosspriteop_switch_output_to_sprite failed: %s",
				error->errmess));
		return;
	}

	/* clear background to white */
	colourtrans_set_gcol(os_COLOUR_WHITE, colourtrans_SET_BG,
			os_ACTION_OVERWRITE, 0);
	os_clg();

	/* render content */
	content_redraw(content, 0, height * 2, width * 2, height * 2,
			0, 0, width * 2, height * 2, scale);

	/* switch output back to screen */
	error = xosspriteop_switch_output_to_sprite(osspriteop_PTR, area,
			0, 0, 0, 0, 0, 0);
	if (error) {
		LOG(("xosspriteop_switch_output_to_sprite failed: %s",
				error->errmess));
		return;
	}
}

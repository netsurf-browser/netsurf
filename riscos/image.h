/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#ifndef _NETSURF_RISCOS_IMAGE_H_
#define _NETSURF_RISCOS_IMAGE_H_

#include <oslib/osspriteop.h>

struct osspriteop_area;

typedef enum {
	IMAGE_PLOT_TINCT_ALPHA,
	IMAGE_PLOT_TINCT_OPAQUE,
	IMAGE_PLOT_OS
} image_type;

bool image_redraw(osspriteop_area *area, int x, int y, int req_width,
		int req_height, int width, int height,
		unsigned long background_colour,
		bool repeatx, bool repeaty, bool background, image_type type);

#endif

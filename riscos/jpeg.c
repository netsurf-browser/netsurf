/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/**
 * This is just a temporary implementation using the JPEG renderer
 * available in some versions of RISC OS.
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "netsurf/content/content.h"
#include "netsurf/riscos/jpeg.h"
#include "netsurf/utils/utils.h"
#include "oslib/jpeg.h"


void jpeg_create(struct content *c)
{
	c->data.jpeg.data = xcalloc(0, 1);
	c->data.jpeg.length = 0;
}


void jpeg_process_data(struct content *c, char *data, unsigned long size)
{
	c->data.jpeg.data = xrealloc(c->data.jpeg.data, c->data.jpeg.length + size);
	memcpy(c->data.jpeg.data + c->data.jpeg.length, data, size);
	c->data.jpeg.length += size;
	c->size += size;
}


int jpeg_convert(struct content *c, unsigned int width, unsigned int height)
{
	os_error *error;
	int w, h;
	error = xjpeginfo_dimensions(c->data.jpeg.data, (int) c->data.jpeg.length,
			0, &w, &h, 0, 0, 0);
	if (error != 0)
		return 1;
	c->width = w;
	c->height = h;
	c->title = xcalloc(100, 1);
	sprintf(c->title, "JPEG image (%ux%u, %lu bytes)", w, h, c->data.jpeg.length);
	c->status = CONTENT_STATUS_DONE;
	return 0;
}


void jpeg_revive(struct content *c, unsigned int width, unsigned int height)
{
}


void jpeg_reformat(struct content *c, unsigned int width, unsigned int height)
{
}


void jpeg_destroy(struct content *c)
{
	xfree(c->data.jpeg.data);
	xfree(c->title);
}


void jpeg_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height)
{
	os_factors factors;
	factors.xmul = width;
	factors.ymul = height;
	factors.xdiv = c->width * 2;
	factors.ydiv = c->height * 2;

	xjpeg_plot_scaled((jpeg_image *) c->data.jpeg.data,
			x, y - height,
			&factors, (int) c->data.jpeg.length,
			jpeg_SCALE_DITHERED);
}


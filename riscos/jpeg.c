/**
 * $Id: jpeg.c,v 1.1 2003/02/25 21:00:27 bursa Exp $
 *
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
}

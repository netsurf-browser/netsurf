/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Content for image/jpeg (interface).
 */

#ifndef _NETSURF_IMAGE_JPEG_H_
#define _NETSURF_IMAGE_JPEG_H_

struct bitmap;
struct content;

struct content_jpeg_data {
	int dummy; /* NOT USED but to satisfy Norcroft */
};

bool nsjpeg_convert(struct content *c, int width, int height);
void nsjpeg_destroy(struct content *c);

#endif

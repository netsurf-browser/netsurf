/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Page thumbnail creation (interface).
 */

#include "oslib/osspriteop.h"
#include "netsurf/image/bitmap.h"

bool thumbnail_create(struct content *content, struct bitmap *bitmap,
		const char *url);
osspriteop_area *thumbnail_convert_8bpp(struct bitmap *bitmap);

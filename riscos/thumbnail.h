/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Page thumbnail creation (interface).
 */

#include <oslib/osspriteop.h>
#include "image/bitmap.h"

osspriteop_area *thumbnail_convert_8bpp(struct bitmap *bitmap);

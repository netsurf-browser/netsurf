/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Page thumbnail creation (interface).
 */

#include "oslib/osspriteop.h"

void thumbnail_create(struct content *content, osspriteop_area *area,
		osspriteop_header *sprite, int width, int height);

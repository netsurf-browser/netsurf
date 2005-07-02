/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Table processing and layout (interface).
 */

#ifndef _NETSURF_RENDER_TABLE_H_
#define _NETSURF_RENDER_TABLE_H_

#include <stdbool.h>

struct box;

bool table_calculate_column_types(struct box *table);
void table_collapse_borders(struct box *table);

#endif

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * HTML layout (interface).
 *
 * The main interface to the layout code is layout_document(), which takes a
 * normalized box tree and assigns coordinates and dimensions to the boxes, and
 * also adds boxes to the tree (eg. when formatting lines of text).
 */

#ifndef _NETSURF_RENDER_LAYOUT_H_
#define _NETSURF_RENDER_LAYOUT_H_

#include "netsurf/utils/pool.h"

bool layout_document(struct box *box, int width, pool box_pool);
bool layout_block_context(struct box *block, pool box_pool);

#endif

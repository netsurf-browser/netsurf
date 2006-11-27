/*
 * This file is part of NetSurf, http://netsurf-browser.org/
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

#define SCROLLBAR_WIDTH 16

struct box;

bool layout_document(struct content *content, int width, int height);
bool layout_block_context(struct box *block, struct content *content);
bool layout_inline_container(struct box *box, int width,
		struct box *cont, int cx, int cy, struct content *content);
void layout_calculate_descendant_bboxes(struct box *box);

#endif

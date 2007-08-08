/*
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

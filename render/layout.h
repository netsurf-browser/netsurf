/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#ifndef _NETSURF_RENDER_LAYOUT_H_
#define _NETSURF_RENDER_LAYOUT_H_

/**
 * interface
 */

void layout_document(struct box * box, unsigned long width);
void layout_block(struct box * box, unsigned long width, struct box * cont,
		unsigned long cx, unsigned long cy);

#endif

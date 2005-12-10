/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net> 
 */

/** \file
 * Content for image/artworks (RISC OS interface).
 */

#ifndef _NETSURF_RISCOS_ARTWORKS_H_
#define _NETSURF_RISCOS_ARTWORKS_H_

struct content;

struct content_artworks_data {
	int x0, y0, x1, y1;

	void *render_routine;
	void *render_workspace;

	/* dunamically-resizable block required by
		ArtWorksRenderer rendering routine */

	void *block;
	size_t size;
};

bool artworks_convert(struct content *c, int width, int height);
void artworks_destroy(struct content *c);
bool artworks_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour);

#endif

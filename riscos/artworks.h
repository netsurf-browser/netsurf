/*
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
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
 * Content for image/x-artworks (RISC OS interface).
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

bool artworks_convert(struct content *c);
void artworks_destroy(struct content *c);
bool artworks_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour);
bool artworks_clone(const struct content *old, struct content *new_content);

#endif

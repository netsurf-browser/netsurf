/*
 * Copyright 2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
 * Content for image/webp (libwebp interface).
 */

#ifndef _NETSURF_WEBP_H_
#define _NETSURF_WEBP_H_

#include "utils/config.h"
#ifdef WITH_WEBP

#include <stdbool.h>

struct content;
struct rect;

struct content_webp_data {
/* empty */
};

bool webp_convert(struct content *c);
void webp_destroy(struct content *c);
bool webp_redraw(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour);
bool webp_clone(const struct content *old, struct content *new_content);

#endif /* WITH_WEBP */

#endif

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
 * Content for image/x-amiga-icon (icon.library interface).
 */

#ifndef AMIGA_ICON_H
#define AMIGA_ICON_H

#include "utils/config.h"
#ifdef WITH_AMIGA_ICON

#include <stdbool.h>
#include "content/hlcache.h"

struct rect;

struct content_amiga_icon_data {
/* empty */
};

bool amiga_icon_convert(struct content *c);
void amiga_icon_destroy(struct content *c);
bool amiga_icon_redraw(struct content *c, int x, int y,
		int width, int height, struct rect *clip,
		float scale, colour background_colour);
bool amiga_icon_clone(const struct content *old, struct content *new_content);

#endif /* WITH_AMIGA_ICON */

struct hlcache_handle;

void ami_superimpose_favicon(char *path, struct hlcache_handle *icon, char *type);
#endif

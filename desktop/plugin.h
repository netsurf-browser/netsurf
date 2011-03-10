/*
 * Copyright 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

/**
 * \file Content handler for plugins (interface)
 */

#ifndef NETSURF_DESKTOP_PLUGIN_H_
#define NETSURF_DESKTOP_PLUGIN_H_

#ifdef WITH_PLUGIN

#include <stdbool.h>

#if defined(riscos)
#include "riscos/plugin.h"
#elif defined(nsamiga)
#include "amiga/plugin.h"
#endif

struct box;
struct browser_window;
struct content;
struct object_params;
struct rect;
struct http_parameter;

/* function definitions */
bool plugin_handleable(const char *mime_type);
bool plugin_create(struct content *c, const struct http_parameter *params);
bool plugin_convert(struct content *c);
void plugin_reformat(struct content *c, int width, int height);
void plugin_destroy(struct content *c);
bool plugin_redraw(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour);
void plugin_open(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params);
void plugin_close(struct content *c);
bool plugin_clone(const struct content *old, struct content *new_content);

#endif /* WITH_PLUGIN */
#endif

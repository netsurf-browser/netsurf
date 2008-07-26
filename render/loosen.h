/*
 * Copyright 2008 Adam Blokus <adamblokus@gmail.com>
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
\file
General idea - a set of routines working themselves recursively through
the box tree and trying to change the layout of the document as little
as possible to acquire the desired width ( - to make it fit in a printed
page ), where possible - also taking the dividing height into consideration,
to prevent objects being cut by ends of pages.
*/

#ifndef NETSURF_RENDER_LOOSEN_H
#define NETSURF_RENDER_LOOSEN_H
#include <stdbool.h>

bool loosen_document_layout(struct content *content, struct box *layout,
		int width, int height);

#endif

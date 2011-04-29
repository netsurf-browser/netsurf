/*
 * Copyright 2008-2009, 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_CLIPBOARD_H
#define AMIGA_CLIPBOARD_H
#include <stdbool.h>

struct bitmap;
struct hlcache_handle;
struct selection;
struct gui_window_2;

struct ami_text_selection
{
	char text[1024];
	int length;
};

void ami_clipboard_init(void);
void ami_clipboard_free(void);
void ami_drag_selection(struct selection *s);
bool ami_easy_clipboard(char *text);
bool ami_easy_clipboard_bitmap(struct bitmap *bitmap);
struct ami_text_selection *ami_selection_to_text(struct gui_window_2 *gwin);
#ifdef WITH_NS_SVG
bool ami_easy_clipboard_svg(struct hlcache_handle *c);
#endif
#endif

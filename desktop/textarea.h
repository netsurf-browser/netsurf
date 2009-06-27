/*
 * Copyright 2006 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net> 
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
 * Single/Multi-line UTF-8 text area (interface)
 */

#ifndef _NETSURF_DESKTOP_TEXTAREA_H_
#define _NETSURF_DESKTOP_TEXTAREA_H_

#include <stdint.h>
#include <stdbool.h>
#include "css/css.h"
#include "desktop/browser.h"

/* Text area flags */
#define TEXTAREA_MULTILINE	0x01	/**< Text area is multiline */
#define TEXTAREA_READONLY	0x02	/**< Text area is read only */

struct text_area;

typedef void(*textarea_start_redraw_callback)(void *data);
typedef void(*textarea_end_redraw_callback)(void *data);

struct text_area *textarea_create(int x, int y, int width, int height, 
		unsigned int flags, const struct css_style *style,
		textarea_start_redraw_callback redraw_start_callback,
		textarea_end_redraw_callback redraw_end_callback, void *data);
void textarea_destroy(struct text_area *ta);
bool textarea_set_text(struct text_area *ta, const char *text);
int textarea_get_text(struct text_area *ta, char *buf, unsigned int len);
bool textarea_set_caret(struct text_area *ta, int caret);
int textarea_get_caret(struct text_area *ta);
void textarea_redraw(struct text_area *ta, int x0, int y0, int x1, int y1);
bool textarea_keypress(struct text_area *ta, uint32_t key);
bool textarea_mouse_action(struct text_area *ta, browser_mouse_state mouse,
		int x, int y);
bool textarea_drag_end(struct text_area *ta, browser_mouse_state mouse,
		int x, int y);

#endif


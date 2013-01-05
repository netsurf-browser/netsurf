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
#include "desktop/browser.h"
#include "desktop/plot_style.h"

/* Text area flags */
typedef enum textarea_flags {
	TEXTAREA_DEFAULT		= (1 << 0),
	TEXTAREA_MULTILINE		= (1 << 1),
	TEXTAREA_READONLY		= (1 << 2)
} textarea_flags;


struct textarea;

typedef void(*textarea_redraw_request_callback)(void *data, int x, int y,
		int width, int height);

struct textarea *textarea_create(int width, int height, 
		textarea_flags flags, const plot_font_style_t *style,
		textarea_redraw_request_callback redraw_request, void *data);
void textarea_destroy(struct textarea *ta);
bool textarea_set_text(struct textarea *ta, const char *text);
int textarea_get_text(struct textarea *ta, char *buf, unsigned int len);
bool textarea_set_caret(struct textarea *ta, int caret);
int textarea_get_caret(struct textarea *ta);
void textarea_redraw(struct textarea *ta, int x, int y,
		const struct rect *clip, const struct redraw_context *ctx);
bool textarea_keypress(struct textarea *ta, uint32_t key);
bool textarea_mouse_action(struct textarea *ta, browser_mouse_state mouse,
		int x, int y);
bool textarea_drag_end(struct textarea *ta, browser_mouse_state mouse,
		int x, int y);
void textarea_get_dimensions(struct textarea *ta, int *width, int *height);
void textarea_set_dimensions(struct textarea *ta, int width, int height);
#endif


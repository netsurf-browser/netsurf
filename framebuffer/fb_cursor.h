/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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

#ifndef FRAMEBUFFER_FB_CURSOR
#define FRAMEBUFFER_FB_CURSOR

void fb_cursor_move(struct framebuffer_s *fb, int x, int y);

void fb_cursor_move_abs(struct framebuffer_s *fb, int x, int y);

void fb_cursor_plot(struct framebuffer_s *fb);

fb_cursor_t *fb_cursor_init(struct framebuffer_s *fb);

void fb_cursor_click(framebuffer_t *fb, struct gui_window *g, browser_mouse_state st);

#endif

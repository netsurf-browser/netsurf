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

#ifndef NETSURF_FB_FRONTEND_H
#define NETSURF_FB_FRONTEND_H

extern framebuffer_t *fb_os_init(int argc, char** argv);
extern void fb_os_quit(framebuffer_t *fb);
extern void fb_os_input(struct gui_window *g, bool active);
extern void fb_os_option_override(void);
extern void fb_os_redraw(struct bbox_s *box);

#endif /* NETSURF_FB_FRONTEND_H */

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

#ifndef FRAMEBUFFER_PLOTTERS_H
#define FRAMEBUFFER_PLOTTERS_H

extern const struct plotter_table framebuffer_1bpp_plot;
extern const struct plotter_table framebuffer_8bpp_plot;
extern const struct plotter_table framebuffer_16bpp_plot;
extern const struct plotter_table framebuffer_32bpp_plot;

/* plotting context */
extern bbox_t fb_plot_ctx;

/* plotter support functions */
bool fb_plotters_clip_rect_ctx(int *x0, int *y0, int *x1, int *y1);
bool fb_plotters_clip_rect(const bbox_t *clip, int *x0, int *y0, int *x1, int *y1);

bool fb_plotters_clip_line_ctx(int *x0, int *y0, int *x1, int *y1);
bool fb_plotters_clip_line(const bbox_t *clip, int *x0, int *y0, int *x1, int *y1);

bool fb_plotters_polygon(int *p, unsigned int n, colour fill, bool (linefn)(int x0, int y0, int x1, int y1, int width, colour c, bool dotted, bool dashed));

bool fb_plotters_bitmap_tile(int x, int y, 
                             int width, int height,
                             struct bitmap *bitmap, colour bg,
                             bool repeat_x, bool repeat_y,
                             struct content *content,
                             bool (bitmapfn)(int x, int y, 
                                             int width, int height,
                                             struct bitmap *bitmap, 
                                             colour bg,
                                             struct content *content));

colour fb_plotters_ablend(colour pixel, colour scrpixel);

bool fb_plotters_move_block(int srcx, int srcy, int width, int height, int dstx, int dsty);

/* generic plotter entry points */
bool fb_clip(int x0, int y0, int x1, int y1);


#endif

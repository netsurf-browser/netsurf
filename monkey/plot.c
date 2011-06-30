/*
 * Copyright 2011 Daniel Silverstone <dsilvers@digital-scurf.org>
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

#include "desktop/plotters.h"

static bool
monkey_plot_disc(int x, int y, int radius, const plot_style_t *style)
{
  return true;
}

static bool 
monkey_plot_arc(int x, int y, int radius, int angle1, int angle2, const plot_style_t *style)
{
  return true;
}

static bool 
monkey_plot_polygon(const int *p, unsigned int n, const plot_style_t *style)
{
  return true;
}


static bool 
monkey_plot_text(int x, int y, const char *text, size_t length,
		const plot_font_style_t *fstyle)
{
  return true;
}

static bool 
monkey_plot_bitmap(int x, int y,
                        int width, int height,
                        struct bitmap *bitmap, colour bg,
                        bitmap_flags_t flags)
{
  return true;
}

static bool 
monkey_plot_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
  return true;
}

static bool 
monkey_plot_line(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
  return true;
}


static bool 
monkey_plot_path(const float *p, 
                      unsigned int n, 
                      colour fill, 
                      float width,
                      colour c, 
                      const float transform[6])
{
  return true;
}

static bool 
monkey_plot_clip(const struct rect *clip)
{
  return true;
}

const struct plotter_table monkey_plotters = {
	.clip = monkey_plot_clip,
	.arc = monkey_plot_arc,
	.disc = monkey_plot_disc,
	.line = monkey_plot_line,
	.rectangle = monkey_plot_rectangle,
	.polygon = monkey_plot_polygon,
	.path = monkey_plot_path,
	.bitmap = monkey_plot_bitmap,
	.text = monkey_plot_text,
        .option_knockout = true,
};

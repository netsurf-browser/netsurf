/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "amiga/plotters.h"
#include "amiga/gui.h"
#include "amiga/bitmap.h"
#include <proto/Picasso96API.h>
#include <intuition/intuition.h>

struct plotter_table plot;
const struct plotter_table amiplot = {
	ami_clg,
	ami_rectangle,
	ami_line,
	ami_polygon,
	ami_fill,
	ami_clip,
	ami_text,
	ami_disc,
	ami_arc,
	ami_bitmap,
	ami_bitmap_tile,
	ami_group_start,
	ami_group_end,
	ami_flush,
	ami_path
};

bool ami_clg(colour c)
{
	printf("clg\n");
}

bool ami_rectangle(int x0, int y0, int width, int height,
			int line_width, colour c, bool dotted, bool dashed)
{
	printf("rect\n");
}

bool ami_line(int x0, int y0, int x1, int y1, int width,
			colour c, bool dotted, bool dashed)
{
	printf("line\n");
}

bool ami_polygon(int *p, unsigned int n, colour fill)
{
}

bool ami_fill(int x0, int y0, int x1, int y1, colour c)
{
}

bool ami_clip(int x0, int y0, int x1, int y1)
{
}

bool ami_text(int x, int y, const struct css_style *style,
			const char *text, size_t length, colour bg, colour c)
{
	printf("%s\n",text);
}

bool ami_disc(int x, int y, int radius, colour c, bool filled)
{
}

bool ami_arc(int x, int y, int radius, int angle1, int angle2,
	    		colour c)
{
}

bool ami_bitmap(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg)
{
	struct RenderInfo ri;

printf("bitmap plotter\n");
	ri.Memory = bitmap->pixdata;
	ri.BytesPerRow = bitmap->width * 3;
	ri.RGBFormat = RGBFB_B8G8R8;

	p96WritePixelArray((struct RenderInfo *)&ri,0,0,curwin->win->RPort,x,y,width,height);
}

bool ami_bitmap_tile(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bool repeat_x, bool repeat_y)
{
	printf("bitmap tile plotter\n");
}

bool ami_group_start(const char *name)
{
	/** optional */
}

bool ami_group_end(void)
{
	/** optional */
}

bool ami_flush(void)
{
}

bool ami_path(float *p, unsigned int n, colour fill, float width,
			colour c, float *transform)
{
}

/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 * Copyright 2009 Michael Drake <tlsa@netsurf-browser.org>
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

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "utils/log.h"
#include "utils/utf8.h"
#include "desktop/browser.h"
#include "desktop/plotters.h"

#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_tk.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_bitmap.h"
#include "framebuffer/fb_font.h"
#include "framebuffer/fb_frontend.h"

extern fbtk_widget_t *fbtk;

/* max height the poly plotter can cope with */
#define WINDOW_HEIGHT (2048)

/* Currently selected ploting routines. */
struct plotter_table plot;

/* Current plotting context */
bbox_t fb_plot_ctx;

enum {
        POINT_LEFTOF_REGION = 1,
        POINT_RIGHTOF_REGION = 2,
        POINT_ABOVE_REGION = 4,
        POINT_BELOW_REGION = 8,
};

#define REGION(x,y,cx1,cx2,cy1,cy2) ( ( (y) > (cy2) ? POINT_BELOW_REGION : 0) |  \
                                      ( (y) < (cy1) ? POINT_ABOVE_REGION : 0) |  \
                                      ( (x) > (cx2) ? POINT_RIGHTOF_REGION : 0) |  \
                                      ( (x) < (cx1) ? POINT_LEFTOF_REGION : 0) )

#define SWAP(a, b) do { int t; t=(a); (a)=(b); (b)=t;  } while(0)

/* clip a rectangle to another rectangle */
bool fb_plotters_clip_rect(const bbox_t * restrict clip,
                           int * restrict x0, int * restrict y0, int * restrict x1, int * restrict y1)
{
        char region1;
        char region2;

	if (*x1 < *x0) SWAP(*x0, *x1);

	if (*y1 < *y0) SWAP(*y0, *y1);

	region1 = REGION(*x0, *y0, clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);
	region2 = REGION(*x1, *y1, clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);

        /* area lies entirely outside the clipping rectangle */
        if ((region1 | region2) && (region1 & region2))
                return false;

        if (*x0 < clip->x0)
                *x0 = clip->x0;
        if (*x0 > clip->x1)
                *x0 = clip->x1;

        if (*x1 < clip->x0)
                *x1 = clip->x0;
        if (*x1 > clip->x1)
                *x1 = clip->x1;

        if (*y0 < clip->y0)
                *y0 = clip->y0;
        if (*y0 > clip->y1)
                *y0 = clip->y1;

        if (*y1 < clip->y0)
                *y1 = clip->y0;
        if (*y1 > clip->y1)
                *y1 = clip->y1;

        return true;
}

bool fb_plotters_clip_rect_ctx(int *x0, int *y0, int *x1, int *y1)
{
        return fb_plotters_clip_rect(&fb_plot_ctx, x0, y0, x1, y1);
}

/** Clip a line to a bounding box.
 */
bool fb_plotters_clip_line(const bbox_t *clip,
                           int *x0, int *y0, int *x1, int *y1)
{
        char region1;
        char region2;
        region1 = REGION(*x0, *y0, clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);
        region2 = REGION(*x1, *y1, clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);

        while (region1 | region2) {
                if (region1 & region2) {
                        /* line lies entirely outside the clipping rectangle */
                        return false;
                }

                if (region1) {
                        /* first point */
                        if (region1 & POINT_BELOW_REGION) {
                                /* divide line at bottom */
                                *x0 = (*x0 + (*x1 - *x0) *
                                       (clip->y1 - 1 - *y0) / (*y1-*y0));
                                *y0 = clip->y1 - 1;
                        } else if (region1 & POINT_ABOVE_REGION) {
                                /* divide line at top */
                                *x0 = (*x0 + (*x1 - *x0) *
                                       (clip->y0 - *y0) / (*y1-*y0));
                                *y0 = clip->y0;
                        } else if (region1 & POINT_RIGHTOF_REGION) {
                                /* divide line at right */
                                *y0 = (*y0 + (*y1 - *y0) *
                                       (clip->x1  - 1 - *x0) / (*x1-*x0));
                                *x0 = clip->x1 - 1;
                        } else if (region1 & POINT_LEFTOF_REGION) {
                                /* divide line at right */
                                *y0 = (*y0 + (*y1 - *y0) *
                                       (clip->x0 - *x0) / (*x1-*x0));
                                *x0 = clip->x0;
                        }

                        region1 = REGION(*x0, *y0,
                                         clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);
                } else {
                        /* second point */
                        if (region2 & POINT_BELOW_REGION) {
                                /* divide line at bottom*/
                                *x1 = (*x0 + (*x1 - *x0) *
                                       (clip->y1  - 1 - *y0) / (*y1-*y0));
                                *y1 = clip->y1 - 1;
                        } else if (region2 & POINT_ABOVE_REGION) {
                                /* divide line at top*/
                                *x1 = (*x0 + (*x1 - *x0) *
                                       (clip->y0 - *y0) / (*y1-*y0));
                                *y1 = clip->y0;
                        } else if (region2 & POINT_RIGHTOF_REGION) {
                                /* divide line at right*/
                                *y1 = (*y0 + (*y1 - *y0) *
                                       (clip->x1  - 1 - *x0) / (*x1 - *x0));
                                *x1 = clip->x1 - 1;
                        } else if (region2 & POINT_LEFTOF_REGION) {
                                /* divide line at right*/
                                *y1 = (*y0 + (*y1 - *y0) *
                                       (clip->x0 - *x0) / (*x1 - *x0));
                                *x1 = clip->x0;
                        }

                        region2 = REGION(*x1, *y1,
                                         clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);
                }
        }

        return true;
}

bool fb_plotters_clip_line_ctx(int *x0, int *y0, int *x1, int *y1)
{
        return fb_plotters_clip_line(&fb_plot_ctx, x0, y0, x1, y1);
}

/* generic setting of clipping rectangle */
bool fb_clip(int x0, int y0, int x1, int y1)
{
        bbox_t clip;

	if (x1 < x0) SWAP(x0, x1);
	if (y1 < y0) SWAP(y0, y1);

        clip.x0 = fbtk_get_x(fbtk);
        clip.y0 = fbtk_get_y(fbtk);
        clip.x1 = fbtk_get_width(fbtk);
        clip.y1 = fbtk_get_height(fbtk);

        if (fb_plotters_clip_rect(&clip, &x0, &y0, &x1, &y1)) {
                /* new clipping region is inside the root window */
                fb_plot_ctx.x0 = x0;
                fb_plot_ctx.y0 = y0;
                fb_plot_ctx.x1 = x1;
                fb_plot_ctx.y1 = y1;
                 }

        /*LOG(("%d, %d - %d, %d clipped to %d, %d - %d, %d",
             x0,y0,x1,y1,
             fb_plot_ctx.x0, fb_plot_ctx.y0, fb_plot_ctx.x1, fb_plot_ctx.y1)); */

	return true;
}

typedef bool (linefn_t)(int x0, int y0, int x1, int y1, int width, colour c,
		bool dotted, bool dashed);


/**
 * Find find first filled span along horizontal line at given coordinate
 *
 * \param  p	array of polygon vertices (x1, y1, x2, y2, ... , xN, yN)
 * \param  n	number of polygon vertex values (N * 2)
 * \param  x	current position along current scan line
 * \param  y	position of current scan line
 * \param  x0	updated to start of filled area
 * \param  x1	updated to end of filled area
 * \return true	if an intersection was found
 */

static bool fb_plotters_find_span(const int *p, int n, int x, int y,
		int *x0, int *x1)
{
	int i;
	int p_x0, p_y0;
	int p_x1, p_y1;
	int x_new;
	bool direction = false;

	*x0 = *x1 = INT_MAX;

	for (i = 0; i < n; i = i + 2) {
		/* get line endpoints */
		if (i != n - 2) {
			/* not the last line */
			p_x0 = p[i];		p_y0 = p[i + 1];
			p_x1 = p[i + 2];	p_y1 = p[i + 3];
		} else {
			/* last line; 2nd endpoint is first vertex */
			p_x0 = p[i];	p_y0 = p[i + 1];
			p_x1 = p[0];	p_y1 = p[1];
		}
		/* ignore horizontal lines */
		if (p_y0 == p_y1)
			continue;

		/* ignore lines that don't cross this y level */
		if ((y < p_y0 && y < p_y1) || (y > p_y0 && y > p_y1))
			continue;

		if (p_x0 == p_x1) {
			/* vertical line, x is constant */
			x_new = p_x0;
		} else {
			/* find intersect */
			x_new = p_x0 + ((long long)(y - p_y0) * (p_x1 - p_x0)) /
					(p_y1 - p_y0);
		}

		/* ignore intersections before current x */
		if (x_new < x)
			continue;

		/* set nearest intersections as filled area endpoints */
		if (x_new < *x0) {
			/* nearer than first endpoint */
			*x1 = *x0;
			*x0 = x_new;
			direction = (p_y0 > p_y1);
		} else if (x_new == *x0) {
			/* same as first endpoint */
			if ((p_y0 > p_y1) != direction)
				*x1 = x_new;
		} else if (x_new < *x1) {
			/* nearer than second endpoint */
			*x1 = x_new;
		}

	}
	if (*x0 == INT_MAX)
		/* no span found */
		return false;

	/* span found */
	if (*x1 == INT_MAX) {
		*x1 = *x0;
		*x0 = x;
		return true;
	}

	return true;
}


/**
 * Plot a polygon
 *
 * \param  p	   array of polygon vertices (x1, y1, x2, y2, ... , xN, yN)
 * \param  n	   number of polygon vertices (N)
 * \param  c	   fill colour
 * \param  linefn  function to call to plot a horizontal line
 * \return true	   if no errors
 */

bool fb_plotters_polygon(const int *p, unsigned int n, colour c,
		linefn_t linefn)
{
	int poly_x0, poly_y0; /* Bounding box top left corner */
	int poly_x1, poly_y1; /* Bounding box bottom right corner */
	int i, j; /* indexes */
	int x0, x1; /* filled span extents */
	int y; /* current y coordinate */
	int y_max; /* bottom of plot area */

	/* find no. of vertex values */
	int v = n * 2;

	/* Can't plot polygons with 2 or fewer vertices */
	if (n <= 2)
		return true;

	/* Find polygon bounding box */
	poly_x0 = poly_x1 = *p;
	poly_y0 = poly_y1 = p[1];
	for (i = 2; i < v; i = i + 2) {
		j = i + 1;
		if (p[i] < poly_x0)
			poly_x0 = p[i];
		else if (p[i] > poly_x1)
			poly_x1 = p[i];
		if (p[j] < poly_y0)
			poly_y0 = p[j];
		else if (p[j] > poly_y1)
			poly_y1 = p[j];
	}

	/* Don't try to plot it if it's outside the clip rectangle */
	if (fb_plot_ctx.y1 < poly_y0 || fb_plot_ctx.y0 > poly_y1 ||
			fb_plot_ctx.x1 < poly_x0 || fb_plot_ctx.x0 > poly_x1)
		return true;

	/* Find the top of the important area */
	if (poly_y0 > fb_plot_ctx.y0)
		y = poly_y0;
	else
		y = fb_plot_ctx.y0;

	/* Find the bottom of the important area */
	if (poly_y1 < fb_plot_ctx.y1)
		y_max = poly_y1;
	else
		y_max = fb_plot_ctx.y1;

	for (; y < y_max; y++) {
		x1 = poly_x0;
		/* For each row */
		while (fb_plotters_find_span(p, v, x1, y, &x0, &x1)) {
			/* don't draw anything outside clip region */
			if (x1 < fb_plot_ctx.x0)
				continue;
			else if (x0 < fb_plot_ctx.x0)
				x0 = fb_plot_ctx.x0;
			if (x0 > fb_plot_ctx.x1)
				break;
			else if (x1 > fb_plot_ctx.x1)
				x1 = fb_plot_ctx.x1;

			/* draw this filled span on current row */
			linefn(x0, y, x1, y, 1, c, false, false);

			/* don't look for more spans if already at end of clip
			 * region or polygon */
			if (x1 == fb_plot_ctx.x1 || x1 == poly_x1)
				break;

			if (x0 == x1)
				x1++;
		}
	}
	return true;
}

bool fb_plotters_bitmap_tile(int x, int y,
                             int width, int height,
                             struct bitmap *bitmap, colour bg,
                             bool repeat_x, bool repeat_y,
                             struct content *content,
                             bool (bitmapfn)(int x, int y,
                                             int width, int height,
                                             struct bitmap *bitmap,
                                             colour bg,
                                             struct content *content))
{
	int xf,yf;

	/* x and y define coordinate of top left of of the initial explicitly
	 * placed tile. The width and height are the image scaling and the
	 * bounding box defines the extent of the repeat (which may go in all
	 * four directions from the initial tile).
	 */

	LOG(("x %d, y %d, width %d, height %d, bitmap %p, repx %d repy %d content %p", x,y,width,height,bitmap,repeat_x, repeat_y, content));

	if (!(repeat_x || repeat_y)) {
		/* Not repeating at all, so just pass it on */
		LOG(("Not repeating"));
		return bitmapfn(x, y, width, height, bitmap, bg,content);
	}

	/* get left most tile position */
	if (repeat_x)
		for (; x > fb_plot_ctx.x0; x -= width)
			;

	/* get top most tile position */
	if (repeat_y)
		for (; y > fb_plot_ctx.y0; y -= height)
			;

	/* tile down and across to extents */
	for (xf = x; xf < fb_plot_ctx.x1; xf += width) {
		for (yf = y; yf < fb_plot_ctx.y1; yf += height) {
			bitmapfn(xf, yf, width, height, bitmap, bg, content);
			if (!repeat_y)
				break;
		}
		if (!repeat_x)
	   		break;
	}
	return true;
}

bool fb_plotters_move_block(int srcx, int srcy, int width, int height, int dstx, int dsty)
{
	uint8_t *srcptr = (framebuffer->ptr +
                           (srcy * framebuffer->linelen) +
			   ((srcx * framebuffer->bpp) / 8));

	uint8_t *dstptr = (framebuffer->ptr +
                           (dsty * framebuffer->linelen) +
			   ((dstx * framebuffer->bpp) / 8));

        bbox_t redrawbox;
        int hloop;

	LOG(("from (%d,%d) w %d h %d to (%d,%d)",srcx,srcy,width,height,dstx,dsty));

        if (width == framebuffer->width) {
                /* take shortcut and use memmove */
                memmove(dstptr, srcptr, (width * height * framebuffer->bpp) / 8);
        } else {

                for (hloop = height; hloop > 0; hloop--) {
                        memmove(dstptr, srcptr, (width * framebuffer->bpp) / 8);
                        srcptr += framebuffer->linelen;
                        dstptr += framebuffer->linelen;
                }
        }
        /* callback to the os specific routine in case it needs to do something
         * explicit to redraw
         */
        redrawbox.x0 = dstx;
        redrawbox.y0 = dsty;
        redrawbox.x1 = dstx + width;
        redrawbox.y1 = dsty + height;
        fb_os_redraw(&redrawbox);

        return true;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

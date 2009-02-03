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

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "utils/log.h"
#include "utils/utf8.h"
#include "desktop/plotters.h"

#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_bitmap.h"
#include "framebuffer/fb_font.h"

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
bool fb_plotters_clip_rect(const bbox_t *clip, 
                           int *x0, int *y0, int *x1, int *y1)
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
        struct gui_window *g;

        g = window_list;

        /* LOG(("x0 %d, y0 %d, x1 %d, y1 %d", x0, y0, x1, y1)); */

	if (x1 < x0) SWAP(x0, x1);
	if (y1 < y0) SWAP(y0, y1);

        clip.x0 = g->x;
        clip.y0 = g->y;
        clip.x1 = g->x + g->width;
        clip.y1 = g->x + g->height;

        if (fb_plotters_clip_rect(&clip, &x0, &y0, &x1, &y1)) {
                /* new clipping region is inside the root window */
                fb_plot_ctx.x0 = x0;
                fb_plot_ctx.y0 = y0;
                fb_plot_ctx.x1 = x1;
                fb_plot_ctx.y1 = y1;
        }
	return true;
}

colour fb_plotters_ablend(colour pixel, colour scrpixel)
{
        int opacity = (pixel >> 24) & 0xFF;
        int r,g,b;

        r = (((pixel & 0xFF) * opacity) >> 8) +
            (((scrpixel & 0xFF) * (0xFF - opacity)) >> 8);

        g = ((((pixel & 0xFF00) >> 8) * opacity) >> 8) +
            ((((scrpixel & 0xFF00) >> 8) * (0xFF - opacity)) >> 8);

        b = ((((pixel & 0xFF0000) >> 16) * opacity) >> 8) +
            ((((scrpixel & 0xFF0000) >> 16) * (0xFF - opacity)) >> 8);
        
        return r | (g << 8) | (b << 16);
}

bool
fb_plotters_polygon(const int *p, unsigned int n, colour fill,bool (linefn)(int x0, int y0, int x1, int y1, int width, colour c, bool dotted, bool dashed))
{
        unsigned int pnt;
        const int *cur = p;
        
        for (pnt = 1; pnt < n; pnt++) {
                cur = p + (pnt << 1);
                linefn(cur[-2], cur[-1], cur[0], cur[1], 1, fill, false, false);
        }

        linefn(cur[0], cur[1], p[0], p[1], 1, fill, false, false);

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

        /* x and y define top left hand corner of tile start, the width height
         * are the mage scaling and the bounding box defines teh extent of the
         * repeat 
         */

        LOG(("x %d, y %d, width %d, height %d, bitmap %p, repx %d repy %d content %p", x,y,width,height,bitmap,repeat_x, repeat_y, content));

	if (!(repeat_x || repeat_y)) {
		/* Not repeating at all, so just pass it on */
                LOG(("Not repeating"));
		return bitmapfn(x, y, width, height, bitmap, bg,content);
	}

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
	LOG(("from (%d,%d) w %d h %d to (%d,%d)",srcx,srcy,width,height,dstx,dsty));
	uint8_t *srcptr = (framebuffer->ptr + 
                           (srcy * framebuffer->linelen) + 
			   (srcx));

	uint8_t *dstptr = (framebuffer->ptr + 
                           (dsty * framebuffer->linelen) + 
			   (dstx));

	memmove(dstptr, srcptr, (width * height * framebuffer->bpp) / 8);
        return true;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

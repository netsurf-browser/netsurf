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

static inline uint8_t *
fb_8bpp_get_xy_loc(int x, int y)
{
        return (uint8_t *)(framebuffer->ptr + 
                           (y * framebuffer->linelen) + 
                           (x));
}


static bool fb_8bpp_line(int x0, int y0, int x1, int y1, int width,
                  colour c, bool dotted, bool dashed)
{
        LOG(("%d, %d, %d, %d, %d, 0x%lx, %d, %d", 
             x0, y0, x1, y1, width, (unsigned long)c, dotted, dashed));

	return true;
}

static bool fb_8bpp_rectangle(int x0, int y0, int width, int height,
                       int line_width, colour c, bool dotted, bool dashed)
{
        fb_8bpp_line(x0, y0, x0 + width, y0, line_width, c, dotted, dashed);
        fb_8bpp_line(x0, y0 + height, x0 + width, y0 + height, line_width, c, dotted, dashed);
        fb_8bpp_line(x0, y0, x0, y0 + height, line_width, c, dotted, dashed);
        fb_8bpp_line(x0 + width, y0, x0 + width, y0 + height, line_width, c, dotted, dashed);
	return true;
}

static bool fb_8bpp_polygon(const int *p, unsigned int n, colour fill)
{
        /*LOG(("%p, %d, 0x%lx", p,n,fill));*/
        return fb_plotters_polygon(p, n, fill, fb_8bpp_line);
}

static colour calc_colour(uint8_t c)
{
        return framebuffer->palette[c];
}


static int
find_closest_palette_entry(colour c)
{
        colour palent;
        int col;

        int dr, dg, db; /* delta red, green blue values */

        int cur_distance;
        int best_distance = INT_MAX;
        int best_col = 0;

        for (col = 0; col < 256; col++) {
                palent = framebuffer->palette[col];

                dr = (c & 0xFF) - (palent & 0xFF);
                dg = ((c >> 8) & 0xFF) - ((palent >> 8) & 0xFF);
                db = ((c >> 16) & 0xFF) - ((palent >> 16) & 0xFF);
                cur_distance = ((dr * dr) + (dg * dg) + (db *db));
                if (cur_distance < best_distance) {
                        best_distance = cur_distance;
                        best_col = col;
                }
        }

        return best_col;
}

static inline uint8_t fb_colour_to_pixel(colour c)
{
        return find_closest_palette_entry(c);
}

static inline colour fb_8bpp_to_colour(uint8_t pixel)
{
        return framebuffer->palette[pixel];
}

static bool fb_8bpp_fill(int x0, int y0, int x1, int y1, colour c)
{
        int y;
        uint8_t ent;
        uint8_t *pvideo;

        if (!fb_plotters_clip_rect_ctx(&x0, &y0, &x1, &y1))
                return true; /* fill lies outside current clipping region */

        pvideo = fb_8bpp_get_xy_loc(x0, y0);

        ent = find_closest_palette_entry(c);

        for (y = y0; y < y1; y++) {
                memset(pvideo, ent, x1 - x0);
                pvideo += framebuffer->linelen;
        }

	return true;
}

static bool fb_8bpp_clg(colour c)
{
        LOG(("colour %lx", (unsigned long)c));
        fb_8bpp_fill(fb_plot_ctx.x0, 
                     fb_plot_ctx.y0, 
                     fb_plot_ctx.x1, 
                     fb_plot_ctx.y1, 
                     c);
	return true;
}

#ifdef FB_USE_FREETYPE

static bool
fb_8bpp_draw_ft_monobitmap(FT_Bitmap *bp, int x, int y, colour c)
{
        return false;
}

static bool
fb_8bpp_draw_ft_bitmap(FT_Bitmap *bp, int x, int y, colour c)
{
        uint8_t *pvideo;
        uint8_t *pixel = (uint8_t *)bp->buffer;
        colour abpixel; /* alphablended pixel */
        int xloop, yloop;
        int x0,y0,x1,y1;
	int xoff, yoff; /* x and y offset into image */
        int height = bp->rows;
        int width = bp->width;
        uint32_t fgcol;

        /* The part of the scaled image actually displayed is cropped to the
         * current context. 
         */
        x0 = x;
        y0 = y;
        x1 = x + width;
        y1 = y + height;

        if (!fb_plotters_clip_rect_ctx(&x0, &y0, &x1, &y1))
                return true;

        if (height > (y1 - y0))
                height = (y1 - y0);

        if (width > (x1 - x0))
                width = (x1 - x0);

	xoff = x0 - x;
	yoff = y0 - y;

        /* plot the image */
        pvideo = fb_8bpp_get_xy_loc(x0, y0);

        fgcol = c & 0xFFFFFF;

        for (yloop = 0; yloop < height; yloop++) {
                for (xloop = 0; xloop < width; xloop++) {
                        abpixel = (pixel[((yoff + yloop) * bp->pitch) + xloop + xoff] << 24) | fgcol;
                        if ((abpixel & 0xFF000000) != 0) {
                                if ((abpixel & 0xFF000000) != 0xFF000000) {
                                        abpixel = fb_plotters_ablend(abpixel, 
                                         fb_8bpp_to_colour(*(pvideo + xloop)));
                                }

                                *(pvideo + xloop) = fb_colour_to_pixel(abpixel);

                        }
                }
                pvideo += framebuffer->linelen;
        }

	return true;
}

static bool fb_8bpp_text(int x, int y, const struct css_style *style,
			const char *text, size_t length, colour bg, colour c)
{
        uint32_t ucs4;
        size_t nxtchr = 0;
        FT_Glyph glyph;
        FT_BitmapGlyph bglyph;

        while (nxtchr < length) {
                ucs4 = utf8_to_ucs4(text + nxtchr, length - nxtchr);
                nxtchr = utf8_next(text, length, nxtchr);

                glyph = fb_getglyph(style, ucs4);
                if (glyph == NULL)
                        continue;
                if (glyph->format == FT_GLYPH_FORMAT_BITMAP) {
                        bglyph = (FT_BitmapGlyph)glyph;

                        /* now, draw to our target surface */  
                        if (bglyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
                                fb_8bpp_draw_ft_monobitmap(&bglyph->bitmap, 
                                                            x + bglyph->left, 
                                                            y - bglyph->top,
                                                            c); 
                        } else {
                                fb_8bpp_draw_ft_bitmap(&bglyph->bitmap, 
                                                        x + bglyph->left, 
                                                        y - bglyph->top,
                                                        c); 
                        }
                }
                x += glyph->advance.x >> 16;

        }
        return true;

}
#else
static bool fb_8bpp_text(int x, int y, const struct css_style *style,
			const char *text, size_t length, colour bg, colour c)
{
        const struct fb_font_desc* fb_font = fb_get_font(style);
        const uint32_t *font_data;
        uint32_t row;

        int xloop, yloop;
        size_t chr;

        uint8_t *pvideo;
        uint8_t fgcol;

	unsigned char *buffer = NULL;
        int x0,y0,x1,y1;
	int xoff, yoff; /* x and y offset into image */
        int height = fb_font->height;

        /* aquire thge text in local font encoding */
	utf8_to_font_encoding(fb_font, text, length, (char **)&buffer);
	if (!buffer) 
                return true;
        length = strlen((char *)buffer);


        /* y is given to the fonts baseline we need it to the fonts top */
        y-=((fb_font->height * 75)/100);

        y+=1; /* the coord is the bottom-left of the pixels offset by 1 to make
               *   it work since fb coords are the top-left of pixels 
               */

        /* The part of the text displayed is cropped to the current context. */
        x0 = x;
        y0 = y;
        x1 = x + (fb_font->width * length);
        y1 = y + fb_font->height;

        if (!fb_plotters_clip_rect_ctx(&x0, &y0, &x1, &y1))
                return true; /* text lies outside current clipping region */

        /* find width and height to plot */
        if (height > (y1 - y0))
                height = (y1 - y0);

	xoff = x0 - x;
	yoff = y0 - y;

        fgcol = find_closest_palette_entry(c);

        /*LOG(("x %d, y %d, style %p, txt %.*s , len %d, bg 0x%lx, fg 0x%lx",
          x,y,style,length,text,length,bg,c));*/

        for (chr = 0; chr < length; chr++, x += fb_font->width) {
                if ((x + fb_font->width) > x1)
                        break;

                if (x < x0) 
                        continue;

                pvideo = fb_8bpp_get_xy_loc(x, y0);

                /* move our font-data to the correct position */
                font_data = fb_font->data + (buffer[chr] * fb_font->height);

                for (yloop = 0; yloop < height; yloop++) {
                        row = font_data[yoff + yloop];
                        for (xloop = fb_font->width; xloop > 0 ; xloop--) {
                                if ((row & 1) != 0)
                                        *(pvideo + xloop) = fgcol;
                                row = row >> 1;
                        }
                        pvideo += framebuffer->linelen;
                }
        }

	free(buffer);
	return true;
}
#endif

static bool fb_8bpp_disc(int x, int y, int radius, colour c, bool filled)
{
        LOG(("x %d, y %d, rad %d, c 0x%lx, fill %d", x, y, radius, (unsigned long)c, filled));
	return true;
}

static bool fb_8bpp_arc(int x, int y, int radius, int angle1, int angle2,
                 colour c)
{
        LOG(("x %d, y %d, radius %d, angle1 %d, angle2 %d, c 0x%lx", 
             x, y, radius, angle1, angle2, (unsigned long)c));
	return true;
}



static bool fb_8bpp_bitmap(int x, int y, int width, int height,
                    struct bitmap *bitmap, colour bg, 
                    struct content *content)
{
        uint8_t *pvideo;
        colour *pixel = (colour *)bitmap->pixdata;
        colour abpixel; /* alphablended pixel */
        int xloop, yloop;
        int x0,y0,x1,y1;
	int xoff, yoff; /* x and y offset into image */

        /* LOG(("x %d, y %d, width %d, height %d, bitmap %p, content %p", 
           x,y,width,height,bitmap,content));*/

        /* TODO here we should scale the image from bitmap->width to width, for
         * now simply crop. 
         */
        if (width > bitmap->width)
                width = bitmap->width;

        if (height > bitmap->height)
                height = bitmap->height;

        /* The part of the scaled image actually displayed is cropped to the
         * current context. 
         */
        x0 = x;
        y0 = y;
        x1 = x + width;
        y1 = y + height;

        if (!fb_plotters_clip_rect_ctx(&x0, &y0, &x1, &y1))
                return true;

        if (height > (y1 - y0))
                height = (y1 - y0);

        if (width > (x1 - x0))
                width = (x1 - x0);

	xoff = x0 - x;
	yoff = (y0 - y) * bitmap->width;
	height = height * bitmap->width + yoff;

        /* plot the image */
        pvideo = fb_8bpp_get_xy_loc(x0, y0);

        if (bitmap->opaque) {
                for (yloop = yoff; yloop < height; yloop += bitmap->width) {
                        for (xloop = 0; xloop < width; xloop++) {
                                abpixel = pixel[yloop + xloop + xoff];
                                *(pvideo + xloop) = fb_colour_to_pixel(abpixel);
                        }
                        pvideo += framebuffer->linelen;
                }
        } else {
                for (yloop = yoff; yloop < height; yloop += bitmap->width) {
                        for (xloop = 0; xloop < width; xloop++) {
                                abpixel = pixel[yloop + xloop + xoff];
                                if ((abpixel & 0xFF000000) != 0) {
                                        if ((abpixel & 0xFF000000) != 0xFF000000) {
                                                abpixel = fb_plotters_ablend(abpixel, 
                                                                             fb_8bpp_to_colour(*(pvideo + xloop)));
                                        }

                                        *(pvideo + xloop) = fb_colour_to_pixel(abpixel);
                                }
                        }
                        pvideo += framebuffer->linelen;
                }
        }

	return true;
}

static bool fb_8bpp_bitmap_tile(int x, int y, int width, int height,
                         struct bitmap *bitmap, colour bg,
                         bool repeat_x, bool repeat_y, 
                         struct content *content)
{
        return fb_plotters_bitmap_tile(x, y, width, height, 
                                       bitmap, bg, repeat_x, repeat_y,
                                       content, fb_8bpp_bitmap);
}

static bool fb_8bpp_flush(void)
{
        LOG(("%s()\n", __func__));
	return true;
}

static bool fb_8bpp_path(const float *p, 
                         unsigned int n, 
                         colour fill, 
                         float width,
                         colour c, 
                         const float transform[6])
{
        LOG(("%f, %d, 0x%lx, %f, 0x%lx, %f", 
             *p, n, (unsigned long)fill, width, (unsigned long)c, *transform));

	return true;
}

const struct plotter_table framebuffer_8bpp_plot = {
	.clg = fb_8bpp_clg,
	.rectangle = fb_8bpp_rectangle,
	.line = fb_8bpp_line,
	.polygon = fb_8bpp_polygon,
	.fill = fb_8bpp_fill,
	.clip = fb_clip,
	.text = fb_8bpp_text,
	.disc = fb_8bpp_disc,
	.arc = fb_8bpp_arc,
	.bitmap = fb_8bpp_bitmap,
	.bitmap_tile = fb_8bpp_bitmap_tile,
	.flush = fb_8bpp_flush, 
	.path = fb_8bpp_path,
        .option_knockout = true,
};


/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

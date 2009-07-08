/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 *
 * Framebuffer interface
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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>
#include <libnsfb_legacy_plot.h>
#include <libnsfb_event.h>
#include <libnsfb_cursor.h>

#include "utils/log.h"
#include "desktop/browser.h"
#include "desktop/plotters.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/framebuffer.h"
#include "framebuffer/bitmap.h"
#include "framebuffer/font.h"

/* netsurf framebuffer library handle */
static nsfb_t *nsfb;

#ifdef FB_USE_FREETYPE

static bool framebuffer_plot_text(int x, int y, const struct css_style *style,
			const char *text, size_t length, colour bg, colour c)
{
        uint32_t ucs4;
        size_t nxtchr = 0;
        FT_Glyph glyph;
        FT_BitmapGlyph bglyph;
        nsfb_bbox_t loc;

        while (nxtchr < length) {
                ucs4 = utf8_to_ucs4(text + nxtchr, length - nxtchr);
                nxtchr = utf8_next(text, length, nxtchr);

                glyph = fb_getglyph(style, ucs4);
                if (glyph == NULL)
                        continue;

                if (glyph->format == FT_GLYPH_FORMAT_BITMAP) {
                        bglyph = (FT_BitmapGlyph)glyph;

                        loc.x0 = x + bglyph->left;
                        loc.y0 = y - bglyph->top;
                        loc.x1 = loc.x0 + bglyph->bitmap.width;
                        loc.y1 = loc.y0 + bglyph->bitmap.rows;

                        /* now, draw to our target surface */
                        if (bglyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
                            nsfb_plot_glyph1(nsfb, 
                                             &loc, 
                                             bglyph->bitmap.buffer, 
                                             bglyph->bitmap.pitch, 
                                             c);
                        } else {
                            nsfb_plot_glyph8(nsfb, 
                                             &loc, 
                                             bglyph->bitmap.buffer, 
                                             bglyph->bitmap.pitch, 
                                             c);
                        }
                }
                x += glyph->advance.x >> 16;

        }
        return true;

}
#else
static bool framebuffer_plot_text(int x, int y, const struct css_style *style,
			const char *text, size_t length, colour bg, colour c)
{
    const struct fb_font_desc* fb_font = fb_get_font(style);
    const uint32_t *chrp;
    char *buffer = NULL;
    int chr;
    int blen;
    nsfb_bbox_t loc;

    utf8_to_font_encoding(fb_font, text, length, &buffer);
    if (buffer == NULL)
        return true;

        /* y is given to the fonts baseline we need it to the fonts top */
        y-=((fb_font->height * 75)/100);

        y+=1; /* the coord is the bottom-left of the pixels offset by 1 to make
               *   it work since fb coords are the top-left of pixels
               */

    blen = strlen(buffer);

    for (chr = 0; chr < blen; chr++) {
        loc.x0 = x;
        loc.y0 = y;
        loc.x1 = loc.x0 + fb_font->width;
        loc.y1 = loc.y0 + fb_font->height;

        chrp = fb_font->data + ((unsigned char)buffer[chr] * fb_font->height);
        nsfb_plot_glyph1(nsfb, &loc, (uint8_t *)chrp, 32, c);

        x+=fb_font->width;

    }

    free(buffer);
    return true;
}
#endif


static bool 
framebuffer_plot_bitmap(int x, int y,
                        int width, int height,
                        struct bitmap *bitmap, colour bg,
                        bitmap_flags_t flags)
{
	int xf,yf;
        nsfb_bbox_t loc;
        nsfb_bbox_t clipbox;
        bool repeat_x = (flags & BITMAPF_REPEAT_X);
        bool repeat_y = (flags & BITMAPF_REPEAT_Y);

        nsfb_plot_get_clip(nsfb, &clipbox);

	/* x and y define coordinate of top left of of the initial explicitly
	 * placed tile. The width and height are the image scaling and the
	 * bounding box defines the extent of the repeat (which may go in all
	 * four directions from the initial tile).
	 */

	if (!(repeat_x || repeat_y)) {
		/* Not repeating at all, so just plot it */
                loc.x0 = x;
                loc.y0 = y;
                loc.x1 = loc.x0 + width;
                loc.y1 = loc.y0 + height;

                return nsfb_plot_bitmap(nsfb, &loc, 
                                        (nsfb_colour_t *)bitmap->pixdata, 
                                        bitmap->width, bitmap->height, 
                                        bitmap->width, !bitmap->opaque);
	}

	/* get left most tile position */
	if (repeat_x)
		for (; x > clipbox.x0; x -= width);

	/* get top most tile position */
	if (repeat_y)
		for (; y > clipbox.y0; y -= height);

	/* tile down and across to extents */
	for (xf = x; xf < clipbox.x1; xf += width) {
		for (yf = y; yf < clipbox.y1; yf += height) {

                        loc.x0 = xf;
                        loc.y0 = yf;
                        loc.x1 = loc.x0 + width;
                        loc.y1 = loc.y0 + height;

                        nsfb_plot_bitmap(nsfb, &loc, 
                                        (nsfb_colour_t *)bitmap->pixdata, 
                                        bitmap->width, bitmap->height, 
                                        bitmap->width, !bitmap->opaque);

			if (!repeat_y)
				break;
		}
		if (!repeat_x)
	   		break;
	}
	return true;
}

static bool 
framebuffer_plot_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	nsfb_bbox_t rect;
	bool dotted = false; 
	bool dashed = false;

	rect.x0 = x0;
	rect.y0 = y0;
	rect.x1 = x1;
	rect.y1 = y1;

	if (style->fill_type != PLOT_OP_TYPE_NONE) {  
		nsfb_plot_rectangle_fill(nsfb, &rect, style->fill_colour);
	}
    
	if (style->stroke_type != PLOT_OP_TYPE_NONE) {
		if (style->stroke_type == PLOT_OP_TYPE_DOT) 
			dotted = true;

		if (style->stroke_type == PLOT_OP_TYPE_DASH) 
			dashed = true;

		nsfb_plot_rectangle(nsfb, &rect, style->stroke_width, style->stroke_colour, dotted, dashed); 
	}

	return true;
}

static bool framebuffer_plot_flush(void)
{
	LOG(("flush unimplemnted"));
	return true;
}

static bool 
framebuffer_plot_path(const float *p, 
                      unsigned int n, 
                      colour fill, 
                      float width,
                      colour c, 
                      const float transform[6])
{
	LOG(("path unimplemented"));
	return true;
}

struct plotter_table plot = {
	.rectangle = framebuffer_plot_rectangle,
	.line = nsfb_lplot_line,
	.polygon = nsfb_lplot_polygon,
	.clip = nsfb_lplot_clip,
	.text = framebuffer_plot_text,
	.disc = nsfb_lplot_disc,
	.arc = nsfb_lplot_arc,
	.bitmap = framebuffer_plot_bitmap,
	.flush = framebuffer_plot_flush,
	.path = framebuffer_plot_path,
        .option_knockout = true,
};



nsfb_t *
framebuffer_initialise(int argc, char** argv)
{
    const char *fename;
    enum nsfb_frontend_e fetype;

    /* select frontend from commandline */
    if ((argc > 2) && (argv[1][0] == '-') && 
        (argv[1][1] == 'f') && 
        (argv[1][2] == 'e') && 
        (argv[1][3] == 0)) {
        int argcmv;
        fename = (const char *)argv[2];
        for (argcmv = 3; argcmv < argc; argcmv++) {
            argv[argcmv - 2] = argv[argcmv];
        }
        argc-=2;
    } else {
        fename="sdl";
    }

    fetype = nsfb_frontend_from_name(fename);
    if (fetype == NSFB_FRONTEND_NONE) {
        LOG(("The %s frontend is not available from libnsfb\n", fename));
        return NULL;
    }

    nsfb = nsfb_init(fetype);
    if (nsfb == NULL) {
        LOG(("Unable to initialise nsfb with %s frontend\n", fename));
        return NULL;
    }
    
    if (nsfb_set_geometry(nsfb, 0, 0, 32) == -1) {
        LOG(("Unable to set geometry\n"));
        nsfb_finalise(nsfb);
        return NULL;
    }

    nsfb_cursor_init(nsfb);
    
    if (nsfb_init_frontend(nsfb) == -1) {
        LOG(("Unable to initialise nsfb frontend\n"));
        nsfb_finalise(nsfb);
        return NULL;
    }

    nsfb_lplot_ctx(nsfb);

    return nsfb;

}

void
framebuffer_finalise(void)
{
    nsfb_finalise(nsfb);    
}

bool
framebuffer_set_cursor(struct bitmap *bm)
{
    return nsfb_cursor_set(nsfb, (nsfb_colour_t *)bm->pixdata, bm->width, bm->height, bm->width);
} 

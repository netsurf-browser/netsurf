/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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
#include <math.h>
#include <stdbool.h>
#include <windom.h>

#include "image/bitmap.h"
#include "utils/log.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "desktop/gui.h"
#include "desktop/plotters.h"

#include "atari/bitmap.h"
#include "atari/gui.h"
#include "atari/plot.h"
#include "atari/options.h"
#include "desktop/options.h"
#include "atari/plot.h"

GEM_PLOTTER plotter = NULL;
GEM_FONT_PLOTTER fplotter = NULL;

extern short vdih;

/*
Init screen and font driver objects.
Returns non-zero value > -1 when the objects could be succesfully created.
Returns value < 0 to indicate an error
*/

int atari_plotter_init( char* drvrname, char * fdrvrname )
{
	GRECT loc_pos={0,0,360,400};
	int err=0;
	struct s_driver_table_entry * drvinfo;
	int flags = 0;
	unsigned long font_flags = 0;

	if( option_atari_dither == 1)
		flags |= PLOT_FLAG_DITHER;
	if( option_atari_transparency == 1 )
		flags |= PLOT_FLAG_TRANS;
	if( option_atari_font_monochrom == 1 )
		font_flags |= FONTPLOT_FLAG_MONOGLYPH;

	vdih = app.graf.handle;
	if( verbose_log ) {
		dump_vdi_info( vdih ) ;
		dump_plot_drivers();
		dump_font_drivers();
	}
	drvinfo = get_screen_driver_entry( drvrname );

	LOG(("using plotters: %s, %s", drvrname, fdrvrname));
	fplotter = new_font_plotter(vdih, fdrvrname, font_flags, &err );
	if(err){
		char * desc = plotter_err_str(err);
		die(("Unable to load font plotter %s -> %s", fdrvrname, desc ));
	}

	plotter = new_plotter( vdih, drvrname, &loc_pos, drvinfo->max_bpp,
							flags, fplotter, &err );
	if(err){
		char * desc = plotter_err_str(err);
		die(("Unable to load graphics plotter %s -> %s", drvrname, desc ));
	}

	return( err );
}

int atari_plotter_finalise( void )
{
	delete_plotter( plotter );
	delete_font_plotter( fplotter );
}

bool plot_rectangle( int x0, int y0, int x1, int y1,
			const plot_style_t *style )
{
	plotter->rectangle( plotter, x0, y0, x1, y1, style );
	return ( true );
}

bool plot_line( int x0, int y0, int x1, int y1,
			const plot_style_t *style )
{
	plotter->line( plotter, x0, y0, x1, y1, style );
	return ( true );
}

static bool plot_polygon(const int *p, unsigned int n,
				const plot_style_t *style)
{
	plotter->polygon( plotter, p, n, style );
	return ( true );
}

bool plot_clip(const struct rect *clip)
{
	plotter->clip( plotter, clip );
	return ( true );
}


bool plot_get_clip(struct rect * out){
	plotter_get_clip( plotter , out );
	return( true );
}


static bool plot_text(int x, int y, const char *text, size_t length, const plot_font_style_t *fstyle )
{
	plotter->text( plotter, x, y, text, length, fstyle );
	return ( true );
}

static bool plot_disc(int x, int y, int radius, const plot_style_t *style)
{
	plotter->disc(plotter, x, y, radius, style );
	return ( true );
}

static bool plot_arc(int x, int y, int radius, int angle1, int angle2,
	    		const plot_style_t *style)
{
	plotter->arc( plotter, x, y, radius, angle1, angle2, style );
	return ( true );
}

static bool plot_bitmap(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bitmap_flags_t flags)
{
	struct bitmap * bm = NULL;
	bool repeat_x = (flags & BITMAPF_REPEAT_X);
	bool repeat_y = (flags & BITMAPF_REPEAT_Y);
	int bmpw,bmph;
	struct rect clip;

	bmpw = bitmap_get_width(bitmap);
	bmph = bitmap_get_height(bitmap);

	if ( repeat_x || repeat_y ) {
		plotter_get_clip( plotter, &clip );
		if( repeat_x && width == 1 && repeat_y && height == 1 ){
			width = MAX( width, clip.x1 - x );
			height = MAX( height,  clip.y1 - y );
		}
		else if( repeat_x && width == 1 ){
			width = MAX( width, clip.x1 - x);
		}
		else if( repeat_y && height == 1){
			height = MAX( height, clip.y1 - y );
		}
	}

	if(  width != bmpw || height != bmph ) {
		plotter->bitmap_resize(plotter, bitmap, width, height );
		if( bitmap->resized )
			bm = bitmap->resized;
		else
			bm = bitmap;
	} else {
		bm = bitmap;
	}

	/* out of memory? */
	if( bm == NULL ) {
		printf("plot: out of memory! bmp: %p, bmpres: %p\n", bitmap, bitmap->resized );
		return( true );
	}

	if (!(repeat_x || repeat_y)) {
		plotter->bitmap( plotter, bm, x, y, bg, flags );
	} else {
		int xf,yf;
		int xoff = x;
		int yoff = y;

		if (yoff > clip.y0 )
			yoff = (clip.y0 - height) + ((yoff - clip.y0) % height);
		if (xoff > clip.x0 )
			xoff = (clip.x0 - width) + ((xoff - clip.x0) % width);
		/* for now, repeating just works in the rigth / down direction */
		/*
		if( repeat_x == true )
			xoff = clip.x0;
		if(repeat_y == true )
			yoff = clip.y0;
		*/

		for( xf = xoff; xf < clip.x1; xf += width ) {
			for( yf = yoff; yf < clip.y1; yf += height ) {
				plotter->bitmap( plotter, bm, xf, yf, bg, flags );
				if (!repeat_y)
					break;
			}
			if (!repeat_x)
	   			break;
		}
	}
	return ( true );
}

static bool plot_path(const float *p, unsigned int n, colour fill, float width,
											colour c, const float transform[6])
{
	plotter->path( plotter, p, n, fill, width, c, transform );
	return ( true );
}



const struct plotter_table atari_plotters = {
	.rectangle = plot_rectangle,
	.line = plot_line,
	.polygon = plot_polygon,
	.clip = plot_clip,
	.text = plot_text,
	.disc = plot_disc,
	.arc = plot_arc,
	.bitmap = plot_bitmap,
	.path = plot_path,
	.flush = NULL,
	.group_start = NULL,
	.group_end = NULL,
	.option_knockout = true
};

/*
 * Copyright 2012 Ole Loots <ole@monochrom.net>
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

#ifdef WITH_GD_PLOTTER

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <windom.h>
#include <gd.h>

#include "atari/plot/eddi.h"
#include "atari/plot/plotter.h"
#include "atari/plot/plotter_gd.h"

#include "plotter.h"

static int dtor( GEM_PLOTTER self );
static int resize( GEM_PLOTTER self, int w, int h );
static int move( GEM_PLOTTER self, short x, short y );
static int lock( GEM_PLOTTER self );
static int unlock( GEM_PLOTTER self );
static int put_pixel(GEM_PLOTTER self, int x, int y, int color );
static int copy_rect( GEM_PLOTTER self, GRECT src, GRECT dst );
static int get_clip( GEM_PLOTTER instance, struct rect * clip);
static int set_clip( GEM_PLOTTER instance, const struct rect * clip );
static int arc(GEM_PLOTTER self,int x, int y, int radius, int angle1, int angle2, const plot_style_t * pstyle);
static int disc(GEM_PLOTTER self,int x, int y, int radius, const plot_style_t * pstyle);
static int line(GEM_PLOTTER self,int x0, int y0, int x1, int y1, const plot_style_t * pstyle);
static int rectangle(GEM_PLOTTER self,int x0, int y0, int x1, int y1, const plot_style_t * pstyle);
static int polygon(GEM_PLOTTER self,const int *p, unsigned int n, const plot_style_t * pstyle);
static int path(GEM_PLOTTER self,const float *p, unsigned int n, int fill, float width, int c, const float transform[6]);
static int bitmap_resize( GEM_PLOTTER self, struct bitmap * img, int nw, int nh );
static int bitmap_convert( GEM_PLOTTER self, struct bitmap * img, int x, int y,
	GRECT * clip,uint32_t bg,uint32_t flags, MFDB *out  );
static int bitmap( GEM_PLOTTER self, struct bitmap * bmp, int x, int y,
					unsigned long bg, unsigned long flags );
static int plot_mfdb( GEM_PLOTTER self, GRECT * where, MFDB * mfdb, unsigned char fgcolor, uint32_t flags);
static int text(GEM_PLOTTER self, int x, int y, const char *text,size_t length, const plot_font_style_t *fstyle);
static int clip(GEM_PLOTTER self, const struct rect * clip);

int ctor_plotter_gd( GEM_PLOTTER instance, GRECT * origin_size )
{

	instance->dtor = dtor;
	instance->resize= resize;
	instance->move = move;
	instance->lock = lock;
	instance->unlock = unlock;
	instance->put_pixel = put_pixel;
	instance->copy_rect = copy_rect;
	instance->get_clip = get_clip;
	instance->set_clip = set_clip;
	instance->arc = arc;
	instance->disc = disc;
	instance->line = line;
	instance->rectangle = rectangle;
	instance->polygon = polygon;
	instance->path = path;
	instance->bitmap = bitmap;
	instance->bitmap_resize = bitmap_resize;
	instance->bitmap_convert = bitmap_convert;
	instance->plot_mfdb = NULL;
	instance->text = text;

	instance->priv_data = malloc( sizeof(struct s_gd_priv_data) );
	if( instance->priv_data == NULL )
		return( 0-ERR_NO_MEM );
	memset( instance->priv_data, 0, sizeof(struct s_gd_priv_data) );

	// allocate framebuffer
	THIS(instance)->vbuf = gdImageCreateTrueColor( origin_size->g_w,
												origin_size->g_h );

	THIS(instance)->origin_x = origin_size->g_x;
	THIS(instance)->origin_y = origin_size->g_y;

	return( 1 );
}

static int dtor( GEM_PLOTTER instance )
{
	int i;
	free( instance->priv_data );
	gdImageDestroy( THIS(instance)->vbuf );
	return( 1 );
}

static int resize( GEM_PLOTTER instance, int w, int h )
{
	return( 1 );
}

static int move( GEM_PLOTTER instance, short x, short y )
{
	THIS(instance)->origin_x = x;
	THIS(instance)->origin_y = y;
	return( 1 );
}

static int lock( GEM_PLOTTER instance ){
	instance->flags |= PLOT_FLAG_LOCKED;
	return( 1 );
}

static int unlock( GEM_PLOTTER instance )
{
	instance->flags &=  ~PLOT_FLAG_LOCKED;
	return( 1 );
}

static int put_pixel(GEM_PLOTTER instance, int x, int y, int color )
{
	gdImageSetPixel( THIS(instance)->vbuf, x, y, color );
	return( 1 );
}

static int copy_rect( GEM_PLOTTER instance, GRECT src, GRECT dst )
{
	return( 1 );
}

static int arc(GEM_PLOTTER instance,int x, int y, int radius, int angle1, int angle2, const plot_style_t * pstyle)
{
	return( 1 );
}

static int disc(GEM_PLOTTER instance,int x, int y, int radius, const plot_style_t * pstyle)
{
	return( 1 );
}

static int line(GEM_PLOTTER instance,int x0, int y0, int x1, int y1, const plot_style_t * pstyle)
{
	int w = pstyle->stroke_width;
	if( ((w % 2) == 0) || (w < 1) ){
		w++;
	}
	gdImageSetThickness( THIS(instance)->vbuf, w );
	// FIXME: set stroke style
	//gdImageSetStyle( THIS(instance), style, nofpix );
	gdImageLine( THIS(instance)->vbuf, x0, y0, x1, y1, pstyle->stroke_colour );
	return( 1 );
}

static int rectangle(GEM_PLOTTER instance,int x0, int y0, int x1, int y1, const plot_style_t * pstyle)
{
	int lw = pstyle->stroke_width;

	if( pstyle->fill_type != PLOT_OP_TYPE_NONE ){
		gdImageFilledRectangle( THIS(instance)->vbuf,
								x0, y0, x1, y1,
								pstyle->fill_colour );
	}

	if( pstyle->stroke_type != PLOT_OP_TYPE_NONE ){
		gdImageLine( THIS(instance)->vbuf,
					x0, y0, x1, y1,
					pstyle->stroke_colour );
	}
	return( 1 );
}

static int polygon(GEM_PLOTTER instance,const int *p, unsigned int n, const plot_style_t * pstyle)
{
	//gdImagePolygon( THIS(instance).vbuf, points, count, c );
	return( 1 );
}
static int path(GEM_PLOTTER instance,const float *p, unsigned int n, int fill, float width, int c, const float transform[6])
{
	return( 1 );
}

static int bitmap_resize( GEM_PLOTTER instance, struct bitmap * img, int nw, int nh )
{
	return( 1 );
}

static int bitmap_convert( GEM_PLOTTER instance, struct bitmap * img, int x, int y,
	GRECT * clip,uint32_t bg,uint32_t flags, MFDB *out  )
{
		return( 1 );
}

static int bitmap( GEM_PLOTTER instance, struct bitmap * bmp, int x, int y,
					unsigned long bg, unsigned long flags )
{
	return( 1 );
}

static int plot_mfdb( GEM_PLOTTER instance, GRECT * where, MFDB * mfdb, unsigned char fgcolor, uint32_t flags)
{
	return( 1 );
}

static int text( GEM_PLOTTER instance, int x, int y, const char *text,size_t length, const plot_font_style_t *fstyle)
{
	return( 1 );
}


static int get_clip( GEM_PLOTTER instance, struct rect * clip)
{
	gdImageGetClip( THIS(instance)->vbuf,
					&clip->x0, &clip->y0,
					&clip->x0, &clip->y0 );
	return( 1 );
}

static int set_clip( GEM_PLOTTER instance, const struct rect * clip )
{
	gdImageSetClip( THIS(instance)->vbuf, clip->x0,
						clip->y0, clip->x1,
						clip->y1 );
	return ( 1 );
}


#endif

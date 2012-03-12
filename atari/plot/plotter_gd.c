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

int ctor_plotter_gd( GEM_PLOTTER instance ){

	instance->dtor = dtor;
	instance->resize= resize;
	instance->move = move;
	instance->lock = lock;
	instance->unlock = unlock;
	instance->put_pixel = put_pixel;
	instance->copy_rect = copy_rect;
	instance->clip = plotter_std_clip;
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

	return( 1 );
}

static int dtor( GEM_PLOTTER instance )
{
	int i;
	for( i=0; i<MAX_FRAMEBUFS; i++) {
		if( instance->fbuf[i].mem != NULL )
			free( instance->fbuf[i].mem );
	}
	free( instance->priv_data );
	return( 1 );
}

static int resize( GEM_PLOTTER self, int w, int h )
{
	return( 1 );
}

static int move( GEM_PLOTTER self, short x, short y )
{
	return( 1 );
}

static int lock( GEM_PLOTTER self ){
	return( 1 );
}

static int unlock( GEM_PLOTTER self )
{
	return( 1 );
}

static int put_pixel(GEM_PLOTTER self, int x, int y, int color )
{
	return( 1 );

}

static int copy_rect( GEM_PLOTTER self, GRECT src, GRECT dst )
{
	return( 1 );
}

static int arc(GEM_PLOTTER self,int x, int y, int radius, int angle1, int angle2, const plot_style_t * pstyle)
{
	return( 1 );
}

static int disc(GEM_PLOTTER self,int x, int y, int radius, const plot_style_t * pstyle)
{
	return( 1 );
}

static int line(GEM_PLOTTER self,int x0, int y0, int x1, int y1, const plot_style_t * pstyle)
{
	return( 1 );
}

static int rectangle(GEM_PLOTTER self,int x0, int y0, int x1, int y1, const plot_style_t * pstyle)
{
	return( 1 );
}

static int polygon(GEM_PLOTTER self,const int *p, unsigned int n, const plot_style_t * pstyle)
{
	return( 1 );
}
static int path(GEM_PLOTTER self,const float *p, unsigned int n, int fill, float width, int c, const float transform[6])
{
	return( 1 );
}

static int bitmap_resize( GEM_PLOTTER self, struct bitmap * img, int nw, int nh )
{
	return( 1 );
}

static int bitmap_convert( GEM_PLOTTER self, struct bitmap * img, int x, int y,
	GRECT * clip,uint32_t bg,uint32_t flags, MFDB *out  )
{
		return( 1 );
}

static int bitmap( GEM_PLOTTER self, struct bitmap * bmp, int x, int y,
					unsigned long bg, unsigned long flags )
{
	return( 1 );
}

static int plot_mfdb( GEM_PLOTTER self, GRECT * where, MFDB * mfdb, unsigned char fgcolor, uint32_t flags)
{
	return( 1 );
}

static int text(GEM_PLOTTER self, int x, int y, const char *text,size_t length, const plot_font_style_t *fstyle)
{
	return( 1 );
}


#endif

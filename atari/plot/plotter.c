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
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <windom.h>
#include <assert.h>
#include <mint/osbind.h>
#include <mint/cookie.h>
#include <Hermes/Hermes.h>

#include "desktop/plot_style.h"
#include "atari/bitmap.h"
#include "image/bitmap.h"
#include "atari/plot/eddi.h"
#include "atari/plot/plotter.h"
#include "atari/plot/plotter_vdi.h"
#ifdef WITH_VDI_FONT_DRIVER
 #include "atari/plot/font_vdi.h"
#endif
#ifdef WITH_INTERNAL_FONT_DRIVER
 #include "atari/plot/font_internal.h"
#endif
#ifdef WITH_FREETYPE_FONT_DRIVER
 #include "atari/plot/font_freetype.h"
#endif
#include "atari/gui.h"
#include "utils/log.h"
#include "atari/misc.h"
#include "atari/osspec.h"


/* get index to driver in driver list by name */
static int drvrname_idx( char * name );

/* Error code translations: */
static const char * plot_error_codes[] =
{
	"None",
	"ERR_BUFFERSIZE_EXCEEDS_SCREEN",
	"ERR_NO_MEM",
	"ERR_PLOTTER_NOT_AVAILABLE"
};

#ifdef WITH_8BPP_SUPPORT
unsigned short vdi_web_pal[216][3] = {
	{0x000,0x000,0x000}, {0x0c8,0x000,0x000}, {0x190,0x000,0x000}, {0x258,0x000,0x000}, {0x320,0x000,0x000}, {0x3e8,0x000,0x000},
	{0x000,0x0c8,0x000}, {0x0c8,0x0c8,0x000}, {0x190,0x0c8,0x000}, {0x258,0x0c8,0x000}, {0x320,0x0c8,0x000}, {0x3e8,0x0c8,0x000},
	{0x000,0x190,0x000}, {0x0c8,0x190,0x000}, {0x190,0x190,0x000}, {0x258,0x190,0x000}, {0x320,0x190,0x000}, {0x3e8,0x190,0x000},
	{0x000,0x258,0x000}, {0x0c8,0x258,0x000}, {0x190,0x258,0x000}, {0x258,0x258,0x000}, {0x320,0x258,0x000}, {0x3e8,0x258,0x000},
	{0x000,0x320,0x000}, {0x0c8,0x320,0x000}, {0x190,0x320,0x000}, {0x258,0x320,0x000}, {0x320,0x320,0x000}, {0x3e8,0x320,0x000},
	{0x000,0x3e8,0x000}, {0x0c8,0x3e8,0x000}, {0x190,0x3e8,0x000}, {0x258,0x3e8,0x000}, {0x320,0x3e8,0x000}, {0x3e8,0x3e8,0x000},
	{0x000,0x000,0x0c8}, {0x0c8,0x000,0x0c8}, {0x190,0x000,0x0c8}, {0x258,0x000,0x0c8}, {0x320,0x000,0x0c8}, {0x3e8,0x000,0x0c8},
	{0x000,0x0c8,0x0c8}, {0x0c8,0x0c8,0x0c8}, {0x190,0x0c8,0x0c8}, {0x258,0x0c8,0x0c8}, {0x320,0x0c8,0x0c8}, {0x3e8,0x0c8,0x0c8},
	{0x000,0x190,0x0c8}, {0x0c8,0x190,0x0c8}, {0x190,0x190,0x0c8}, {0x258,0x190,0x0c8}, {0x320,0x190,0x0c8}, {0x3e8,0x190,0x0c8},
	{0x000,0x258,0x0c8}, {0x0c8,0x258,0x0c8}, {0x190,0x258,0x0c8}, {0x258,0x258,0x0c8}, {0x320,0x258,0x0c8}, {0x3e8,0x258,0x0c8},
	{0x000,0x320,0x0c8}, {0x0c8,0x320,0x0c8}, {0x190,0x320,0x0c8}, {0x258,0x320,0x0c8}, {0x320,0x320,0x0c8}, {0x3e8,0x320,0x0c8},
	{0x000,0x3e8,0x0c8}, {0x0c8,0x3e8,0x0c8}, {0x190,0x3e8,0x0c8}, {0x258,0x3e8,0x0c8}, {0x320,0x3e8,0x0c8}, {0x3e8,0x3e8,0x0c8},
	{0x000,0x000,0x190}, {0x0c8,0x000,0x190}, {0x190,0x000,0x190}, {0x258,0x000,0x190}, {0x320,0x000,0x190}, {0x3e8,0x000,0x190},
	{0x000,0x0c8,0x190}, {0x0c8,0x0c8,0x190}, {0x190,0x0c8,0x190}, {0x258,0x0c8,0x190}, {0x320,0x0c8,0x190}, {0x3e8,0x0c8,0x190},
	{0x000,0x190,0x190}, {0x0c8,0x190,0x190}, {0x190,0x190,0x190}, {0x258,0x190,0x190}, {0x320,0x190,0x190}, {0x3e8,0x190,0x190},
	{0x000,0x258,0x190}, {0x0c8,0x258,0x190}, {0x190,0x258,0x190}, {0x258,0x258,0x190}, {0x320,0x258,0x190}, {0x3e8,0x258,0x190},
	{0x000,0x320,0x190}, {0x0c8,0x320,0x190}, {0x190,0x320,0x190}, {0x258,0x320,0x190}, {0x320,0x320,0x190}, {0x3e8,0x320,0x190},
	{0x000,0x3e8,0x190}, {0x0c8,0x3e8,0x190}, {0x190,0x3e8,0x190}, {0x258,0x3e8,0x190}, {0x320,0x3e8,0x190}, {0x3e8,0x3e8,0x190},
	{0x000,0x000,0x258}, {0x0c8,0x000,0x258}, {0x190,0x000,0x258}, {0x258,0x000,0x258}, {0x320,0x000,0x258}, {0x3e8,0x000,0x258},
	{0x000,0x0c8,0x258}, {0x0c8,0x0c8,0x258}, {0x190,0x0c8,0x258}, {0x258,0x0c8,0x258}, {0x320,0x0c8,0x258}, {0x3e8,0x0c8,0x258},
	{0x000,0x190,0x258}, {0x0c8,0x190,0x258}, {0x190,0x190,0x258}, {0x258,0x190,0x258}, {0x320,0x190,0x258}, {0x3e8,0x190,0x258},
	{0x000,0x258,0x258}, {0x0c8,0x258,0x258}, {0x190,0x258,0x258}, {0x258,0x258,0x258}, {0x320,0x258,0x258}, {0x3e8,0x258,0x258},
	{0x000,0x320,0x258}, {0x0c8,0x320,0x258}, {0x190,0x320,0x258}, {0x258,0x320,0x258}, {0x320,0x320,0x258}, {0x3e8,0x320,0x258},
	{0x000,0x3e8,0x258}, {0x0c8,0x3e8,0x258}, {0x190,0x3e8,0x258}, {0x258,0x3e8,0x258}, {0x320,0x3e8,0x258}, {0x3e8,0x3e8,0x258},
	{0x000,0x000,0x320}, {0x0c8,0x000,0x320}, {0x190,0x000,0x320}, {0x258,0x000,0x320}, {0x320,0x000,0x320}, {0x3e8,0x000,0x320},
	{0x000,0x0c8,0x320}, {0x0c8,0x0c8,0x320}, {0x190,0x0c8,0x320}, {0x258,0x0c8,0x320}, {0x320,0x0c8,0x320}, {0x3e8,0x0c8,0x320},
	{0x000,0x190,0x320}, {0x0c8,0x190,0x320}, {0x190,0x190,0x320}, {0x258,0x190,0x320}, {0x320,0x190,0x320}, {0x3e8,0x190,0x320},
	{0x000,0x258,0x320}, {0x0c8,0x258,0x320}, {0x190,0x258,0x320}, {0x258,0x258,0x320}, {0x320,0x258,0x320}, {0x3e8,0x258,0x320},
	{0x000,0x320,0x320}, {0x0c8,0x320,0x320}, {0x190,0x320,0x320}, {0x258,0x320,0x320}, {0x320,0x320,0x320}, {0x3e8,0x320,0x320},
	{0x000,0x3e8,0x320}, {0x0c8,0x3e8,0x320}, {0x190,0x3e8,0x320}, {0x258,0x3e8,0x320}, {0x320,0x3e8,0x320}, {0x3e8,0x3e8,0x320},
	{0x000,0x000,0x3e8}, {0x0c8,0x000,0x3e8}, {0x190,0x000,0x3e8}, {0x258,0x000,0x3e8}, {0x320,0x000,0x3e8}, {0x3e8,0x000,0x3e8},
	{0x000,0x0c8,0x3e8}, {0x0c8,0x0c8,0x3e8}, {0x190,0x0c8,0x3e8}, {0x258,0x0c8,0x3e8}, {0x320,0x0c8,0x3e8}, {0x3e8,0x0c8,0x3e8},
	{0x000,0x190,0x3e8}, {0x0c8,0x190,0x3e8}, {0x190,0x190,0x3e8}, {0x258,0x190,0x3e8}, {0x320,0x190,0x3e8}, {0x3e8,0x190,0x3e8},
	{0x000,0x258,0x3e8}, {0x0c8,0x258,0x3e8}, {0x190,0x258,0x3e8}, {0x258,0x258,0x3e8}, {0x320,0x258,0x3e8}, {0x3e8,0x258,0x3e8},
	{0x000,0x320,0x3e8}, {0x0c8,0x320,0x3e8}, {0x190,0x320,0x3e8}, {0x258,0x320,0x3e8}, {0x320,0x320,0x3e8}, {0x3e8,0x320,0x3e8},
	{0x000,0x3e8,0x3e8}, {0x0c8,0x3e8,0x3e8}, {0x190,0x3e8,0x3e8}, {0x258,0x3e8,0x3e8}, {0x320,0x3e8,0x3e8}, {0x3e8,0x3e8,0x3e8}
};
#endif

static short prev_vdi_clip[4];
struct s_vdi_sysinfo vdi_sysinfo;

struct s_driver_table_entry screen_driver_table[] =
{
	{(char*)"vdi", ctor_plotter_vdi, 0, 32},
	{(char*)NULL, NULL, 0, 0 }
};

const struct s_font_driver_table_entry font_driver_table[] =
{
#ifdef WITH_VDI_FONT_DRIVER
	{(char*)"vdi", ctor_font_plotter_vdi, 0},
#endif
#ifdef WITH_FREETYPE_FONT_DRIVER
	{(char*)"freetype", ctor_font_plotter_freetype, 0},
#endif
#ifdef WITH_INTERNAL_FONT_DRIVER
	{(char*)"internal", ctor_font_plotter_internal, 0},
#endif
	{(char*)NULL, NULL, 0}
};

struct s_vdi_sysinfo * read_vdi_sysinfo( short vdih, struct s_vdi_sysinfo * info ) {

	unsigned long cookie_EdDI=0;
	short out[300];
	memset( info, 0, sizeof(struct s_vdi_sysinfo) );

	info->vdi_handle = vdih;
	if ( tos_getcookie(C_EdDI, &cookie_EdDI) == C_NOTFOUND ) {
		info->EdDiVersion = 0;
	} else {
		info->EdDiVersion = EdDI_version( (void *)cookie_EdDI );
	}

	memset( &out, 0, sizeof(short)*300 );
	vq_extnd( vdih, 0, (short*)&out );
	info->scr_w = out[0]+1;
	info->scr_h = out[1]+1;
	if( out[39] == 2 ) {
		info->scr_bpp = 1;
		info->colors = out[39];
	} else {
		info->colors = out[39];
	}

	memset( &out, 0, sizeof(short)*300 );
	vq_extnd( vdih, 1, (short*)&out );
	info->scr_bpp = out[4];
	info->maxpolycoords = out[14];
	info->maxintin = out[15];
	if( out[30] & 1 ) {
		info->rasterscale = true;
	} else {
		info->rasterscale = false;
	}

	switch( info->scr_bpp ) {
		case 8:
			info->pixelsize=1;
			break;
		case 15:
		case 16:
			info->pixelsize=2;
			break;
		case 24:
			info->pixelsize=3;
			break;
		case 32:
			info->pixelsize=4;
			break;
		case 64:
			info->pixelsize=8;
			break;
		default:
			info->pixelsize=1;
			break;

	}
	info->pitch = info->scr_w * info->pixelsize;
	info->vdiformat = ( (info->scr_bpp <= 8) ? VDI_FORMAT_INTER : VDI_FORMAT_PACK);
	info->screensize = ( info->scr_w * info->pixelsize )  * info->scr_h;

	if( info->EdDiVersion >= EDDI_10 ) {
		memset( &out, 0, sizeof(short)*300 );
		vq_scrninfo(vdih, (short*)&out);
		info->vdiformat = out[0];
		info->clut = out[1];
		info->scr_bpp = out[2];
		info->hicolors =  *((unsigned long*) &out[3]);
		if( info->EdDiVersion >= EDDI_11 ) {
			info->pitch = out[5];
			info->screen = (void *) *((unsigned long *) &out[6]);
		}

		switch( info->clut ) {

			case VDI_CLUT_HARDWARE:
			{

			}
			break;

			case VDI_CLUT_SOFTWARE:
			{
				int component; /* red, green, blue, alpha, overlay */
				int num_bit;
				unsigned short *tmp_p;

				/* We can build masks with info here */
				tmp_p = (unsigned short *) &out[16];
				for (component=0;component<5;component++) {
					for (num_bit=0;num_bit<16;num_bit++) {
						unsigned short val;

						val = *tmp_p++;

						if (val == 0xffff) {
							continue;
						}

						switch(component) {
							case 0:
								info->mask_r |= 1<< val;
								break;
							case 1:
								info->mask_g |= 1<< val;
								break;
							case 2:
								info->mask_b |= 1<< val;
								break;
							case 3:
								info->mask_a |= 1<< val;
								break;
						}
					}
				}
			}

			/* Remove lower green bits for Intel endian screen */
			if ((info->mask_g == ((7<<13)|3)) || (info->mask_g == ((7<<13)|7))) {
				info->mask_g &= ~(7<<13);
			}
			break;

			case VDI_CLUT_NONE:
			break;
		}
	}
}


/*
	lookup an plotter ID by name
*/
static int drvrname_idx( char * name )
{
	int i;
	for( i = 0; ; i++) {
		if( screen_driver_table[i].name == NULL ) {
			return( -1 );
		}
		else {
			if( strcmp(name, screen_driver_table[i].name) == 0 ) {
				return( i );
			}
		}
	}
}

/*
	lookup of font plotter ID by name
*/
static int font_drvrname_idx( char * name )
{
	int i;
	for( i = 0; ; i++) {
		if( font_driver_table[i].name == NULL ) {
			return( -1 );
		}
		else {
			if( strcmp(name, font_driver_table[i].name) == 0 ) {
				return( i );
			}
		}
	}
}

/*
	Get an plotter info entry, the entry contains an pointer to ctor
*/
struct s_driver_table_entry * get_screen_driver_entry( char * name )
{
	int idx = drvrname_idx( name );
	if( idx < 0 )
		return( 0 );
	else
	return( &screen_driver_table[idx] );
}

/*
	Get an font plotter info entry, the entry contains an pointer to ctor.
*/
struct s_font_driver_table_entry * get_font_driver_entry( char * name )
{
	int idx = font_drvrname_idx( name );
	if( idx < 0 )
		return( 0 );
	else
	return( (struct s_font_driver_table_entry *)&font_driver_table[idx] );
}


/*
	Create an new text plotter object
*/
FONT_PLOTTER new_font_plotter( int vdihandle, char * name, unsigned long flags, int * error)
{
	int i=0;
	int res = 0-ERR_PLOTTER_NOT_AVAILABLE;
	FONT_PLOTTER fplotter = (FONT_PLOTTER)malloc( sizeof(struct s_font_plotter) );
	if( fplotter == NULL ) {
		*error = 0-ERR_NO_MEM;
		return( NULL );
	}
	memset( fplotter, 0, sizeof(FONT_PLOTTER));
	fplotter->vdi_handle = vdihandle;
	fplotter->name = name;
	fplotter->flags = 0;
	fplotter->flags |= flags;
	for( i = 0; ; i++) {
		if( font_driver_table[i].name == NULL ) {
			res = 0-ERR_PLOTTER_NOT_AVAILABLE;
			break;
		} else {
			if( strcmp(name, font_driver_table[i].name) == 0 ) {
				if( font_driver_table[i].ctor  ) {
					res = font_driver_table[i].ctor( fplotter );
					*error = 0;
				} else {
					res = 0-ERR_PLOTTER_NOT_AVAILABLE;
					*error = res;
					return (NULL);
				}
				break;
			}
		}
	}
	if( res < 0 ) {
		free( fplotter );
		*error = res;
		return( NULL );
   	}
	fplotter->plotter = NULL;
	return( fplotter );
}

static bool init=false;
static int inst=0;

/*
	Create an new plotter object
*/
GEM_PLOTTER new_plotter(int vdihandle, char * name, GRECT * loc_size,
						int virt_bpp, unsigned long flags, FONT_PLOTTER fplotter, int * error )
{
	int res = 0-ERR_PLOTTER_NOT_AVAILABLE;
	int i;
	assert( fplotter != NULL );

	GEM_PLOTTER gemplotter = (GEM_PLOTTER)malloc( sizeof(struct s_gem_plotter) );
	if( !gemplotter ) {
		*error = 0-ERR_NO_MEM;
		return( NULL );
   	}
	memset( gemplotter, 0, sizeof(struct s_gem_plotter));

	gemplotter->name = name;
	gemplotter->vdi_handle = vdihandle;
	gemplotter->flags |= flags;
	gemplotter->font_plotter = fplotter;
	gemplotter->bpp_virt = virt_bpp;
	gemplotter->cfbi = 0;
	memset(&gemplotter->fbuf, 0, sizeof(struct s_frame_buf) * MAX_FRAMEBUFS );
	gemplotter->fbuf[0].x = loc_size->g_x;
	gemplotter->fbuf[0].y = loc_size->g_y;
	gemplotter->fbuf[0].w = loc_size->g_w;
	gemplotter->fbuf[0].h = loc_size->g_h;

	/* request vdi info once, so every plotter is able to access the info */
	if( !init ) {
		/* vdi_sysinfo */
		read_vdi_sysinfo( vdihandle, &vdi_sysinfo );
		init = true;
	}
	for( i = 0; ; i++) {
		if( screen_driver_table[i].name == NULL ) {
			res = 0-ERR_PLOTTER_NOT_AVAILABLE;
			break;
		}
		else {
			if( strcmp(name, screen_driver_table[i].name) == 0 ) {
				if( screen_driver_table[i].ctor  ) {
					gemplotter->flags |= screen_driver_table[i].flags;
					res = screen_driver_table[i].ctor( gemplotter );
					*error = 0;
				} else {
					res = 0-ERR_PLOTTER_NOT_AVAILABLE;
					*error = res;
					return (NULL);
				}
				break;
			}
		}
	}
	if( res < 0 ) {
		free( gemplotter );
		*error = res;
		return( NULL );
   	}
	inst++;
	gemplotter->font_plotter->plotter = gemplotter;
	return( gemplotter );
}

/*
	Free an plotter
*/
int delete_plotter( GEM_PLOTTER p )
{
	if( p ) {
		p->dtor( p );
		free( p );
		p = NULL;
		inst--;
		if( inst == 0 ){

		}
	}
	else
		return( -1 );
	return( 0 );
}

/*
	Free an font plotter
*/
int delete_font_plotter( FONT_PLOTTER p )
{
	if( p ) {
		p->dtor(p);
		free( p );
		p = NULL;
	}
	else
		return( -1 );
	return( 0 );
}

/*
	x - x coord
	y - y coord
	stride - stride in bytes
	bpp - bits per pixel
*/
int calc_chunked_buffer_size(int x, int y, int stride, int bpp)
{
   return( (x * (bpp >> 3)) * y );
}

/*
	x - x coord
	y - y coord
	stride - stride in bytes
	bpp - bits per pixel
*/
int get_pixel_offset( int x, int y, int stride, int bpp )
{
   LOG(("byte_pp: %d, pure: %d, result: %d\n",(bpp >> 3),(y * stride + x), (y * stride + x) * (bpp >> 3)));
   return( ( (y * stride) + x) * (bpp >> 3) );
}

/*
	1. calculate visible area of framebuffer in coords relative to framebuffer position

	result:
	this function should calc offsets into x,y coords of the framebuffer which
	can be drawn. If the framebuffer coords do not fall within the screen region,
	all values of visible region are set to zero.
*/
void update_visible_rect( GEM_PLOTTER p )
{
	GRECT screen;
	GRECT common;
	GRECT frame;

	screen.g_x = 0;
	screen.g_y = 0;
	screen.g_w = vdi_sysinfo.scr_w;
	screen.g_h = vdi_sysinfo.scr_h;

    common.g_x = frame.g_x = CURFB(p).x;
	common.g_y = frame.g_y = CURFB(p).y;
	common.g_w = frame.g_w = CURFB(p).w;
	common.g_h = frame.g_h = CURFB(p).h;

	if( rc_intersect( &screen, &common ) ) {
		CURFB(p).vis_w = common.g_w;
		CURFB(p).vis_h = common.g_h;
		if( CURFB(p).x < screen.g_x )
			CURFB(p).vis_x = frame.g_w - common.g_w;
		else
			CURFB(p).vis_x = 0;
		if( CURFB(p).y <screen.g_y )
			CURFB(p).vis_y = frame.g_h - common.g_h;
		else
			CURFB(p).vis_y = 0;
	} else {
		CURFB(p).vis_w = CURFB(p).vis_h = 0;
		CURFB(p).vis_x = CURFB(p).vis_y = 0;
	}
}

/*
	Returns the visible parts of the box (relative coords within framebuffer),
   	relative to screen coords (normally starting at 0,0 )
*/
bool fbrect_to_screen( GEM_PLOTTER self, GRECT box, GRECT * ret )
{
	GRECT out, vis, screen;

	screen.g_x = 0;
	screen.g_y = 0;
	screen.g_w = vdi_sysinfo.scr_w;
	screen.g_h = vdi_sysinfo.scr_h;

	/* get visible region: */
    vis.g_x = CURFB(self).x;
	vis.g_y = CURFB(self).y;
	vis.g_w = CURFB(self).w;
	vis.g_h = CURFB(self).h;

	if ( !rc_intersect( &screen, &vis ) ) {
		return( false );
	}
	vis.g_x = CURFB(self).w - vis.g_w;
	vis.g_y = CURFB(self).h - vis.g_h;

	/* clip box to visible region: */
	if( !rc_intersect(&vis, &box) ) {
		return( false );
	}
	out.g_x = box.g_x + CURFB(self).x;
	out.g_y = box.g_y + CURFB(self).y;
	out.g_w = box.g_w;
	out.g_h = box.g_h;
	*ret = out;
	return ( true );
}

const char* plotter_err_str(int i) { return(plot_error_codes[abs(i)]); }

void dump_vdi_info( short vdih )
{
	struct s_vdi_sysinfo temp;
	read_vdi_sysinfo( vdih, &temp );
	printf("struct s_vdi_sysinfo {\n");
	printf("	short vdi_handle: %d\n", temp.vdi_handle);
	printf("	short scr_w: %d \n", temp.scr_w);
	printf("	short scr_h: %d\n", temp.scr_h);
	printf("	short scr_bpp: %d\n", temp.scr_bpp);
	printf("	int colors: %d\n", temp.colors);
	printf("	ulong hicolors: %d\n", temp.hicolors);
	printf("	short pixelsize: %d\n", temp.pixelsize);
	printf("	unsigned short pitch: %d\n", temp.pitch);
	printf("	unsigned short vdiformat: %d\n", temp.vdiformat);
	printf("	unsigned short clut: %d\n", temp.clut);
	printf("	void * screen: 0x0%p\n", temp.screen);
	printf("	unsigned long  screensize: %d\n", temp.screensize);
	printf("	unsigned long  mask_r: 0x0%08x\n", temp.mask_r);
	printf("	unsigned long  mask_g: 0x0%08x\n", temp.mask_g);
	printf("	unsigned long  mask_b: 0x0%08x\n", temp.mask_b);
	printf("	unsigned long  mask_a: 0x0%08x\n", temp.mask_a);
	printf("	short maxintin: %d\n", temp.maxintin);
	printf("	short maxpolycoords: %d\n", temp.maxpolycoords);
	printf("	unsigned long EdDiVersion: 0x0%03x\n", temp.EdDiVersion);
	printf("	unsigned short rasterscale: 0x%2x\n", temp.rasterscale);
	printf("};\n");
}

void dump_plot_drivers(void)
{
	int i = 0;
	while( screen_driver_table[i].name != NULL ) {
		printf("%s -> max_bpp: %d, flags: %d\n",
			screen_driver_table[i].name,
			screen_driver_table[i].max_bpp,
			screen_driver_table[i].flags
		);
		i++;
	}
}

void dump_font_drivers(void)
{
	int i = 0;
	while( font_driver_table[i].name != NULL ) {
		printf("%s -> flags: %d\n",
			font_driver_table[i].name,
			font_driver_table[i].flags
		);
		i++;
	}
}

/*
	Convert an RGB color to an VDI Color
*/
void rgb_to_vdi1000( unsigned char * in, unsigned short * out )
{
	double r = ((double)in[3]/255); /* prozentsatz red */
	double g = ((double)in[2]/255);	/* prozentsatz green */
	double b = ((double)in[1]/255);	/* prozentsatz blue */
	out[0] = 1000 * r + 0.5;
	out[1] = 1000 * g + 0.5;
	out[2] = 1000 * b + 0.5;
	return;
}

void vdi1000_to_rgb( unsigned short * in, unsigned char * out )
{
	double r = ((double)in[0]/1000); /* prozentsatz red */
	double g = ((double)in[1]/1000);	/* prozentsatz green */
	double b = ((double)in[2]/1000);	/* prozentsatz blue */
	out[2] = 255 * r + 0.5;
	out[1] = 255 * g + 0.5;
	out[0] = 255 * b + 0.5;
	return;
}


static short web_std_colors[6] = {0, 51, 102, 153, 204, 255};

/*
	Convert an RGB color into an index into the 216 colors web pallette
*/
short rgb_to_666_index(unsigned char r, unsigned char g, unsigned char b)
{
	short ret = 0;
	short i;
	unsigned char rgb[3] = {r,g,b};
	unsigned char tval[3];

	int diff_a, diff_b, diff_c;
	diff_a = abs(r-g);
	diff_b = abs(r-b);
	diff_c = abs(r-b);
	if( diff_a < 2 && diff_b < 2 && diff_c < 2 ){
		if( (r!=0XFF) && (g!=0XFF) && (g!=0XFF)  ){
			if( ((r&0xF0)>>4) != 0 )
				//printf("conv gray: %x -> %d\n", ((r&0xF0)>>4) , (OFFSET_CUST_PAL) + ((r&0xF0)>>4) );
			return( (OFFSET_CUST_PAL - OFFSET_WEB_PAL) + ((r&0xF0)>>4) );
		}
	}

	/* convert each 8bit color to 6bit web color: */
	for( i=0; i<3; i++) {
		if(0 == rgb[i] % web_std_colors[1] ) {
			tval[i] = rgb[i] / web_std_colors[1];
		}
		else {
			int pos = ((short)rgb[i] / web_std_colors[1]);
			if( abs(rgb[i] - web_std_colors[pos]) > abs(rgb[i] - web_std_colors[pos+1]) )
				tval[i] = pos+1;
			else
				tval[i] = pos;
		}
	}
	return( tval[2]*36+tval[1]*6+tval[0] );
}

/*
	bpp: bits per pixel,

*/
int init_mfdb(int bpp, int w, int h, uint32_t flags, MFDB * out )
{
	int dststride;
	dststride = MFDB_STRIDE( w );
	int size = MFDB_SIZE( bpp, dststride, h );
	if( bpp > 0 ) {
		if( (flags & MFDB_FLAG_NOALLOC) == 0  ) {
			out->fd_addr = malloc( size );
			if( out->fd_addr == NULL ){
				return( 0 );
			}
			if( (flags & MFDB_FLAG_ZEROMEM) ){
				memset( out->fd_addr, 0, size );
			}
		}
		out->fd_stand = (flags & MFDB_FLAG_STAND) ? 1 : 0;
		out->fd_nplanes = (short)bpp;
		out->fd_r1 = out->fd_r2 = out->fd_r3 = 0;
	} else {
		memset( out, 0, sizeof(MFDB) );
	}
	out->fd_w = dststride;
	out->fd_h = h;
	out->fd_wdwidth = dststride >> 4;
	return( size );
}


int plotter_get_clip( GEM_PLOTTER self, struct rect * out )
{
	out->x0 = self->clipping.x0;
	out->y0 = self->clipping.y0;
	out->x1 = self->clipping.x1;
	out->y1 = self->clipping.y1;
	return( 1 );
}

void plotter_get_clip_grect( GEM_PLOTTER self, GRECT * out )
{
	out->g_x = self->clipping.x0;
	out->g_y = self->clipping.y0;
	out->g_w = self->clipping.x1 - self->clipping.x0;
	out->g_h = self->clipping.y1 - self->clipping.y0;
}

void plotter_get_visible_grect( GEM_PLOTTER self, GRECT * out )
{
	/*todo: !!! */
	out->g_x = self->clipping.x0;
	out->g_y = self->clipping.y0;
	out->g_w = self->clipping.x1 - self->clipping.x0;
	out->g_h = self->clipping.y1 - self->clipping.y0;
}

int plotter_std_clip(GEM_PLOTTER self, const struct rect * clip)
{
	self->clipping.x0 = clip->x0;
	self->clipping.y0 =	clip->y0;
	self->clipping.x1 = clip->x1;
	self->clipping.y1 = clip->y1;
	return ( 1 );
}


void plotter_vdi_clip( GEM_PLOTTER self, bool set)
{
	if( set == true ) {
		struct rect * c = &self->clipping;
		short vdiflags[58];
		short newclip[4];
		vq_extnd( self->vdi_handle, 1, (short*)&vdiflags);
		prev_vdi_clip[0] = vdiflags[45];
		prev_vdi_clip[1] = vdiflags[46];
		prev_vdi_clip[2] = vdiflags[47];
		prev_vdi_clip[3] = vdiflags[48];
		newclip[0] = CURFB(self).x + MAX(c->x0, 0);
		newclip[1] = CURFB(self).y + MAX(c->y0, 0);
		newclip[2] = MIN(CURFB(self).x+CURFB(self).w, newclip[0] + (c->x1 - c->x0) )-1;
		newclip[3] = MIN(CURFB(self).y+CURFB(self).h, newclip[1] + (c->y1 - c->y0) )-1;
		vs_clip( self->vdi_handle, 1, (short*)&newclip );
	} else {
		vs_clip( self->vdi_handle, 1, (short *)&prev_vdi_clip );
	}
}


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
#ifdef WITH_GD_PLOTTER
 #include "atari/plot/plotter_gd.h"
#endif

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


struct s_driver_table_entry screen_driver_table[] =
{
	{
		(char*)"vdi",
		ctor_plotter_vdi,
		PLOT_FLAG_HAS_ALPHA,
		32
	},
#ifdef WITH_GD_PLOTTER
	{
		(char*)"gd",
		ctor_plotter_gd,
		PLOT_FLAG_OFFSCREEN | PLOT_FLAG_HAS_ALPHA,
		32
	},
#endif
	{(char*)NULL, NULL, 0, 0 }
};

const struct s_font_driver_table_entry font_driver_table[] =
{
#ifdef WITH_VDI_FONT_DRIVER
	{"vdi", ctor_font_plotter_vdi, 0},
#endif
#ifdef WITH_FREETYPE_FONT_DRIVER
	{"freetype", ctor_font_plotter_freetype, 0},
#endif
#ifdef WITH_INTERNAL_FONT_DRIVER
	{"internal", ctor_font_plotter_internal, 0},
#endif
	{(char*)NULL, NULL, 0}
};


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

struct s_vdi_sysinfo vdi_sysinfo;

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
	gemplotter->flags = 0;
	gemplotter->font_plotter = fplotter;
	gemplotter->bpp_virt = virt_bpp;

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
					gemplotter->flags = (screen_driver_table[i].flags | flags);
					res = screen_driver_table[i].ctor( gemplotter, loc_size );
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

void plotter_get_clip_grect( GEM_PLOTTER self, GRECT * out )
{
	struct rect clip;
	self->get_clip( self, &clip );
	out->g_x = clip.x0;
	out->g_y = clip.y0;
	out->g_w = clip.x1 - clip.x0;
	out->g_h = clip.y1 - clip.y0;
}

/*
	Convert an RGB color to an VDI Color
*/
void rgb_to_vdi1000( unsigned char * in, unsigned short * out )
{
	double r = ((double)in[3]/255); /* prozentsatz red   */
	double g = ((double)in[2]/255);	/* prozentsatz green */
	double b = ((double)in[1]/255);	/* prozentsatz blue  */
	out[0] = 1000 * r + 0.5;
	out[1] = 1000 * g + 0.5;
	out[2] = 1000 * b + 0.5;
	return;
}

void vdi1000_to_rgb( unsigned short * in, unsigned char * out )
{
	double r = ((double)in[0]/1000); /* prozentsatz red   */
	double g = ((double)in[1]/1000); /* prozentsatz green */
	double b = ((double)in[2]/1000); /* prozentsatz blue  */
	out[2] = 255 * r + 0.5;
	out[1] = 255 * g + 0.5;
	out[0] = 255 * b + 0.5;
	return;
}


#ifdef WITH_8BPP_SUPPORT


short web_std_colors[6] = {0, 51, 102, 153, 204, 255};

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
#endif



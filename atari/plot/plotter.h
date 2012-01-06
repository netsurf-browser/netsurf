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
#ifndef _GEM_PLOTTER_API_H_
#define _GEM_PLOTTER_API_H_
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <windom.h>

#include "desktop/plotters.h"
#include "desktop/plot_style.h"
#include "image/bitmap.h"
#include "atari/bitmap.h"
#include "atari/osspec.h"
#include "atari/gui.h"
#include "atari/font.h"
#include "atari/options.h"
#include "atari/findfile.h"
#include "utils/utf8.h"
#include "utils/log.h"

#ifndef ceilf
#warning "ceilf emulation"
#define ceilf(x) (float)ceil((double)x)
#endif

#ifdef TEST_PLOTTER
#define verbose_log 1
#define LOG(x) do { if (verbose_log) (printf(__FILE__ " %s %i: ", __PRETTY_FUNCTION__, __LINE__), printf x, fputc('\n', stdout)); } while (0)
#endif

#define MAX_FRAMEBUFS 0x010
#define C2P (1<<0)	                    /* C2P convert buffer 1 to buffer 2 */
/* TODO: implement offscreen buffer switch */

/* Plotter Flags: */
#define PLOT_FLAG_OFFSCREEN 0x01	/* offsreen plotter should set/accept this flag */
#define PLOT_FLAG_LOCKED 	0x02		/* plotter should set this flag during screen updates */
#define PLOT_FLAG_DITHER 	0x04		/* true if the plotter shall dither images */
#define PLOT_FLAG_TRANS		0x08		/* true if the plotter supports transparent operations */

/* Font Plotter flags: */
#define FONTPLOT_FLAG_MONOGLYPH 0x01

/* Flags for init_mfdb function: */
#define MFDB_FLAG_STAND			0x01
#define MFDB_FLAG_ZEROMEM		0x02
#define MFDB_FLAG_NOALLOC		0x04

/* Error codes: */
#define ERR_BUFFERSIZE_EXCEEDS_SCREEN 1	/* The buffer allocated is larger than the screen */
#define ERR_NO_MEM 2					/* Not enough memory for requested operation */
#define ERR_PLOTTER_NOT_AVAILABLE 3		/* invalid plotter driver name passed */

/* Grapics & Font Plotter "Objects": */
typedef struct s_font_plotter * FONT_PLOTTER;
typedef struct s_gem_plotter * GEM_PLOTTER;
typedef struct s_font_plotter * GEM_FONT_PLOTTER; /* for public use ... */


/* declaration of font plotter member functions: (_fpmf_ prefix) */
typedef int (*_fpmf_str_width)( FONT_PLOTTER self, const plot_font_style_t *fstyle,
						const char * str, size_t length, int * width);
typedef int (*_fpmf_str_split)( FONT_PLOTTER self, const plot_font_style_t *fstyle,
						const char *string, size_t length,
						int x, size_t *char_offset, int *actual_x);
typedef int (*_fpmf_pixel_pos)( FONT_PLOTTER self, const plot_font_style_t *fstyle,
						const char *string, size_t length,
						int x, size_t *char_offset, int *actual_x);
typedef int (*_fpmf_text)( FONT_PLOTTER self, int x, int y, const char *text,
													size_t length, const plot_font_style_t *fstyle);

typedef void (*_fpmf_draw_glyph)(FONT_PLOTTER self, GRECT * loc, uint8_t * pixdata,
						int pitch, uint32_t colour);
typedef int (*_fpmf_dtor)( FONT_PLOTTER self );


/* prototype of the font plotter "object" */
struct s_font_plotter
{
	char * name;
	int flags;
	int vdi_handle;
	void * priv_data;
	GEM_PLOTTER plotter;

	_fpmf_str_width str_width;
	_fpmf_str_split str_split;
	_fpmf_pixel_pos pixel_pos;
	_fpmf_text text;
	_fpmf_draw_glyph draw_glyph;
	_fpmf_dtor dtor;
};


struct rect;

struct s_vdi_sysinfo {
	short vdi_handle;					/* vdi handle 						*/
	short scr_w;							/* resolution horz. 			*/
	short scr_h;							/* resolution vert. 			*/
	short scr_bpp;						/* bits per pixel 				*/
	int colors;								/* 0=hiclor, 2=mono				*/
	unsigned long hicolors;		/* if colors = 0					*/
	short pixelsize;					/* bytes per pixel 				*/
	unsigned short pitch;			/* row pitch 							*/
	unsigned short vdiformat;	/* pixel format 					*/
	unsigned short clut;			/* type of clut support 	*/
	void * screen;						/* pointer to screen, or NULL	*/
	unsigned long  screensize;/* size of screen (in bytes)	*/
	unsigned long  mask_r;		/* color masks 						*/
	unsigned long  mask_g;
	unsigned long  mask_b;
	unsigned long  mask_a;
	short maxintin;						/* maximum pxy items 			*/
	short maxpolycoords;			/* max coords for p_line etc.	*/
	unsigned long EdDiVersion;/* EdDi Version or 0 			*/
	bool rasterscale;					/* raster scaling support	*/
};


struct s_frame_buf
{
	short x;
	short y;
	short w;
	short h;
	short vis_x;	/* visible rectangle of the screen buffer */
	short vis_y;	/* coords are relative to framebuffer location */
	short vis_w;
	short vis_h;
	int size;
	bool swapped;
	void * mem;
};

/* declaration of plotter member functions ( _pmf_ prefix )*/
typedef int (*_pmf_resize)(GEM_PLOTTER self, int w, int h);
typedef	int (*_pmf_move)(GEM_PLOTTER self, short x, short y );
typedef	void * (*_pmf_create_framebuffer)(GEM_PLOTTER self);
typedef	void * (*_pmf_switch_to_framebuffer)(GEM_PLOTTER self);
typedef	int (*_pmf_lock)(GEM_PLOTTER self);
typedef	int (*_pmf_unlock)(GEM_PLOTTER self);
typedef	int (*_pmf_update_region)(GEM_PLOTTER self, GRECT region);
typedef	int (*_pmf_update_screen_region)( GEM_PLOTTER self, GRECT region );
typedef	int (*_pmf_update_screen)(GEM_PLOTTER self);
typedef	int (*_pmf_put_pixel)(GEM_PLOTTER self, int x, int y, int color );
typedef	int (*_pmf_copy_rect)(GEM_PLOTTER self, GRECT src, GRECT dst );
typedef	int (*_pmf_clip)(GEM_PLOTTER self, const struct rect * clip );
typedef	int (*_pmf_arc)(GEM_PLOTTER self, int x, int y, int radius, int angle1, int angle2, const plot_style_t * pstyle);
typedef	int (*_pmf_disc)(GEM_PLOTTER self, int x, int y, int radius, const plot_style_t * pstyle);
typedef	int (*_pmf_line)(GEM_PLOTTER self, int x0, int y0, int x1,	int y1, const plot_style_t * pstyle);
typedef	int (*_pmf_rectangle)(GEM_PLOTTER self, int x0, int y0, int x1, int y1, const plot_style_t * pstyle);
typedef	int (*_pmf_polygon)(GEM_PLOTTER self, const int *p, unsigned int n,  const plot_style_t * pstyle);
typedef	int (*_pmf_path)(GEM_PLOTTER self, const float *p, unsigned int n, int fill, float width, int c, const float transform[6]);
typedef	int (*_pmf_bitmap_resize) ( GEM_PLOTTER self, struct bitmap * bm, int nw, int nh );
typedef int (*_pmf_bitmap_convert)( GEM_PLOTTER self, struct bitmap * img, int x, int y,
				GRECT * clip, uint32_t bg, uint32_t flags, MFDB *out  );
typedef	int (*_pmf_bitmap)(GEM_PLOTTER self, struct bitmap * bmp, int x, int y,
				unsigned long bg, unsigned long flags );
typedef int (*_pmf_plot_mfdb)(GEM_PLOTTER self, GRECT * loc, MFDB * mfdb, unsigned char fgcolor, uint32_t flags);
typedef	int (*_pmf_text)(GEM_PLOTTER self, int x, int y, const char *text, size_t length, const plot_font_style_t *fstyle);
typedef	int (*_pmf_dtor)(GEM_PLOTTER self);


/* this is the prototype of an plotter "object" */
struct s_gem_plotter
{
	char * name;         /* name that identifies the Plotter */
	unsigned long flags;
	int vdi_handle;
	struct s_vdi_sysinfo * scr;
	void * priv_data;
	/* bit depth of framebuffers: */
	int bpp_virt;
	struct rect clipping;
	struct s_frame_buf fbuf[MAX_FRAMEBUFS];
	/* current framebuffer index: */
	int cfbi;

	FONT_PLOTTER font_plotter;
	/* set new dimensions (realloc memory): */
	_pmf_resize resize;
	/* set drawing origin: */
	_pmf_move move;
	_pmf_lock lock;
	_pmf_unlock unlock;
	_pmf_create_framebuffer create_framebuffer;
	_pmf_switch_to_framebuffer switch_to_framebuffer;
	_pmf_update_region update_region;
	_pmf_update_screen update_screen;
	_pmf_update_screen_region update_screen_region;
	_pmf_put_pixel put_pixel;
	_pmf_copy_rect copy_rect;
	_pmf_clip clip;
	_pmf_arc arc;
	_pmf_disc disc;
	_pmf_line line;
	_pmf_rectangle rectangle;
	_pmf_polygon polygon;
	_pmf_path path;
	/* scale an netsurf bitmap: */
	_pmf_bitmap_resize bitmap_resize;
	/* convert an ABGR (netsurf) bitmap to screen format, ready for vro_cpyfm */
	_pmf_bitmap_convert bitmap_convert;
	/* plot an netsurf bitmap into the buffer / screen: */
	_pmf_bitmap bitmap;
	/* plot an mfdb into the buffer / screen: */
	_pmf_plot_mfdb plot_mfdb;
	_pmf_text text;
	_pmf_dtor dtor;
};


/* these 2 structs hold info about an specific driver. */
/* a table in plotter.c defines all the available plotters */
struct s_driver_table_entry
{
	char * name;											/* name (unique) */
	int (*ctor)( GEM_PLOTTER self );	/* pointer to ctor of the plotter */
	int flags;												/* a bitmask containing info about supported operations */
	int max_bpp;											/* the maximum supported screen depth of the plotter */
};

struct s_font_driver_table_entry
{
	char * name;
	int (*ctor)( FONT_PLOTTER self );
	int flags;
};

typedef struct s_driver_table_entry * PLOTTER_INFO;
typedef struct s_font_driver_table_entry * FONT_PLOTTER_INFO;

/* get s_driver_table_entry from driver table */
struct s_driver_table_entry * get_screen_driver_entry(char * name);

/* get s_font_driver_table_entry from driver table */
struct s_font_driver_table_entry * get_font_driver_entry(char * name);

/* fill screen / sys info */
struct s_vdi_sysinfo * read_vdi_sysinfo(short vdih, struct s_vdi_sysinfo * info );

/*
   Create an new plotter object
   Error Values:
      -1 no mem
      -2 error configuring plotter
      -3 Plotter not available
*/
GEM_PLOTTER new_plotter(int vdihandle, char * name,
	GRECT *, int virt_bpp, unsigned long flags, FONT_PLOTTER font_renderer,
	int * error);

/*
   Create an new font plotter object
   Error Values:
      -1 no mem
      -2 error configuring font plotter
      -3 Font Plotter not available
*/
FONT_PLOTTER new_font_plotter(int vdihandle, char * name, unsigned long flags, int * error );

/* free the plotter resources */
int delete_plotter( GEM_PLOTTER p );
int delete_font_plotter( FONT_PLOTTER p );


/* calculate size of intermediate buffer */
int calc_chunked_buffer_size(int x, int y, int stride, int bpp);

/* calculates the pixel offset from x,y pos */
int get_pixel_offset( int x, int y, int stride, int bpp );

/* Recalculate visible parts of the framebuffer */
void update_visible_rect( GEM_PLOTTER p );

/* resolve possible visible parts of the framebuffer in screen coords */
bool fbrect_to_screen( GEM_PLOTTER self, GRECT box, GRECT * ret );

/* translate an error number */
const char* plotter_err_str(int i) ;

void dump_font_drivers(void);
void dump_plot_drivers(void);
void dump_vdi_info(short);

/* convert an vdi color to bgra */
void vdi1000_to_rgb( unsigned short * in, unsigned char * out );

/* convert an bgra color to vdi1000 color */
void rgb_to_vdi1000( unsigned char * in, unsigned short * out );

/* convert an rgb color to an index into the web palette */
short rgb_to_666_index(unsigned char r, unsigned char g, unsigned char b);

/*
	setup an MFDB struct and allocate memory for it when it is needed.
	If bpp == 0, this function assumes that the MFDB shall point to the screen
	and will not allocate any memory (mfdb.fd_addr == 0).
	The function will return 0 when the memory allocation fails
	( out of memory), otherwise it returns the size of the mfdb.fd_addr
  as number of bytes.
*/
int init_mfdb(int bpp, int w, int h, uint32_t flags, MFDB * out );

/* shared / static methods follows */

/*
	Get clipping for current framebuffer
*/
int plotter_get_clip( GEM_PLOTTER self, struct rect * out );

/*
	Get clipping for current framebuffer as GRECT
*/
void plotter_get_clip_grect( GEM_PLOTTER self, GRECT * out );

/*
	Get current visible coords
*/
void plotter_get_visible_grect( GEM_PLOTTER self, GRECT * out );

/*
	Set clipping for current framebuffer
*/
int plotter_std_clip(GEM_PLOTTER self, const struct rect * clip);


/*
	convert framebuffer clipping to vdi clipping and activates it
*/
void plotter_vdi_clip( GEM_PLOTTER self, bool set);


#define PLOTTER_IS_LOCKED(plotter) ( plotter->private_flags & PLOTTER_FLAG_LOCKED )

#define CURFB( p ) \
	p->fbuf[p->cfbi]

#define FIRSTFB( p ) \
	p->fbuf[0]

/* some Well known indexes into the VDI palette */
/* common indexes into the VDI palette */
/* (only used when running with 256 colors or less ) */
#define OFFSET_WEB_PAL 16
#define OFFSET_CUST_PAL 232
#define OFFSET_CUSTOM_COLOR 255	/* this one is used by the TC renderer */
#define RGB_TO_VDI(c) rgb_to_666_index( (c&0xFF),(c&0xFF00)>>8,(c&0xFF0000)>>16)+OFFSET_WEB_PAL
/* the name of this macro is crap - it should be named bgr_to_rgba ... or so */
#define ABGR_TO_RGB(c)  ( ((c&0xFF)<<16) | (c&0xFF00) | ((c&0xFF0000)>>16) ) << 8

/* calculate MFDB compatible rowstride (in number of bits) */
#define MFDB_STRIDE( w ) (((w & 15) != 0) ? (w | 15)+1 : w)

/*
Calculate size of an mfdb, params:
	Bits per pixel,
	Word aligned rowstride (width) as returned by MFDB_STRIDE,
	height in pixels
*/
#define MFDB_SIZE( bpp, stride, h ) ( ((stride >> 3) * h) * bpp )

#endif

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
#include <assert.h>
#include <windom.h>
#include <Hermes/Hermes.h>

#include "atari/plot/eddi.h"
#include "atari/plot/plotter.h"
#include "atari/plot/plotter_vdi.h"

/* assign vdi line style to dst ( netsurf type ) */
#define NSLT2VDI(dst, src) \
	dst = 0;\
	switch( src->stroke_type ) {\
		case PLOT_OP_TYPE_DOT: \
			dst = (0xAAAA00 | 7);\
		break;\
		case PLOT_OP_TYPE_DASH:\
			dst = 3;	\
		break;\
		case PLOT_OP_TYPE_SOLID:\
		case PLOT_OP_TYPE_NONE:\
		default:\
			dst = 1;\
		break;\
	}\

static int dtor( GEM_PLOTTER self );
static int resize( GEM_PLOTTER self, int w, int h );
static int move( GEM_PLOTTER self, short x, short y );
static int lock( GEM_PLOTTER self );
static int unlock( GEM_PLOTTER self );
static int update_region( GEM_PLOTTER self, GRECT region );
static int update_screen_region( GEM_PLOTTER self, GRECT region );
static int update_screen( GEM_PLOTTER self );
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
static int bitmap_convert_8( GEM_PLOTTER self, struct bitmap * img,int x, int y,
	GRECT * clip,uint32_t bg,uint32_t flags, MFDB *out  );
static int bitmap( GEM_PLOTTER self, struct bitmap * bmp, int x, int y,
					unsigned long bg, unsigned long flags );
static int plot_mfdb( GEM_PLOTTER self, GRECT * where, MFDB * mfdb, unsigned char fgcolor, uint32_t flags);
static int text(GEM_PLOTTER self, int x, int y, const char *text,size_t length, const plot_font_style_t *fstyle);

static inline void set_stdpx( MFDB * dst, int wdplanesz, int x, int y, unsigned char val );
static inline unsigned char get_stdpx(MFDB * dst, int wdplanesz, int x, int y );


#ifdef WITH_8BPP_SUPPORT
static unsigned short sys_pal[256][3]; /*RGB*/
static unsigned short pal[256][3];     /*RGB*/
static char rgb_lookup[256][4];
extern unsigned short vdi_web_pal[126][3];
#endif
extern struct s_vdi_sysinfo vdi_sysinfo;

static HermesHandle hermes_pal_h; /* hermes palette handle */
static HermesHandle hermes_cnv_h; /* hermes converter instance handle */
static HermesHandle hermes_res_h;


static inline void vsl_rgbcolor( short vdih, uint32_t cin )
{
	if( vdi_sysinfo.scr_bpp > 8 ) {
		unsigned short c[4];
		rgb_to_vdi1000( (unsigned char*)&cin, (unsigned short*)&c );
		vs_color( vdih, OFFSET_CUSTOM_COLOR, (unsigned short*)&c[0] );
		vsl_color( vdih, OFFSET_CUSTOM_COLOR );
	} else {
		if( vdi_sysinfo.scr_bpp >= 4 ){
			vsl_color( vdih, RGB_TO_VDI(cin) );
		}
		else
			vsl_color( vdih, BLACK );
	}
}

static inline void vsf_rgbcolor( short vdih, uint32_t cin )
{
	if( vdi_sysinfo.scr_bpp > 8 ) {
		unsigned short c[4];
		rgb_to_vdi1000( (unsigned char*)&cin, (unsigned short*)&c );
		vs_color( vdih, OFFSET_CUSTOM_COLOR, &c[0] );
		vsf_color( vdih, OFFSET_CUSTOM_COLOR );
	} else {
		if( vdi_sysinfo.scr_bpp >= 4 ){
			vsf_color( vdih, RGB_TO_VDI(cin) );
		}
		else
			vsf_color( vdih, WHITE );
	}
}

int ctor_plotter_vdi(GEM_PLOTTER self )
{
	int retval = 0;
	int i;
	struct rect clip;

	self->dtor = dtor;
	self->resize= resize;
	self->move = move;
	self->lock = lock;
	self->unlock = unlock;
	self->update_region = update_region;
	self->update_screen_region = update_screen_region;
	self->update_screen = update_screen;
	self->put_pixel = put_pixel;
	self->copy_rect = copy_rect;
	self->clip = plotter_std_clip;
	self->arc = arc;
	self->disc = disc;
	self->line = line;
	self->rectangle = rectangle;
	self->polygon = polygon;
	self->path = path;
	self->bitmap = bitmap;
	self->bitmap_resize = bitmap_resize;
#ifdef WITH_8BPP_SUPPORT
	self->bitmap_convert =(app.nplanes > 8) ? bitmap_convert : bitmap_convert_8;
#else
	self->bitmap_convert = bitmap_convert;
#endif
	//self->bitmap_convert =bitmap_convert;
	self->plot_mfdb = plot_mfdb;
	self->text = text;
	LOG(("Screen: x: %d, y: %d\n", vdi_sysinfo.scr_w, vdi_sysinfo.scr_h));

	self->priv_data = malloc( sizeof(struct s_vdi_priv_data) );
	if( self->priv_data == NULL )
		return( 0-ERR_NO_MEM );
	memset( self->priv_data, 0, sizeof(struct s_vdi_priv_data) );
	DUMMY_PRIV(self)->bufops = 0;
	DUMMY_PRIV(self)->size_buf_packed = 0;
	DUMMY_PRIV(self)->size_buf_planar = 0;
	DUMMY_PRIV(self)->buf_packed = NULL;
	DUMMY_PRIV(self)->buf_planar = NULL;
	if( vdi_sysinfo.vdiformat == VDI_FORMAT_PACK  ) {
		self->bpp_virt = vdi_sysinfo.scr_bpp;
	} else {
		DUMMY_PRIV(self)->bufops = C2P;
		self->bpp_virt = 8;
	}
	if( FIRSTFB(self).w > vdi_sysinfo.scr_w || FIRSTFB(self).h > vdi_sysinfo.scr_h ){
		return( 0-ERR_BUFFERSIZE_EXCEEDS_SCREEN );
	}

	FIRSTFB(self).size = calc_chunked_buffer_size( FIRSTFB(self).w, FIRSTFB(self).h, FIRSTFB(self).w, self->bpp_virt );
	/* offscreen: FIRSTFB(self).mem = malloc( FIRSTFB(self).size ); */
	FIRSTFB(self).mem = NULL;
	update_visible_rect( self );

	clip.x0 = 0;
	clip.y0 = 0;
	clip.x1 = FIRSTFB(self).w;
	clip.y1 = FIRSTFB(self).h;
	self->clip( self, &clip );

	assert( Hermes_Init() );
	/* store system palette & setup the new (web) palette: */
#ifdef WITH_8BPP_SUPPORT
	i = 0;

	unsigned char * col;
	unsigned char rgbcol[4];
	unsigned char graytone=0;
	if( app.nplanes <= 8 ){
		for( i=0; i<=255; i++ ) {

			// get the current color and save it for restore:
			vq_color(self->vdi_handle, i, 1, (unsigned short*)&sys_pal[i][0] );
			if( i<OFFSET_WEB_PAL ) {
				pal[i][0] = sys_pal[i][0];
		 		pal[i][1] = sys_pal[i][1];
				pal[i][2] = sys_pal[i][2];
			} else if( app.nplanes >= 8 ) {
				if ( i < OFFSET_CUST_PAL ){
					pal[i][0] = vdi_web_pal[i-OFFSET_WEB_PAL][0];
					pal[i][1] = vdi_web_pal[i-OFFSET_WEB_PAL][1];
					pal[i][2] = vdi_web_pal[i-OFFSET_WEB_PAL][2];
					//set the new palette color to websafe value:
					vs_color( self->vdi_handle, i, &pal[i][0] );
				}
				if( i >= OFFSET_CUST_PAL && i<OFFSET_CUST_PAL+16 ) {
					/* here we define 20 additional gray colors... */
					rgbcol[1] = rgbcol[2] = rgbcol[3] = ((graytone&0x0F) << 4);
					rgb_to_vdi1000( &rgbcol[0], &pal[i][0] );
					vs_color( self->vdi_handle, i, &pal[i][0] );
					graytone++;
				}

			}
			vdi1000_to_rgb( &pal[i][0],  &rgb_lookup[i][0] );
		}

	} else {
		/* no need to change the palette - its application specific */
	}


#endif

	unsigned long flags = ( self->flags & PLOT_FLAG_DITHER ) ? HERMES_CONVERT_DITHER : 0;
	hermes_cnv_h = Hermes_ConverterInstance( flags );
	assert( hermes_cnv_h );
	hermes_res_h = Hermes_ConverterInstance( flags );
	assert( hermes_res_h );

	/* set up the src & dst format: */
	/* netsurf uses RGBA ... */
	DUMMY_PRIV(self)->nsfmt.a = 0xFFUL;
	DUMMY_PRIV(self)->nsfmt.b = 0x0FF00UL;
	DUMMY_PRIV(self)->nsfmt.g = 0x0FF0000UL;
	DUMMY_PRIV(self)->nsfmt.r = 0x0FF000000UL;
	DUMMY_PRIV(self)->nsfmt.bits = 32;
	DUMMY_PRIV(self)->nsfmt.indexed = false;
	DUMMY_PRIV(self)->nsfmt.has_colorkey = false;

	DUMMY_PRIV(self)->vfmt.r = vdi_sysinfo.mask_r;
	DUMMY_PRIV(self)->vfmt.g = vdi_sysinfo.mask_g;
	DUMMY_PRIV(self)->vfmt.b = vdi_sysinfo.mask_b;
	DUMMY_PRIV(self)->vfmt.a = vdi_sysinfo.mask_a;
	DUMMY_PRIV(self)->vfmt.bits = self->bpp_virt;
	DUMMY_PRIV(self)->vfmt.indexed = ( app.nplanes <= 8 ) ? 1 : 0;
	DUMMY_PRIV(self)->vfmt.has_colorkey = 0;

	return( 1 );
}

static int dtor( GEM_PLOTTER self )
{
	int i=0;
	LOG(("%s: %s\n", (char*)__FILE__, __FUNCTION__));
	for( i=0; i<MAX_FRAMEBUFS; i++) {
		if( self->fbuf[i].mem != NULL )
			free( self->fbuf[i].mem );
	}

#ifdef WITH_8BPP_SUPPORT
	for( i=OFFSET_WEB_PAL; i<OFFSET_CUST_PAL+16; i++){
		vs_color( self->vdi_handle, i, &sys_pal[i][0] );
	}
#endif

	/* close Hermes stuff: */
	Hermes_ConverterReturn( hermes_cnv_h );

	Hermes_Done();

	if( self->priv_data != NULL ){
		if( DUMMY_PRIV(self)->buf_packed )
			free( DUMMY_PRIV(self)->buf_packed );
		if( DUMMY_PRIV(self)->buf_planar )
			free( DUMMY_PRIV(self)->buf_planar );
		free( self->priv_data );
	}
	snapshot_destroy( self );
	return( 1 );
}

static int resize( GEM_PLOTTER self, int w, int h )
{
	if( w == CURFB(self).w && h == CURFB(self).h )
		return( 1 );
	/* todo: needed when using offscreen buffers...
	int newsize = calc_chunked_buffer_size( w, h, w, self->bpp_virt );
	LOG(("%s: %s, oldsize: %d\n", (char*)__FILE__, __FUNCTION__, CURFB(self).size ));
	if( newsize > self->screen_buffer_size ) {
		self->screen_buffer_size = newsize;
		self->screen_buffer =realloc( self->screen_buffer , self->screen_buffer_size );
	}
	*/
	CURFB(self).w = w;
	CURFB(self).h = h;
	update_visible_rect( self );
	LOG(("%s: %s, newsize: %d\n", (char*)__FILE__, (char*)__FUNCTION__, CURFB(self).size ));
	return( 1 );
}
static int move( GEM_PLOTTER self,short x, short y )
{
	bool upd;
	if(x == CURFB(self).x && y == CURFB(self).y ){
		return 1;
	}
	LOG(("%s: x: %d, y: %d\n",(char*)__FUNCTION__, x, y));
	CURFB(self).x = x;
	CURFB(self).y = y;
	update_visible_rect( self );
	return( 1 );
}


static int lock( GEM_PLOTTER self )
{
	LOG(("%s: %s\n", (char*)__FILE__, __FUNCTION__));
	if( (self->flags & PLOT_FLAG_LOCKED) != 0 )
		return(1);
	self->flags |= PLOT_FLAG_LOCKED;
	if( !wind_update(BEG_UPDATE|0x100) )
		return(0);
	if( !wind_update(BEG_MCTRL|0x100) ){
		wind_update(END_UPDATE);
		return(0);
	}
	graf_mouse(M_OFF, NULL);
	return( 1 );
}

static int unlock( GEM_PLOTTER self )
{
	LOG(("%s: %s\n", (char*)__FILE__, __FUNCTION__));
	if( (self->flags & PLOT_FLAG_LOCKED) == 0 )
		return(1);
	self->flags &=  ~PLOT_FLAG_LOCKED;
	wind_update(END_MCTRL);
	wind_update(END_UPDATE);
	graf_mouse(M_ON, NULL);
	return( 1 );
}

/*
   region specifies an rectangle within the framebuffer
   calculation of screen coords is done automatically.
*/
static int update_region( GEM_PLOTTER self, GRECT region )
{
	int src_offs;
	GRECT screen_area, tmp, visible;
	short pxy[10];
	plotter_get_visible_grect( self, &visible );

/*
	LOG(("%s: %s %d\n", (char*)__FILE__, __FUNCTION__, __LINE__));
	LOG(("region: x:%d, y:%d, w:%d, h:%d\n", region.g_x, region.g_y, region.g_w, region.g_h ));
	LOG(("visible: x:%d, y:%d, w:%d, h:%d\n", visible.g_x, visible.g_y, visible.g_w, visible.g_h ));
*/
	/* sanitize region: */
	tmp = region;
	if( !rc_intersect(&visible, &tmp) )
		return( 0 );
/*
    region is partially out of bottom or left:
   if( region.g_x < self->visible.g_x )
   {
      region.g_w = self->visible.g_x - region.g_x;
      region.g_x = self->visible.g_x;
   }
   if( region.g_y < self->visible.g_y )
   {
      region.g_h = self->visible.g_y - region.g_y;
      region.g_y = self->visible.g_y;
   }
    region is partially out of top or right:
   if( region.g_x + region.g_w > self->visible.g_x + self->visible.g_w )
   {
      region.g_w = self->visible.g_w - region.g_x;
   }
   if( region.g_y + region.g_h > self->visible.g_y + self->visible.g_h )
   {
      region.g_h = self->visible.g_h - region.g_y;
   }
    now region contains coords of framebuffer that needs redraw.
*/
	if( fbrect_to_screen( self, tmp, &screen_area) ) {
		pxy[0] = screen_area.g_x;
		pxy[1] = screen_area.g_y;
		pxy[2] = screen_area.g_x + screen_area.g_w;
		pxy[3] = screen_area.g_y;
		pxy[4] = screen_area.g_x + screen_area.g_w;
		pxy[5] = screen_area.g_y + screen_area.g_h;
		pxy[6] = screen_area.g_x;
		pxy[7] = screen_area.g_y + screen_area.g_h;
		pxy[8] = screen_area.g_x;
		pxy[9] = screen_area.g_y;
	}
   return( 1 );
}

/*
   region specifies an rectangle within the screen,
   calculation of framebuffer coords is done automatically.
*/
static int update_screen_region( GEM_PLOTTER self, GRECT region )
{
   LOG(("%s: %s\n", (char*)__FILE__, __FUNCTION__));

   return( 1 );
}

/* Updates all visible parts of the framebuffer */
static int update_screen( GEM_PLOTTER self )
{
	GRECT target, src;
	int src_offset;
	int i,x;
	LOG(("%s: %s\n", (char*)__FILE__, __FUNCTION__));
	if( !(PLOT_FLAG_OFFSCREEN & self->flags) )
    	return( 0 );
	target.g_x = src.g_x = 0;
	target.g_y = src.g_y = 0;
	target.g_w = src.g_w = CURFB(self).w;
	target.g_h = src.g_h = CURFB(self).h;
	if( !fbrect_to_screen( self, target, &target ) )
		return( -1 );
	src_offset = get_pixel_offset( CURFB(self).vis_x, CURFB(self).vis_y, CURFB(self).w, self->bpp_virt );
	LOG(("area: x:%d ,y:%d ,w:%d ,h:%d, from: %p (offset: %d) \n",
		target.g_x, target.g_y,
         target.g_w, target.g_h,
		((char*)CURFB(self).mem)+src_offset, src_offset
	));

   return( 1 );
}
static int put_pixel(GEM_PLOTTER self, int x, int y, int color )
{
   LOG(("%s: %s\n", (char*)__FILE__, __FUNCTION__));
   return( 1 );
}

/* copy an rectangle from the plot buffer to screen */
/* because this is an on-screen plotter, this is an screen to screen copy. */
static int copy_rect( GEM_PLOTTER self, GRECT src, GRECT dst )
{
	MFDB devmf;
	MFDB scrmf;
	short pxy[8];
	GRECT vis;

	/* clip to visible rect, only needed for onscreen renderer: */
	plotter_get_visible_grect( self, &vis );

	if( !rc_intersect(&vis, &src) )
		return 1;
	if( !rc_intersect(&vis, &dst) )
		return 1;

	src.g_x = CURFB(self).x + src.g_x;
	src.g_y = CURFB(self).y + src.g_y;
	dst.g_x = CURFB(self).x + dst.g_x;
	dst.g_y = CURFB(self).y + dst.g_y;

	devmf.fd_addr = NULL;
	devmf.fd_w = src.g_w;
	devmf.fd_h = src.g_h;
	devmf.fd_wdwidth = 0;
	devmf.fd_stand = 0;
	devmf.fd_nplanes = 0;
	devmf.fd_r1 = devmf.fd_r2 = devmf.fd_r3 = 0;

	scrmf.fd_addr = NULL;
	scrmf.fd_w = dst.g_w;
	scrmf.fd_h = dst.g_h;
	scrmf.fd_wdwidth = 0 ;
	scrmf.fd_stand = 0;
	scrmf.fd_nplanes = 0;
	scrmf.fd_r1 = scrmf.fd_r2 = scrmf.fd_r3 = 0;

	pxy[0] = src.g_x;
	pxy[1] = src.g_y;
	pxy[2] = pxy[0] + src.g_w-1;
	pxy[3] = pxy[1] + src.g_h-1;
	pxy[4] = dst.g_x;
	pxy[5] = dst.g_y;
	pxy[6] = pxy[4] + dst.g_w-1;
	pxy[7] = pxy[5] + dst.g_h-1;
	self->lock( self );
	vro_cpyfm( self->vdi_handle, S_ONLY, (short*)&pxy, &devmf,  &scrmf);
	self->unlock( self );

	return( 1 );
}

static int arc(GEM_PLOTTER self,int x, int y, int radius, int angle1, int angle2, const plot_style_t * pstyle)
{
	//plotter_vdi_clip( self, 1);
	vswr_mode( self->vdi_handle, MD_REPLACE );
	if( pstyle->fill_type == PLOT_OP_TYPE_NONE )
		return 1;
	if( pstyle->fill_type != PLOT_OP_TYPE_SOLID) {
		vsl_rgbcolor( self->vdi_handle, pstyle->stroke_colour);
		vsf_perimeter( self->vdi_handle, 1);
		vsf_interior( self->vdi_handle, 1 );
		v_arc( self->vdi_handle, CURFB(self).x + x, CURFB(self).y + y, radius, angle1*10, angle2*10 );
	} else {
		vsf_rgbcolor( self->vdi_handle, pstyle->fill_colour);
		vsl_width( self->vdi_handle, 1 );
		vsf_perimeter( self->vdi_handle, 1);
		v_arc( self->vdi_handle, CURFB(self).x + x, CURFB(self).y + y, radius, angle1*10, angle2*10 );
	}
	//plotter_vdi_clip( self, 0);
	return ( 1 );
}

static int disc(GEM_PLOTTER self,int x, int y, int radius, const plot_style_t * pstyle)
{
	plotter_vdi_clip( self, 1);
	if( pstyle->fill_type != PLOT_OP_TYPE_SOLID) {
		vsf_rgbcolor( self->vdi_handle, pstyle->stroke_colour );
		vsf_perimeter( self->vdi_handle, 1);
		vsf_interior( self->vdi_handle, 0 );
		v_circle( self->vdi_handle, CURFB(self).x + x, CURFB(self).y + y, radius  );
	} else {
		vsf_rgbcolor( self->vdi_handle, pstyle->fill_colour );
		vsf_perimeter( self->vdi_handle, 0);
		vsf_interior( self->vdi_handle, FIS_SOLID );
		v_circle( self->vdi_handle, CURFB(self).x + x, CURFB(self).y + y, radius  );
	}
	plotter_vdi_clip( self, 0);
	return ( 1 );
}


static int line(GEM_PLOTTER self,int x0, int y0, int x1, int y1, const plot_style_t * pstyle)
{
	short pxy[4];
	uint32_t lt;
	int sw = pstyle->stroke_width;

	pxy[0] = CURFB(self).x + x0;
	pxy[1] = CURFB(self).y + y0;
	pxy[2] = CURFB(self).x + x1;
	pxy[3] = CURFB(self).y + y1;

	plotter_vdi_clip( self, 1);
	if( sw == 0)
		sw = 1;
	NSLT2VDI(lt, pstyle)
	vsl_type( self->vdi_handle, (lt&0x0F) );
	/* if the line style is not available within VDI system,define own style: */
	if( (lt&0x0F) == 7 ){
		vsl_udsty(self->vdi_handle, ((lt&0xFFFF00) >> 8) );
	}
	vsl_width( self->vdi_handle, (short)sw );
	vsl_rgbcolor( self->vdi_handle, pstyle->stroke_colour );
	v_pline(self->vdi_handle, 2, (short *)&pxy );
	plotter_vdi_clip( self, 0);
   return ( 1 );
}

static int rectangle(GEM_PLOTTER self,int x0, int y0, int x1, int y1,  const plot_style_t * pstyle)
{
	short pxy[4];
	GRECT r, rclip, sclip;
	int sw = pstyle->stroke_width;
	uint32_t lt;

	/* current canvas clip: */
	rclip.g_x = self->clipping.x0;
	rclip.g_y = self->clipping.y0;
	rclip.g_w = self->clipping.x1 - self->clipping.x0;
	rclip.g_h = self->clipping.y1 - self->clipping.y0;

	/* physical clipping: */
	sclip.g_x = rclip.g_x;
	sclip.g_y = rclip.g_y;
	sclip.g_w = CURFB(self).vis_w;
	sclip.g_h = CURFB(self).vis_h;

	rc_intersect(&sclip, &rclip);
	r.g_x = x0;
	r.g_y = y0;
	r.g_w = x1 - x0;
	r.g_h = y1 - y0;

	if( !rc_intersect( &rclip, &r ) ) {
		return( 1 );
	}
	if( pstyle->stroke_type != PLOT_OP_TYPE_NONE ){
		/*
			manually draw the line, because we do not need vdi clipping
			for vertical / horizontal line draws.
		*/
		if( sw == 0)
			sw = 1;

		NSLT2VDI(lt, pstyle);
		vsl_type( self->vdi_handle, (lt&0x0F) );
		/*
			if the line style is not available within VDI system,
			define own style:
		*/
		if( (lt&0x0F) == 7 ){
			vsl_udsty(self->vdi_handle, ((lt&0xFFFF00) >> 8) );
		}
		vsl_width( self->vdi_handle, (short)sw );
		vsl_rgbcolor( self->vdi_handle, pstyle->stroke_colour );
		/* top border: */
		if( r.g_y == y0){
			pxy[0] = CURFB(self).x + r.g_x;
			pxy[1] = CURFB(self).y + r.g_y ;
			pxy[2] = CURFB(self).x + r.g_x + r.g_w;
			pxy[3] = CURFB(self).y + r.g_y;
			v_pline(self->vdi_handle, 2, (short *)&pxy );
		}

		/* right border: */
		if( r.g_x + r.g_w == x1 ){
			pxy[0] = CURFB(self).x + r.g_x + r.g_w;
			pxy[1] = CURFB(self).y + r.g_y;
			pxy[2] = CURFB(self).x + r.g_x + r.g_w;
			pxy[3] = CURFB(self).y + r.g_y + r.g_h;
			v_pline(self->vdi_handle, 2, (short *)&pxy );
		}

		/* bottom border: */
		if( r.g_y+r.g_h == y1 ){
			pxy[0] = CURFB(self).x + r.g_x;
			pxy[1] = CURFB(self).y + r.g_y+r.g_h;
			pxy[2] = CURFB(self).x + r.g_x+r.g_w;
			pxy[3] = CURFB(self).y + r.g_y+r.g_h;
			v_pline(self->vdi_handle, 2, (short *)&pxy );
		}

		/* left border: */
		if( r.g_x == x0 ){
			pxy[0] = CURFB(self).x + r.g_x;
			pxy[1] = CURFB(self).y + r.g_y;
			pxy[2] = CURFB(self).x + r.g_x;
			pxy[3] = CURFB(self).y + r.g_y + r.g_h;
			v_pline(self->vdi_handle, 2, (short *)&pxy );
		}
	}

	if( pstyle->fill_type != PLOT_OP_TYPE_NONE ){

		short stroke_width = (short)(pstyle->stroke_type != PLOT_OP_TYPE_NONE) ?
								pstyle->stroke_width : 0;

		vsf_rgbcolor( self->vdi_handle, pstyle->fill_colour );
		vsf_perimeter( self->vdi_handle, 0);
		vsf_interior( self->vdi_handle, FIS_SOLID );


		pxy[0] = CURFB(self).x + r.g_x + stroke_width;
		pxy[1] = CURFB(self).y + r.g_y + stroke_width;
		pxy[2] = CURFB(self).x + r.g_x + r.g_w -1 - stroke_width ;
		pxy[3] = CURFB(self).y + r.g_y + r.g_h -1 - stroke_width;

		vsf_style( self->vdi_handle, 1);
		v_bar( self->vdi_handle, (short*)&pxy );
	}

	return ( 1 );
}

static int polygon(GEM_PLOTTER self,const int *p, unsigned int n,  const plot_style_t * pstyle)
{
	short pxy[n*2];
	unsigned int i=0;
	short d[4];
	if( vdi_sysinfo.maxpolycoords > 0 )
		assert( (signed int)n < vdi_sysinfo.maxpolycoords );
	plotter_vdi_clip( self, 1);
	vsf_interior( self->vdi_handle, FIS_SOLID );
	vsf_style( self->vdi_handle, 1);
	for( i = 0; i<n*2; i=i+2 ) {
		pxy[i] = (short)CURFB(self).x+p[i];
		pxy[i+1] = (short)CURFB(self).y+p[i+1];
	}
	if( pstyle->fill_type == PLOT_OP_TYPE_SOLID){
		vsf_rgbcolor( self->vdi_handle, pstyle->fill_colour);
		v_fillarea(self->vdi_handle, n, (short*)&pxy);

	} else {
		pxy[n*2]=pxy[0];
		pxy[n*2+1]=pxy[1];
		vsl_rgbcolor( self->vdi_handle, pstyle->stroke_colour);
		v_pline(self->vdi_handle, n+1,  (short *)&pxy );
	}
	plotter_vdi_clip( self, 0);
	return ( 1 );
}

static int path(GEM_PLOTTER self,const float *p, unsigned int n, int fill, float width,
			int c, const float transform[6])
{
	LOG(("%s: %s\n", (char*)__FILE__, __FUNCTION__));
	return ( 1 );
}


static inline uint32_t ablend(uint32_t pixel, uint32_t scrpixel)
{
    int opacity = pixel & 0xFF;
    int transp = 0x100 - opacity;
    uint32_t rb, g;
	pixel >>= 8;
	scrpixel >>= 8;
	rb = ((pixel & 0xFF00FF) * opacity +
    	(scrpixel & 0xFF00FF) * transp) >> 8;
    g  = ((pixel & 0x00FF00) * opacity +
    	(scrpixel & 0x00FF00) * transp) >> 8;

    return ((rb & 0xFF00FF) | (g & 0xFF00)) << 8;
}

static int bitmap_resize( GEM_PLOTTER self, struct bitmap * img, int nw, int nh )
{
	HermesFormat fmt;
	short bpp = bitmap_get_bpp( img );
	int stride = bitmap_get_rowstride( img );
	int err;

	if( img->resized != NULL ) {
		if( img->resized->width != nw || img->resized->height != nh ) {
			bitmap_destroy( img->resized );
			img->resized = NULL;
		} else {
			/* the bitmap is already resized */
			return( 0 );
		}
	}

	/* allocate the mem for resized bitmap */
	img->resized = bitmap_create_ex( nw, nh, bpp, nw*bpp, 0, NULL );
	if( img->resized == NULL ) {
			printf("W: %d, H: %d, bpp: %d\n", nw, nh, bpp);
			assert( img->resized );
			return ( -ERR_NO_MEM );
	}

	/* allocate an converter, only for resizing */
	err = Hermes_ConverterRequest( hermes_res_h,
			&DUMMY_PRIV(self)->nsfmt,
			&DUMMY_PRIV(self)->nsfmt
	);
	if( err == 0 ) {
		return( -ERR_PLOTTER_NOT_AVAILABLE );
	}

	err = Hermes_ConverterCopy( hermes_res_h,
		img->pixdata,
		0,			/* x src coord of top left in pixel coords */
		0,			/* y src coord of top left in pixel coords */
		bitmap_get_width( img ), bitmap_get_height( img ),
		stride, 	/* stride as bytes */
		img->resized->pixdata,
		0,			/* x dst coord of top left in pixel coords */
		0,			/* y dst coord of top left in pixel coords */
		nw, nh,
		bitmap_get_rowstride( img->resized ) /* stride as bytes */
	);
	if( err == 0 ) {
		bitmap_destroy( img->resized );
		img->resized = NULL;
		return( -2 );
	}

	return( 0 );
}

// create snapshot, native screen format
static MFDB * snapshot_create_native_mfdb( GEM_PLOTTER self, int x, int y, int w, int h)
{
	MFDB scr;
	short pxy[8];

	/* allocate memory for the snapshot */
	{
		int scr_stride = MFDB_STRIDE( w );
		int scr_size = ( ((scr_stride >> 3) * h) * vdi_sysinfo.scr_bpp );
		if( DUMMY_PRIV(self)->size_buf_scr == 0 ){
			/* init screen mfdb */
			DUMMY_PRIV(self)->buf_scr.fd_addr = malloc( scr_size );
			DUMMY_PRIV(self)->size_buf_scr = scr_size;
		} else {
			if( scr_size > DUMMY_PRIV(self)->size_buf_scr ) {
				DUMMY_PRIV(self)->buf_scr.fd_addr = realloc(
					DUMMY_PRIV(self)->buf_scr.fd_addr, scr_size
				);
				DUMMY_PRIV(self)->size_buf_scr = scr_size;
			}
		}
		if( DUMMY_PRIV(self)->buf_scr.fd_addr == NULL ) {
			DUMMY_PRIV(self)->size_buf_scr = 0;
			return( NULL );
		}
		DUMMY_PRIV(self)->buf_scr.fd_nplanes = vdi_sysinfo.scr_bpp;
		DUMMY_PRIV(self)->buf_scr.fd_w = scr_stride;
		DUMMY_PRIV(self)->buf_scr.fd_h = h;
		DUMMY_PRIV(self)->buf_scr.fd_wdwidth = scr_stride >> 4;
		assert( DUMMY_PRIV(self)->buf_scr.fd_addr != NULL );
	}
	init_mfdb( 0, w, h, 0, &scr );
	pxy[0] = x;
	pxy[1] = y;
	pxy[2] = pxy[0] + w-1;
	pxy[3] = pxy[1] + h-1;
	pxy[4] = 0;
	pxy[5] = 0;
	pxy[6] = w-1;
	pxy[7] = h-1;
	vro_cpyfm(
			self->vdi_handle, S_ONLY, (short*)&pxy,
			&scr,  &DUMMY_PRIV(self)->buf_scr
	);

	return( &DUMMY_PRIV(self)->buf_scr );
}

// create snapshot, vdi std. format
static MFDB * snapshot_create_std_mfdb(GEM_PLOTTER self, int x, int y, int w, int h)
{
	/* allocate memory for the snapshot */
	{
		int scr_stride = MFDB_STRIDE( w );
		int scr_size = ( ((scr_stride >> 3) * h) * app.nplanes );
		if( DUMMY_PRIV(self)->size_buf_std == 0 ){
			/* init screen mfdb */
			DUMMY_PRIV(self)->buf_std.fd_addr = malloc( scr_size );
			DUMMY_PRIV(self)->size_buf_std = scr_size;
		} else {
			if( scr_size > DUMMY_PRIV(self)->size_buf_std ) {
				DUMMY_PRIV(self)->buf_std.fd_addr = realloc(
					DUMMY_PRIV(self)->buf_std.fd_addr, scr_size
				);
				DUMMY_PRIV(self)->size_buf_std = scr_size;
			}
		}
		if( DUMMY_PRIV(self)->buf_std.fd_addr == NULL ) {
			DUMMY_PRIV(self)->size_buf_std = 0;
			return( NULL );
		}
		DUMMY_PRIV(self)->buf_std.fd_nplanes = app.nplanes;
		DUMMY_PRIV(self)->buf_std.fd_w = scr_stride;
		DUMMY_PRIV(self)->buf_std.fd_h = h;
		DUMMY_PRIV(self)->buf_std.fd_stand = 1;
		DUMMY_PRIV(self)->buf_std.fd_wdwidth = scr_stride >> 4;
		assert( DUMMY_PRIV(self)->buf_std.fd_addr != NULL );
	}
	MFDB * native = snapshot_create_native_mfdb( self, x,y,w,h );
	assert( native );

	vr_trnfm( self->vdi_handle, native, &DUMMY_PRIV(self)->buf_std );
	return( &DUMMY_PRIV(self)->buf_std );
}

/*
	This will create an snapshot of the screen in netsurf ABGR format
*/
static struct bitmap * snapshot_create(GEM_PLOTTER self, int x, int y, int w, int h)
{
	int err;
	MFDB * native;

	native = snapshot_create_native_mfdb( self, x, y, w, h );

	/* allocate buffer for result bitmap: */
	if( DUMMY_PRIV(self)->buf_scr_compat == NULL ) {
		DUMMY_PRIV(self)->buf_scr_compat = bitmap_create(w, h, 0);
	} else {
		DUMMY_PRIV(self)->buf_scr_compat = bitmap_realloc( w, h,
			DUMMY_PRIV(self)->buf_scr_compat->bpp,
			w * DUMMY_PRIV(self)->buf_scr_compat->bpp,
			BITMAP_GROW,
			DUMMY_PRIV(self)->buf_scr_compat );
	}

	/* convert screen buffer to ns format: */
	err = Hermes_ConverterRequest( hermes_cnv_h,
			&DUMMY_PRIV(self)->vfmt,
			&DUMMY_PRIV(self)->nsfmt
	);
	assert( err != 0 );
	err = Hermes_ConverterCopy( hermes_cnv_h,
		native->fd_addr,
		0,			/* x src coord of top left in pixel coords */
		0,			/* y src coord of top left in pixel coords */
		w, h,
		native->fd_w * vdi_sysinfo.pixelsize, /* stride as bytes */
		DUMMY_PRIV(self)->buf_scr_compat->pixdata,
		0,			/* x dst coord of top left in pixel coords */
		0,			/* y dst coord of top left in pixel coords */
		w, h,
		bitmap_get_rowstride(DUMMY_PRIV(self)->buf_scr_compat) /* stride as bytes */
	);
	assert( err != 0 );
	return( (struct bitmap * )DUMMY_PRIV(self)->buf_scr_compat );
}

static void snapshot_suspend(GEM_PLOTTER self )
{
	if( DUMMY_PRIV(self)->size_buf_scr > CONV_KEEP_LIMIT  ) {
		DUMMY_PRIV(self)->buf_scr.fd_addr = realloc(
			DUMMY_PRIV(self)->buf_scr.fd_addr, CONV_KEEP_LIMIT
		);
		if( DUMMY_PRIV(self)->buf_scr.fd_addr != NULL ) {
			DUMMY_PRIV(self)->size_buf_scr = CONV_KEEP_LIMIT;
		} else {
			DUMMY_PRIV(self)->size_buf_scr = 0;
		}
	}

	if( DUMMY_PRIV(self)->size_buf_std > CONV_KEEP_LIMIT  ) {
		DUMMY_PRIV(self)->buf_std.fd_addr = realloc(
			DUMMY_PRIV(self)->buf_std.fd_addr, CONV_KEEP_LIMIT
		);
		if( DUMMY_PRIV(self)->buf_std.fd_addr != NULL ) {
			DUMMY_PRIV(self)->size_buf_std = CONV_KEEP_LIMIT;
		} else {
			DUMMY_PRIV(self)->size_buf_std = 0;
		}
	}

	if( bitmap_buffer_size( DUMMY_PRIV(self)->buf_scr_compat ) > CONV_KEEP_LIMIT ) {
		int w = 0;
		int h = 1;
		w = (CONV_KEEP_LIMIT / DUMMY_PRIV(self)->buf_scr_compat->bpp);
		assert( CONV_KEEP_LIMIT == w*DUMMY_PRIV(self)->buf_scr_compat->bpp );
		DUMMY_PRIV(self)->buf_scr_compat = bitmap_realloc( w, h,
			DUMMY_PRIV(self)->buf_scr_compat->bpp,
			CONV_KEEP_LIMIT, BITMAP_SHRINK, DUMMY_PRIV(self)->buf_scr_compat
		);
	}
}

static void snapshot_destroy( GEM_PLOTTER self )
{
	if( DUMMY_PRIV(self)->buf_scr.fd_addr ) {
		free( DUMMY_PRIV(self)->buf_scr.fd_addr  );
		DUMMY_PRIV(self)->buf_scr.fd_addr = NULL;
	}

	if( DUMMY_PRIV(self)->buf_std.fd_addr ) {
		free( DUMMY_PRIV(self)->buf_std.fd_addr  );
		DUMMY_PRIV(self)->buf_std.fd_addr = NULL;
	}

	if( DUMMY_PRIV(self)->buf_scr_compat ) {
		bitmap_destroy( DUMMY_PRIV(self)->buf_scr_compat );
		DUMMY_PRIV(self)->buf_scr_compat = NULL;
	}
}

inline void set_stdpx( MFDB * dst, int wdplanesz, int x, int y, unsigned char val )
{
	short * buf;
	short whichbit = (1<<(15-(x%16)));

	buf = dst->fd_addr;
	buf += ((dst->fd_wdwidth*(y))+(x>>4));

	*buf = (val&1) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));

	buf += wdplanesz;
	*buf = (val&(1<<1)) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));

	buf += wdplanesz;
	*buf = (val&(1<<2)) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));

	buf += wdplanesz;
	*buf = (val&(1<<3)) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));

	buf += wdplanesz;
	*buf = (val&(1<<4)) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));

	buf += wdplanesz;
	*buf = (val&(1<<5)) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));

	buf += wdplanesz;
	*buf = (val&(1<<6)) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));

	buf += wdplanesz;
	*buf = (val&(1<<7)) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));
}

inline unsigned char get_stdpx(MFDB * dst, int wdplanesz, int x, int y )
{
	unsigned char ret=0;
	short * buf;
	short whichbit = (1<<(15-(x%16)));

	buf = dst->fd_addr;
	buf += ((dst->fd_wdwidth*(y))+(x>>4));

	if( *buf & whichbit )
		ret |= 1;

	buf += wdplanesz;
	if( *buf & whichbit )
		ret |= 2;

	buf += wdplanesz;
	if( *buf & whichbit )
		ret |= 4;

	buf += wdplanesz;
	if( *buf & whichbit )
		ret |= 8;

	buf += wdplanesz;
	if( *buf & whichbit )
		ret |= 16;

	buf += wdplanesz;
	if( *buf & whichbit )
		ret |= 32;

	buf += wdplanesz;
	if( *buf & whichbit )
		ret |= 64;

	buf += wdplanesz;
	if( *buf & whichbit )
		ret |= 128;

	return( ret );
}

#ifdef WITH_8BPP_SUPPORT
static int bitmap_convert_8( GEM_PLOTTER self,
	struct bitmap * img,
	int x,
	int y,
	GRECT * clip,
	uint32_t bg,
	uint32_t flags,
	MFDB *out  )
{

	int dststride;						/* stride of dest. image */
	int dstsize;						/* size of dest. in byte */
	int err;
	int bw;
	struct bitmap * scrbuf = NULL;
	struct bitmap * bm;
	bool transp = ( ( (img->opaque == false) || ( (flags & BITMAP_MONOGLYPH) != 0) )
					&& ((self->flags & PLOT_FLAG_TRANS) != 0) );

	assert( clip->g_h > 0 );
	assert( clip->g_w > 0 );

	bm = img;
	bw = bitmap_get_width( img );

	dststride = MFDB_STRIDE( clip->g_w );
	dstsize = ( ((dststride >> 3) * clip->g_h) * self->bpp_virt );

	/* (re)allocate buffer for out image: */
	/* altough the buffer is named "buf_packed" on 8bit systems */
	/* it's not... */
	if( dstsize > DUMMY_PRIV(self)->size_buf_packed) {
		int blocks = (dstsize / (CONV_BLOCK_SIZE-1))+1;
		if( DUMMY_PRIV(self)->buf_packed == NULL )
			DUMMY_PRIV(self)->buf_packed =(void*)malloc( blocks * CONV_BLOCK_SIZE );
		 else
			DUMMY_PRIV(self)->buf_packed =(void*)realloc(
													DUMMY_PRIV(self)->buf_packed,
													blocks * CONV_BLOCK_SIZE
												);
		assert( DUMMY_PRIV(self)->buf_packed );
		if( DUMMY_PRIV(self)->buf_packed == NULL ) {
			return( 0-ERR_NO_MEM );
		}
		DUMMY_PRIV(self)->size_buf_packed = blocks * CONV_BLOCK_SIZE;
	}


	/*
		on 8 bit systems we must convert the TC (ABGR) image
		to vdi standard format. ( only tested for 256 colors )
		and then convert it to native format
	*/

	// realloc mem for stdform
	MFDB stdform;
	if( transp ){
		if( ((self->flags & PLOT_FLAG_TRANS) != 0) || ( (flags & BITMAP_MONOGLYPH) != 0) ) {
			// point image to snapshot buffer, otherwise allocate mem
			MFDB * bg = snapshot_create_std_mfdb( self, x+clip->g_x,y+clip->g_y, clip->g_w, clip->g_h );
			stdform.fd_addr = bg->fd_addr;
		} else {
			if( dstsize > DUMMY_PRIV(self)->size_buf_planar) {
				int blocks = (dstsize / (CONV_BLOCK_SIZE-1))+1;
				if( DUMMY_PRIV(self)->buf_planar == NULL )
					DUMMY_PRIV(self)->buf_planar =(void*)malloc( blocks * CONV_BLOCK_SIZE );
				 else
					DUMMY_PRIV(self)->buf_planar =(void*)realloc(
															DUMMY_PRIV(self)->buf_planar,
															blocks * CONV_BLOCK_SIZE
														);
				assert( DUMMY_PRIV(self)->buf_planar );
				if( DUMMY_PRIV(self)->buf_planar == NULL ) {
					return( 0-ERR_NO_MEM );
				}
				DUMMY_PRIV(self)->size_buf_planar = blocks * CONV_BLOCK_SIZE;
			}
			stdform.fd_addr = DUMMY_PRIV(self)->buf_planar;
		}
	}
	stdform.fd_w = dststride;
	stdform.fd_h = clip->g_h;
	stdform.fd_wdwidth = dststride >> 4;
	stdform.fd_stand = 1;
	stdform.fd_nplanes = (short)self->bpp_virt;
	stdform.fd_r1 = stdform.fd_r2 = stdform.fd_r3 = 0;

	int img_stride = bitmap_get_rowstride(bm);
	uint32_t prev_pixel = 0x12345678;
	unsigned long col = 0;
	unsigned char val = 0;
	uint32_t * row;
	uint32_t pixel;
	int wdplanesize = stdform.fd_wdwidth*stdform.fd_h;


	// apply transparency.
	if( transp ){
		unsigned long bgcol = 0;
		unsigned char prev_col = 0;


		for( y=0; y<clip->g_h; y++ ){

			row = (uint32_t *)(bm->pixdata + (img_stride * (y+clip->g_y)));

			for( x=0; x<clip->g_w; x++ ){

				pixel = row[x+clip->g_x];

				if( (pixel&0xFF) == 0 ){
					continue;
				}

				if( (pixel&0xFF) < 0xF0 ){
					col = get_stdpx( &stdform, wdplanesize,x,y );
					if( (col != prev_col) || (y == 0) )
						bgcol = (((rgb_lookup[col][2] << 16) | (rgb_lookup[col][1] << 8) | (rgb_lookup[col][0]))<<8);
					if( prev_col != col || prev_pixel != pixel ){
						prev_col = col;
						pixel = ablend( pixel, bgcol );
						prev_pixel = pixel;
						pixel = pixel >> 8;
						/* convert pixel value to vdi color index: */
						col = ( ((pixel&0xFF)<<16)
									| (pixel&0xFF00)
									| ((pixel&0xFF0000)>>16) );
						val = RGB_TO_VDI( col );
					}
					set_stdpx( &stdform, wdplanesize, x,y, val );
				} else {
					if( pixel != prev_pixel ){
						/* convert pixel value to vdi color index: */
						pixel = pixel >> 8;
						col = ( ((pixel&0xFF)<<16)
									| (pixel&0xFF00)
									| ((pixel&0xFF0000)>>16) );
						val = RGB_TO_VDI( col );
						prev_pixel = pixel;
					}
					set_stdpx( &stdform, wdplanesize, x,y, val );
				}
			}
		}
	} else {
		for( y=0; y<clip->g_h; y++ ){

			row = (uint32_t *)(bm->pixdata + (img_stride * (y+clip->g_y)));

			for( x=0; x<clip->g_w; x++ ){

				pixel = row[x+clip->g_x];
				if( pixel != prev_pixel ){
					/* convert pixel value to vdi color index: */
					pixel = pixel >> 8;
					col = ( ((pixel&0xFF)<<16)
								| (pixel&0xFF00)
								| ((pixel&0xFF0000)>>16) );
					val = RGB_TO_VDI( col );
					prev_pixel = pixel;
				}
				set_stdpx( &stdform, wdplanesize, x,y, val );
			}
		}
	}

	// convert into native format:
	MFDB native;
	native.fd_addr = DUMMY_PRIV(self)->buf_packed;
	native.fd_w = dststride;
	native.fd_h = clip->g_h;
	native.fd_wdwidth = dststride >> 4;
	native.fd_stand = 0;
	native.fd_nplanes = (short)self->bpp_virt;
	native.fd_r1 = native.fd_r2 = native.fd_r3 = 0;
	vr_trnfm( self->vdi_handle, &stdform, &native );
	*out = native;

	return(0);
}
#endif

/* convert bitmap to the virutal (chunked) framebuffer format */
static int bitmap_convert( GEM_PLOTTER self,
	struct bitmap * img,
	int x,
	int y,
	GRECT * clip,
	uint32_t bg,
	uint32_t flags,
	MFDB *out  )
{
	int dststride;						/* stride of dest. image */
	int dstsize;						/* size of dest. in byte */
	int err;
	int bw;
	struct bitmap * scrbuf = NULL;
	struct bitmap * bm;

	assert( clip->g_h > 0 );
	assert( clip->g_w > 0 );

	bm = img;
	bw = bitmap_get_width( img );

	/* rem. if eddi xy is installed, we could directly access the screen! */
	/* apply transparency to the image: */
	if( (img->opaque == false)
		&& ( (self->flags & PLOT_FLAG_TRANS) != 0)
		&& (
			(vdi_sysinfo.vdiformat == VDI_FORMAT_PACK )
			||
			( (flags & BITMAP_MONOGLYPH) != 0)
		) ) {
		uint32_t * imgpixel;
		uint32_t * screenpixel;
		int img_x, img_y;	/* points into old bitmap */
		int screen_x, screen_y;	/* pointers into new bitmap */
		/* copy the screen to an temp buffer: */
		scrbuf = snapshot_create(self, x, y, clip->g_w, clip->g_h );
		if( scrbuf != NULL ) {
			/* copy blended pixels the new buffer (which contains screen content): */
			int img_stride = bitmap_get_rowstride(bm);
			int screen_stride = bitmap_get_rowstride(scrbuf);
			for( img_y = clip->g_y, screen_y = 0; screen_y < clip->g_h; screen_y++, img_y++) {
				imgpixel = (uint32_t *)(bm->pixdata + (img_stride * img_y));
				screenpixel = (uint32_t *)(scrbuf->pixdata + (screen_stride * screen_y));
				for( img_x = clip->g_x, screen_x = 0; screen_x < clip->g_w; screen_x++, img_x++ ) {
					if( (imgpixel[img_x] & 0xFF) == 0xFF ) {
						screenpixel[screen_x] = imgpixel[img_x];
					} else {
						if( (imgpixel[img_x] & 0x0FF) != 0 ) {
							screenpixel[screen_x] = ablend( imgpixel[img_x], screenpixel[screen_x]);
						}
					}
				}
			}
			clip->g_x = 0;
			clip->g_y = 0;
			bm = scrbuf;
		}
	}

	/* (re)allocate buffer for framebuffer image: */
	dststride = MFDB_STRIDE( clip->g_w );
	dstsize = ( ((dststride >> 3) * clip->g_h) * self->bpp_virt);
	if( dstsize > DUMMY_PRIV(self)->size_buf_packed) {
		int blocks = (dstsize / (CONV_BLOCK_SIZE-1))+1;
		if( DUMMY_PRIV(self)->buf_packed == NULL )
			DUMMY_PRIV(self)->buf_packed =(void*)malloc( blocks * CONV_BLOCK_SIZE );
		 else
			DUMMY_PRIV(self)->buf_packed =(void*)realloc(
													DUMMY_PRIV(self)->buf_packed,
													blocks * CONV_BLOCK_SIZE
												);
		assert( DUMMY_PRIV(self)->buf_packed );
		if( DUMMY_PRIV(self)->buf_packed == NULL ) {
			if( scrbuf != NULL )
				bitmap_destroy( scrbuf );
			return( 0-ERR_NO_MEM );
		}
		DUMMY_PRIV(self)->size_buf_packed = blocks * CONV_BLOCK_SIZE;
	}
	out->fd_addr = DUMMY_PRIV(self)->buf_packed;
	out->fd_w = dststride;
	out->fd_h = clip->g_h;
	out->fd_wdwidth = dststride >> 4;
	out->fd_stand = 0;
	out->fd_nplanes = (short)self->bpp_virt;
	out->fd_r1 = out->fd_r2 = out->fd_r3 = 0;

	err = Hermes_ConverterRequest(
			hermes_cnv_h,
			&DUMMY_PRIV(self)->nsfmt,
			&DUMMY_PRIV(self)->vfmt
	);
	assert( err != 0 );
	/* convert image to virtual format: */
	err = Hermes_ConverterCopy( hermes_cnv_h,
		bm->pixdata,
		clip->g_x,			/* x src coord of top left in pixel coords */
		clip->g_y,			/* y src coord of top left in pixel coords */
		clip->g_w, clip->g_h,
		bm->rowstride, 	/* stride as bytes */
		out->fd_addr,
		0,			/* x dst coord of top left in pixel coords */
		0,			/* y dst coord of top left in pixel coords */
		clip->g_w, clip->g_h,
		(dststride >> 3) *  self->bpp_virt /* stride as bytes */
	);
	assert( err != 0 );

	return( 0 );

}

static void convert_bitmap_done( GEM_PLOTTER self )
{
	if( DUMMY_PRIV(self)->size_buf_packed > CONV_KEEP_LIMIT ) {
		/* free the mem if it was an large allocation ... */
		DUMMY_PRIV(self)->buf_packed = realloc( DUMMY_PRIV(self)->buf_packed, CONV_KEEP_LIMIT );
		DUMMY_PRIV(self)->size_buf_packed = CONV_KEEP_LIMIT;
	}
	snapshot_suspend( self );
}


static int bitmap( GEM_PLOTTER self, struct bitmap * bmp, int x, int y,
					unsigned long bg, unsigned long flags )
{
	MFDB src_mf;
	MFDB scrmf;
	short pxy[8];
	GRECT off, clip, loc, vis;

	src_mf.fd_addr = NULL;
	scrmf.fd_addr = NULL;

	off.g_x = x;
	off.g_y = y;
	off.g_h = bmp->height;
	off.g_w = bmp->width;

	clip.g_x = self->clipping.x0;
	clip.g_y = self->clipping.y0;
	clip.g_w = self->clipping.x1 - self->clipping.x0;
	clip.g_h = self->clipping.y1 - self->clipping.y0;

	if( !rc_intersect( &clip, &off) ) {
		return( true );
	}

	plotter_get_visible_grect( self, &vis );
	if( !rc_intersect( &vis, &off) ) {
		return( true );
	}

	loc = off;
	off.g_x = MAX(0, off.g_x - x);
	off.g_y = MAX(0, off.g_y - y);
	loc.g_x = MAX(0, loc.g_x);
	loc.g_y = MAX(0, loc.g_y);

	pxy[0] = 0;
	pxy[1] = 0;
	pxy[2] = off.g_w-1;
	pxy[3] = off.g_h-1;
	pxy[4] = CURFB(self).x + loc.g_x;
	pxy[5] = CURFB(self).y + loc.g_y;
	pxy[6] = CURFB(self).x + loc.g_x + off.g_w-1;
	pxy[7] = CURFB(self).y + loc.g_y + off.g_h-1;
	/* Convert the Bitmap to native screen format - ready for output*/
	/* This includes blending transparent pixels */
	if( self->bitmap_convert( self, bmp, pxy[4], pxy[5], &off, bg, flags, &src_mf) != 0 ) {
		return( true );
	}
	vro_cpyfm( self->vdi_handle, S_ONLY, (short*)&pxy, &src_mf,  &scrmf);
	convert_bitmap_done( self );
	return( true );
}

static int plot_mfdb (GEM_PLOTTER self, GRECT * loc, MFDB * insrc, unsigned char fgcolor, uint32_t flags)
{

	MFDB screen, tran;
	MFDB * src;
	short pxy[8];
	short c[2] = {fgcolor, WHITE};
	GRECT off;

	plotter_get_clip_grect( self, &off );
	if( rc_intersect(loc, &off) == 0 ){
		return( 1 );
	}

	init_mfdb( 0, loc->g_w, loc->g_h, 0, &screen );

	if( insrc->fd_stand ){
		int size = init_mfdb( insrc->fd_nplanes, loc->g_w, loc->g_h,
			MFDB_FLAG_NOALLOC,
			&tran
		);
		if( DUMMY_PRIV(self)->size_buf_scr == 0 ){
			DUMMY_PRIV(self)->buf_scr.fd_addr = malloc( size );
			DUMMY_PRIV(self)->size_buf_scr = size;
		} else {
			if( size > DUMMY_PRIV(self)->size_buf_scr ) {
				DUMMY_PRIV(self)->buf_scr.fd_addr = realloc(
					DUMMY_PRIV(self)->buf_scr.fd_addr, size
				);
				DUMMY_PRIV(self)->size_buf_scr = size;
			}
		}
		tran.fd_addr = DUMMY_PRIV(self)->buf_scr.fd_addr;
		vr_trnfm( self->vdi_handle, insrc, &tran );
		src = &tran;
	} else {
		src = insrc;
	}

	pxy[0] = off.g_x - loc->g_x;
	pxy[1] = off.g_y - loc->g_y;
	pxy[2] = pxy[0] + off.g_w - 1;
	pxy[3] = pxy[1] + off.g_h - 1;
	pxy[4] = CURFB(self).x + off.g_x;
	pxy[5] = CURFB(self).y + off.g_y;
	pxy[6] = pxy[4] + off.g_w-1;
	pxy[7] = pxy[5] + off.g_h-1;


	if( flags & PLOT_FLAG_TRANS && src->fd_nplanes == 1){
		vrt_cpyfm( self->vdi_handle, MD_TRANS, (short*)pxy, src, &screen, (short*)&c );
	} else {
		/* this method only plots transparent bitmaps, right now... */
	}
	return( 1 );
}

static int text(GEM_PLOTTER self, int x, int y, const char *text, size_t length, const plot_font_style_t *fstyle)
{
	self->font_plotter->text( self->font_plotter, x, y,
		text, length, fstyle
	);
	return ( 1 );
}

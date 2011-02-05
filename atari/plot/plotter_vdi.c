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
#include "atari/plot/font_vdi.h"

/* assign vdi line style to dst ( netsurf type ) */ 
#define NSLT2VDI(dst, src) \
	switch( src->stroke_type ) {\
		case PLOT_OP_TYPE_DOT: \
			dst = 3;	\
		break;\
		case PLOT_OP_TYPE_DASH:\
			dst = 5;	\
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
static void * lock( GEM_PLOTTER self );
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
static int bitmap( GEM_PLOTTER self, struct bitmap * bmp, int x, int y,
					unsigned long bg, unsigned long flags );
static int text(GEM_PLOTTER self, int x, int y, const char *text,size_t length, const plot_font_style_t *fstyle);

static unsigned short sys_pal[256][3]; /*RGB*/
static unsigned short pal[256][3];     /*RGB*/

extern unsigned char rgb_web_pal[126][3];
extern unsigned short vdi_web_pal[126][3];
extern struct s_vdi_sysinfo vdi_sysinfo;

static HermesHandle hermes_pal_h; /* hermes palette handle */
static HermesHandle hermes_cnv_h; /* hermes converter instance handle */
static HermesHandle hermes_res_h;
int32 * hermes_pal_p;

/*
static inline void vs_rgbcolor(short vdih, uint32_t cin ) 
{
	unsigned short c[4];
	rgb_to_vdi1000( &cin, &c );	
	vs_color( vdih, OFFSET_CUST_PAL, &c[0] );
}
*/

static inline void vsl_rgbcolor( short vdih, uint32_t cin )
{
	if( vdi_sysinfo.scr_bpp > 8 ) {
		unsigned short c[4];
		rgb_to_vdi1000( (unsigned char*)&cin, (unsigned short*)&c );	
		vs_color( vdih, OFFSET_CUSTOM_COLOR, (unsigned short*)&c[0] );
		vsl_color( vdih, OFFSET_CUSTOM_COLOR );
	} else {
		if( vdi_sysinfo.scr_bpp >= 4 )
			vsl_color( vdih, RGB_TO_VDI(cin) );
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
		if( vdi_sysinfo.scr_bpp >= 4 )
			vsf_color( vdih, RGB_TO_VDI(cin) );
		else
			vsf_color( vdih, WHITE );
	}
}

int ctor_plotter_vdi(GEM_PLOTTER self )
{
	int retval = 0;
	int i;
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
	self->clip( self, 0, 0, FIRSTFB(self).w, FIRSTFB(self).h );
	/* store system palette & setup the new (web) palette: */
	i = 0;
	if( app.nplanes <= 8 ){
		for( i=0; i<=255; i++ ) {
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
				}
				if( i >= OFFSET_CUST_PAL ) {
					/* here we could define 22 additional colors... */ 
					/* rgb_to_vdi1000( &rgb_font_pal[i-OFFSET_FONT_PAL], &pal[i] ); */
				}
				vs_color( self->vdi_handle, i, &pal[i][0] );
			}
		}
	} else {
		/* no need to change the palette - its application specific */
	}

	unsigned char * col;
	assert( Hermes_Init() );
	hermes_pal_h = Hermes_PaletteInstance();
	hermes_pal_p = Hermes_PaletteGet(hermes_pal_h);
	assert(hermes_pal_p);

	for( i = 0; i<OFFSET_CUST_PAL; i++) {
		col = (unsigned char *)(hermes_pal_p+i);
		if( i < OFFSET_WEB_PAL ) {
			col[0] = sys_pal[i][0];
			col[1] = sys_pal[i][1];
			col[2] = sys_pal[i][2];
		}
		if( i >= OFFSET_WEB_PAL ) {
			col[0] = rgb_web_pal[i-OFFSET_WEB_PAL][0];
			col[1] = rgb_web_pal[i-OFFSET_WEB_PAL][1];
			col[2] = rgb_web_pal[i-OFFSET_WEB_PAL][2];
		}
		/* font colors missing */
		col[3] = 0;
	}
	Hermes_PaletteInvalidateCache(hermes_pal_h);

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
	DUMMY_PRIV(self)->vfmt.indexed = false;
	DUMMY_PRIV(self)->vfmt.has_colorkey = false;

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

	if( app.nplanes <= 8 ){
		/* restore system palette */
		for( i=0; i<=255; i++ ) {
			vs_color( self->vdi_handle, i, &sys_pal[i][0] );
		}
	}

	/* close Hermes stuff: */
	Hermes_ConverterReturn( hermes_cnv_h );
	Hermes_PaletteReturn( hermes_pal_h );
	Hermes_Done();

	if( self->priv_data != NULL ){
		if( DUMMY_PRIV(self)->buf_packed )
			free( DUMMY_PRIV(self)->buf_packed );
		if( DUMMY_PRIV(self)->buf_planar )
			free( DUMMY_PRIV(self)->buf_planar );
		free( self->priv_data );
	}
	return( 1 );
}

static int resize( GEM_PLOTTER self, int w, int h )
{
	if( w == CURFB(self).w && h == CURFB(self).h )
		return( 1 );
	int newsize = calc_chunked_buffer_size( w, h, w, self->bpp_virt );
	LOG(("%s: %s, oldsize: %d\n", (char*)__FILE__, __FUNCTION__, CURFB(self).size ));
	/* todo: needed when using offscreen buffers...
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

   /*
      for offscreen plotters:
      copy current contents to new pos?
      we could also copy content of our own screen buffer,
      but only when it's unlocked
		...nono, the user must do this manually. Because window move will already be handled by the OS
   */
   /*update_screen( self );*/
   return( 1 );
}


static void * lock( GEM_PLOTTER self )
{
	LOG(("%s: %s\n", (char*)__FILE__, __FUNCTION__));
	self->flags |= PLOT_FLAG_LOCKED;
	wind_update(BEG_UPDATE);
	wind_update(BEG_MCTRL);
	graf_mouse(M_OFF, NULL);
	return( NULL );
}

static int unlock( GEM_PLOTTER self )
{
	LOG(("%s: %s\n", (char*)__FILE__, __FUNCTION__));
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
	visible.g_x = CURFB(self).vis_x;
	visible.g_y = CURFB(self).vis_y;
	visible.g_w = CURFB(self).vis_w;
	visible.g_h = CURFB(self).vis_h;

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

static int copy_rect( GEM_PLOTTER self, GRECT src, GRECT dst )
{
	MFDB devmf;
	MFDB scrmf;
	short pxy[8];
	GRECT vis;

	/* clip to visible rect, only needed for onscreen renderer: */
	vis.g_x = CURFB(self).vis_x;
	vis.g_y = CURFB(self).vis_y;
	vis.g_w = CURFB(self).vis_w;
	vis.g_h = CURFB(self).vis_h;

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
	short lt;
	int sw = pstyle->stroke_width;

	pxy[0] = CURFB(self).x + x0;
	pxy[1] = CURFB(self).y + y0;
	pxy[2] = CURFB(self).x + x1;
	pxy[3] = CURFB(self).y + y1;

	plotter_vdi_clip( self, 1);
	if( sw == 0)
		sw = 1;
	NSLT2VDI(lt, pstyle)
	vsl_type( self->vdi_handle, lt );
	vsl_width( self->vdi_handle, (short)sw );
	vsl_rgbcolor( self->vdi_handle, pstyle->stroke_colour );
	v_pline(self->vdi_handle, 2, (short *)&pxy );
	plotter_vdi_clip( self, 0);
   return ( 1 );
}

static int rectangle(GEM_PLOTTER self,int x0, int y0, int x1, int y1,  const plot_style_t * pstyle)
{
   short pxy[10];

	GRECT r, rclip, sclip;

	rclip.g_x = self->clipping.x0;
	rclip.g_y = self->clipping.y0;
	rclip.g_w = self->clipping.x1 - self->clipping.x0;
	rclip.g_h = self->clipping.y1 - self->clipping.y0;

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
	vsf_rgbcolor( self->vdi_handle, pstyle->fill_colour );
	vsf_perimeter( self->vdi_handle, 0);
	vsf_interior( self->vdi_handle, FIS_SOLID );

	pxy[0] = CURFB(self).x + r.g_x;
	pxy[1] = CURFB(self).y + r.g_y;
	pxy[2] = CURFB(self).x + r.g_x + r.g_w -1;
	pxy[3] = CURFB(self).y + r.g_y + r.g_h -1;

	vsf_style( self->vdi_handle, 1);
	v_bar( self->vdi_handle, (short*)&pxy );
	return ( 1 );
}

static int polygon(GEM_PLOTTER self,const int *p, unsigned int n,  const plot_style_t * pstyle)
{
	short pxy[n*2];
	unsigned int i=0;
	short d[4];
	if( vdi_sysinfo.maxpolycoords > 0 )
		assert( n < vdi_sysinfo.maxpolycoords );
/*
	Does this double check make sense? 
	else 
		assert( n < vdi_sysinfo.maxintin ); 
*/
	/* test this:  */
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

    rb = ((pixel & 0xFF00FF00UL) * opacity +
          (scrpixel & 0xFF00FF00UL) * transp) >> 8;
    g  = ((pixel & 0x00FF0000UL) * opacity +
          (scrpixel & 0x00FF0000UL) * transp) >> 8;

    return (rb & 0xFF00FF00) | (g & 0x00FF0000);
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
			return( 0 );
		}
	}

	/* allocate the mem for resized bitmap */
	img->resized = bitmap_create_ex( nw, nh, bpp, nw*bpp, 0, NULL );
	if( img->resized == NULL ) {
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

/*
	fuellt ein mfdb, wenn bpp==null wird angenommen das ein MFDB fr 
    den Bildschirm initialisiert werden soll, der Speicher fuer das Bild
	wird daher nicht alloziert ( fd_addr == 0 )
*/
static int init_mfdb(int bpp, int w, int h, MFDB * out )
{
	int pxsize = bpp >> 3;
	int dststride;
	dststride = MFDB_STRIDE( w );
	if( bpp > 0 ) { 
		out->fd_addr = malloc( dststride * pxsize * h );
		out->fd_stand = 0;
		out->fd_nplanes = (short)bpp;
		out->fd_r1 = out->fd_r2 = out->fd_r3 = 0;
	} else {
		memset( out, 0, sizeof(MFDB) );
	}
	out->fd_w = dststride;
	out->fd_h = h;
	out->fd_wdwidth = dststride >> 4;
	return( 1 );
}

/*
* Capture the screen at x,y location
* param self instance
* param x absolute screen coords
* param y absolute screen coords
* param w width
* param h height
*/
static struct bitmap * snapshot_create(GEM_PLOTTER self, int x, int y, int w, int h)
{
	MFDB scr;
	short pxy[8];
	int err;

	/* make sure the screen format is pixel packed... */
	/* no method to convert planar screen to pixel packed ... right now */
	assert( vdi_sysinfo.vdiformat == VDI_FORMAT_PACK  );	

	{
		int pxsize =  vdi_sysinfo.scr_bpp >> 3;
		int scr_stride = MFDB_STRIDE( w );
		int scr_size = scr_stride * pxsize * h;
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

	init_mfdb( 0, w, h, &scr );
	pxy[0] = x;
	pxy[1] = y;
	pxy[2] = pxy[0] + w-1;
	pxy[3] = pxy[1] + h-1;
	pxy[4] = 0;
	pxy[5] = 0;
	pxy[6] = pxy[2];
	pxy[7] = pxy[3];
	vro_cpyfm( self->vdi_handle, S_ONLY, (short*)&pxy, 
				&scr,  &DUMMY_PRIV(self)->buf_scr 
	);

	/* convert screen buffer to ns format: */
	if( DUMMY_PRIV(self)->buf_scr_compat == NULL ) {
		DUMMY_PRIV(self)->buf_scr_compat = bitmap_create(w, h, 0);
	} else {
		DUMMY_PRIV(self)->buf_scr_compat = bitmap_realloc( w, h, 
			DUMMY_PRIV(self)->buf_scr_compat->bpp, 
			w * DUMMY_PRIV(self)->buf_scr_compat->bpp, 
			BITMAP_GROW, 
			DUMMY_PRIV(self)->buf_scr_compat );	
	}
	err = Hermes_ConverterRequest( hermes_cnv_h, 
			&DUMMY_PRIV(self)->vfmt, 
			&DUMMY_PRIV(self)->nsfmt
	);
	assert( err != 0 );
	err = Hermes_ConverterCopy( hermes_cnv_h,
		DUMMY_PRIV(self)->buf_scr.fd_addr,
		0,			/* x src coord of top left in pixel coords */
		0,			/* y src coord of top left in pixel coords */
		w, h,
		DUMMY_PRIV(self)->buf_scr.fd_w * vdi_sysinfo.pixelsize, /* stride as bytes */
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

	if( DUMMY_PRIV(self)->buf_scr_compat ) {
		bitmap_destroy( DUMMY_PRIV(self)->buf_scr_compat );
		DUMMY_PRIV(self)->buf_scr_compat = NULL;
	}
} 

/* convert bitmap to framebuffer format */
static int convert_bitmap( GEM_PLOTTER self,
	struct bitmap * img,
	int x,
	int y,
	GRECT * clip,
	uint32_t bg,
	MFDB *out  )
{
	short vpxsize = self->bpp_virt >> 3; /* / 8 */
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
		&& ((self->flags & PLOT_FLAG_TRANS) != 0) 
		&& (vdi_sysinfo.vdiformat == VDI_FORMAT_PACK )  ) {
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
					if( (imgpixel[img_x] & 0xFF) != 0xFF ) {
						if( (imgpixel[img_x] & 0x0FF) != 0 ) {
							screenpixel[screen_x] = ablend( imgpixel[img_x], screenpixel[screen_x]);
						} 
					} else {
						screenpixel[screen_x] = imgpixel[img_x];
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
	dstsize = dststride * vpxsize * clip->g_h;
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
		dststride * vpxsize /* stride as bytes */
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

	vis.g_x = CURFB(self).vis_x;
	vis.g_y = CURFB(self).vis_y;
	vis.g_w = CURFB(self).vis_w;
	vis.g_h = CURFB(self).vis_h;
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
	if( convert_bitmap( self, bmp, pxy[4], pxy[5], &off, bg, &src_mf) != 0 ) {
		return( true ); 
	}
	vro_cpyfm( self->vdi_handle, S_ONLY, (short*)&pxy, &src_mf,  &scrmf);
	convert_bitmap_done( self );
	return( true );
}

static int text(GEM_PLOTTER self, int x, int y, const char *text, size_t length, const plot_font_style_t *fstyle)
{
	self->font_plotter->text( self->font_plotter,
		x,
		y,
		text, length,
		fstyle
	);
	return ( 1 );
}

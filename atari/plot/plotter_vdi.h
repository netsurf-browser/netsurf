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
#ifndef GEM_PLOTTER_DUMMY_H_INCLUDED
#define GEM_PLOTTER_DUMMY_H_INCLUDED
#include "plotter.h"
#include <Hermes/Hermes.h>

struct s_vdi_priv_data {
	short bufops;
	void * buf_packed;		/* temp buffer for bitmap conversion */
	int size_buf_packed;	
	void * buf_planar;		/* temp buffer for bitmap conversion */
	int size_buf_planar;
	MFDB buf_scr;					/* buffer for native screen capture  */
	int size_buf_scr;  
	struct bitmap * buf_scr_compat;
	HermesFormat vfmt;		/* framebuffer format */
												/* no screen format here, hermes may not suitable for it */
	HermesFormat nsfmt;		/* netsurf bitmap format */
}; 

#define CONV_KEEP_LIMIT 512000	/* how much memory should be kept allocated for temp. conversion bitmaps? */
#define CONV_BLOCK_SIZE	32000	/* how much memory to allocate if some is needed */
#define DUMMY_PRIV(self) ((struct s_vdi_priv_data*)self->priv_data)

/* Each driver must export 1 method to create the plotter object: */
int ctor_plotter_vdi( GEM_PLOTTER p );

static struct bitmap * snapshot_create(GEM_PLOTTER self, int x, int y, int w, int h);
static void snapshot_suspend(GEM_PLOTTER self );
static void snapshot_destroy( GEM_PLOTTER self );

#endif

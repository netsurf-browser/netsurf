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

struct s_view
{
	short x;			/* drawing (screen) offset x 						*/
	short y;			/* drawing (screen) offset y						*/
	short w;			/* width of buffer, not in sync with vis_w			*/
	short h;			/* height of buffer, not in sync with vis_w			*/
	short vis_x;		/* visible rectangle of the screen buffer 			*/
	short vis_y;		/* coords are relative to plot location 			*/
	short vis_w;		/* clipped to screen dimensions 					*/
	short vis_h;		/*  												*/
	struct rect clipping;
	int size;
	bool swapped;
	void * mem;
};

struct s_vdi_priv_data {
	short bufops;
	/* temp buffer for bitmap conversion: */
	void * buf_packed;
	int size_buf_packed;

	/* temp buffer for bitmap conversion: */
	void * buf_planar;
	int size_buf_planar;

	/* buffer for plot operations that require device format, */
	/* currently used for transparent mfdb blits and snapshots: */
	MFDB buf_scr;
	int size_buf_scr;

	/* buffer for std form, used during 8bpp snapshot */
	MFDB buf_std;
	int size_buf_std;

	struct bitmap * buf_scr_compat;

	/* intermediate bitmap format */
	HermesFormat vfmt;

	/* no screen format here, hermes may not suitable for it */

	/* netsurf source bitmap format */
	HermesFormat nsfmt;

	/* Some internal structure used everywhere 								*/
	/* trying to describe screen position, plotting origins and maxmimum 	*/
	/* extent. 																*/
	struct s_view  view;
};

/* how much memory should be kept allocated for temp. conversion bitmaps: */
#define CONV_KEEP_LIMIT 512000
/* how much memory to allocate if some is needed: */
#define CONV_BLOCK_SIZE	32000

/* this is an shortcut cast to access the members of the s_vdi_priv_data */
#define DUMMY_PRIV(_self) ((struct s_vdi_priv_data*)_self->priv_data)

#define VIEW( priv ) DUMMY_PRIV( priv )->view

/* Each driver object must export 1 it's own constructor: */
int ctor_plotter_vdi( GEM_PLOTTER p, GRECT * loc_size );

/*
* Capture the screen at x,y location
* param self instance
* param x absolute screen coords
* param y absolute screen coords
* param w width
* param h height
*
* This creates an snapshot in RGBA format (NetSurf's native format)
*
*/
static struct bitmap * snapshot_create(GEM_PLOTTER self, int x, int y, int w, int h);

/* Garbage collection of the snapshot routine */
/* this should be called after you are done with the data returned by snapshot_create */
/* don't access the screenshot after you called this function */
static void snapshot_suspend(GEM_PLOTTER self );

/* destroy memory used by screenshot */
static void snapshot_destroy( GEM_PLOTTER self );

#endif

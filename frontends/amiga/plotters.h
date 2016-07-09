/*
 * Copyright 2008, 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_PLOTTERS_H
#define AMIGA_PLOTTERS_H

#include "netsurf/plotters.h"
#include <proto/layers.h>
#include <proto/graphics.h>

struct IBox;

struct gui_globals
{
	struct BitMap *bm;
	struct RastPort *rp;
	struct Layer_Info *layerinfo;
	APTR areabuf;
	APTR tmprasbuf;
	struct Rectangle rect;
	struct MinList *shared_pens;
	bool palette_mapped;
	ULONG apen;
	ULONG open;
	LONG apen_num;
	LONG open_num;
	int width;  /* size of bm and    */
	int height; /* associated memory */
};

extern const struct plotter_table amiplot;

extern struct gui_globals *glob;

void ami_init_layers(struct gui_globals *gg, ULONG width, ULONG height, bool force32bit);
void ami_free_layers(struct gui_globals *gg);
void ami_clearclipreg(struct gui_globals *gg);
void ami_plot_clear_bbox(struct RastPort *rp, struct IBox *bbox);
void ami_plot_release_pens(struct MinList *shared_pens);
bool ami_plot_screen_is_palettemapped(void);

#endif

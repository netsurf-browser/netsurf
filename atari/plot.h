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

#ifndef NS_ATARI_PLOT_H
#define NS_ATARI_PLOT_H

#include "desktop/plotters.h"
#include "atari/plot/plotter.h"

struct rect;

extern const struct plotter_table atari_plotters;

int atari_plotter_init( char*, char * );
int atari_plotter_finalise( void );
void plot_set_knockout( int set );
bool plot_get_clip(struct rect * out);
bool plot_clip(const struct rect *clip);
bool plot_rectangle( int x0, int y0, int x1, int y1,const plot_style_t *style );
bool plot_line( int x0, int y0, int x1, int y1, const plot_style_t *style );

#endif

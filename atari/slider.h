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

#ifndef NS_SLIDER_H_INCLUDED
#define NS_SLIDER_H_INCLUDED

/* -------------------------------------------------------------------------- */
/* Slider Interface:                                                          */
/* -------------------------------------------------------------------------- */

#define BR_SCROLLBAR_SZ 16

/* Calculate how many pixel an specific page of the content contains */
#define SLIDER_PIXELS_ON_PAGE( long content_dim, long workarea_dim, long page )\
	min( workarea_dim, content_dim - page * workarea_dim)

/* Returns max internal slider pos, counting from zero */
#define SLIDER_MAX_POS(content_dim, workarea_dim) max( 0, content_dim - workarea_dim)

/* Calculate the number of content-"pages" for window size */
long inline slider_pages( long content_dim, long workarea_dim );
float inline slider_pages_dec( long content_dim, long workarea_dim )

/* Convert content size into gem slider size ( 0 - 1000 ) */
int inline slider_gem_size( long content_dim, long workarea_dim );

/* convert internal slider position to gem slider position */
int slider_pos_to_gem_pos( long content_dim, long workarea_dim, long slider_pos );

/* convert gem slider pos to "internal" pos system */
long slider_gem_pos_to_pos( long content_dim, long workarea_dim, int slider_pos );

/*  Calculate the gem slider mover size into pixel size */
long slider_gem_size_to_res( long workarea_dim, int gem_size );

/* Convert the gem Slider pos to an pixel value */
long slider_gem_pos_to_res( long content_dim, long workarea_dim, int gem_pos );




#endif 

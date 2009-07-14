/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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

/** \file
 * Ploter styles.
 */

#ifndef _NETSURF_DESKTOP_PLOT_STYLE_H_
#define _NETSURF_DESKTOP_PLOT_STYLE_H_

/* html widget colours */
#define WIDGET_BASEC 0xd9d9d9
#define WIDGET_BLOBC 0x000000

/* Darken a colour by taking three quaters of each channels intensity */
#define darken_colour(c1)				 		\
	((((3 * (c1 >> 16)) >> 2) << 16) |		 		\
	 (((3 * ((c1 >> 8) & 0xff)) >> 2) << 8) |	 		\
	 (((3 * (c1 & 0xff)) >> 2) << 0))

/* Darken a colour by taking nine sixteenths of each channels intensity */
#define double_darken_colour(c1)			 		\
	((((9 * (c1 >> 16)) >> 4) << 16) |		 		\
	 (((9 * ((c1 >> 8) & 0xff)) >> 4) << 8) |	 		\
	 (((9 * (c1 & 0xff)) >> 4) << 0))

/* Lighten a colour by taking three quaters of each channels intensity
 * and adding a full quater
 */
#define lighten_colour(c1)						\
	(((((3 * (c1 >> 16)) >> 2) + 64) << 16) |			\
	 ((((3 * ((c1 >> 8) & 0xff)) >> 2) + 64) << 8) |		\
	 ((((3 * (c1 & 0xff)) >> 2) + 64) << 0))

/* Lighten a colour by taking nine sixteenths of each channels intensity and
 * adding a full intensity 7/16ths */
#define double_lighten_colour(c1)					\
	(((((9 * (c1 >> 16)) >> 4) + 112) << 16) |			\
	 ((((9 * ((c1 >> 8) & 0xff)) >> 4) + 112) << 8) |		\
	 ((((9 * (c1 & 0xff)) >> 4) + 112) << 0))

/* Blend two colours by taking half the intensity of each channel in the first
 * colour and adding them to half the intensity of each channel in the second
 * colour */
#define blend_colour(c0, c1)						\
	 ((((c0 >> 16) + (c1 >> 16)) >> 1) << 16) |			\
	(((((c0 >> 8) & 0xff) + ((c1 >> 8) & 0xff)) >> 1) << 8) |	\
	 ((((c0 & 0xff) + (c1 & 0xff)) >> 1) << 0)

typedef enum {
    PLOT_OP_TYPE_NONE = 0, /**< No operation */
    PLOT_OP_TYPE_SOLID, /**< Solid colour */
    PLOT_OP_TYPE_DOT, /**< Doted plot */
    PLOT_OP_TYPE_DASH, /**< dashed plot */
} plot_operation_type_t;

typedef struct {
    plot_operation_type_t stroke_type;
    int stroke_width;
    colour stroke_colour;
    plot_operation_type_t fill_type;
    colour fill_colour;
} plot_style_t;

/* global fill styles */
extern plot_style_t *plot_style_fill_white;
extern plot_style_t *plot_style_fill_red;
extern plot_style_t *plot_style_fill_black;

/* Box model debug outline styles for content, padding and margin edges */
extern plot_style_t const * const plot_style_content_edge;
extern plot_style_t const * const plot_style_padding_edge;
extern plot_style_t const * const plot_style_margin_edge;

/* other styles */
extern plot_style_t *plot_style_caret;
extern plot_style_t *plot_style_stroke_history;
extern plot_style_t *plot_style_fill_wbasec;
extern plot_style_t *plot_style_fill_darkwbasec;
extern plot_style_t *plot_style_fill_lightwbasec;
extern plot_style_t *plot_style_fill_wblobc;
extern plot_style_t *plot_style_stroke_wblobc;
extern plot_style_t *plot_style_stroke_darkwbasec;
extern plot_style_t *plot_style_stroke_lightwbasec;

#endif

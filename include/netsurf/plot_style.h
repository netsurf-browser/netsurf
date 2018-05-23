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

/**
 * \file
 * plotter style interfaces, generic styles and style colour helpers.
 */

#ifndef NETSURF_PLOT_STYLE_H
#define NETSURF_PLOT_STYLE_H

#include <stdint.h>
#include <stdint.h>
#include <libwapcaplet/libwapcaplet.h>
#include "netsurf/types.h"

/** light grey widget base colour */
#define WIDGET_BASEC 0xd9d9d9

/** black blob colour */
#define WIDGET_BLOBC 0x000000

/** Transparent colour value. */
#define NS_TRANSPARENT 0x01000000

/** 22:10 fixed point */
#define PLOT_STYLE_RADIX (10)

/** Scaling factor for plot styles */
#define PLOT_STYLE_SCALE (1 << PLOT_STYLE_RADIX)

/* type for fixed point numbers */
typedef int32_t plot_style_fixed;

/* Convert an int to fixed point */
#define plot_style_int_to_fixed(v) ((v) << PLOT_STYLE_RADIX)

/* Convert fixed point to int */
#define plot_style_fixed_to_int(v) ((v) >> PLOT_STYLE_RADIX)

/* Convert fixed point to float */
#define plot_style_fixed_to_float(v) (((float)v) / PLOT_STYLE_SCALE)

/* Convert fixed point to double */
#define plot_style_fixed_to_double(v) (((double)v) / PLOT_STYLE_SCALE)

/**
 * Type of plot operation
 */
typedef enum {
	PLOT_OP_TYPE_NONE = 0, /**< No operation */
	PLOT_OP_TYPE_SOLID, /**< Solid colour */
	PLOT_OP_TYPE_DOT, /**< Dotted plot */
	PLOT_OP_TYPE_DASH, /**< Dashed plot */
} plot_operation_type_t;


/**
 * Plot style for stroke/fill plotters
 */
typedef struct plot_style_s {
	plot_operation_type_t stroke_type; /**< Stroke plot type */
	plot_style_fixed stroke_width; /**< Width of stroke, in pixels */
	colour stroke_colour; /**< Colour of stroke */
	plot_operation_type_t fill_type; /**< Fill plot type */
	colour fill_colour; /**< Colour of fill */
} plot_style_t;


/**
 * Generic font family type
 */
typedef enum {
	PLOT_FONT_FAMILY_SANS_SERIF = 0,
	PLOT_FONT_FAMILY_SERIF,
	PLOT_FONT_FAMILY_MONOSPACE,
	PLOT_FONT_FAMILY_CURSIVE,
	PLOT_FONT_FAMILY_FANTASY,
	PLOT_FONT_FAMILY_COUNT /**< Number of generic families */
} plot_font_generic_family_t;


/**
 * Font plot flags
 */
typedef enum {
	FONTF_NONE = 0,
	FONTF_ITALIC = 1,
	FONTF_OBLIQUE = 2,
	FONTF_SMALLCAPS = 4,
} plot_font_flags_t;

/**
 * Font style for plotting
 */
typedef struct plot_font_style {
	/**
	 * Array of pointers to font families.
	 *
	 * May be NULL.  Array is NULL terminated.
	 */
	lwc_string * const * families;
	plot_font_generic_family_t family; /**< Generic family to plot with */
	plot_style_fixed size; /**< Font size, in pt */
	int weight; /**< Font weight: value in range [100,900] as per CSS */
	plot_font_flags_t flags; /**< Font flags */
	colour background; /**< Background colour to blend to, if appropriate */
	colour foreground; /**< Colour of text */
} plot_font_style_t;


/* Darken a colour by taking three quarters of each channel's intensity */
#define darken_colour(c1)				 		\
	((((3 * (c1 & 0xff00ff)) >> 2) & 0xff00ff) |			\
	 (((3 * (c1 & 0x00ff00)) >> 2) & 0x00ff00))

/* Darken a colour by taking nine sixteenths of each channel's intensity */
#define double_darken_colour(c1)					\
	((((9 * (c1 & 0xff00ff)) >> 4) & 0xff00ff) |			\
	 (((9 * (c1 & 0x00ff00)) >> 4) & 0x00ff00))

/* Lighten a colour by taking 12/16ths of each channel's intensity
 * and adding a full 4/16ths intensity */
#define lighten_colour(c1)						\
	(((((3 * (c1 & 0xff00ff)) >> 2) + 0x400040) & 0xff00ff) |	\
	 ((((3 * (c1 & 0x00ff00)) >> 2) + 0x004000) & 0x00ff00))

/* Lighten a colour by taking 9/16ths of each channel's intensity
 * and adding a full 7/16ths intensity */
#define double_lighten_colour(c1)					\
	(((((9 * (c1 & 0xff00ff)) >> 4) + 0x700070) & 0xff00ff) |	\
	 ((((9 * (c1 & 0x00ff00)) >> 4) + 0x007000) & 0x00ff00))

/* Blend two colours by taking half the intensity of each channel in the first
 * colour and adding them to half the intensity of each channel in the second
 * colour */
#define blend_colour(c0, c1)						\
	(((((c0 & 0xff00ff) + (c1 & 0xff00ff)) >> 1) & 0xff00ff) |	\
	 ((((c0 & 0x00ff00) + (c1 & 0x00ff00)) >> 1) & 0x00ff00))

/* Choose either black or white, depending on which is furthest from the
 * percieved lightness of the supplied colour, c0. */
#define colour_to_bw_furthest(c0)					\
	((((((c0 & 0x0000ff) *  77) >>  8) +				\
	   (((c0 & 0x00ff00) * 151) >> 16) +				\
	   (((c0 & 0xff0000) *  28) >> 24)) >				\
	  (0xff / 2)) ? 0x000000 : 0xffffff)

/* Mix two colours according to the proportion given by p, where 0 <= p <= 255
 * p = 0 gives result ==> c1,  p = 255 gives result ==> c0 */
#define mix_colour(c0, c1, p)						\
	((((((c1 & 0xff00ff) * (255 - p)) +				\
	    ((c0 & 0xff00ff) * (      p))   ) >> 8) & 0xff00ff) |	\
	 (((((c1 & 0x00ff00) * (255 - p)) +				\
	    ((c0 & 0x00ff00) * (      p))   ) >> 8) & 0x00ff00))

/* get a bitmap pixel (image/bitmap.h) into a plot colour */
#define pixel_to_colour(b)						\
	b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)

/* Get the red channel from a colour */
#define red_from_colour(c)						\
	((c      ) & 0xff)

/* Get the green channel from a colour */
#define green_from_colour(c)						\
	((c >>  8) & 0xff)

/* Get the blue channel from a colour */
#define blue_from_colour(c)						\
	((c >> 16) & 0xff)


/* global fill styles */
extern plot_style_t *plot_style_fill_white;
extern plot_style_t *plot_style_fill_red;
extern plot_style_t *plot_style_fill_black;


/* Box model debug outline styles for content, padding and margin edges */
extern plot_style_t const * const plot_style_content_edge;
extern plot_style_t const * const plot_style_padding_edge;
extern plot_style_t const * const plot_style_margin_edge;


/* Broken object replacement styles */
extern plot_style_t const * const plot_style_broken_object;
extern plot_font_style_t const * const plot_fstyle_broken_object;


/* other styles */
extern plot_style_t *plot_style_caret;
extern plot_style_t *plot_style_fill_wbasec;
extern plot_style_t *plot_style_fill_darkwbasec;
extern plot_style_t *plot_style_fill_lightwbasec;
extern plot_style_t *plot_style_fill_wblobc;
extern plot_style_t *plot_style_stroke_wblobc;
extern plot_style_t *plot_style_stroke_darkwbasec;
extern plot_style_t *plot_style_stroke_lightwbasec;


/* Default font style */
extern plot_font_style_t const * const plot_style_font;


#endif

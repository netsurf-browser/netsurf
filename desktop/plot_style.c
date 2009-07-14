/*
 * Copyright 2009 Vincent Sanders <vince@kyllikki.org>
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

/** \file Plotter global styles.
 *
 * These plot styles are globaly available and used in many places. 
 */

#include "desktop/plotters.h"

static plot_style_t plot_style_fill_white_static = {
	.fill_type = PLOT_OP_TYPE_SOLID,
	.fill_colour = 0xffffff,
};
plot_style_t *plot_style_fill_white = &plot_style_fill_white_static;

static plot_style_t plot_style_fill_black_static = {
	.fill_type = PLOT_OP_TYPE_SOLID,
	.fill_colour = 0x0,
};
plot_style_t *plot_style_fill_black = &plot_style_fill_black_static;

static plot_style_t plot_style_fill_red_static = {
	.fill_type = PLOT_OP_TYPE_SOLID,
	.fill_colour = 0x000000ff,
};
plot_style_t *plot_style_fill_red = &plot_style_fill_red_static;

static plot_style_t plot_style_stroke_red_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = 0x000000ff,
	.stroke_width = 1,
};
plot_style_t *plot_style_stroke_red = &plot_style_stroke_red_static;

static plot_style_t plot_style_stroke_blue_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = 0x00ff0000,
	.stroke_width = 1,
};
plot_style_t *plot_style_stroke_blue = &plot_style_stroke_blue_static;

static plot_style_t plot_style_stroke_yellow_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = 0x0000ffff,
	.stroke_width = 1,
};
plot_style_t *plot_style_stroke_yellow = &plot_style_stroke_yellow_static;

/* caret style used in html_redraw_caret */
static plot_style_t plot_style_caret_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = 0x808080,  /* todo - choose a proper colour */
};
plot_style_t *plot_style_caret = &plot_style_caret_static;



/* html redraw widget styles */

/** plot style for filled widget base colour. */
static plot_style_t plot_style_fill_wbasec_static = {
	.fill_type = PLOT_OP_TYPE_SOLID,
	.fill_colour = WIDGET_BASEC,
};
plot_style_t *plot_style_fill_wbasec = &plot_style_fill_wbasec_static;

/** plot style for dark filled widget base colour . */
static plot_style_t plot_style_fill_darkwbasec_static = {
	.fill_type = PLOT_OP_TYPE_SOLID,
	.fill_colour = double_darken_colour(WIDGET_BASEC),
};
plot_style_t *plot_style_fill_darkwbasec = &plot_style_fill_darkwbasec_static;

/** plot style for light filled widget base colour. */
static plot_style_t plot_style_fill_lightwbasec_static = {
	.fill_type = PLOT_OP_TYPE_SOLID,
	.fill_colour = double_lighten_colour(WIDGET_BASEC),
};
plot_style_t *plot_style_fill_lightwbasec = &plot_style_fill_lightwbasec_static;


/** plot style for widget background. */
static plot_style_t plot_style_fill_wblobc_static = {
	.fill_type = PLOT_OP_TYPE_SOLID,
	.fill_colour = WIDGET_BLOBC,
};
plot_style_t *plot_style_fill_wblobc = &plot_style_fill_wblobc_static;

/** plot style for checkbox cross. */
static plot_style_t plot_style_stroke_wblobc_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = WIDGET_BLOBC,
	.stroke_width = 2,
};
plot_style_t *plot_style_stroke_wblobc = &plot_style_stroke_wblobc_static;

/** stroke style for widget double dark colour. */
static plot_style_t plot_style_stroke_darkwbasec_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = double_darken_colour(WIDGET_BASEC),
};
plot_style_t *plot_style_stroke_darkwbasec = &plot_style_stroke_darkwbasec_static;

/** stroke style for widget double light colour. */
static plot_style_t plot_style_stroke_lightwbasec_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = double_lighten_colour(WIDGET_BASEC),
};
plot_style_t *plot_style_stroke_lightwbasec = &plot_style_stroke_lightwbasec_static;

/* history styles */

/** stroke style for history core. */
static plot_style_t plot_style_stroke_history_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = 0x333333,
};
plot_style_t *plot_style_stroke_history = &plot_style_stroke_history_static;


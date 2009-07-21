/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
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

#include "css/css.h"
#include "render/font.h"

static plot_font_generic_family_t plot_font_generic_family(
		css_font_family css);
static int plot_font_weight(css_font_weight css);
static plot_font_flags_t plot_font_flags(css_font_style style,
		css_font_variant variant);

/**
 * Populate a font style using data from a computed CSS style
 *
 * \param css     Computed style to consider
 * \param fstyle  Font style to populate
 */
void font_plot_style_from_css(const struct css_style *css,
		plot_font_style_t *fstyle)
{
	fstyle->family = plot_font_generic_family(css->font_family);
	fstyle->size = 
		css_len2pt(&css->font_size.value.length, css) * FONT_SIZE_SCALE;
	fstyle->weight = plot_font_weight(css->font_weight);
	fstyle->flags = plot_font_flags(css->font_style, css->font_variant);
	fstyle->foreground = css->color;
	fstyle->background = 0;
}

/******************************************************************************
 * Helper functions                                                           *
 ******************************************************************************/

/**
 * Map a generic CSS font family to a generic plot font family
 *
 * \param css  Generic CSS font family
 * \return Plot font family
 */
plot_font_generic_family_t plot_font_generic_family(
		css_font_family css)
{
	plot_font_generic_family_t plot;

	switch (css) {
	case CSS_FONT_FAMILY_SERIF:
		plot = PLOT_FONT_FAMILY_SERIF;
		break;
	case CSS_FONT_FAMILY_MONOSPACE:
		plot = PLOT_FONT_FAMILY_MONOSPACE;
		break;
	case CSS_FONT_FAMILY_CURSIVE:
		plot = PLOT_FONT_FAMILY_CURSIVE;
		break;
	case CSS_FONT_FAMILY_FANTASY:
		plot = PLOT_FONT_FAMILY_FANTASY;
		break;
	case CSS_FONT_FAMILY_SANS_SERIF:
	default:
		plot = PLOT_FONT_FAMILY_SANS_SERIF;
		break;
	}

	return plot;
}

/**
 * Map a CSS font weight to a plot weight value
 *
 * \param css  CSS font weight
 * \return Plot weight
 */
int plot_font_weight(css_font_weight css)
{
	int weight;

	switch (css) {
	case CSS_FONT_WEIGHT_100:
		weight = 100;
		break;
	case CSS_FONT_WEIGHT_200:
		weight = 200;
		break;
	case CSS_FONT_WEIGHT_300:
		weight = 300;
		break;
	case CSS_FONT_WEIGHT_400:
	case CSS_FONT_WEIGHT_NORMAL:
	default:
		weight = 400;
		break;
	case CSS_FONT_WEIGHT_500:
		weight = 500;
		break;
	case CSS_FONT_WEIGHT_600:
		weight = 600;
		break;
	case CSS_FONT_WEIGHT_700:
	case CSS_FONT_WEIGHT_BOLD:
		weight = 700;
		break;
	case CSS_FONT_WEIGHT_800:
		weight = 800;
		break;
	case CSS_FONT_WEIGHT_900:
		weight = 900;
		break;
	}

	return weight;
}

/**
 * Map a CSS font style and font variant to plot font flags
 * 
 * \param style    CSS font style
 * \param variant  CSS font variant
 * \return Computed plot flags
 */
plot_font_flags_t plot_font_flags(css_font_style style,
		css_font_variant variant)
{
	plot_font_flags_t flags = FONTF_NONE;

	if (style == CSS_FONT_STYLE_ITALIC)
		flags |= FONTF_ITALIC;
	else if (style == CSS_FONT_STYLE_OBLIQUE)
		flags |= FONTF_OBLIQUE;

	if (variant == CSS_FONT_VARIANT_SMALL_CAPS)
		flags |= FONTF_SMALLCAPS;

	return flags;
}


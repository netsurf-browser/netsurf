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

#ifndef NETSURF_CSS_UTILS_H_
#define NETSURF_CSS_UTILS_H_

#include <libcss/libcss.h>

#include "netsurf/css.h"

/** DPI of the screen, in fixed point units */
extern css_fixed nscss_screen_dpi;

/**
 * Convert an absolute CSS length to points.
 *
 * \param[in] length Absolute CSS length.
 * \param[in] unit Unit of the length.
 * \return length in points
 */
css_fixed nscss_len2pt(css_fixed length, css_unit unit);

/**
 * Convert a CSS length to pixels.
 *
 * \param length Length to convert
 * \param unit Corresponding unit
 * \param style Computed style applying to length. May be NULL if unit is
 *                 neither em nor ex
 * \return length in pixels
 */
css_fixed nscss_len2px(css_fixed length, css_unit unit, const css_computed_style *style);

#endif

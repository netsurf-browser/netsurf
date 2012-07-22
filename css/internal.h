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

#ifndef NETSURF_CSS_INTERNAL_H_
#define NETSURF_CSS_INTERNAL_H_

#include "css/css.h"

css_error nscss_resolve_url(void *pw, const char *base, 
		lwc_string *rel, lwc_string **abs);

/* lwc_string strings */
extern lwc_string *nscss_lwc_string_a;
extern lwc_string *nscss_lwc_string_abscenter;
extern lwc_string *nscss_lwc_string_absmiddle;
extern lwc_string *nscss_lwc_string_align;
extern lwc_string *nscss_lwc_string_applet;
extern lwc_string *nscss_lwc_string_baseline;
extern lwc_string *nscss_lwc_string_body;
extern lwc_string *nscss_lwc_string_bottom;
extern lwc_string *nscss_lwc_string_caption;
extern lwc_string *nscss_lwc_string_center;
extern lwc_string *nscss_lwc_string_col;
extern lwc_string *nscss_lwc_string_div;
extern lwc_string *nscss_lwc_string_embed;
extern lwc_string *nscss_lwc_string_font;
extern lwc_string *nscss_lwc_string_h1;
extern lwc_string *nscss_lwc_string_h2;
extern lwc_string *nscss_lwc_string_h3;
extern lwc_string *nscss_lwc_string_h4;
extern lwc_string *nscss_lwc_string_h5;
extern lwc_string *nscss_lwc_string_h6;
extern lwc_string *nscss_lwc_string_hr;
extern lwc_string *nscss_lwc_string_iframe;
extern lwc_string *nscss_lwc_string_img;
extern lwc_string *nscss_lwc_string_input;
extern lwc_string *nscss_lwc_string_justify;
extern lwc_string *nscss_lwc_string_left;
extern lwc_string *nscss_lwc_string_middle;
extern lwc_string *nscss_lwc_string_object;
extern lwc_string *nscss_lwc_string_p;
extern lwc_string *nscss_lwc_string_password;
extern lwc_string *nscss_lwc_string_right;
extern lwc_string *nscss_lwc_string_table;
extern lwc_string *nscss_lwc_string_tbody;
extern lwc_string *nscss_lwc_string_td;
extern lwc_string *nscss_lwc_string_text;
extern lwc_string *nscss_lwc_string_textarea;
extern lwc_string *nscss_lwc_string_texttop;
extern lwc_string *nscss_lwc_string_tfoot;
extern lwc_string *nscss_lwc_string_th;
extern lwc_string *nscss_lwc_string_thead;
extern lwc_string *nscss_lwc_string_top;
extern lwc_string *nscss_lwc_string_tr;

/* dom_string strings */
extern struct dom_string *nscss_dom_string_align;
extern struct dom_string *nscss_dom_string_background;
extern struct dom_string *nscss_dom_string_bgcolor;
extern struct dom_string *nscss_dom_string_border;
extern struct dom_string *nscss_dom_string_bordercolor;
extern struct dom_string *nscss_dom_string_cellpadding;
extern struct dom_string *nscss_dom_string_cellspacing;
extern struct dom_string *nscss_dom_string_color;
extern struct dom_string *nscss_dom_string_cols;
extern struct dom_string *nscss_dom_string_height;
extern struct dom_string *nscss_dom_string_href;
extern struct dom_string *nscss_dom_string_hspace;
extern struct dom_string *nscss_dom_string_link;
extern struct dom_string *nscss_dom_string_rows;
extern struct dom_string *nscss_dom_string_size;
extern struct dom_string *nscss_dom_string_text;
extern struct dom_string *nscss_dom_string_type;
extern struct dom_string *nscss_dom_string_valign;
extern struct dom_string *nscss_dom_string_vlink;
extern struct dom_string *nscss_dom_string_vspace;
extern struct dom_string *nscss_dom_string_width;

#endif

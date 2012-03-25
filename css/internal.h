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

extern struct dom_string *nscss_dom_string_a;
extern struct dom_string *nscss_dom_string_abscenter;
extern struct dom_string *nscss_dom_string_absmiddle;
extern struct dom_string *nscss_dom_string_align;
extern struct dom_string *nscss_dom_string_applet;
extern struct dom_string *nscss_dom_string_background;
extern struct dom_string *nscss_dom_string_bgcolor;
extern struct dom_string *nscss_dom_string_body;
extern struct dom_string *nscss_dom_string_bordercolor;
extern struct dom_string *nscss_dom_string_bottom;
extern struct dom_string *nscss_dom_string_caption;
extern struct dom_string *nscss_dom_string_cellpadding;
extern struct dom_string *nscss_dom_string_cellspacing;
extern struct dom_string *nscss_dom_string_center;
extern struct dom_string *nscss_dom_string_color;
extern struct dom_string *nscss_dom_string_cols;
extern struct dom_string *nscss_dom_string_div;
extern struct dom_string *nscss_dom_string_embed;
extern struct dom_string *nscss_dom_string_font;
extern struct dom_string *nscss_dom_string_h1;
extern struct dom_string *nscss_dom_string_h2;
extern struct dom_string *nscss_dom_string_h3;
extern struct dom_string *nscss_dom_string_h4;
extern struct dom_string *nscss_dom_string_h5;
extern struct dom_string *nscss_dom_string_h6;
extern struct dom_string *nscss_dom_string_height;
extern struct dom_string *nscss_dom_string_hr;
extern struct dom_string *nscss_dom_string_href;
extern struct dom_string *nscss_dom_string_hspace;
extern struct dom_string *nscss_dom_string_iframe;
extern struct dom_string *nscss_dom_string_img;
extern struct dom_string *nscss_dom_string_input;
extern struct dom_string *nscss_dom_string_justify;
extern struct dom_string *nscss_dom_string_left;
extern struct dom_string *nscss_dom_string_link;
extern struct dom_string *nscss_dom_string_middle;
extern struct dom_string *nscss_dom_string_object;
extern struct dom_string *nscss_dom_string_p;
extern struct dom_string *nscss_dom_string_password;
extern struct dom_string *nscss_dom_string_right;
extern struct dom_string *nscss_dom_string_rows;
extern struct dom_string *nscss_dom_string_size;
extern struct dom_string *nscss_dom_string_table;
extern struct dom_string *nscss_dom_string_tbody;
extern struct dom_string *nscss_dom_string_td;
extern struct dom_string *nscss_dom_string_text;
extern struct dom_string *nscss_dom_string_textarea;
extern struct dom_string *nscss_dom_string_tfoot;
extern struct dom_string *nscss_dom_string_th;
extern struct dom_string *nscss_dom_string_thead;
extern struct dom_string *nscss_dom_string_tr;
extern struct dom_string *nscss_dom_string_type;
extern struct dom_string *nscss_dom_string_valign;
extern struct dom_string *nscss_dom_string_vlink;
extern struct dom_string *nscss_dom_string_vspace;
extern struct dom_string *nscss_dom_string_width;

#endif

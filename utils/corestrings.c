/*
 * Copyright 2012 Michael Drake <tlsa@netsurf-browser.org>
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
 * Useful interned string pointers (implementation).
 */

#include <dom/dom.h>

#include "utils/corestrings.h"
#include "utils/utils.h"

/* lwc_string strings */
lwc_string *corestring_lwc_a;
lwc_string *corestring_lwc_abscenter;
lwc_string *corestring_lwc_absmiddle;
lwc_string *corestring_lwc_align;
lwc_string *corestring_lwc_applet;
lwc_string *corestring_lwc_baseline;
lwc_string *corestring_lwc_body;
lwc_string *corestring_lwc_bottom;
lwc_string *corestring_lwc_button;
lwc_string *corestring_lwc_caption;
lwc_string *corestring_lwc_center;
lwc_string *corestring_lwc_circle;
lwc_string *corestring_lwc_col;
lwc_string *corestring_lwc_default;
lwc_string *corestring_lwc_div;
lwc_string *corestring_lwc_embed;
lwc_string *corestring_lwc_font;
lwc_string *corestring_lwc_h1;
lwc_string *corestring_lwc_h2;
lwc_string *corestring_lwc_h3;
lwc_string *corestring_lwc_h4;
lwc_string *corestring_lwc_h5;
lwc_string *corestring_lwc_h6;
lwc_string *corestring_lwc_hr;
lwc_string *corestring_lwc_iframe;
lwc_string *corestring_lwc_img;
lwc_string *corestring_lwc_input;
lwc_string *corestring_lwc_justify;
lwc_string *corestring_lwc_left;
lwc_string *corestring_lwc_middle;
lwc_string *corestring_lwc_object;
lwc_string *corestring_lwc_p;
lwc_string *corestring_lwc_password;
lwc_string *corestring_lwc_poly;
lwc_string *corestring_lwc_polygon;
lwc_string *corestring_lwc_rect;
lwc_string *corestring_lwc_rectangle;
lwc_string *corestring_lwc_right;
lwc_string *corestring_lwc_table;
lwc_string *corestring_lwc_tbody;
lwc_string *corestring_lwc_td;
lwc_string *corestring_lwc_text;
lwc_string *corestring_lwc_textarea;
lwc_string *corestring_lwc_texttop;
lwc_string *corestring_lwc_tfoot;
lwc_string *corestring_lwc_th;
lwc_string *corestring_lwc_thead;
lwc_string *corestring_lwc_top;
lwc_string *corestring_lwc_tr;

/* dom_string strings */
dom_string *corestring_dom_a;
dom_string *corestring_dom_align;
dom_string *corestring_dom_area;
dom_string *corestring_dom_background;
dom_string *corestring_dom_bgcolor;
dom_string *corestring_dom_border;
dom_string *corestring_dom_bordercolor;
dom_string *corestring_dom_cellpadding;
dom_string *corestring_dom_cellspacing;
dom_string *corestring_dom_color;
dom_string *corestring_dom_cols;
dom_string *corestring_dom_coords;
dom_string *corestring_dom_height;
dom_string *corestring_dom_href;
dom_string *corestring_dom_hspace;
dom_string *corestring_dom_id;
dom_string *corestring_dom_link;
dom_string *corestring_dom_map;
dom_string *corestring_dom_name;
dom_string *corestring_dom_nohref;
dom_string *corestring_dom_rect;
dom_string *corestring_dom_rows;
dom_string *corestring_dom_size;
dom_string *corestring_dom_shape;
dom_string *corestring_dom_src;
dom_string *corestring_dom_target;
dom_string *corestring_dom_text;
dom_string *corestring_dom_text_javascript;
dom_string *corestring_dom_type;
dom_string *corestring_dom_valign;
dom_string *corestring_dom_vlink;
dom_string *corestring_dom_vspace;
dom_string *corestring_dom_width;


/*
 * Free the core strings
 */
void corestrings_fini(void)
{
#define CSS_LWC_STRING_UNREF(NAME)					\
	do {								\
		if (corestring_lwc_##NAME != NULL) {			\
			lwc_string_unref(corestring_lwc_##NAME);	\
			corestring_lwc_##NAME = NULL;			\
		}							\
	} while (0)

	CSS_LWC_STRING_UNREF(a);
	CSS_LWC_STRING_UNREF(abscenter);
	CSS_LWC_STRING_UNREF(absmiddle);
	CSS_LWC_STRING_UNREF(align);
	CSS_LWC_STRING_UNREF(applet);
	CSS_LWC_STRING_UNREF(baseline);
	CSS_LWC_STRING_UNREF(body);
	CSS_LWC_STRING_UNREF(bottom);
	CSS_LWC_STRING_UNREF(button);
	CSS_LWC_STRING_UNREF(caption);
	CSS_LWC_STRING_UNREF(center);
	CSS_LWC_STRING_UNREF(circle);
	CSS_LWC_STRING_UNREF(col);
	CSS_LWC_STRING_UNREF(default);
	CSS_LWC_STRING_UNREF(div);
	CSS_LWC_STRING_UNREF(embed);
	CSS_LWC_STRING_UNREF(font);
	CSS_LWC_STRING_UNREF(h1);
	CSS_LWC_STRING_UNREF(h2);
	CSS_LWC_STRING_UNREF(h3);
	CSS_LWC_STRING_UNREF(h4);
	CSS_LWC_STRING_UNREF(h5);
	CSS_LWC_STRING_UNREF(h6);
	CSS_LWC_STRING_UNREF(hr);
	CSS_LWC_STRING_UNREF(iframe);
	CSS_LWC_STRING_UNREF(img);
	CSS_LWC_STRING_UNREF(input);
	CSS_LWC_STRING_UNREF(justify);
	CSS_LWC_STRING_UNREF(left);
	CSS_LWC_STRING_UNREF(middle);
	CSS_LWC_STRING_UNREF(object);
	CSS_LWC_STRING_UNREF(p);
	CSS_LWC_STRING_UNREF(password);
	CSS_LWC_STRING_UNREF(poly);
	CSS_LWC_STRING_UNREF(polygon);
	CSS_LWC_STRING_UNREF(rect);
	CSS_LWC_STRING_UNREF(rectangle);
	CSS_LWC_STRING_UNREF(right);
	CSS_LWC_STRING_UNREF(table);
	CSS_LWC_STRING_UNREF(tbody);
	CSS_LWC_STRING_UNREF(td);
	CSS_LWC_STRING_UNREF(text);
	CSS_LWC_STRING_UNREF(textarea);
	CSS_LWC_STRING_UNREF(texttop);
	CSS_LWC_STRING_UNREF(tfoot);
	CSS_LWC_STRING_UNREF(th);
	CSS_LWC_STRING_UNREF(thead);
	CSS_LWC_STRING_UNREF(top);
	CSS_LWC_STRING_UNREF(tr);
#undef CSS_LWC_STRING_UNREF

#define CSS_DOM_STRING_UNREF(NAME)					\
	do {								\
		if (corestring_dom_##NAME != NULL) {			\
			dom_string_unref(corestring_dom_##NAME);	\
			corestring_dom_##NAME = NULL;			\
		}							\
	} while (0)

	CSS_DOM_STRING_UNREF(a);
	CSS_DOM_STRING_UNREF(align);
	CSS_DOM_STRING_UNREF(area);
	CSS_DOM_STRING_UNREF(background);
	CSS_DOM_STRING_UNREF(bgcolor);
	CSS_DOM_STRING_UNREF(border);
	CSS_DOM_STRING_UNREF(bordercolor);
	CSS_DOM_STRING_UNREF(cellpadding);
	CSS_DOM_STRING_UNREF(cellspacing);
	CSS_DOM_STRING_UNREF(color);
	CSS_DOM_STRING_UNREF(cols);
	CSS_DOM_STRING_UNREF(coords);
	CSS_DOM_STRING_UNREF(height);
	CSS_DOM_STRING_UNREF(href);
	CSS_DOM_STRING_UNREF(hspace);
	CSS_DOM_STRING_UNREF(id);
	CSS_DOM_STRING_UNREF(link);
	CSS_DOM_STRING_UNREF(map);
	CSS_DOM_STRING_UNREF(name);
	CSS_DOM_STRING_UNREF(nohref);
	CSS_DOM_STRING_UNREF(rect);
	CSS_DOM_STRING_UNREF(rows);
	CSS_DOM_STRING_UNREF(size);
	CSS_DOM_STRING_UNREF(shape);
	CSS_DOM_STRING_UNREF(src);
	CSS_DOM_STRING_UNREF(target);
	CSS_DOM_STRING_UNREF(text);
	CSS_DOM_STRING_UNREF(text_javascript);
	CSS_DOM_STRING_UNREF(type);
	CSS_DOM_STRING_UNREF(valign);
	CSS_DOM_STRING_UNREF(vlink);
	CSS_DOM_STRING_UNREF(vspace);
	CSS_DOM_STRING_UNREF(width);
#undef CSS_DOM_STRING_UNREF
}


/*
 * Create the core strings
 */
nserror corestrings_init(void)
{
	lwc_error lerror;
	nserror error;
	dom_exception exc;

#define CSS_LWC_STRING_INTERN(NAME)					\
	do {								\
		lerror = lwc_intern_string(				\
				(const char *)#NAME,			\
				sizeof(#NAME) - 1,			\
				&corestring_lwc_##NAME );		\
		if ((lerror != lwc_error_ok) || 			\
				(corestring_lwc_##NAME == NULL)) {	\
			error = NSERROR_NOMEM;				\
			goto error;					\
		}							\
	} while(0)

	CSS_LWC_STRING_INTERN(a);
	CSS_LWC_STRING_INTERN(abscenter);
	CSS_LWC_STRING_INTERN(absmiddle);
	CSS_LWC_STRING_INTERN(align);
	CSS_LWC_STRING_INTERN(applet);
	CSS_LWC_STRING_INTERN(baseline);
	CSS_LWC_STRING_INTERN(body);
	CSS_LWC_STRING_INTERN(bottom);
	CSS_LWC_STRING_INTERN(button);
	CSS_LWC_STRING_INTERN(caption);
	CSS_LWC_STRING_INTERN(center);
	CSS_LWC_STRING_INTERN(circle);
	CSS_LWC_STRING_INTERN(col);
	CSS_LWC_STRING_INTERN(default);
	CSS_LWC_STRING_INTERN(div);
	CSS_LWC_STRING_INTERN(embed);
	CSS_LWC_STRING_INTERN(font);
	CSS_LWC_STRING_INTERN(h1);
	CSS_LWC_STRING_INTERN(h2);
	CSS_LWC_STRING_INTERN(h3);
	CSS_LWC_STRING_INTERN(h4);
	CSS_LWC_STRING_INTERN(h5);
	CSS_LWC_STRING_INTERN(h6);
	CSS_LWC_STRING_INTERN(hr);
	CSS_LWC_STRING_INTERN(iframe);
	CSS_LWC_STRING_INTERN(img);
	CSS_LWC_STRING_INTERN(input);
	CSS_LWC_STRING_INTERN(justify);
	CSS_LWC_STRING_INTERN(left);
	CSS_LWC_STRING_INTERN(middle);
	CSS_LWC_STRING_INTERN(object);
	CSS_LWC_STRING_INTERN(p);
	CSS_LWC_STRING_INTERN(password);
	CSS_LWC_STRING_INTERN(poly);
	CSS_LWC_STRING_INTERN(polygon);
	CSS_LWC_STRING_INTERN(rect);
	CSS_LWC_STRING_INTERN(rectangle);
	CSS_LWC_STRING_INTERN(right);
	CSS_LWC_STRING_INTERN(table);
	CSS_LWC_STRING_INTERN(tbody);
	CSS_LWC_STRING_INTERN(td);
	CSS_LWC_STRING_INTERN(text);
	CSS_LWC_STRING_INTERN(textarea);
	CSS_LWC_STRING_INTERN(texttop);
	CSS_LWC_STRING_INTERN(tfoot);
	CSS_LWC_STRING_INTERN(th);
	CSS_LWC_STRING_INTERN(thead);
	CSS_LWC_STRING_INTERN(top);
	CSS_LWC_STRING_INTERN(tr);
#undef CSS_LWC_STRING_INTERN

#define CSS_DOM_STRING_INTERN(NAME)					\
	do {								\
		exc = dom_string_create_interned(			\
				(const uint8_t *)#NAME,			\
				sizeof(#NAME) - 1,			\
				&corestring_dom_##NAME );		\
		if ((exc != DOM_NO_ERR) || 				\
				(corestring_dom_##NAME == NULL)) {	\
			error = NSERROR_NOMEM;				\
			goto error;					\
		}							\
	} while(0)

	CSS_DOM_STRING_INTERN(a);
	CSS_DOM_STRING_INTERN(align);
	CSS_DOM_STRING_INTERN(area);
	CSS_DOM_STRING_INTERN(background);
	CSS_DOM_STRING_INTERN(bgcolor);
	CSS_DOM_STRING_INTERN(border);
	CSS_DOM_STRING_INTERN(bordercolor);
	CSS_DOM_STRING_INTERN(cellpadding);
	CSS_DOM_STRING_INTERN(cellspacing);
	CSS_DOM_STRING_INTERN(color);
	CSS_DOM_STRING_INTERN(cols);
	CSS_DOM_STRING_INTERN(coords);
	CSS_DOM_STRING_INTERN(height);
	CSS_DOM_STRING_INTERN(href);
	CSS_DOM_STRING_INTERN(hspace);
	CSS_DOM_STRING_INTERN(id);
	CSS_DOM_STRING_INTERN(link);
	CSS_DOM_STRING_INTERN(map);
	CSS_DOM_STRING_INTERN(name);
	CSS_DOM_STRING_INTERN(nohref);
	CSS_DOM_STRING_INTERN(rect);
	CSS_DOM_STRING_INTERN(rows);
	CSS_DOM_STRING_INTERN(size);
	CSS_DOM_STRING_INTERN(shape);
	CSS_DOM_STRING_INTERN(src);
	CSS_DOM_STRING_INTERN(target);
	CSS_DOM_STRING_INTERN(text);
	CSS_DOM_STRING_INTERN(type);
	CSS_DOM_STRING_INTERN(valign);
	CSS_DOM_STRING_INTERN(vlink);
	CSS_DOM_STRING_INTERN(vspace);
	CSS_DOM_STRING_INTERN(width);
#undef CSS_DOM_STRING_INTERN

	exc = dom_string_create_interned((const uint8_t *) "text/javascript",
			SLEN("text/javascript"),
			&corestring_dom_text_javascript);
	if ((exc != DOM_NO_ERR) || (corestring_dom_text_javascript == NULL))
		goto error;

	return NSERROR_OK;

error:
	corestrings_fini();

	return error;
}

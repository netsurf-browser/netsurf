/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * CSS ruleset parsing.
 *
 * This file implements the last stage of CSS parsing. It converts trees of
 * struct css_node produced by the parser into struct style, and adds them to a
 * stylesheet.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#define CSS_INTERNALS
#define NDEBUG
#include "netsurf/css/css.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/options.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"



static bool css_compare_selectors(const struct css_selector *n0,
		const struct css_selector *n1);
static int parse_length(struct css_length * const length,
		const struct css_node * const v, bool non_negative);
static colour parse_colour(const struct css_node * const v);
static void parse_background(struct css_style * const s, const struct css_node * v);
static void parse_background_attachment(struct css_style * const s, const struct css_node * const v);
static void parse_background_color(struct css_style * const s, const struct css_node * const v);
static void parse_background_image(struct css_style * const s, const struct css_node * const v);
static void parse_background_position(struct css_style * const s, const struct css_node * const v);
static void parse_background_repeat(struct css_style * const s, const struct css_node * const v);
static void parse_border(struct css_style * const s, const struct css_node * v);
static void parse_border_bottom(struct css_style * const s, const struct css_node * v);
static void parse_border_bottom_color(struct css_style * const s, const struct css_node * v);
static void parse_border_bottom_style(struct css_style * const s, const struct css_node * v);
static void parse_border_bottom_width(struct css_style * const s, const struct css_node * v);
static void parse_border_color(struct css_style * const s, const struct css_node * v);
static void parse_border_color_side(struct css_style * const s,
		const struct css_node * const v, unsigned int i);
static void parse_border_left(struct css_style * const s, const struct css_node * v);
static void parse_border_left_color(struct css_style * const s, const struct css_node * v);
static void parse_border_left_style(struct css_style * const s, const struct css_node * v);
static void parse_border_left_width(struct css_style * const s, const struct css_node * v);
static void parse_border_right(struct css_style * const s, const struct css_node * v);
static void parse_border_right_color(struct css_style * const s, const struct css_node * v);
static void parse_border_right_style(struct css_style * const s, const struct css_node * v);
static void parse_border_right_width(struct css_style * const s, const struct css_node * v);
static void parse_border_side(struct css_style * const s,
		const struct css_node *v, unsigned int i);
static void parse_border_style(struct css_style * const s, const struct css_node * v);
static void parse_border_style_side(struct css_style * const s,
		const struct css_node * const v, unsigned int i);
static void parse_border_top(struct css_style * const s, const struct css_node * v);
static void parse_border_top_color(struct css_style * const s, const struct css_node * v);
static void parse_border_top_style(struct css_style * const s, const struct css_node * v);
static void parse_border_top_width(struct css_style * const s, const struct css_node * v);
static void parse_border_width(struct css_style * const s, const struct css_node * v);
static void parse_border_width_side(struct css_style * const s,
		const struct css_node * const v, unsigned int i);
static void parse_clear(struct css_style * const s, const struct css_node * const v);
static void parse_color(struct css_style * const s, const struct css_node * const v);
static void parse_cursor(struct css_style * const s, const struct css_node * v);
static void parse_display(struct css_style * const s, const struct css_node * const v);
static void parse_float(struct css_style * const s, const struct css_node * const v);
static void parse_font(struct css_style * const s, const struct css_node * v);
static void parse_font_family(struct css_style * const s, const struct css_node * v);
static void parse_font_size(struct css_style * const s, const struct css_node * const v);
static void parse_font_style(struct css_style * const s, const struct css_node * const v);
static void parse_font_variant(struct css_style * const s, const struct css_node * const v);
static void parse_font_weight(struct css_style * const s, const struct css_node * const v);
static void parse_height(struct css_style * const s, const struct css_node * const v);
static void parse_line_height(struct css_style * const s, const struct css_node * const v);
static void parse_margin(struct css_style * const s, const struct css_node * const v);
static void parse_margin_bottom(struct css_style * const s, const struct css_node * const v);
static void parse_margin_left(struct css_style * const s, const struct css_node * const v);
static void parse_margin_right(struct css_style * const s, const struct css_node * const v);
static void parse_margin_top(struct css_style * const s, const struct css_node * const v);
static void parse_margin_side(struct css_style * const s, const struct css_node * const v,
		unsigned int i);
static void parse_padding(struct css_style * const s, const struct css_node * const v);
static void parse_padding_bottom(struct css_style * const s, const struct css_node * const v);
static void parse_padding_left(struct css_style * const s, const struct css_node * const v);
static void parse_padding_right(struct css_style * const s, const struct css_node * const v);
static void parse_padding_top(struct css_style * const s, const struct css_node * const v);
static void parse_padding_side(struct css_style * const s, const struct css_node * const v,
		unsigned int i);
static void parse_text_align(struct css_style * const s, const struct css_node * const v);
static void parse_text_decoration(struct css_style * const s, const struct css_node * const v);
static void parse_text_indent(struct css_style * const s, const struct css_node * const v);
static void parse_text_transform(struct css_style * const s, const struct css_node * const v);
static void parse_visibility(struct css_style * const s, const struct css_node * const v);
static void parse_width(struct css_style * const s, const struct css_node * const v);
static void parse_white_space(struct css_style * const s, const struct css_node * const v);
static css_text_decoration css_text_decoration_parse(const char * const s,
		int length);


/** An entry in css_property_table. */
struct css_property_entry {
	const char name[25];
	void (*parse) (struct css_style * const s,
			const struct css_node * const v);
};

/** Table of property parsers. MUST be sorted by property name. */
static const struct css_property_entry css_property_table[] = {
	{ "background",       parse_background },
	{ "background-attachment", parse_background_attachment },
	{ "background-color", parse_background_color },
	{ "background-image", parse_background_image },
	{ "background-position", parse_background_position },
	{ "background-repeat", parse_background_repeat },
	{ "border",           parse_border },
	{ "border-bottom",    parse_border_bottom },
	{ "border-bottom-color", parse_border_bottom_color },
	{ "border-bottom-style", parse_border_bottom_style },
	{ "border-bottom-width", parse_border_bottom_width },
	{ "border-color",     parse_border_color },
	{ "border-left",      parse_border_left },
	{ "border-left-color", parse_border_left_color },
	{ "border-left-style", parse_border_left_style },
	{ "border-left-width", parse_border_left_width },
	{ "border-right",     parse_border_right },
	{ "border-right-color", parse_border_right_color },
	{ "border-right-style", parse_border_right_style },
	{ "border-right-width", parse_border_right_width },
	{ "border-style",     parse_border_style },
	{ "border-top",       parse_border_top },
	{ "border-top-color", parse_border_top_color },
	{ "border-top-style", parse_border_top_style },
	{ "border-top-width", parse_border_top_width },
	{ "border-width",     parse_border_width },
	{ "clear",            parse_clear },
	{ "color",            parse_color },
	{ "cursor",           parse_cursor },
	{ "display",          parse_display },
	{ "float",            parse_float },
	{ "font",             parse_font },
	{ "font-family",      parse_font_family },
	{ "font-size",        parse_font_size },
	{ "font-style",       parse_font_style },
	{ "font-variant",     parse_font_variant },
	{ "font-weight",      parse_font_weight },
	{ "height",           parse_height },
	{ "line-height",      parse_line_height },
	{ "margin",           parse_margin },
	{ "margin-bottom",    parse_margin_bottom },
	{ "margin-left",      parse_margin_left },
	{ "margin-right",     parse_margin_right },
	{ "margin-top",       parse_margin_top },
	{ "padding",          parse_padding },
	{ "padding-bottom",   parse_padding_bottom },
	{ "padding-left",     parse_padding_left },
	{ "padding-right",    parse_padding_right },
	{ "padding-top",      parse_padding_top },
	{ "text-align",       parse_text_align },
	{ "text-decoration",  parse_text_decoration },
	{ "text-indent",      parse_text_indent },
	{ "text-transform",   parse_text_transform },
	{ "visibility",       parse_visibility },
	{ "white-space",      parse_white_space },
	{ "width",            parse_width },
};


/** An entry in css_colour_table. */
struct css_colour_entry {
	const char name[12];
	colour col;
};

/* Table of standard colour names. MUST be sorted by colour name.
 * Note: colour is 0xbbggrr. */
static const struct css_colour_entry css_colour_table[] = {
	{ "aqua",    0xffff00 },
	{ "black",   0x000000 },
	{ "blue",    0xff0000 },
	{ "fuchsia", 0xff00ff },
	{ "gray",    0x808080 },
	{ "green",   0x008000 },
	{ "lime",    0x00ff00 },
	{ "maroon",  0x000080 },
	{ "navy",    0x800000 },
	{ "olive",   0x008080 },
	{ "orange",  0x00a5ff },
	{ "purple",  0x800080 },
	{ "red",     0x0000ff },
	{ "silver",  0xc0c0c0 },
	{ "teal",    0x808000 },
	{ "transparent", TRANSPARENT },
	{ "white",   0xffffff },
	{ "yellow",  0x00ffff },
};


/** An entry in css_font_size_table. */
struct css_font_size_entry {
	const char name[10];
	float size;
};

/** Table of font sizes. MUST be sorted by name. */
#define SIZE_FACTOR 1.2
static const struct css_font_size_entry css_font_size_table[] = {
	{ "large", 1.0 * SIZE_FACTOR },
	{ "medium", 1.0 },
	{ "small", 1.0 / SIZE_FACTOR },
	{ "x-large", 1.0 * SIZE_FACTOR * SIZE_FACTOR },
	{ "x-small", 1.0 / (SIZE_FACTOR * SIZE_FACTOR) },
	{ "xx-large", 1.0 * SIZE_FACTOR * SIZE_FACTOR * SIZE_FACTOR },
	{ "xx-small", 1.0 / (SIZE_FACTOR * SIZE_FACTOR * SIZE_FACTOR) },
};


/**
 * Add a ruleset to a stylesheet.
 */

void css_add_ruleset(struct content *c,
		struct css_selector *selector,
		struct css_node *declaration)
{
	bool found;
	struct css_stylesheet *stylesheet = c->data.css.css;
	struct css_selector *n, *sel, *next_sel, *prev;
	struct css_style *style;
	unsigned int hash;

	for (sel = selector; sel != 0; sel = next_sel) {
		next_sel = sel->next;

		/* check if this selector is already present */
		found = false;
		prev = 0;
		hash = css_hash(sel->data, sel->data_length);
		/* selectors are ordered by specificity in the hash chain */
		for (n = stylesheet->rule[hash];
				n && n->specificity < sel->specificity;
				n = n->next)
			prev = n;
		for ( ;	n && n->specificity == sel->specificity;
				n = n->next) {
			prev = n;
			if (css_compare_selectors(sel, n)) {
				found = true;
				break;
			}
		}
		if (!found) {
			/* not present: construct a new struct css_style */
			LOG(("constructing new style"));
			style = malloc(sizeof *style);
			if (!style) {
				/** \todo report to user */
				css_free_selector(sel);
				return;
			}
			memcpy(style, &css_empty_style, sizeof(*style));
			sel->style = style;
			sel->next = n;
			if (prev)
				prev->next = sel;
			else
				stylesheet->rule[hash] = sel;
			c->size += sizeof(*style);
		} else {
			/* already exists: augument existing style */
			LOG(("augumenting existing style"));
			style = n->style;
			sel->next = 0;
			css_free_selector(sel);
		}

		/* fill in the declarations */
		css_add_declarations(style, declaration);
	}
}


/**
 * Add declarations to a style.
 */

void css_add_declarations(struct css_style *style, struct css_node *declaration)
{
	char name[20];
	struct css_node *n;
	for (n = declaration; n != 0; n = n->next) {
		struct css_property_entry *p;
		assert(n->type == CSS_NODE_DECLARATION && n->data && n->value);
		if (19 < n->data_length)
			continue;
		strncpy(name, n->data, n->data_length);
		name[n->data_length] = 0;
		p = bsearch(name, css_property_table,
				sizeof css_property_table /
						sizeof css_property_table[0],
				sizeof css_property_table[0],
				(int (*)(const void *, const void *))
						strcasecmp);
		if (p == 0)
			continue;
		p->parse(style, n->value);
	}
}


/**
 * Compare two css_selectors.
 */

bool css_compare_selectors(const struct css_selector *n0,
		const struct css_selector *n1)
{
	struct css_selector *m0, *m1;
	unsigned int count0 = 0, count1 = 0;

	/* compare element name */
	if (!((n0->data == 0 && n1->data == 0) ||
			(n0->data != 0 && n1->data != 0 &&
			n0->data_length == n1->data_length &&
			strncmp(n0->data, n1->data, n0->data_length) == 0)))
		return false;

	if (n0->comb != n1->comb)
		return false;

	/* compare classes and ids */
	for (m0 = n0->detail; m0 != 0; m0 = m0->next)
		count0++;
	for (m1 = n1->detail; m1 != 0; m1 = m1->next)
		count1++;
	if (count0 != count1)
		return false;
	for (m0 = n0->detail; m0 != 0; m0 = m0->next) {
		bool found = false;
		for (m1 = n1->detail; m1 != 0; m1 = m1->next) {
			/* TODO: should this be case sensitive for IDs? */
			if (m0->type == m1->type &&
					m0->data_length == m1->data_length &&
					strncasecmp(m0->data, m1->data,
					m0->data_length) == 0 &&
					((m0->data2 == 0 && m1->data2 == 0) ||
					 (m0->data2_length == m1->data2_length &&
					 strncasecmp(m0->data2, m1->data2,
					 m0->data2_length) == 0))) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}

	/* compare ancestors */
	if (n0->comb == CSS_COMB_NONE)
		return true;

	return css_compare_selectors(n0->combiner, n1->combiner);
}


/*
 * Property parsers.
 */

/* TODO: consider CSS_NODE_NUMBER whenever a value may be '0' */

int parse_length(struct css_length * const length,
		const struct css_node * const v, bool non_negative)
{
	css_unit u;
	float value;
	int num_length;
	if (v->type == CSS_NODE_NUMBER && atof(v->data) == 0) {
		length->unit = CSS_UNIT_EM;
		length->value = 0;
		return 0;
	}
	if (v->type != CSS_NODE_DIMENSION)
		return 1;
	num_length = strspn(v->data, "0123456789+-.");
	u = css_unit_parse(v->data + num_length, v->data_length - num_length);
	if (u == CSS_UNIT_UNKNOWN)
		return 1;
	value = atof(v->data);
	if (non_negative && value < 0)
		return 1;
	length->unit = u;
	length->value = value;
	return 0;
}


colour named_colour(const char *name)
{
	struct css_colour_entry *col;
	unsigned int r, g, b;

	col = bsearch(name, css_colour_table,
			sizeof css_colour_table / sizeof css_colour_table[0],
			sizeof css_colour_table[0],
			(int (*)(const void *, const void *)) strcasecmp);
	if (col == 0) {
	        /* A common error is the omission of the '#' from the
	         * start of a colour specified in #rrggbb format.
	         * This attempts to detect and recover from this.
	         */
	        if (strlen(name) == 6 &&
	            sscanf(name, "%2x%2x%2x", &r, &g, &b) == 3) {
	                return (b << 16) | (g << 8) | r;
	        }
	        else
		        return TRANSPARENT;
	}
	return col->col;
}


colour parse_colour(const struct css_node * const v)
{
	colour c = CSS_COLOR_NONE;
	unsigned int r, g, b;
	struct css_colour_entry *col;
	char colour_name[12];
	const char *u;
	char sb[5];
	int i;

	switch (v->type) {
		case CSS_NODE_HASH:
			if (v->data_length == 4) {
				if (sscanf(v->data + 1, "%1x%1x%1x", &r, &g, &b) == 3)
					c = (b << 20) | (b << 16) | (g << 12) | (g << 8) | (r << 4) | r;
			} else if (v->data_length == 7) {
				if (sscanf(v->data + 1, "%2x%2x%2x", &r, &g, &b) == 3)
					c = (b << 16) | (g << 8) | r;
			}
			break;

		case CSS_NODE_FUNCTION:
			for (u = v->data+4;
			        *u == ' ' || *u == '\t' || *u == '\r' ||
				*u == '\n' || *u == '\f';
				u++)
				;
			/* extract r */
			for (i=0; *u != ',' && i != 4; i++) {
			        sb[i] = *u++;
			}
			sb[i] = 0;
			u++;

			if (sb[i-1] == '%') {
			        sb[i-1] = 0;
			        r = (int)((float)(atoi(sb) / 100.0) * 255);
			}
			else
		        	r = atoi(sb);

                        /* extract g */
			for (i=0; *u != ',' && i != 4; i++) {
			        sb[i] = *u++;
			}
			sb[i] = 0;
			u++;

			if (sb[i-1] == '%') {
			        sb[i-1] = 0;
			        g = (int)((float)(atoi(sb) / 100.0) * 255);
			}
			else
		        	g = atoi(sb);

                        /* extract b */
			for (i=0; *u != ')' && i != 4; i++) {
			        sb[i] = *u++;
			}
			sb[i] = 0;

			if (sb[i-1] == '%') {
			        sb[i-1] = 0;
			        b = (int)((float)(atoi(sb) / 100.0) * 255);
			}
			else
		        	b = atoi(sb);

                        /* calculate c, ensuring that r,g,b are in range */
			c = ((b > 255 ? 255 : b) << 16) |
			    ((g > 255 ? 255 : g) <<  8) |
			     (r > 255 ? 255 : r);

			break;

		case CSS_NODE_IDENT:
			if (11 < v->data_length)
				break;
			strncpy(colour_name, v->data, v->data_length);
			colour_name[v->data_length] = 0;
			col = bsearch(colour_name, css_colour_table,
					sizeof css_colour_table /
						sizeof css_colour_table[0],
					sizeof css_colour_table[0],
					(int (*)(const void *, const void *))
							strcasecmp);
			if (col != 0)
				c = col->col;
			break;

		default:
			break;
	}

	return c;
}


void parse_background(struct css_style * const s, const struct css_node * v)
{
	colour c;
	for (; v; v = v->next) {
		switch (v->type) {
			case CSS_NODE_HASH:
			case CSS_NODE_FUNCTION:
			case CSS_NODE_IDENT:
				c = parse_colour(v);
				if (c != CSS_COLOR_NONE)
					s->background_color = c;
				break;
			default:
				break;
		}
	}
}

void parse_background_attachment(struct css_style * const s, const struct css_node * const v)
{
        css_background_attachment z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_background_attachment_parse(v->data, v->data_length);
	if (z != CSS_BACKGROUND_ATTACHMENT_UNKNOWN)
		s->background_attachment = z;
}

void parse_background_color(struct css_style * const s, const struct css_node * const v)
{
	colour c = parse_colour(v);
	if (c != CSS_COLOR_NONE)
		s->background_color = c;
}

void parse_background_image(struct css_style * const s, const struct css_node * const v)
{
        bool string = false;
        const char *u;
        char *t, *url;
        s->background_image.uri = 0;

        switch (v->type) {
                case CSS_NODE_URI:
                        for (u = v->data + 4;
					*u == ' ' || *u == '\t' || *u == '\r' ||
					*u == '\n' || *u == '\f';
					u++)
				;
			if (*u == '\'' || *u == '"') {
				string = true;
				u++;
			}
			url = strndup(u, v->data_length - (u - v->data));
			if (!url) {
				return;
			}
			for (t = url + strlen(url) - 2;
					*t == ' ' || *t == '\t' || *t == '\r' ||
					*t == '\n' || *t == '\f';
					t--)
				;
			if (string)
				*t = 0;
			else
				*(t + 1) = 0;

                        /* for inline style attributes, the stylesheet
                         * content is the parent HTML content
                         */
                        if (v->stylesheet->type == CONTENT_HTML)
		        	s->background_image.uri = url_join(url, v->stylesheet->data.html.base_url);
		        else
		                s->background_image.uri = url_join(url, v->stylesheet->url);
			free(url);
			if (!s->background_image.uri) return;
			s->background_image.type = CSS_BACKGROUND_IMAGE_URI;
                        break;
                case CSS_NODE_STRING:
                        url = strndup(v->data, v->data_length);
                        if (!url)
                                return;

                        s->background_image.uri = url_join(url, v->stylesheet->url);
			free(url);
			if (!s->background_image.uri) return;
			s->background_image.type = CSS_BACKGROUND_IMAGE_URI;
                        break;
                case CSS_NODE_IDENT:
                        if (v->data_length == 7 && strncasecmp(v->data, "inherit", 7) == 0)
                                s->background_image.type = CSS_BACKGROUND_IMAGE_INHERIT;
                        else if (v->data_length == 4 && strncasecmp(v->data, "none", 4) == 0)
                                s->background_image.type = CSS_BACKGROUND_IMAGE_NONE;
                        break;
                default:
                        break;
        }
}

void parse_background_position(struct css_style * const s, const struct css_node * v)
{
        struct css_node *w = v->next;

        if (!w) { /* only one value specified */
                if (v->type == CSS_NODE_IDENT) {
                        if (v->data_length == 3 && strncasecmp(v->data, "top", 3) == 0) {
                                s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
                                s->background_position.horz.value.percent = 50.0;
                                s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                                s->background_position.vert.value.percent = 0.0;
                        }
                        else if (v->data_length == 4 && strncasecmp(v->data, "left", 4) == 0) {
                                s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
                                s->background_position.horz.value.percent = 0.0;
                                s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                                s->background_position.vert.value.percent = 50.0;
                        }
                        else if (v->data_length == 6 && strncasecmp(v->data, "center", 6) == 0) {
                                s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
                                s->background_position.horz.value.percent = 50.0;
                                s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                                s->background_position.vert.value.percent = 50.0;
                        }
                        else if (v->data_length == 5 && strncasecmp(v->data, "right", 5) == 0) {
                                s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
                                s->background_position.horz.value.percent = 100.0;
                                s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                                s->background_position.vert.value.percent = 50.0;
                        }
                        else if (v->data_length == 6 && strncasecmp(v->data, "bottom", 6) == 0) {
                                s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
                                s->background_position.horz.value.percent = 50.0;
                                s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                                s->background_position.vert.value.percent = 100.0;
                        }
                }
                else if (v->type == CSS_NODE_PERCENTAGE) {
                        s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.horz.value.percent = atof(v->data);
                        s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.vert.value.percent = 50.0;
                }
                else if (v->type == CSS_NODE_DIMENSION) {

                        if (parse_length(&s->background_position.horz.value.length, v, false) == 0) {
                                s->background_position.horz.pos = CSS_BACKGROUND_POSITION_LENGTH;
                                s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                                s->background_position.vert.value.percent = 50.0;
                        }
                }
        }

        /* two values specified */
        if (v->type == CSS_NODE_IDENT && w->type == CSS_NODE_IDENT) {
                /* both keywords */
                if ((v->data_length == 3 && strncasecmp(v->data, "top", 3) == 0 && w->data_length == 4 && strncasecmp(w->data, "left", 4) == 0) ||
                    (v->data_length == 4 && strncasecmp(v->data, "left", 4) == 0 && w->data_length == 3 && strncasecmp(w->data, "top", 3) == 0)) {
                        /* top left / left top */
                        s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.horz.value.percent = 0.0;
                        s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.vert.value.percent = 0.0;
                }
                else if ((v->data_length == 3 && strncasecmp(v->data, "top", 3) == 0 && w->data_length == 6 && strncasecmp(w->data, "center", 6) == 0) ||
                    (v->data_length == 6 && strncasecmp(v->data, "center", 6) == 0 && w->data_length == 3 && strncasecmp(w->data, "top", 3) == 0)) {
                        /* top center / center top */
                        s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.horz.value.percent = 50.0;
                        s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.vert.value.percent = 0.0;
                }
                else if ((v->data_length == 3 && strncasecmp(v->data, "top", 3) == 0 && w->data_length == 5 && strncasecmp(w->data, "right", 5) == 0) ||
                    (v->data_length == 5 && strncasecmp(v->data, "right", 5) == 0 && w->data_length == 3 && strncasecmp(w->data, "top", 3) == 0)) {
                        /* top right / right top */
                        s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.horz.value.percent = 100.0;
                        s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.vert.value.percent = 0.0;
                }
                else if ((v->data_length == 4 && strncasecmp(v->data, "left", 4) == 0 && w->data_length == 6 && strncasecmp(w->data, "center", 6) == 0) ||
                    (v->data_length == 6 && strncasecmp(v->data, "center", 6) == 0 && w->data_length == 4 && strncasecmp(w->data, "left", 4) == 0)) {
                        /* left center / center left */
                        s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.horz.value.percent = 0.0;
                        s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.vert.value.percent = 50.0;
                }
                else if (v->data_length == 6 && strncasecmp(v->data, "center", 6) == 0 && w->data_length == 6 && strncasecmp(w->data, "center", 6) == 0) {
                        /* center center */
                        s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.horz.value.percent = 50.0;
                        s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.vert.value.percent = 50.0;
                }
                else if ((v->data_length == 5 && strncasecmp(v->data, "right", 5) == 0 && w->data_length == 6 && strncasecmp(w->data, "center", 6) == 0) ||
                    (v->data_length == 6 && strncasecmp(v->data, "center", 6) == 0 && w->data_length == 5 && strncasecmp(w->data, "right", 5) == 0)) {
                        /* right center / center right */
                        s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.horz.value.percent = 100.0;
                        s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.vert.value.percent = 50.0;
                }
                else if ((v->data_length == 6 && strncasecmp(v->data, "bottom", 6) == 0 && w->data_length == 4 && strncasecmp(w->data, "left", 4) == 0) ||
                    (v->data_length == 4 && strncasecmp(v->data, "left", 4) == 0 && w->data_length == 6 && strncasecmp(w->data, "bottom", 6) == 0)) {
                        /* bottom left / left bottom */
                        s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.horz.value.percent = 0.0;
                        s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.vert.value.percent = 100.0;
                }
                else if ((v->data_length == 6 && strncasecmp(v->data, "bottom", 6) == 0 && w->data_length == 6 && strncasecmp(w->data, "center", 6) == 0) ||
                    (v->data_length == 6 && strncasecmp(v->data, "center", 6) == 0 && w->data_length == 6 && strncasecmp(w->data, "bottom", 6) == 0)) {
                        /* bottom center / center bottom */
                        s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.horz.value.percent = 50.0;
                        s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.vert.value.percent = 100.0;
                }
                else if ((v->data_length == 6 && strncasecmp(v->data, "bottom", 6) == 0 && w->data_length == 5 && strncasecmp(w->data, "right", 5) == 0) ||
                    (v->data_length == 5 && strncasecmp(v->data, "right", 5) == 0 && w->data_length == 6 && strncasecmp(w->data, "bottom", 6) == 0)) {
                        /* bottom right / right bottom */
                        s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.horz.value.percent = 100.0;
                        s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
                        s->background_position.vert.value.percent = 100.0;
                }
        }
        else {
        	switch (v->type) { /* horizontal value */
	        	case CSS_NODE_IDENT:
	                        if (v->data_length == 7 && strncasecmp(v->data, "inherit", 7) == 0)
	                                s->background_position.horz.pos = CSS_BACKGROUND_POSITION_INHERIT;
	                        else if (v->data_length == 4 && strncasecmp(v->data, "left", 4) == 0) {
	                                s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
	                                s->background_position.horz.value.percent = 0.0;
	                        }
	                        else if (v->data_length == 5 && strncasecmp(v->data, "right", 5) == 0) {
	                                s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
	                                s->background_position.horz.value.percent = 100.0;
	                        }
	                        else if (v->data_length == 6 && strncasecmp(v->data, "center", 6) == 0) {
	                                s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
	                                s->background_position.horz.value.percent = 50.0;
	                        }
			        break;
		        case CSS_NODE_PERCENTAGE:
		                s->background_position.horz.pos = CSS_BACKGROUND_POSITION_PERCENT;
		                s->background_position.horz.value.percent = atof(v->data);
		                break;
		        case CSS_NODE_DIMENSION:
		                if (parse_length(&s->background_position.horz.value.length, v, false) == 0)
		                        s->background_position.horz.pos = CSS_BACKGROUND_POSITION_LENGTH;
		                break;
		        default:
			        break;
		}
		switch (w->type) { /* vertical value */
	        	case CSS_NODE_IDENT:
	                        if (v->data_length == 7 && strncasecmp(v->data, "inherit", 7) == 0)
	                                s->background_position.vert.pos = CSS_BACKGROUND_POSITION_INHERIT;
	                        else if (v->data_length == 3 && strncasecmp(v->data, "top", 3) == 0) {
	                                s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
	                                s->background_position.vert.value.percent = 0.0;
	                        }
	                        else if (v->data_length == 6 && strncasecmp(v->data, "bottom", 6) == 0) {
	                                s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
	                                s->background_position.vert.value.percent = 100.0;
	                        }
	                        else if (v->data_length == 6 && strncasecmp(v->data, "center", 6) == 0) {
	                                s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
	                                s->background_position.vert.value.percent = 50.0;
	                        }
			        break;
		        case CSS_NODE_PERCENTAGE:
		                s->background_position.vert.pos = CSS_BACKGROUND_POSITION_PERCENT;
		                s->background_position.vert.value.percent = atof(v->data);
		                break;
		        case CSS_NODE_DIMENSION:
		                if (parse_length(&s->background_position.vert.value.length, v, false) == 0)
		                        s->background_position.vert.pos = CSS_BACKGROUND_POSITION_LENGTH;
		                break;
		        default:
			        break;
		}
	}
}

void parse_background_repeat(struct css_style * const s, const struct css_node * const v)
{
        css_background_repeat z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_background_repeat_parse(v->data, v->data_length);
	if (z != CSS_BACKGROUND_REPEAT_UNKNOWN)
		s->background_repeat = z;
}

void parse_border_width(struct css_style * const s,
		const struct css_node * const v)
{
	unsigned int count = 0;
	const struct css_node *w;

	for (w = v; w; w = w->next, count++)
		if (!((w->type == CSS_NODE_IDENT && (
				  (w->data_length == 7 &&
				   strncasecmp(w->data, "inherit", 7) == 0) ||
				  (w->data_length == 4 &&
				   strncasecmp(w->data, "thin", 4) == 0) ||
				  (w->data_length == 6 &&
				   strncasecmp(w->data, "medium", 6) == 0) ||
				  (w->data_length == 5 &&
				   strncasecmp(w->data, "thick", 5) == 0))) ||
				(w->type == CSS_NODE_DIMENSION) ||
				(w->type == CSS_NODE_NUMBER)))
			return;

	w = v;
	switch (count) {
		case 1: /* one value: applies to all sides */
			parse_border_width_side(s, w, TOP);
			parse_border_width_side(s, w, RIGHT);
			parse_border_width_side(s, w, BOTTOM);
			parse_border_width_side(s, w, LEFT);
			break;
		case 2: /* (top and bottom), (left and right) */
			parse_border_width_side(s, w, TOP);
			parse_border_width_side(s, w, BOTTOM);
			w = w->next;
			parse_border_width_side(s, w, RIGHT);
			parse_border_width_side(s, w, LEFT);
			break;
		case 3: /* top, (left and right), bottom */
			parse_border_width_side(s, w, TOP);
			w = w->next;
			parse_border_width_side(s, w, RIGHT);
			parse_border_width_side(s, w, LEFT);
			w = w->next;
			parse_border_width_side(s, w, BOTTOM);
			break;
		case 4: /* top, right, bottom, left */
			parse_border_width_side(s, w, TOP);
			w = w->next;
			parse_border_width_side(s, w, RIGHT);
			w = w->next;
			parse_border_width_side(s, w, BOTTOM);
			w = w->next;
			parse_border_width_side(s, w, LEFT);
			break;
	}
}

#define PARSE_BORDER_WIDTH(side, z) \
void parse_border_ ## side ## _width(struct css_style * const s, \
		const struct css_node * const v)	\
{							\
	if (v->next != 0)				\
		return;					\
	parse_border_width_side(s, v, z);		\
}

PARSE_BORDER_WIDTH(top,    TOP)
PARSE_BORDER_WIDTH(right,  RIGHT)
PARSE_BORDER_WIDTH(bottom, BOTTOM)
PARSE_BORDER_WIDTH(left,   LEFT)

void parse_border_width_side(struct css_style * const s,
		const struct css_node * const v, unsigned int i)
{
	if (v->type == CSS_NODE_IDENT) {
		if (v->data_length == 7 &&
				strncasecmp(v->data, "inherit", 7) == 0)
			s->border[i].width.width = CSS_BORDER_WIDTH_INHERIT;
		else if (v->data_length == 4 &&
				strncasecmp(v->data, "thin", 4) == 0) {
			s->border[i].width.width = CSS_BORDER_WIDTH_LENGTH;
			s->border[i].width.value.value = 1;
			s->border[i].width.value.unit = CSS_UNIT_PX;
		} else if (v->data_length == 6 &&
				strncasecmp(v->data, "medium", 6) == 0) {
			s->border[i].width.width = CSS_BORDER_WIDTH_LENGTH;
			s->border[i].width.value.value = 2;
			s->border[i].width.value.unit = CSS_UNIT_PX;
		} else if (v->data_length == 5 &&
				strncasecmp(v->data, "thick", 5) == 0) {
			s->border[i].width.width = CSS_BORDER_WIDTH_LENGTH;
			s->border[i].width.value.value = 4;
			s->border[i].width.value.unit = CSS_UNIT_PX;
		}
	} else if ((v->type == CSS_NODE_DIMENSION ||
			v->type == CSS_NODE_NUMBER) &&
			parse_length(&s->border[i].width.value, v, true) == 0)
		s->border[i].width.width = CSS_BORDER_WIDTH_LENGTH;
}

void parse_border_color(struct css_style * const s,
		const struct css_node * const v)
{
	unsigned int count = 0;
	const struct css_node *w;

	for (w = v; w; w = w->next, count++)
		if (!(w->type == CSS_NODE_HASH ||
				w->type == CSS_NODE_FUNCTION ||
				w->type == CSS_NODE_IDENT))
			return;

	w = v;
	switch (count) {
		case 1: /* one value: applies to all sides */
			parse_border_color_side(s, w, TOP);
			parse_border_color_side(s, w, RIGHT);
			parse_border_color_side(s, w, BOTTOM);
			parse_border_color_side(s, w, LEFT);
			break;
		case 2: /* (top and bottom), (left and right) */
			parse_border_color_side(s, w, TOP);
			parse_border_color_side(s, w, BOTTOM);
			w = w->next;
			parse_border_color_side(s, w, RIGHT);
			parse_border_color_side(s, w, LEFT);
			break;
		case 3: /* top, (left and right), bottom */
			parse_border_color_side(s, w, TOP);
			w = w->next;
			parse_border_color_side(s, w, RIGHT);
			parse_border_color_side(s, w, LEFT);
			w = w->next;
			parse_border_color_side(s, w, BOTTOM);
			break;
		case 4: /* top, right, bottom, left */
			parse_border_color_side(s, w, TOP);
			w = w->next;
			parse_border_color_side(s, w, RIGHT);
			w = w->next;
			parse_border_color_side(s, w, BOTTOM);
			w = w->next;
			parse_border_color_side(s, w, LEFT);
			break;
	}
}

#define PARSE_BORDER_COLOR(side, z) \
void parse_border_ ## side ## _color(struct css_style * const s, \
		const struct css_node * const v)	\
{							\
	if (v->next != 0)				\
		return;					\
	parse_border_color_side(s, v, z);		\
}

PARSE_BORDER_COLOR(top,    TOP)
PARSE_BORDER_COLOR(right,  RIGHT)
PARSE_BORDER_COLOR(bottom, BOTTOM)
PARSE_BORDER_COLOR(left,   LEFT)

void parse_border_color_side(struct css_style * const s,
		const struct css_node * const v, unsigned int i)
{
	colour c = parse_colour(v);
	if (c != CSS_COLOR_NONE)
		s->border[i].color = c;
}

void parse_border_style(struct css_style * const s,
		const struct css_node * const v)
{
	unsigned int count = 0;
	const struct css_node *w;

	for (w = v; w; w = w->next, count++)
		if (w->type != CSS_NODE_IDENT)
			return;

	w = v;
	switch (count) {
		case 1: /* one value: applies to all sides */
			parse_border_style_side(s, w, TOP);
			parse_border_style_side(s, w, RIGHT);
			parse_border_style_side(s, w, BOTTOM);
			parse_border_style_side(s, w, LEFT);
			break;
		case 2: /* (top and bottom), (left and right) */
			parse_border_style_side(s, w, TOP);
			parse_border_style_side(s, w, BOTTOM);
			w = w->next;
			parse_border_style_side(s, w, RIGHT);
			parse_border_style_side(s, w, LEFT);
			break;
		case 3: /* top, (left and right), bottom */
			parse_border_style_side(s, w, TOP);
			w = w->next;
			parse_border_style_side(s, w, RIGHT);
			parse_border_style_side(s, w, LEFT);
			w = w->next;
			parse_border_style_side(s, w, BOTTOM);
			break;
		case 4: /* top, right, bottom, left */
			parse_border_style_side(s, w, TOP);
			w = w->next;
			parse_border_style_side(s, w, RIGHT);
			w = w->next;
			parse_border_style_side(s, w, BOTTOM);
			w = w->next;
			parse_border_style_side(s, w, LEFT);
			break;
	}
}

#define PARSE_BORDER_STYLE(side, z) \
void parse_border_ ## side ## _style(struct css_style * const s, \
		const struct css_node * const v)	\
{							\
	if (v->next != 0 || v->type != CSS_NODE_IDENT)	\
		return;					\
	parse_border_style_side(s, v, z);		\
}

PARSE_BORDER_STYLE(top,    TOP)
PARSE_BORDER_STYLE(right,  RIGHT)
PARSE_BORDER_STYLE(bottom, BOTTOM)
PARSE_BORDER_STYLE(left,   LEFT)

void parse_border_style_side(struct css_style * const s,
		const struct css_node * const v, unsigned int i)
{
	css_border_style z = css_border_style_parse(v->data, v->data_length);
	if (z != CSS_BORDER_STYLE_UNKNOWN)
		s->border[i].style = z;
}

void parse_border(struct css_style * const s,
		const struct css_node * const v)
{
	parse_border_side(s, v, TOP);
	parse_border_side(s, v, RIGHT);
	parse_border_side(s, v, BOTTOM);
	parse_border_side(s, v, LEFT);
}

#define PARSE_BORDER(side, z) \
void parse_border_ ## side(struct css_style * const s,	\
		const struct css_node * const v)	\
{							\
	parse_border_side(s, v, z);			\
}

PARSE_BORDER(top,    TOP)
PARSE_BORDER(right,  RIGHT)
PARSE_BORDER(bottom, BOTTOM)
PARSE_BORDER(left,   LEFT)

void parse_border_side(struct css_style * const s,
		const struct css_node *v, unsigned int i)
{
	colour c;
	css_border_style z;

	if (!v->next && v->type == CSS_NODE_IDENT &&
			v->data_length == 7 &&
			strncasecmp(v->data, "inherit", 7) == 0) {
		s->border[i].color = CSS_COLOR_INHERIT;
		s->border[i].width.width = CSS_BORDER_WIDTH_INHERIT;
		s->border[i].style = CSS_BORDER_STYLE_INHERIT;
		return;
	}

	for (; v; v = v->next) {
		c = parse_colour(v);
		if (c != CSS_COLOR_NONE) {
			s->border[i].color = c;
			continue;
		}

		if (v->type == CSS_NODE_IDENT) {
			z = css_border_style_parse(v->data, v->data_length);
			if (z != CSS_BORDER_STYLE_UNKNOWN) {
				s->border[i].style = z;
				continue;
			}
		}

		parse_border_width_side(s, v, i);
	}
}

void parse_clear(struct css_style * const s, const struct css_node * const v)
{
	css_clear z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_clear_parse(v->data, v->data_length);
	if (z != CSS_CLEAR_UNKNOWN)
		s->clear = z;
}

void parse_color(struct css_style * const s, const struct css_node * const v)
{
	colour c = parse_colour(v);
	if (c != CSS_COLOR_NONE)
		s->color = c;
}

void parse_cursor(struct css_style * const s, const struct css_node * v)
{
	css_cursor z;
	for (; v; v = v->next) {
		switch (v->type) {
			case CSS_NODE_IDENT:
				z = css_cursor_parse(v->data, v->data_length);
				if (z != CSS_CURSOR_UNKNOWN) {
					s->cursor = z;
					return;
				}
				break;
			default:
				break;
		}
	}
}

void parse_display(struct css_style * const s, const struct css_node * const v)
{
	css_display z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_display_parse(v->data, v->data_length);
	if (z != CSS_DISPLAY_UNKNOWN)
		s->display = z;
}

void parse_float(struct css_style * const s, const struct css_node * const v)
{
	css_float z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_float_parse(v->data, v->data_length);
	if (z != CSS_FLOAT_UNKNOWN)
		s->float_ = z;
}

void parse_font(struct css_style * const s, const struct css_node * v)
{
	css_font_family ff;
	css_font_style fs;
	css_font_variant fv;
	css_font_weight fw;
	s->font_family = CSS_FONT_FAMILY_SANS_SERIF;
	s->font_style = CSS_FONT_STYLE_NORMAL;
	s->font_weight = CSS_FONT_WEIGHT_NORMAL;
	s->line_height.size = CSS_LINE_HEIGHT_ABSOLUTE;
	s->line_height.value.absolute = 1.3;
	for (; v; v = v->next) {
		switch (v->type) {
			case CSS_NODE_IDENT:
			        /* font-family */
			        ff = css_font_family_parse(v->data,
			        		v->data_length);
			        if (ff != CSS_FONT_FAMILY_UNKNOWN) {
			                s->font_family = ff;
			                break;
			        }
				/* font-style, font-variant, or font-weight */
				fs = css_font_style_parse(v->data,
						v->data_length);
				if (fs != CSS_FONT_STYLE_UNKNOWN) {
					s->font_style = fs;
					break;
				}
				fv = css_font_variant_parse(v->data,
						v->data_length);
				if (fv != CSS_FONT_VARIANT_UNKNOWN) {
				        s->font_variant = fv;
				        break;
				}
				fw = css_font_weight_parse(v->data,
						v->data_length);
				if (fw != CSS_FONT_WEIGHT_UNKNOWN) {
					s->font_weight = fw;
					break;
				}
			case CSS_NODE_PERCENTAGE:
			case CSS_NODE_DIMENSION:
				parse_font_size(s, v);
				break;
			case CSS_NODE_DELIM:
				if (v->data[0] == '/' && v->data_length == 1 &&
						v->next) {
					v = v->next;
					parse_line_height(s, v);
				}
				break;
			default:
				break;
		}
	}
}

void parse_font_family(struct css_style * const s, const struct css_node * v)
{
	css_font_family z;
	for (; v; v = v->next) {
		switch (v->type) {
			case CSS_NODE_IDENT:
				z = css_font_family_parse(v->data,
						v->data_length);
				if (z != CSS_FONT_FAMILY_UNKNOWN) {
					s->font_family = z;
					return;
				}
				break;
			default:
				break;
		}
	}
}

void parse_font_size(struct css_style * const s, const struct css_node * const v)
{
	char font_size_name[10];
	struct css_font_size_entry *fs;
	switch (v->type) {
		case CSS_NODE_IDENT:
			if (9 < v->data_length)
				break;
			strncpy(font_size_name, v->data, v->data_length);
			font_size_name[v->data_length] = 0;
			fs = bsearch(font_size_name, css_font_size_table,
					sizeof css_font_size_table /
						sizeof css_font_size_table[0],
					sizeof css_font_size_table[0],
					(int (*)(const void *, const void *))
							strcasecmp);
			if (fs != 0) {
				s->font_size.size = CSS_FONT_SIZE_LENGTH;
				s->font_size.value.length.unit = CSS_UNIT_PT;
				s->font_size.value.length.value = fs->size *
						option_font_size / 10;
			} else if (v->data_length == 6 &&
					strncasecmp(v->data, "larger", 6) == 0) {
				s->font_size.size = CSS_FONT_SIZE_PERCENT;
				s->font_size.value.percent = SIZE_FACTOR * 100;
			} else if (v->data_length == 7 &&
					strncasecmp(v->data, "smaller", 7) == 0) {
				s->font_size.size = CSS_FONT_SIZE_PERCENT;
				s->font_size.value.percent = 1 / SIZE_FACTOR * 100;
		        }
		        break;

		case CSS_NODE_PERCENTAGE:
			s->font_size.size = CSS_FONT_SIZE_PERCENT;
			s->font_size.value.percent = atof(v->data);
			break;

		case CSS_NODE_DIMENSION:
			if (parse_length(&s->font_size.value.length, v, true) == 0)
				s->font_size.size = CSS_FONT_SIZE_LENGTH;
			break;

		default:
			break;
	}
}

void parse_font_style(struct css_style * const s, const struct css_node * const v)
{
	css_font_style z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_font_style_parse(v->data, v->data_length);
	if (z != CSS_FONT_STYLE_UNKNOWN)
		s->font_style = z;
}

void parse_font_variant(struct css_style * const s, const struct css_node * const v)
{
	css_font_variant z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_font_variant_parse(v->data, v->data_length);
	if (z != CSS_FONT_VARIANT_UNKNOWN)
		s->font_variant = z;
}

void parse_font_weight(struct css_style * const s, const struct css_node * const v)
{
	css_font_weight z;
	if ((v->type != CSS_NODE_IDENT && v->type != CSS_NODE_NUMBER) || v->next != 0)
		return;
	z = css_font_weight_parse(v->data, v->data_length);
	if (z != CSS_FONT_WEIGHT_UNKNOWN)
		s->font_weight = z;
}

void parse_height(struct css_style * const s, const struct css_node * const v)
{
	if (v->type == CSS_NODE_IDENT && v->data_length == 4 &&
			strncasecmp(v->data, "auto", 4) == 0)
		s->height.height = CSS_HEIGHT_AUTO;
	else if ((v->type == CSS_NODE_DIMENSION || v->type == CSS_NODE_NUMBER) &&
			parse_length(&s->height.length, v, true) == 0)
		s->height.height = CSS_HEIGHT_LENGTH;
}

void parse_line_height(struct css_style * const s, const struct css_node * const v)
{
	if (v->type == CSS_NODE_IDENT && v->data_length == 6 &&
			strncasecmp(v->data, "normal", 6) == 0) {
		s->line_height.size = CSS_LINE_HEIGHT_ABSOLUTE;
		s->line_height.value.absolute = 1.3;
	} else if (v->type == CSS_NODE_PERCENTAGE) {
		s->line_height.size = CSS_LINE_HEIGHT_PERCENT;
		s->line_height.value.percent = atof(v->data);
	} else if (v->type == CSS_NODE_DIMENSION &&
			parse_length(&s->line_height.value.length, v, true) == 0) {
		s->line_height.size = CSS_LINE_HEIGHT_LENGTH;
	} else if (v->type == CSS_NODE_NUMBER) {
		s->line_height.size = CSS_LINE_HEIGHT_ABSOLUTE;
		s->line_height.value.absolute = atof(v->data);
	}
}

void parse_margin(struct css_style * const s, const struct css_node * const v)
{
	unsigned int count = 0;
	const struct css_node *w;

	for (w = v; w; w = w->next, count++)
		if (!((w->type == CSS_NODE_IDENT && (
				  (w->data_length == 7 &&
				   strncasecmp(w->data, "inherit", 7) == 0) ||
				  (w->data_length == 4 &&
				   strncasecmp(w->data, "auto", 4) == 0))) ||
				(w->type == CSS_NODE_PERCENTAGE) ||
				(w->type == CSS_NODE_DIMENSION) ||
				(w->type == CSS_NODE_NUMBER)))
			return;

	w = v;
	switch (count) {
		case 1: /* one value: applies to all sides */
			parse_margin_side(s, w, TOP);
			parse_margin_side(s, w, RIGHT);
			parse_margin_side(s, w, BOTTOM);
			parse_margin_side(s, w, LEFT);
			break;
		case 2: /* (top and bottom), (left and right) */
			parse_margin_side(s, w, TOP);
			parse_margin_side(s, w, BOTTOM);
			w = w->next;
			parse_margin_side(s, w, RIGHT);
			parse_margin_side(s, w, LEFT);
			break;
		case 3: /* top, (left and right), bottom */
			parse_margin_side(s, w, TOP);
			w = w->next;
			parse_margin_side(s, w, RIGHT);
			parse_margin_side(s, w, LEFT);
			w = w->next;
			parse_margin_side(s, w, BOTTOM);
			break;
		case 4: /* top, right, bottom, left */
			parse_margin_side(s, w, TOP);
			w = w->next;
			parse_margin_side(s, w, RIGHT);
			w = w->next;
			parse_margin_side(s, w, BOTTOM);
			w = w->next;
			parse_margin_side(s, w, LEFT);
			break;
	}
}

#define PARSE_MARGIN_(side, z) \
void parse_margin_ ## side(struct css_style * const s,	\
		const struct css_node * const v)	\
{							\
	if (v->next != 0)				\
		return;					\
	parse_margin_side(s, v, z);			\
}

PARSE_MARGIN_(top,    TOP)
PARSE_MARGIN_(right,  RIGHT)
PARSE_MARGIN_(bottom, BOTTOM)
PARSE_MARGIN_(left,   LEFT)

void parse_margin_side(struct css_style * const s, const struct css_node * const v,
		unsigned int i)
{
	if (v->type == CSS_NODE_IDENT && v->data_length == 7 &&
			strncasecmp(v->data, "inherit", 7) == 0)
		s->margin[i].margin = CSS_MARGIN_INHERIT;
	else if (v->type == CSS_NODE_IDENT && v->data_length == 4 &&
			strncasecmp(v->data, "auto", 4) == 0)
		s->margin[i].margin = CSS_MARGIN_AUTO;
	else if (v->type == CSS_NODE_PERCENTAGE) {
		s->margin[i].margin = CSS_MARGIN_PERCENT;
		s->margin[i].value.percent = atof(v->data);
	} else if ((v->type == CSS_NODE_DIMENSION || v->type == CSS_NODE_NUMBER) &&
			parse_length(&s->margin[i].value.length, v, false) == 0) {
		s->margin[i].margin = CSS_MARGIN_LENGTH;
	}
}

void parse_padding(struct css_style * const s, const struct css_node * const v)
{
	unsigned int count = 0;
	const struct css_node *w;

	for (w = v; w; w = w->next, count++)
		if (!((w->type == CSS_NODE_IDENT && w->data_length == 7 &&
				 strncasecmp(w->data, "inherit", 7) == 0) ||
				(w->type == CSS_NODE_PERCENTAGE) ||
				(w->type == CSS_NODE_DIMENSION) ||
				(w->type == CSS_NODE_NUMBER)))
			return;

	w = v;
	switch (count) {
		case 1: /* one value: applies to all sides */
			parse_padding_side(s, w, TOP);
			parse_padding_side(s, w, RIGHT);
			parse_padding_side(s, w, BOTTOM);
			parse_padding_side(s, w, LEFT);
			break;
		case 2: /* (top and bottom), (left and right) */
			parse_padding_side(s, w, TOP);
			parse_padding_side(s, w, BOTTOM);
			w = w->next;
			parse_padding_side(s, w, RIGHT);
			parse_padding_side(s, w, LEFT);
			break;
		case 3: /* top, (left and right), bottom */
			parse_padding_side(s, w, TOP);
			w = w->next;
			parse_padding_side(s, w, RIGHT);
			parse_padding_side(s, w, LEFT);
			w = w->next;
			parse_padding_side(s, w, BOTTOM);
			break;
		case 4: /* top, right, bottom, left */
			parse_padding_side(s, w, TOP);
			w = w->next;
			parse_padding_side(s, w, RIGHT);
			w = w->next;
			parse_padding_side(s, w, BOTTOM);
			w = w->next;
			parse_padding_side(s, w, LEFT);
			break;
	}
}

#define PARSE_PADDING_(side, z) \
void parse_padding_ ## side(struct css_style * const s,	\
		const struct css_node * const v)	\
{							\
	if (v->next != 0)				\
		return;					\
	parse_padding_side(s, v, z);			\
}

PARSE_PADDING_(top,    TOP)
PARSE_PADDING_(right,  RIGHT)
PARSE_PADDING_(bottom, BOTTOM)
PARSE_PADDING_(left,   LEFT)

void parse_padding_side(struct css_style * const s, const struct css_node * const v,
		unsigned int i)
{
	if (v->type == CSS_NODE_IDENT && v->data_length == 7 &&
			strncasecmp(v->data, "inherit", 7) == 0)
		s->padding[i].padding = CSS_PADDING_INHERIT;
	else if (v->type == CSS_NODE_PERCENTAGE) {
		s->padding[i].padding = CSS_PADDING_PERCENT;
		s->padding[i].value.percent = atof(v->data);
	} else if ((v->type == CSS_NODE_DIMENSION || v->type == CSS_NODE_NUMBER) &&
			parse_length(&s->padding[i].value.length, v, true) == 0) {
		s->padding[i].padding = CSS_PADDING_LENGTH;
	}
}

void parse_text_align(struct css_style * const s, const struct css_node * const v)
{
	css_text_align z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_text_align_parse(v->data, v->data_length);
	if (z != CSS_TEXT_ALIGN_UNKNOWN)
		s->text_align = z;
}

void parse_text_indent(struct css_style * const s, const struct css_node * const v)
{
	if (v->type == CSS_NODE_IDENT) {
	        return;
	} else if (v->type == CSS_NODE_PERCENTAGE) {
	        s->text_indent.size = CSS_TEXT_INDENT_PERCENT;
	        s->text_indent.value.percent = atof(v->data);
	} else if ((v->type == CSS_NODE_DIMENSION || v->type == CSS_NODE_NUMBER) &&
			parse_length(&s->text_indent.value.length, v, true) == 0) {
	        s->text_indent.size = CSS_TEXT_INDENT_LENGTH;
	}
}

void parse_text_decoration(struct css_style * const s, const struct css_node * const v)
{
        struct css_node *temp;
	css_text_decoration z;
	if (v->type != CSS_NODE_IDENT)
		return;
	z = css_text_decoration_parse(v->data, v->data_length);
	if (z == CSS_TEXT_DECORATION_INHERIT || z == CSS_TEXT_DECORATION_NONE) {
		if (v->next != 0)
			return;
		s->text_decoration = z;
	}
	if (z != CSS_TEXT_DECORATION_UNKNOWN)
		s->text_decoration |= z;
	for (temp = v->next; temp; temp = temp->next) {
		z = css_text_decoration_parse(temp->data, temp->data_length);
		if (z != CSS_TEXT_DECORATION_UNKNOWN)
			s->text_decoration |= z;
	}
}

void parse_text_transform(struct css_style * const s, const struct css_node * const v)
{
	css_text_transform z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_text_transform_parse(v->data, v->data_length);
	if (z != CSS_TEXT_TRANSFORM_UNKNOWN)
		s->text_transform = z;
}

void parse_visibility(struct css_style * const s, const struct css_node * const v)
{
	css_visibility z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_visibility_parse(v->data, v->data_length);
	if (z != CSS_VISIBILITY_UNKNOWN)
		s->visibility = z;
}

void parse_width(struct css_style * const s, const struct css_node * const v)
{
	if (v->type == CSS_NODE_IDENT && v->data_length == 4 &&
			strncasecmp(v->data, "auto", 4) == 0)
		s->width.width = CSS_WIDTH_AUTO;
	else if (v->type == CSS_NODE_PERCENTAGE) {
		s->width.width = CSS_WIDTH_PERCENT;
		s->width.value.percent = atof(v->data);
	} else if ((v->type == CSS_NODE_DIMENSION || v->type == CSS_NODE_NUMBER) &&
			parse_length(&s->width.value.length, v, true) == 0)
		s->width.width = CSS_WIDTH_LENGTH;
}

void parse_white_space(struct css_style * const s, const struct css_node * const v)
{
	css_white_space z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_white_space_parse(v->data, v->data_length);
	if (z != CSS_WHITE_SPACE_UNKNOWN)
		s->white_space = z;
}

css_text_decoration css_text_decoration_parse(const char * const s,
		int length)
{
	if (length == 7 && strncasecmp(s, "inherit", 7) == 0)
		return CSS_TEXT_DECORATION_INHERIT;
	if (length == 4 && strncasecmp(s, "none", 4) == 0)
		return CSS_TEXT_DECORATION_NONE;
	if (length == 5 && strncasecmp(s, "blink", 5) == 0)
		return CSS_TEXT_DECORATION_BLINK;
	if (length == 12 && strncasecmp(s, "line-through", 12) == 0)
		return CSS_TEXT_DECORATION_LINE_THROUGH;
	if (length == 8 && strncasecmp(s, "overline", 8) == 0)
		return CSS_TEXT_DECORATION_OVERLINE;
	if (length == 9 && strncasecmp(s, "underline", 9) == 0)
		return CSS_TEXT_DECORATION_UNDERLINE;
	return CSS_TEXT_DECORATION_UNKNOWN;
}


/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
#include "netsurf/utils/utils.h"


struct property_entry {
	const char name[20];
	void (*parse) (struct css_style * const s, const struct css_node * const v);
};

struct colour_entry {
	const char name[12];
	colour col;
};

struct font_size_entry {
	const char name[10];
	float size;
};


static int compare_selectors(const struct css_node *n0, const struct css_node *n1);
static int parse_length(struct css_length * const length,
		const struct css_node * const v, bool non_negative);
static colour parse_colour(const struct css_node * const v);
static void parse_background(struct css_style * const s, const struct css_node * v);
static void parse_background_color(struct css_style * const s, const struct css_node * const v);
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
static css_text_decoration css_text_decoration_parse(const char * const s);


/* table of property parsers: MUST be sorted by property name */
static const struct property_entry property_table[] = {
	{ "background",       parse_background },
	{ "background-color", parse_background_color },
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

/* table of standard colour names: MUST be sorted by colour name
 * note: colour is 0xbbggrr */
static const struct colour_entry colour_table[] = {
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

/* table of font sizes: MUST be sorted by name */
#define SIZE_FACTOR 1.2
static const struct font_size_entry font_size_table[] = {
	{ "large", 1.0 * SIZE_FACTOR },
	{ "medium", 1.0 },
	{ "small", 1.0 / SIZE_FACTOR },
	{ "x-large", 1.0 * SIZE_FACTOR * SIZE_FACTOR },
	{ "x-small", 1.0 / (SIZE_FACTOR * SIZE_FACTOR) },
	{ "xx-large", 1.0 * SIZE_FACTOR * SIZE_FACTOR * SIZE_FACTOR },
	{ "xx-small", 1.0 / (SIZE_FACTOR * SIZE_FACTOR * SIZE_FACTOR) },
};


/**
 * css_add_ruleset -- add a ruleset to a stylesheet
 */

void css_add_ruleset(struct content *c,
		struct css_node *selector,
		struct css_node *declaration)
{
	bool found;
	struct css_stylesheet *stylesheet = c->data.css.css;
	struct css_node *n, *sel, *next_sel, *prev;
	struct css_style *style;
	unsigned int hash;

	for (sel = selector; sel != 0; sel = next_sel) {
		next_sel = sel->next;

		/*LOG(("+++"));
		for (n = sel; n != 0; n = n->right) {
			struct css_node *m;
			if (n->data != 0)
				fprintf(stderr, "%s", n->data);
			for (m = n->left; m != 0; m = m->next) {
				switch (m->type) {
					case CSS_NODE_ID: fprintf(stderr, "%s", m->data); break;
					case CSS_NODE_CLASS: fprintf(stderr, ".%s", m->data); break;
					default: fprintf(stderr, "unexpected node");
				}
			}
			fprintf(stderr, " ");
		}
		fprintf(stderr, "\n");*/

		/* check if this selector is already present */
		found = false;
		prev = 0;
		hash = css_hash(sel->data);
		/* selectors are ordered by specificity in the hash chain */
		for (n = stylesheet->rule[hash];
				n && n->specificity < sel->specificity;
				n = n->next)
			prev = n;
		for ( ;	n && n->specificity == sel->specificity;
				n = n->next) {
			prev = n;
			if (compare_selectors(sel, n)) {
				found = true;
				break;
			}
		}
		if (!found) {
			/* not present: construct a new struct css_style */
			LOG(("constructing new style"));
			style = xcalloc(1, sizeof(*style));
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
			css_free_node(sel);
		}

		/* fill in the declarations */
		css_add_declarations(style, declaration);
	}
}


void css_add_declarations(struct css_style *style, struct css_node *declaration)
{
	struct css_node *n;
	for (n = declaration; n != 0; n = n->next) {
		struct property_entry *p;
		assert(n->type == CSS_NODE_DECLARATION && n->data != 0 && n->left != 0);
		p = bsearch(n->data, property_table,
				sizeof(property_table) / sizeof(property_table[0]),
				sizeof(property_table[0]), (void*)strcasecmp);
		if (p == 0)
			continue;
		p->parse(style, n->left);
	}
}


int compare_selectors(const struct css_node *n0, const struct css_node *n1)
{
	struct css_node *m0, *m1;
	unsigned int count0 = 0, count1 = 0;

	/* compare element name */
	if (!((n0->data == 0 && n1->data == 0) ||
	      (n0->data != 0 && n1->data != 0 && strcmp(n0->data, n1->data) == 0)))
		return 0;

	if (n0->comb != n1->comb)
		return 0;

	/* compare classes and ids */
	for (m0 = n0->left; m0 != 0; m0 = m0->next)
		count0++;
	for (m1 = n1->left; m1 != 0; m1 = m1->next)
		count1++;
	if (count0 != count1)
		return 0;
	for (m0 = n0->left; m0 != 0; m0 = m0->next) {
		int found = 0;
		for (m1 = n1->left; m1 != 0; m1 = m1->next) {
			/* TODO: should this be case sensitive for IDs? */
			if (m0->type == m1->type &&
					strcasecmp(m0->data, m1->data) == 0 &&
					((m0->data2 == 0 && m1->data2 == 0) ||
					 strcasecmp(m0->data2, m1->data2) == 0)) {
				found = 1;
				break;
			}
		}
		if (!found)
			return 0;
	}

	/* compare ancestors */
	if (n0->comb == CSS_COMB_NONE)
		return 1;

	return compare_selectors(n0->right, n1->right);
}



/**
 * property parsers
 */

/* TODO: consider CSS_NODE_NUMBER whenever a value may be '0' */

int parse_length(struct css_length * const length,
		const struct css_node * const v, bool non_negative)
{
	css_unit u;
	float value;
	if (v->type == CSS_NODE_NUMBER && atof(v->data) == 0) {
		length->unit = CSS_UNIT_EM;
		length->value = 0;
		return 0;
	}
	if (v->type != CSS_NODE_DIMENSION)
		return 1;
	u = css_unit_parse(v->data + strspn(v->data, "0123456789+-."));
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
	struct colour_entry *col;
	unsigned int r, g, b;

	col = bsearch(name, colour_table,
			sizeof(colour_table) / sizeof(colour_table[0]),
			sizeof(colour_table[0]), (void*)strcasecmp);
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
	int len;
	unsigned int r, g, b;
	struct colour_entry *col;

	switch (v->type) {
		case CSS_NODE_HASH:
			len = strlen(v->data);
			if (len == 4) {
				if (sscanf(v->data + 1, "%1x%1x%1x", &r, &g, &b) == 3)
					c = (b << 20) | (b << 16) | (g << 12) | (g << 8) | (r << 4) | r;
			} else if (len == 7) {
				if (sscanf(v->data + 1, "%2x%2x%2x", &r, &g, &b) == 3)
					c = (b << 16) | (g << 8) | r;
			}
			break;

		case CSS_NODE_FUNCTION:
			/* TODO: rgb(r, g, b) */
			break;

		case CSS_NODE_IDENT:
			col = bsearch(v->data, colour_table,
					sizeof(colour_table) / sizeof(colour_table[0]),
					sizeof(colour_table[0]), (void*)strcasecmp);
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

void parse_background_color(struct css_style * const s, const struct css_node * const v)
{
	colour c = parse_colour(v);
	if (c != CSS_COLOR_NONE)
		s->background_color = c;
}

void parse_border_width(struct css_style * const s,
		const struct css_node * const v)
{
	unsigned int count = 0;
	const struct css_node *w;

	for (w = v; w; w = w->next, count++)
		if (!((w->type == CSS_NODE_IDENT && (
				  (strcasecmp(w->data, "inherit") == 0) ||
				  (strcasecmp(w->data, "thin") == 0) ||
				  (strcasecmp(w->data, "medium") == 0) ||
				  (strcasecmp(w->data, "thick") == 0))) ||
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
		if (strcasecmp(v->data, "inherit") == 0)
			s->border[i].width.width = CSS_BORDER_WIDTH_INHERIT;
		else if (strcasecmp(v->data, "thin") == 0) {
			s->border[i].width.width = CSS_BORDER_WIDTH_LENGTH;
			s->border[i].width.value.value = 1;
			s->border[i].width.value.unit = CSS_UNIT_PX;
		} else if (strcasecmp(v->data, "medium") == 0) {
			s->border[i].width.width = CSS_BORDER_WIDTH_LENGTH;
			s->border[i].width.value.value = 2;
			s->border[i].width.value.unit = CSS_UNIT_PX;
		} else if (strcasecmp(v->data, "thick") == 0) {
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
	css_border_style z = css_border_style_parse(v->data);
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
			strcasecmp(v->data, "inherit") == 0) {
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
			z = css_border_style_parse(v->data);
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
	z = css_clear_parse(v->data);
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
				z = css_cursor_parse(v->data);
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
	z = css_display_parse(v->data);
	if (z != CSS_DISPLAY_UNKNOWN)
		s->display = z;
}

void parse_float(struct css_style * const s, const struct css_node * const v)
{
	css_float z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_float_parse(v->data);
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
			        ff = css_font_family_parse(v->data);
			        if (ff != CSS_FONT_FAMILY_UNKNOWN) {
			                s->font_family = ff;
			                break;
			        }
				/* font-style, font-variant, or font-weight */
				fs = css_font_style_parse(v->data);
				if (fs != CSS_FONT_STYLE_UNKNOWN) {
					s->font_style = fs;
					break;
				}
				fv = css_font_variant_parse(v->data);
				if (fv != CSS_FONT_VARIANT_UNKNOWN) {
				        s->font_variant = fv;
				        break;
				}
				fw = css_font_weight_parse(v->data);
				if (fw != CSS_FONT_WEIGHT_UNKNOWN) {
					s->font_weight = fw;
					break;
				}
			case CSS_NODE_PERCENTAGE:
			case CSS_NODE_DIMENSION:
				parse_font_size(s, v);
				break;
			case CSS_NODE_DELIM:
				if (v->data[0] == '/' && v->data[1] == 0 &&
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
				z = css_font_family_parse(v->data);
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
	struct font_size_entry *fs;
	switch (v->type) {
		case CSS_NODE_IDENT:
			fs = bsearch(v->data, font_size_table,
					sizeof(font_size_table) / sizeof(font_size_table[0]),
					sizeof(font_size_table[0]), (void*)strcasecmp);
			if (fs != 0) {
				s->font_size.size = CSS_FONT_SIZE_LENGTH;
				s->font_size.value.length.unit = CSS_UNIT_PT;
				s->font_size.value.length.value = fs->size *
						option_font_size / 10;
			} else if (strcasecmp(v->data, "larger") == 0) {
				s->font_size.size = CSS_FONT_SIZE_PERCENT;
				s->font_size.value.percent = SIZE_FACTOR * 100;
			} else if (strcmp(v->data, "smaller") == 0) {
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
	z = css_font_style_parse(v->data);
	if (z != CSS_FONT_STYLE_UNKNOWN)
		s->font_style = z;
}

void parse_font_variant(struct css_style * const s, const struct css_node * const v)
{
	css_font_variant z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_font_variant_parse(v->data);
	if (z != CSS_FONT_VARIANT_UNKNOWN)
		s->font_variant = z;
}

void parse_font_weight(struct css_style * const s, const struct css_node * const v)
{
	css_font_weight z;
	if ((v->type != CSS_NODE_IDENT && v->type != CSS_NODE_NUMBER) || v->next != 0)
		return;
	z = css_font_weight_parse(v->data);
	if (z != CSS_FONT_WEIGHT_UNKNOWN)
		s->font_weight = z;
}

void parse_height(struct css_style * const s, const struct css_node * const v)
{
	if (v->type == CSS_NODE_IDENT && strcasecmp(v->data, "auto") == 0)
		s->height.height = CSS_HEIGHT_AUTO;
	else if ((v->type == CSS_NODE_DIMENSION || v->type == CSS_NODE_NUMBER) &&
			parse_length(&s->height.length, v, true) == 0)
		s->height.height = CSS_HEIGHT_LENGTH;
}

void parse_line_height(struct css_style * const s, const struct css_node * const v)
{
	if (v->type == CSS_NODE_IDENT && strcasecmp(v->data, "normal") == 0) {
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
				  (strcasecmp(w->data, "inherit") == 0) ||
				  (strcasecmp(w->data, "auto") == 0))) ||
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
	if (v->type == CSS_NODE_IDENT && strcasecmp(v->data, "inherit") == 0)
		s->margin[i].margin = CSS_MARGIN_INHERIT;
	else if (v->type == CSS_NODE_IDENT && strcasecmp(v->data, "auto") == 0)
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
		if (!((w->type == CSS_NODE_IDENT && strcasecmp(w->data, "inherit") == 0) ||
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
	if (v->type == CSS_NODE_IDENT && strcasecmp(v->data, "inherit") == 0)
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
	z = css_text_align_parse(v->data);
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
	z = css_text_decoration_parse(v->data);
	if (z == CSS_TEXT_DECORATION_INHERIT || z == CSS_TEXT_DECORATION_NONE) {
		if (v->next != 0)
			return;
		s->text_decoration = z;
	}
	if (z != CSS_TEXT_DECORATION_UNKNOWN)
		s->text_decoration |= z;
	for (temp = v->next; temp; temp = temp->next) {
		z = css_text_decoration_parse(temp->data);
		if (z != CSS_TEXT_DECORATION_UNKNOWN)
			s->text_decoration |= z;
	}
}

void parse_text_transform(struct css_style * const s, const struct css_node * const v)
{
	css_text_transform z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_text_transform_parse(v->data);
	if (z != CSS_TEXT_TRANSFORM_UNKNOWN)
		s->text_transform = z;
}

void parse_visibility(struct css_style * const s, const struct css_node * const v)
{
	css_visibility z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_visibility_parse(v->data);
	if (z != CSS_VISIBILITY_UNKNOWN)
		s->visibility = z;
}

void parse_width(struct css_style * const s, const struct css_node * const v)
{
	if (v->type == CSS_NODE_IDENT && strcasecmp(v->data, "auto") == 0)
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
	z = css_white_space_parse(v->data);
	if (z != CSS_WHITE_SPACE_UNKNOWN)
		s->white_space = z;
}

css_text_decoration css_text_decoration_parse(const char * const s)
{
	if (strcasecmp(s, "inherit") == 0) return CSS_TEXT_DECORATION_INHERIT;
	if (strcasecmp(s, "none") == 0) return CSS_TEXT_DECORATION_NONE;
	if (strcasecmp(s, "blink") == 0) return CSS_TEXT_DECORATION_BLINK;
	if (strcasecmp(s, "line-through") == 0) return CSS_TEXT_DECORATION_LINE_THROUGH;
	if (strcasecmp(s, "overline") == 0) return CSS_TEXT_DECORATION_OVERLINE;
	if (strcasecmp(s, "underline") == 0) return CSS_TEXT_DECORATION_UNDERLINE;
	return CSS_TEXT_DECORATION_UNKNOWN;
}


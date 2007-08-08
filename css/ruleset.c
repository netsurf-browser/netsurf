/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
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
 * CSS ruleset parsing.
 *
 * This file implements the last stage of CSS parsing. It converts trees of
 * struct css_node produced by the parser into struct style, and adds them to a
 * stylesheet.
 *
 * This code is complicated by the CSS error handling rules. According to
 * CSS 2.1 4.2 "Illegal values", the whole of a declaration must be legal for
 * any of it to be used.
 */

#define _GNU_SOURCE  /* for strndup */
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#define CSS_INTERNALS
#define NDEBUG
#include "css/css.h"
#include "content/content.h"
#include "desktop/options.h"
#include "utils/log.h"
#include "utils/url.h"
#include "utils/utils.h"



static bool css_compare_selectors(const struct css_selector *n0,
		const struct css_selector *n1);
static int parse_length(struct css_length * const length,
		const struct css_node * const v, bool non_negative);
static colour parse_colour(const struct css_node * const v);
static colour css_parse_rgb(struct css_node *v);
static bool parse_uri(const struct css_node *v, char **uri);
static struct css_content *parse_content_new(struct css_content **current, css_content_type_generated generated);
static bool parse_content_counter(struct css_content **current, struct css_node *t, bool counters);
bool parse_counter_control_data(struct css_counter_control **current, const struct css_node * v, int empty);
struct css_counter_control *parse_counter_control_new(struct css_counter_control **current);

static void parse_background(struct css_style * const s,
		const struct css_node * v);
static void parse_background_attachment(struct css_style * const s, const struct css_node * const v);
static void parse_background_color(struct css_style * const s, const struct css_node * const v);
static void parse_background_image(struct css_style * const s,
		const struct css_node * const v);
static bool css_background_image_parse(const struct css_node *v,
		css_background_image_type *type, char **uri);
static struct css_background_entry *css_background_lookup(
		const struct css_node *v);
static void parse_background_position(struct css_style * const s,
		const struct css_node * const v);
static bool css_background_position_parse(const struct css_node **node,
		struct css_background_position *horz,
		struct css_background_position *vert);
static void parse_background_repeat(struct css_style * const s, const struct css_node * const v);
static void parse_border(struct css_style * const s, const struct css_node * v);
static void parse_border_bottom(struct css_style * const s, const struct css_node * v);
static void parse_border_bottom_color(struct css_style * const s, const struct css_node * v);
static void parse_border_bottom_style(struct css_style * const s, const struct css_node * v);
static void parse_border_bottom_width(struct css_style * const s, const struct css_node * v);
static void parse_border_collapse(struct css_style * const s, const struct css_node * v);
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
static void parse_border_spacing(struct css_style * const s, const struct css_node * v);
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
static void parse_bottom(struct css_style * const s, const struct css_node * v);
static void parse_caption_side(struct css_style * const s, const struct css_node * v);
static void parse_clear(struct css_style * const s, const struct css_node * const v);
static void parse_clip(struct css_style * const s, const struct css_node * v);
static void parse_color(struct css_style * const s, const struct css_node * const v);
static void parse_content(struct css_style * const s, const struct css_node * v);
static void parse_counter_increment(struct css_style * const s, const struct css_node * v);
static void parse_counter_reset(struct css_style * const s, const struct css_node * v);
static void parse_cursor(struct css_style * const s, const struct css_node * v);
static void parse_direction(struct css_style * const s, const struct css_node * v);
static void parse_display(struct css_style * const s, const struct css_node * const v);
static void parse_empty_cells(struct css_style * const s, const struct css_node * v);
static void parse_float(struct css_style * const s, const struct css_node * const v);
static void parse_font(struct css_style * const s, const struct css_node * v);
static void parse_font_family(struct css_style * const s, const struct css_node * v);
static void parse_font_size(struct css_style * const s, const struct css_node * const v);
static void parse_font_style(struct css_style * const s, const struct css_node * const v);
static void parse_font_variant(struct css_style * const s, const struct css_node * const v);
static void parse_font_weight(struct css_style * const s, const struct css_node * const v);
static void parse_height(struct css_style * const s, const struct css_node * const v);
static void parse_left(struct css_style * const s, const struct css_node * v);
static void parse_letter_spacing(struct css_style * const s, const struct css_node * v);
static void parse_line_height(struct css_style * const s, const struct css_node * const v);
static void parse_list_style(struct css_style * const s, const struct css_node * v);
static void parse_list_style_image(struct css_style * const s, const struct css_node * v);
static bool css_list_style_image_parse(const struct css_node *v,
		css_list_style_image_type *type, char **uri);
static void parse_list_style_position(struct css_style * const s, const struct css_node * v);
static void parse_list_style_type(struct css_style * const s, const struct css_node * v);
static void parse_margin(struct css_style * const s, const struct css_node * const v);
static void parse_margin_bottom(struct css_style * const s, const struct css_node * const v);
static void parse_margin_left(struct css_style * const s, const struct css_node * const v);
static void parse_margin_right(struct css_style * const s, const struct css_node * const v);
static void parse_margin_top(struct css_style * const s, const struct css_node * const v);
static void parse_margin_side(struct css_style * const s, const struct css_node * const v,
		unsigned int i);
static void parse_max_height(struct css_style *const s, const struct css_node * v);
static void parse_max_width(struct css_style *const s, const struct css_node * v);
static void parse_min_height(struct css_style *const s, const struct css_node * v);
static void parse_min_width(struct css_style *const s, const struct css_node * v);
static void parse_orphans(struct css_style * const s, const struct css_node * const v);
static void parse_outline(struct css_style * const s, const struct css_node * v);
static void parse_outline_color(struct css_style * const s, const struct css_node * const v);
static void parse_outline_style(struct css_style * const s, const struct css_node * const v);
static void parse_outline_width(struct css_style * const s, const struct css_node * const v);
static bool css_outline_width_parse(const struct css_node * v, struct css_border_width * w);
static void parse_overflow(struct css_style * const s, const struct css_node * const v);
static void parse_padding(struct css_style * const s, const struct css_node * const v);
static void parse_padding_bottom(struct css_style * const s, const struct css_node * const v);
static void parse_padding_left(struct css_style * const s, const struct css_node * const v);
static void parse_padding_right(struct css_style * const s, const struct css_node * const v);
static void parse_padding_top(struct css_style * const s, const struct css_node * const v);
static void parse_padding_side(struct css_style * const s, const struct css_node * const v,
		unsigned int i);
static void parse_page_break_after(struct css_style * const s, const struct css_node * v);
static void parse_page_break_before(struct css_style * const s, const struct css_node * v);
static void parse_page_break_inside(struct css_style * const s, const struct css_node * v);
static void parse_pos(struct css_style * const s, const struct css_node * v, unsigned int i);
static void parse_position(struct css_style * const s, const struct css_node * v);
static void parse_right(struct css_style * const s, const struct css_node * v);
static void parse_table_layout(struct css_style * const s, const struct css_node * v);
static void parse_text_align(struct css_style * const s, const struct css_node * const v);
static void parse_text_decoration(struct css_style * const s, const struct css_node * const v);
static void parse_text_indent(struct css_style * const s, const struct css_node * const v);
static void parse_text_transform(struct css_style * const s, const struct css_node * const v);
static void parse_top(struct css_style * const s, const struct css_node * v);
static void parse_unicode_bidi(struct css_style * const s, const struct css_node * const v);
static void parse_vertical_align(struct css_style * const s, const struct css_node * v);
static void parse_visibility(struct css_style * const s, const struct css_node * const v);
static void parse_widows(struct css_style * const s, const struct css_node * const v);
static void parse_width(struct css_style * const s, const struct css_node * const v);
static void parse_white_space(struct css_style * const s, const struct css_node * const v);
static void parse_word_spacing(struct css_style * const s, const struct css_node * v);
static void parse_z_index(struct css_style * const s, const struct css_node * const v);
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
	{ "background",			parse_background },
	{ "background-attachment",	parse_background_attachment },
	{ "background-color",		parse_background_color },
	{ "background-image",		parse_background_image },
	{ "background-position",	parse_background_position },
	{ "background-repeat",		parse_background_repeat },
	{ "border",			parse_border },
	{ "border-bottom",		parse_border_bottom },
	{ "border-bottom-color",	parse_border_bottom_color },
	{ "border-bottom-style",	parse_border_bottom_style },
	{ "border-bottom-width",	parse_border_bottom_width },
	{ "border-collapse",		parse_border_collapse },
	{ "border-color",		parse_border_color },
	{ "border-left",		parse_border_left },
	{ "border-left-color",		parse_border_left_color },
	{ "border-left-style",		parse_border_left_style },
	{ "border-left-width",		parse_border_left_width },
	{ "border-right",		parse_border_right },
	{ "border-right-color",		parse_border_right_color },
	{ "border-right-style",		parse_border_right_style },
	{ "border-right-width",		parse_border_right_width },
	{ "border-spacing",		parse_border_spacing },
	{ "border-style",		parse_border_style },
	{ "border-top",			parse_border_top },
	{ "border-top-color",		parse_border_top_color },
	{ "border-top-style",		parse_border_top_style },
	{ "border-top-width",		parse_border_top_width },
	{ "border-width",		parse_border_width },
	{ "bottom",			parse_bottom },
	{ "caption-side",		parse_caption_side },
	{ "clear",			parse_clear },
	{ "clip",			parse_clip },
	{ "color",			parse_color },
	{ "content",			parse_content },
	{ "counter-increment",		parse_counter_increment },
	{ "counter-reset",		parse_counter_reset },
	{ "cursor",			parse_cursor },
	{ "direction",			parse_direction },
	{ "display",			parse_display },
	{ "empty-cells",		parse_empty_cells },
	{ "float",			parse_float },
	{ "font",			parse_font },
	{ "font-family",		parse_font_family },
	{ "font-size",			parse_font_size },
	{ "font-style",			parse_font_style },
	{ "font-variant",		parse_font_variant },
	{ "font-weight",		parse_font_weight },
	{ "height",			parse_height },
	{ "left",			parse_left },
	{ "letter-spacing",		parse_letter_spacing },
	{ "line-height",		parse_line_height },
	{ "list-style",			parse_list_style },
	{ "list-style-image",		parse_list_style_image },
	{ "list-style-position",	parse_list_style_position },
	{ "list-style-type",		parse_list_style_type },
	{ "margin",			parse_margin },
	{ "margin-bottom",		parse_margin_bottom },
	{ "margin-left",		parse_margin_left },
	{ "margin-right",		parse_margin_right },
	{ "margin-top",			parse_margin_top },
	{ "max-height",			parse_max_height },
	{ "max-width",			parse_max_width },
	{ "min-height",			parse_min_height },
	{ "min-width",			parse_min_width },
	{ "orphans",			parse_orphans },
	{ "outline",			parse_outline },
	{ "outline-color",		parse_outline_color },
	{ "outline-style",		parse_outline_style },
	{ "outline-width",		parse_outline_width },
	{ "overflow",			parse_overflow },
	{ "padding",			parse_padding },
	{ "padding-bottom",		parse_padding_bottom },
	{ "padding-left",		parse_padding_left },
	{ "padding-right",		parse_padding_right },
	{ "padding-top",		parse_padding_top },
	{ "page-break-after",		parse_page_break_after },
	{ "page-break-before",		parse_page_break_before },
	{ "page-break-inside",		parse_page_break_inside },
	{ "position",			parse_position },
	{ "right",			parse_right },
	{ "table-layout",		parse_table_layout },
	{ "text-align",			parse_text_align },
	{ "text-decoration",		parse_text_decoration },
	{ "text-indent",		parse_text_indent },
	{ "text-transform",		parse_text_transform },
	{ "top",			parse_top },
	{ "unicode-bidi",		parse_unicode_bidi },
	{ "vertical-align",		parse_vertical_align },
	{ "visibility",			parse_visibility },
	{ "white-space",		parse_white_space },
	{ "widows",			parse_widows },
	{ "width",			parse_width },
	{ "word-spacing",		parse_word_spacing },
	{ "z-index",			parse_z_index }
};


/** An entry in css_colour_table. */
struct css_colour_entry {
	const char name[21];
	colour col;
};

/* Table of standard colour names. MUST be sorted by colour name.
 * Note: colour is 0xbbggrr. */
static const struct css_colour_entry css_colour_table[] = {
	{ "aliceblue",			0xfff8f0 },
	{ "antiquewhite",		0xd7ebfa },
	{ "aqua",			0xffff00 },
	{ "aquamarine",			0xd4ff7f },
	{ "azure",			0xfffff0 },
	{ "beige",			0xdcf5f5 },
	{ "bisque",			0xc4e4ff },
	{ "black",			0x000000 },
	{ "blanchedalmond",		0xcdebff },
	{ "blue",			0xff0000 },
	{ "blueviolet",			0xe22b8a },
	{ "brown",			0x2a2aa5 },
	{ "burlywood",			0x87b8de },
	{ "cadetblue",			0xa09e5f },
	{ "chartreuse",			0x00ff7f },
	{ "chocolate",			0x1e69d2 },
	{ "coral",			0x507fff },
	{ "cornflowerblue",		0xed9564 },
	{ "cornsilk",			0xdcf8ff },
	{ "crimson",			0x3c14dc },
	{ "cyan",			0xffff00 },
	{ "darkblue",			0x8b0000 },
	{ "darkcyan",			0x8b8b00 },
	{ "darkgoldenrod",		0x0b86b8 },
	{ "darkgray",			0xa9a9a9 },
	{ "darkgreen",			0x006400 },
	{ "darkgrey",			0xa9a9a9 },
	{ "darkkhaki",			0x6bb7bd },
	{ "darkmagenta",		0x8b008b },
	{ "darkolivegreen",		0x2f6b55 },
	{ "darkorange",			0x008cff },
	{ "darkorchid",			0xcc3299 },
	{ "darkred",			0x00008b },
	{ "darksalmon",			0x7a96e9 },
	{ "darkseagreen",		0x8fbc8f },
	{ "darkslateblue",		0x8b3d48 },
	{ "darkslategray",		0x4f4f2f },
	{ "darkslategrey",		0x4f4f2f },
	{ "darkturquoise",		0xd1ce00 },
	{ "darkviolet",			0xd30094 },
	{ "deeppink",			0x9314ff },
	{ "deepskyblue",		0xffbf00 },
	{ "dimgray",			0x696969 },
	{ "dimgrey",			0x696969 },
	{ "dodgerblue",			0xff901e },
	{ "feldspar",			0x7592d1 }, /* not SVG-1.0 */
	{ "firebrick",			0x2222b2 },
	{ "floralwhite",		0xf0faff },
	{ "forestgreen",		0x228b22 },
	{ "fuchsia",			0xff00ff },
	{ "gainsboro",			0xdcdcdc },
	{ "ghostwhite",			0xfff8f8 },
	{ "gold",			0x00d7ff },
	{ "goldenrod",			0x20a5da },
	{ "gray",			0x808080 },
	{ "green",			0x008000 },
	{ "greenyellow",		0x2fffad },
	{ "grey",			0x808080 },
	{ "honeydew",			0xf0fff0 },
	{ "hotpink",			0xb469ff },
	{ "indianred",			0x5c5ccd },
	{ "indigo",			0x82004b },
	{ "ivory",			0xf0ffff },
	{ "khaki",			0x8ce6f0 },
	{ "lavender",			0xfae6e6 },
	{ "lavenderblush",		0xf5f0ff },
	{ "lawngreen",			0x00fc7c },
	{ "lemonchiffon",		0xcdfaff },
	{ "lightblue",			0xe6d8ad },
	{ "lightcoral",			0x8080f0 },
	{ "lightcyan",			0xffffe0 },
	{ "lightgoldenrodyellow",	0xd2fafa },
	{ "lightgray",			0xd3d3d3 },
	{ "lightgreen",			0x90ee90 },
	{ "lightgrey",			0xd3d3d3 },
	{ "lightpink",			0xc1b6ff },
	{ "lightsalmon",		0x7aa0ff },
	{ "lightseagreen",		0xaab220 },
	{ "lightskyblue",		0xface87 },
	{ "lightslateblue",		0xff7084 }, /* not SVG-1.0*/
	{ "lightslategray",		0x998877 },
	{ "lightslategrey",		0x998877 },
	{ "lightsteelblue",		0xdec4b0 },
	{ "lightyellow",		0xe0ffff },
	{ "lime",			0x00ff00 },
	{ "limegreen",			0x32cd32 },
	{ "linen",			0xe6f0fa },
	{ "magenta",			0xff00ff },
	{ "maroon",			0x000080 },
	{ "mediumaquamarine",		0xaacd66 },
	{ "mediumblue",			0xcd0000 },
	{ "mediumorchid",		0xd355ba },
	{ "mediumpurple",		0xdb7093 },
	{ "mediumseagreen",		0x71b33c },
	{ "mediumslateblue",		0xee687b },
	{ "mediumspringgreen",		0x9afa00 },
	{ "mediumturquoise",		0xccd148 },
	{ "mediumvioletred",		0x8515c7 },
	{ "midnightblue",		0x701919 },
	{ "mintcream",			0xfafff5 },
	{ "mistyrose",			0xe1e4ff },
	{ "moccasin",			0xb5e4ff },
	{ "navajowhite",		0xaddeff },
	{ "navy",			0x800000 },
	{ "oldlace",			0xe6f5fd },
	{ "olive",			0x008080 },
	{ "olivedrab",			0x238e6b },
	{ "orange",			0x00a5ff },
	{ "orangered",			0x0045ff },
	{ "orchid",			0xd670da },
	{ "palegoldenrod",		0xaae8ee },
	{ "palegreen",			0x98fb98 },
	{ "paleturquoise",		0xeeeeaf },
	{ "palevioletred",		0x9370db },
	{ "papayawhip",			0xd5efff },
	{ "peachpuff",			0xb9daff },
	{ "peru",			0x3f85cd },
	{ "pink",			0xcbc0ff },
	{ "plum",			0xdda0dd },
	{ "powderblue",			0xe6e0b0 },
	{ "purple",			0x800080 },
	{ "red",			0x0000ff },
	{ "rosybrown",			0x8f8fbc },
	{ "royalblue",			0xe16941 },
	{ "saddlebrown",		0x13458b },
	{ "salmon",			0x7280fa },
	{ "sandybrown",			0x60a4f4 },
	{ "seagreen",			0x578b2e },
	{ "seashell",			0xeef5ff },
	{ "sienna",			0x2d52a0 },
	{ "silver",			0xc0c0c0 },
	{ "skyblue",			0xebce87 },
	{ "slateblue",			0xcd5a6a },
	{ "slategray",			0x908070 },
	{ "slategrey",			0x908070 },
	{ "snow",			0xfafaff },
	{ "springgreen",		0x7fff00 },
	{ "steelblue",			0xb48246 },
	{ "tan",			0x8cb4d2 },
	{ "teal",			0x808000 },
	{ "thistle",			0xd8bfd8 },
	{ "tomato",			0x4763ff },
	{ "transparent",		TRANSPARENT },
	{ "turquoise",			0xd0e040 },
	{ "violet",			0xee82ee },
	{ "violetred",			0x9020d0 }, /* not SVG-1.0*/
	{ "wheat",			0xb3def5 },
	{ "white",			0xffffff },
	{ "whitesmoke",			0xf5f5f5 },
	{ "yellow",			0x00ffff },
	{ "yellowgreen",		0x32cd9a },
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
			style = css_duplicate_style(&css_empty_style);
			if (!style) {
				/** \todo report to user */
				css_free_selector(sel);
				return;
			}
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
	char name[25]; /* this must be the same length as p->name */
	struct css_node *n;
	for (n = declaration; n != 0; n = n->next) {
		struct css_property_entry *p;
		assert(n->type == CSS_NODE_DECLARATION && n->data && n->value);
		if (24 < n->data_length)
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
		length->unit = CSS_UNIT_PX;
		length->value = 0;
		return 0;
	}
	if ((v->type != CSS_NODE_DIMENSION) && (v->type != CSS_NODE_NUMBER))
		return 1;
	num_length = strspn(v->data, "0123456789+-.");
	if (v->type == CSS_NODE_DIMENSION) {
		u = css_unit_parse(v->data + num_length, v->data_length - num_length);
		if (u == CSS_UNIT_UNKNOWN) {
			return 1;
		}
	} else {
	  	u = CSS_UNIT_PX;
	}
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
	char colour_name[21];

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
			if (v->data_length == 4 &&
					strncasecmp(v->data, "rgb", 3) == 0)
				c = css_parse_rgb(v->value);
			break;

		case CSS_NODE_IDENT:
			if (20 < v->data_length)
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


/**
 * Parse an RGB value in functional notation.
 */

colour css_parse_rgb(struct css_node *v)
{
	unsigned int i;
	int c[3];

	/* we expect exactly the nodes
	 *     X COMMA X COMMA X
	 *     where X is NUMBER or PERCENTAGE
	 */

	for (i = 0; i != 3; i++) {
		if (!v)
			return CSS_COLOR_NONE;
		if (v->type == CSS_NODE_NUMBER)
			c[i] = atoi(v->data);
		else if (v->type == CSS_NODE_PERCENTAGE)
			c[i] = atoi(v->data) * 255 / 100;
		else
			return CSS_COLOR_NONE;
		if (c[i] < 0)
			c[i] = 0;
		if (255 < c[i])
			c[i] = 255;

		v = v->next;

		if (i == 2) {
			if (v)
				return CSS_COLOR_NONE;
		} else {
			if (!v || v->type != CSS_NODE_COMMA)
				return CSS_COLOR_NONE;

			v = v->next;
		}
	}

	return (c[2] << 16) | (c[1] << 8) | c[0];
}

/**
 * Parse a uri
 *
 * \param v node to parse
 * \param uri updated to uri, if successful
 * \return true on success, false on failure
 */
bool parse_uri(const struct css_node *v, char **uri)
{
	bool string = false;
	const char *u;
	char *t, *url;
	url_func_result res;

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
			if (!url)
				return false;
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
				res = url_join(url, v->stylesheet->data.html.base_url, uri);
			else
				res = url_join(url, v->stylesheet->url, uri);
			free(url);
			if (res != URL_FUNC_OK)
				return false;
			break;
		case CSS_NODE_STRING:
			url = strndup(v->data, v->data_length);
			if (!url)
				return false;

			if (v->stylesheet->type == CONTENT_HTML)
				res = url_join(url, v->stylesheet->data.html.base_url, uri);
			else
				res = url_join(url, v->stylesheet->url, uri);
			free(url);
			if (res != URL_FUNC_OK)
				return false;
			break;
		default:
			return false;
	}

	return true;
}

/**
 * \name  Individual property parsers.
 * \{
 */

void parse_background(struct css_style * const s,
		const struct css_node * v)
{
	colour c = TRANSPARENT, c2;
	css_background_image_type bi = CSS_BACKGROUND_IMAGE_NONE, bi2;
	char *bi_uri = 0;
	css_background_repeat br = CSS_BACKGROUND_REPEAT_REPEAT, br2;
	css_background_attachment ba = CSS_BACKGROUND_ATTACHMENT_SCROLL, ba2;
	struct css_background_position horz =
			{ CSS_BACKGROUND_POSITION_PERCENT, { 0 } };
	struct css_background_position vert =
			{ CSS_BACKGROUND_POSITION_PERCENT, { 0 } };
	struct css_background_position horz2, vert2;

	while (v) {
		switch (v->type) {
			case CSS_NODE_URI:
			case CSS_NODE_STRING:
				/* background-image */
				if (!css_background_image_parse(v, &bi2,
						&bi_uri))
					goto error;
				bi = bi2;
				v = v->next;
				break;

			case CSS_NODE_DIMENSION:
			case CSS_NODE_NUMBER:
			case CSS_NODE_PERCENTAGE:
				/* background-position */
				if (!css_background_position_parse(&v,
						&horz2, &vert2))
					goto error;
				horz = horz2;
				vert = vert2;
				break;

			case CSS_NODE_IDENT:
				/* could be background-image: none */
				if (v->data_length == 4 &&
						strncasecmp(v->data, "none",
						4) == 0) {
					bi = CSS_BACKGROUND_IMAGE_NONE;
					v = v->next;
					break;
				}

				/* background-repeat */
				br2 = css_background_repeat_parse(v->data,
						v->data_length);
				if (br2 != CSS_BACKGROUND_REPEAT_UNKNOWN) {
					br = br2;
					v = v->next;
					break;
				}

				/* background-attachment */
				ba2 = css_background_attachment_parse(v->data,
						v->data_length);
				if (ba2 != CSS_BACKGROUND_ATTACHMENT_UNKNOWN) {
					ba = ba2;
					v = v->next;
					break;
				}

				/* background-position */
				if (css_background_position_parse(&v,
						&horz2, &vert2)) {
					horz = horz2;
					vert = vert2;
					break;
				}

				/* fall through */
			case CSS_NODE_HASH:
			case CSS_NODE_FUNCTION:
				/* background-color */
				c2 = parse_colour(v);
				if (c2 != CSS_COLOR_NONE) {
					c = c2;
					v = v->next;
					break;
				}

				/* fall through */
			default:
				/* parsing failed */
				goto error;
		}
	}

	s->background_color = c;
	s->background_image.type = bi;
	if (s->background_image.type == CSS_BACKGROUND_IMAGE_URI)
		free(s->background_image.uri);
	s->background_image.uri = bi_uri;
	s->background_repeat = br;
	s->background_attachment = ba;
	s->background_position.horz = horz;
	s->background_position.vert = vert;

	return;

error:
	free(bi_uri);
}


void parse_background_attachment(struct css_style * const s,
		const struct css_node * const v)
{
	css_background_attachment z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_background_attachment_parse(v->data, v->data_length);
	if (z != CSS_BACKGROUND_ATTACHMENT_UNKNOWN)
		s->background_attachment = z;
}


void parse_background_color(struct css_style * const s,
		const struct css_node * const v)
{
	colour c;
	if (v->next)
		return;
	c = parse_colour(v);
	if (c != CSS_COLOR_NONE)
		s->background_color = c;
}


void parse_background_image(struct css_style * const s,
		const struct css_node * const v)
{
	css_background_image_type type;
	char *uri = 0;

	if (v->next)
		return;
	if (!css_background_image_parse(v, &type, &uri))
		return;

	if (s->background_image.type == CSS_BACKGROUND_IMAGE_URI)
		free(s->background_image.uri);
	s->background_image.type = type;
	s->background_image.uri = uri;
}



/**
 * Parse a background-image property.
 *
 * \param  node  node to parse
 * \param  type  updated to background image type
 * \param  uri	 updated to background image uri, if type is
 *		 CSS_BACKGROUND_IMAGE_URI
 * \return  true on success, false on parse failure
 */

bool css_background_image_parse(const struct css_node *v,
		css_background_image_type *type, char **uri)
{
	switch (v->type) {
		case CSS_NODE_URI:
		case CSS_NODE_STRING:
			if (!parse_uri(v, uri))
				return false;
			*type = CSS_BACKGROUND_IMAGE_URI;
			break;
		case CSS_NODE_IDENT:
			if (v->data_length == 7 &&
					strncasecmp(v->data, "inherit", 7) == 0)
				*type = CSS_BACKGROUND_IMAGE_INHERIT;
			else if (v->data_length == 4 &&
					strncasecmp(v->data, "none", 4) == 0)
				*type = CSS_BACKGROUND_IMAGE_NONE;
			break;
		default:
			return false;
	}
	return true;
}


/** An entry in css_background_table. */
struct css_background_entry {
	const char *keyword;
	unsigned int length;
	float value;
	bool horizontal;
	bool vertical;
};

/** Lookup table for parsing background-postion. */
struct css_background_entry css_background_table[] = {
	{ "left",   4,	 0,  true, false },
	{ "right",  5, 100,  true, false },
	{ "top",    3,	 0, false, true  },
	{ "bottom", 6, 100, false, true  },
	{ "center", 6,  50, false, false }  /* true, true would be more
			logical, but this actually simplifies the code */
};

#define CSS_BACKGROUND_TABLE_ENTRIES (sizeof css_background_table / \
		sizeof css_background_table[0])


/**
 * Lookup a background-position keyword in css_background_table.
 */

struct css_background_entry *css_background_lookup(
		const struct css_node *v)
{
	unsigned int i;
	for (i = 0; i != CSS_BACKGROUND_TABLE_ENTRIES; i++)
		if (css_background_table[i].length == v->data_length &&
				strncasecmp(v->data,
				css_background_table[i].keyword,
				css_background_table[i].length) == 0)
			break;
	if (i == CSS_BACKGROUND_TABLE_ENTRIES)
		return 0;
	return &css_background_table[i];
}


void parse_background_position(struct css_style * const s,
		const struct css_node * v)
{
	const struct css_node *node = v;
	struct css_background_position horz, vert;

	if (v->next && v->next->next)
		/* more than two nodes */
		return;

	if (!css_background_position_parse(&node, &horz, &vert))
		return;
	if (node)
		/* didn't parse all the nodes */
		return;

	s->background_position.horz = horz;
	s->background_position.vert = vert;
}


/**
 * Parse a background-position property.
 *
 * \param  node  list of nodes, updated to first unused node
 * \param  horz  updated to horizontal background position
 * \param  vert  updated to vertical background position
 * \return  true on success, false on parse failure
 */

bool css_background_position_parse(const struct css_node **node,
		struct css_background_position *horz,
		struct css_background_position *vert)
{
	const struct css_node *v = *node;
	const struct css_node *w = v->next;
	const struct css_node *n_temp = 0;
	struct css_background_entry *bg = 0, *bg2 = 0, *b_temp = 0;
	bool switched = false;

	if (v->type == CSS_NODE_IDENT)
		bg = css_background_lookup(v);
	if (w && w->type == CSS_NODE_IDENT)
		bg2 = css_background_lookup(w);

	if (!(w && ((w->type == CSS_NODE_IDENT && bg2) ||
			w->type == CSS_NODE_PERCENTAGE ||
			w->type == CSS_NODE_DIMENSION ||
			w->type == CSS_NODE_NUMBER))) {
		/* only one value specified */
		if (v->type == CSS_NODE_IDENT) {
			if (v->data_length == 7 &&
					strncasecmp(v->data, "inherit", 7)
					== 0) {
				horz->pos = vert->pos =
						CSS_BACKGROUND_POSITION_INHERIT;
				return false;
			}

			if (!bg)
				return false;
			horz->pos = vert->pos = CSS_BACKGROUND_POSITION_PERCENT;
			horz->value.percent = bg->horizontal ? bg->value : 50;
			vert->value.percent = bg->vertical ? bg->value : 50;
		}
		else if (v->type == CSS_NODE_PERCENTAGE) {
			horz->pos = vert->pos = CSS_BACKGROUND_POSITION_PERCENT;
			horz->value.percent = atof(v->data);
			vert->value.percent = 50.0;
		}
		else if ((v->type == CSS_NODE_DIMENSION) ||
				(v->type == CSS_NODE_NUMBER)) {
			if (parse_length(&horz->value.
					length, v, false) == 0) {
				horz->pos = CSS_BACKGROUND_POSITION_LENGTH;
				vert->pos = CSS_BACKGROUND_POSITION_PERCENT;
				vert->value.percent = 50.0;
			}
		}

		*node = w;
		return true;
	}

	/* two values specified */
	if (v->type == CSS_NODE_IDENT && w->type == CSS_NODE_IDENT) {
		/* both keywords */
		if (!bg || !bg2)
			return false;
		if ((bg->horizontal && bg2->horizontal) ||
				(bg->vertical && bg2->vertical))
			return false;
		horz->pos = vert->pos = CSS_BACKGROUND_POSITION_PERCENT;
		horz->value.percent = vert->value.percent = 50;
		if (bg->horizontal)
			horz->value.percent = bg->value;
		else if (bg2->horizontal)
			horz->value.percent = bg2->value;
		if (bg->vertical)
			vert->value.percent = bg->value;
		else if (bg2->vertical)
			vert->value.percent = bg2->value;

		*node = w->next;
		return true;
	}

	/* reverse specifiers such that idents are places in h, v order */
	if ((v->type == CSS_NODE_IDENT && bg && bg->vertical) ||
			(w->type == CSS_NODE_IDENT && bg2 && bg2->horizontal)) {
	  	n_temp = v; v = w; w = n_temp;
	  	b_temp = bg; bg = bg2; bg2 = b_temp;
	  	switched = true;
	}

	if (v->type == CSS_NODE_IDENT) { /* horizontal value */
		if (!bg || bg->vertical)
			return false;
	}
	if (w->type == CSS_NODE_IDENT) { /* vertical value */
		if (!bg2 || bg2->horizontal)
			return false;
	}

	if (v->type == CSS_NODE_IDENT) { /* horizontal value */
		horz->pos = CSS_BACKGROUND_POSITION_PERCENT;
		horz->value.percent = bg->value;
	} else if (v->type == CSS_NODE_PERCENTAGE) {
		horz->pos = CSS_BACKGROUND_POSITION_PERCENT;
		horz->value.percent = atof(v->data);
	} else if ((v->type == CSS_NODE_DIMENSION) ||
			(v->type == CSS_NODE_NUMBER)) {
		if (parse_length(&horz->value.length,
				v, false) == 0)
			horz->pos = CSS_BACKGROUND_POSITION_LENGTH;
	}

	if (w->type == CSS_NODE_IDENT) { /* vertical value */
		vert->pos = CSS_BACKGROUND_POSITION_PERCENT;
		vert->value.percent = bg2->value;
	} else if (w->type == CSS_NODE_PERCENTAGE) {
		vert->pos = CSS_BACKGROUND_POSITION_PERCENT;
		vert->value.percent = atof(w->data);
	} else if ((w->type == CSS_NODE_DIMENSION) ||
			(w->type == CSS_NODE_NUMBER)) {
		if (parse_length(&vert->value.length,
				w, false) == 0)
			vert->pos = CSS_BACKGROUND_POSITION_LENGTH;
	}

	/* undo any switching we did */
	if (switched) {
	  	n_temp = v; v = w; w = n_temp;
	  	b_temp = bg; bg = bg2; bg2 = b_temp;
	}

	*node = w->next;
	return true;
}


void parse_background_repeat(struct css_style * const s,
		const struct css_node * const v)
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

PARSE_BORDER_WIDTH(top,	   TOP)
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

PARSE_BORDER_COLOR(top,	   TOP)
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

PARSE_BORDER_STYLE(top,	   TOP)
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

void parse_border_collapse(struct css_style * const s, const struct css_node * v)
{
	css_border_collapse z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_border_collapse_parse(v->data, v->data_length);
	if (z != CSS_BORDER_COLLAPSE_UNKNOWN)
		s->border_collapse = z;
}

void parse_border_spacing(struct css_style * const s, const struct css_node * v)
{
	if (v->next && v->next->next)
		/* more than two nodes */
		return;

	if (!v->next) {
		/* one node */
		if (v->type == CSS_NODE_IDENT && v->data_length == 7 &&
				strncasecmp(v->data, "inherit", 7) == 0)
			s->border_spacing.border_spacing =
					CSS_BORDER_SPACING_INHERIT;
		else if (v->type == CSS_NODE_DIMENSION ||
					v->type == CSS_NODE_NUMBER) {
			if (parse_length(&s->border_spacing.horz,
					v, true) == 0 &&
					parse_length(&s->border_spacing.vert,
					v, true) == 0)
				s->border_spacing.border_spacing =
						CSS_BORDER_SPACING_LENGTH;
		}
	} else {
		/* two nodes */
		if ((v->type == CSS_NODE_DIMENSION ||
				v->type == CSS_NODE_NUMBER) &&
				(v->next->type == CSS_NODE_DIMENSION ||
				v->next->type == CSS_NODE_NUMBER)) {
			if (parse_length(&s->border_spacing.horz,
					v, true) == 0 &&
					parse_length(&s->border_spacing.vert,
					v->next, true) == 0)
				s->border_spacing.border_spacing =
						CSS_BORDER_SPACING_LENGTH;
		}
	}
}

void parse_caption_side(struct css_style * const s, const struct css_node * v)
{
	css_caption_side z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_caption_side_parse(v->data, v->data_length);
	if (z != CSS_CAPTION_SIDE_UNKNOWN)
		s->caption_side = z;
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

void parse_clip(struct css_style * const s, const struct css_node * v)
{
	int i;
	struct css_node *t;

	if (v->next != 0)
		return;

	switch (v->type) {
	case CSS_NODE_IDENT:
		if (v->data_length == 7 &&
		    strncasecmp(v->data, "inherit", 7) == 0)
			s->clip.clip = CSS_CLIP_INHERIT;
		else if (v->data_length == 4 &&
			strncasecmp(v->data, "auto", 4) == 0)
			s->clip.clip = CSS_CLIP_AUTO;
		break;
	case CSS_NODE_FUNCTION:
		/* must be rect(X,X,X,X) */
		if (v->data_length == 5 &&
		    strncasecmp(v->data, "rect", 4) == 0) {
			struct {
				enum { CSS_CLIP_RECT_AUTO,
					CSS_CLIP_RECT_LENGTH } rect;
				struct css_length value;
			} rect[4];

			t = v->value;
			if (!t)
				return;

			for (i = 0; i != 4; i++) {
				switch (t->type) {
				case CSS_NODE_IDENT:
					if (t->data_length == 4 &&
							strncasecmp(t->data, "auto", 4) == 0) {
						rect[i].rect = CSS_CLIP_AUTO;
					}
					else
						return;
					break;
				case CSS_NODE_DIMENSION:
				case CSS_NODE_NUMBER:
					if (parse_length(&rect[i].value,
							t, false) != 0)
						return;
					rect[i].rect = CSS_CLIP_RECT_LENGTH;
					break;
				default:
					return;
				}

				/* move to comma or end */
				t = t->next;

				if (i == 3 && t)
					/* excess arguments - ignore rule */
					return;
				else {
					if (!t || t->type != CSS_NODE_COMMA)
						/* insufficient arguments or
						 * no comma - ignore rule */
						return;
				}

				/* move to next argument */
				t = t->next;
			}

			/* If we reach here, rule is valid, so apply to s */
			for (i = 0; i != 4; i++) {
				s->clip.rect[i].rect = rect[i].rect;
				s->clip.rect[i].value.value =
						rect[i].value.value;
				s->clip.rect[i].value.unit =
						rect[i].value.unit;
			}
			s->clip.clip = CSS_CLIP_RECT;
		}
		break;
	default:
		break;
	}
}

void parse_color(struct css_style * const s, const struct css_node * const v)
{
	colour c;
	if (v->next)
		return;
	c = parse_colour(v);
	if (c != CSS_COLOR_NONE)
		s->color = c;
}

void parse_content(struct css_style * const s, const struct css_node * v)
{
	struct css_content *new_content = NULL;
	struct css_content *content;
	struct css_node *t;
	bool first = true;

	for (; v; v = v->next) {
		switch (v->type) {
			case CSS_NODE_STRING:
				content = parse_content_new(&new_content, CSS_CONTENT_STRING);
				if (!content)
					return;
				content->data.string = strndup(v->data, v->data_length);
				if (!content->data.string) {
					css_deep_free_content(new_content);
					return;
				}
				break;
			case CSS_NODE_URI:
				content = parse_content_new(&new_content, CSS_CONTENT_URI);
				if (!content)
					return;
				if (!parse_uri(v, &content->data.uri)) {
					css_deep_free_content(new_content);
					return;
				}
				break;
			case CSS_NODE_IDENT:
				if (v->data_length == 7 &&
						strncasecmp(v->data, "inherit", 7) == 0) {
					if ((!first) || (v->next))
						return;
					css_deep_free_content(s->content.content);
					s->content.content = NULL;
					s->content.type = CSS_CONTENT_INHERIT;
					return;
				} else if (v->data_length == 6 &&
						strncasecmp(v->data, "normal", 6) == 0) {
					if ((!first) || (v->next))
						return;
					css_deep_free_content(s->content.content);
					s->content.content = NULL;
					s->content.type = CSS_CONTENT_NORMAL;
					return;
				} else if (v->data_length == 10 &&
						strncasecmp(v->data, "open-quote", 10) == 0) {
					if (!parse_content_new(&new_content, CSS_CONTENT_OPEN_QUOTE))
						return;
				} else if (v->data_length == 11 &&
						strncasecmp(v->data, "close-quote", 11) == 0) {
					if (!parse_content_new(&new_content, CSS_CONTENT_CLOSE_QUOTE))
						return;
				} else if (v->data_length == 13 &&
						strncasecmp(v->data, "no-open-quote", 13) == 0) {
					if (!parse_content_new(&new_content, CSS_CONTENT_NO_OPEN_QUOTE))
						return;
				} else if (v->data_length == 14 &&
						strncasecmp(v->data, "no-close-quote", 14) == 0) {
					if (!parse_content_new(&new_content, CSS_CONTENT_NO_CLOSE_QUOTE))
						return;
				} else {
					css_deep_free_content(new_content);
					return;
				}
				break;
			case CSS_NODE_FUNCTION:
				if (v->data_length == 5 &&
						strncasecmp(v->data, "attr", 4) == 0) {
					content = parse_content_new(&new_content, CSS_CONTENT_URI);
					if (!content)
						return;
					t = v->value;
					if ((t->type == CSS_NODE_STRING) && (!t->next)) {
						content->data.string = strndup(t->data, t->data_length);
						if (!content->data.string) {
							css_deep_free_content(new_content);
							return;
						}
					} else {
						css_deep_free_content(new_content);
						return;
					}
				} else if (v->data_length == 8 &&
						strncasecmp(v->data, "counter", 7) == 0) {
					if (!parse_content_counter(&new_content, v->value, false))
						return;
				} else if (v->data_length == 9 &&
						strncasecmp(v->data, "counters", 8) == 0) {
					if (!parse_content_counter(&new_content, v->value, true))
						return;
				} else {
					css_deep_free_content(new_content);
					return;
				}
			default:
				css_deep_free_content(new_content);
				return;
		}
		first = false;
	}

	if (new_content) {
		css_deep_free_content(s->content.content);
		s->content.type = CSS_CONTENT_INTERPRET;
		s->content.content = new_content;
	}
}

struct css_content *parse_content_new(struct css_content **current, css_content_type_generated generated) {
	struct css_content *content;
	struct css_content *link;

	content = (struct css_content *)calloc(1, sizeof(struct css_content));
	if (!content) {
		css_deep_free_content(*current);
		return NULL;
	}

	content->type = generated;
	if (!*current) {
		*current = content;
	} else {
		for (link = *current; link->next; link = link->next);
		link->next = content;
	}
	return content;
}

bool parse_content_counter(struct css_content **current, struct css_node *t, bool counters) {
	struct css_content *content;
	css_list_style_type z;

	content = parse_content_new(current, CSS_CONTENT_COUNTER);
	if ((!content) || (t->type != CSS_NODE_IDENT))
		return false;

	content->data.counter.name = strndup(t->data, t->data_length);
	content->data.counter.style = CSS_LIST_STYLE_TYPE_DECIMAL;
	t = t->next;

	if (counters) {
		if ((!t) || (t->type != CSS_NODE_STRING)) {
			css_deep_free_content(*current);
			return false;
		}
		content->data.counter.separator = strndup(t->data, t->data_length);
		t = t->next;
	}

	if (!t)
		return true;

	if ((t->type != CSS_NODE_IDENT) || (t->next)) {
		css_deep_free_content(*current);
		return false;
	}
	z = css_list_style_type_parse(t->data, t->data_length);
	if (z != CSS_LIST_STYLE_TYPE_UNKNOWN)
		content->data.counter.style = z;
	return true;
}

void parse_counter_reset(struct css_style * const s, const struct css_node * v) {
	struct css_counter_control *counter = NULL;

	if (!parse_counter_control_data(&counter, v, 0))
		return;

	if (counter) {
		css_deep_free_counter_control(s->counter_reset.data);
		s->counter_reset.type = CSS_COUNTER_RESET_INTERPRET;
		s->counter_reset.data = counter;
	}
}

void parse_counter_increment(struct css_style * const s, const struct css_node * v) {
	struct css_counter_control *counter = NULL;

	if (!parse_counter_control_data(&counter, v, 1))
		return;

	if (counter) {
		css_deep_free_counter_control(s->counter_increment.data);
		s->counter_increment.type = CSS_COUNTER_INCREMENT_INTERPRET;
		s->counter_increment.data = counter;
	}
}

bool parse_counter_control_data(struct css_counter_control **current, const struct css_node * v, int empty) {
	struct css_counter_control *open = NULL;

	for (; v; v = v->next) {
		switch (v->type) {
			case CSS_NODE_IDENT:
				open = parse_counter_control_new(current);
				if (!open)
					return false;
				open->name = strndup(v->data, v->data_length);
				open->value = empty;
				if (!open->name) {
					css_deep_free_counter_control(*current);
					return false;
				}
				break;
			case CSS_NODE_NUMBER:
				if (!open) {
					css_deep_free_counter_control(*current);
					return false;
				}
				open->value = atoi(v->data);
				open = NULL;
				break;
			default:
				css_deep_free_counter_control(*current);
				return false;
		}
	}
	return true;
}

struct css_counter_control *parse_counter_control_new(struct css_counter_control **current) {
	struct css_counter_control *counter;
	struct css_counter_control *link;

	counter = (struct css_counter_control *)calloc(1, sizeof(struct css_counter_control));
	if (!counter) {
		css_deep_free_counter_control(*current);
		return NULL;
	}

	if (!*current) {
		*current = counter;
	} else {
		for (link = *current; link->next; link = link->next);
		link->next = counter;
	}
	return counter;
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

void parse_direction(struct css_style * const s, const struct css_node * v)
{
	css_direction z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_direction_parse(v->data, v->data_length);
	if (z != CSS_DIRECTION_UNKNOWN)
		s->direction = z;
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

void parse_empty_cells(struct css_style * const s, const struct css_node * v)
{
	css_empty_cells z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_empty_cells_parse(v->data, v->data_length);
	if (z != CSS_EMPTY_CELLS_UNKNOWN)
		s->empty_cells = z;
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
		case CSS_NODE_NUMBER:
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

void parse_letter_spacing(struct css_style * const s, const struct css_node * v)
{
	if (v->next != 0)
		return;

	switch (v->type) {
		case CSS_NODE_IDENT:
			if (v->data_length == 7 &&
			    strncasecmp(v->data, "inherit", 7) == 0)
				s->letter_spacing.letter_spacing = CSS_LETTER_SPACING_INHERIT;
			else if (v->data_length == 6 &&
				strncasecmp(v->data, "normal", 6) == 0)
				s->letter_spacing.letter_spacing = CSS_LETTER_SPACING_NORMAL;
			break;
		case CSS_NODE_DIMENSION:
		case CSS_NODE_NUMBER:
			if (parse_length(&s->letter_spacing.length, v, false) == 0)
				s->letter_spacing.letter_spacing = CSS_LETTER_SPACING_LENGTH;
			break;
		default:
			break;
	}
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

void parse_list_style(struct css_style * const s, const struct css_node * v)
{
	css_list_style_type t = CSS_LIST_STYLE_TYPE_DISC, t2;
	css_list_style_position p = CSS_LIST_STYLE_POSITION_OUTSIDE, p2;
	css_list_style_image_type i = CSS_LIST_STYLE_IMAGE_NONE, i2;
	char *lsi_uri = 0;

	while (v) {
		switch (v->type) {
			case CSS_NODE_IDENT:
				t2 = css_list_style_type_parse(v->data, v->data_length);
				if (t2 != CSS_LIST_STYLE_TYPE_UNKNOWN) {
					t = t2;
					v = v->next;
					break;
				}

				p2 = css_list_style_position_parse(v->data, v->data_length);
				if (p2 != CSS_LIST_STYLE_POSITION_UNKNOWN) {
					p = p2;
					v = v->next;
					break;
				}

				/* drop through */
			case CSS_NODE_STRING:
			case CSS_NODE_URI:
				if (!css_list_style_image_parse(v, &i2, &lsi_uri))
					return;
				i = i2;
				v = v->next;
				break;
			default:
				return;
		}
	}

	s->list_style_type = t;
	s->list_style_position = p;
	s->list_style_image.type = i;
	s->list_style_image.uri = lsi_uri;
}

void parse_list_style_image(struct css_style * const s, const struct css_node * v)
{
	css_list_style_image_type type;
	char *uri;

	if (v->next != 0)
		return;
	if (!css_list_style_image_parse(v, &type, &uri))
		return;

	if (s->list_style_image.type == CSS_LIST_STYLE_IMAGE_URI)
		free(s->list_style_image.uri);
	s->list_style_image.type = type;
	s->list_style_image.uri = uri;
}

/**
 * Parse a list-style-image property.
 *
 * \param  node  node to parse
 * \param  type  updated to list-style-image type
 * \param  uri	 updated to image uri, if type is
 *		 CSS_LIST_STYLE_IMAGE_URI
 * \return  true on success, false on parse failure
 */

bool css_list_style_image_parse(const struct css_node *v,
		css_list_style_image_type *type, char **uri)
{
	switch (v->type) {
		case CSS_NODE_URI:
		case CSS_NODE_STRING:
			if (!parse_uri(v, uri))
				return false;
			*type = CSS_LIST_STYLE_IMAGE_URI;
			break;
		case CSS_NODE_IDENT:
			if (v->data_length == 7 &&
					strncasecmp(v->data, "inherit", 7) == 0)
				*type = CSS_LIST_STYLE_IMAGE_INHERIT;
			else if (v->data_length == 4 &&
					strncasecmp(v->data, "none", 4) == 0)
				*type = CSS_LIST_STYLE_IMAGE_NONE;
			break;
		default:
			return false;
	}
	return true;
}

void parse_list_style_position(struct css_style * const s, const struct css_node * v)
{
	css_list_style_position z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_list_style_position_parse(v->data, v->data_length);
	if (z != CSS_LIST_STYLE_POSITION_UNKNOWN)
		s->list_style_position = z;
}

void parse_list_style_type(struct css_style * const s, const struct css_node * v)
{
	css_list_style_type z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_list_style_type_parse(v->data, v->data_length);
	if (z != CSS_LIST_STYLE_TYPE_UNKNOWN)
		s->list_style_type = z;
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

void parse_max_height(struct css_style *const s, const struct css_node * v)
{
	if (v->next != 0)
		return;
	switch (v->type) {
		case CSS_NODE_IDENT:
			if (v->data_length == 7 &&
			    strncasecmp(v->data, "inherit", 7) == 0)
				s->max_height.max_height = CSS_MAX_HEIGHT_INHERIT;
			else if (v->data_length == 4 &&
				strncasecmp(v->data, "none", 4) == 0)
				s->max_height.max_height = CSS_MAX_HEIGHT_NONE;
			break;
		case CSS_NODE_DIMENSION:
		case CSS_NODE_NUMBER:
			if (!parse_length(&s->max_height.value.length, v, true))
				s->max_height.max_height = CSS_MAX_HEIGHT_LENGTH;
			break;
		case CSS_NODE_PERCENTAGE:
			s->max_height.value.percent = atof(v->data);
			s->max_height.max_height = CSS_MAX_HEIGHT_PERCENT;
			break;
		default:
			break;
	}
}

void parse_max_width(struct css_style *const s, const struct css_node * v)
{
	if (v->next != 0)
		return;
	switch (v->type) {
		case CSS_NODE_IDENT:
			if (v->data_length == 7 &&
			    strncasecmp(v->data, "inherit", 7) == 0)
				s->max_width.max_width = CSS_MAX_WIDTH_INHERIT;
			else if (v->data_length == 4 &&
				strncasecmp(v->data, "none", 4) == 0)
				s->max_width.max_width = CSS_MAX_WIDTH_NONE;
			break;
		case CSS_NODE_DIMENSION:
		case CSS_NODE_NUMBER:
			if (!parse_length(&s->max_width.value.length, v, true))
				s->max_width.max_width = CSS_MAX_WIDTH_LENGTH;
			break;
		case CSS_NODE_PERCENTAGE:
			s->max_width.value.percent = atof(v->data);
			s->max_width.max_width = CSS_MAX_WIDTH_PERCENT;
			break;
		default:
			break;
	}
}

void parse_min_height(struct css_style *const s, const struct css_node * v)
{
	if (v->next != 0)
		return;
	switch (v->type) {
		case CSS_NODE_IDENT:
			if (v->data_length == 7 &&
			    strncasecmp(v->data, "inherit", 7) == 0)
				s->min_height.min_height = CSS_MIN_HEIGHT_INHERIT;
			break;
		case CSS_NODE_DIMENSION:
		case CSS_NODE_NUMBER:
			if (!parse_length(&s->min_height.value.length, v, true))
				s->min_height.min_height = CSS_MIN_HEIGHT_LENGTH;
			break;
		case CSS_NODE_PERCENTAGE:
			s->min_height.value.percent = atof(v->data);
			s->min_height.min_height = CSS_MIN_HEIGHT_PERCENT;
			break;
		default:
			break;
	}
}

void parse_min_width(struct css_style *const s, const struct css_node * v)
{
	if (v->next != 0)
		return;
	switch (v->type) {
		case CSS_NODE_IDENT:
			if (v->data_length == 7 &&
			    strncasecmp(v->data, "inherit", 7) == 0)
				s->min_width.min_width = CSS_MIN_WIDTH_INHERIT;
			break;
		case CSS_NODE_DIMENSION:
		case CSS_NODE_NUMBER:
			if (!parse_length(&s->min_width.value.length, v, true))
				s->min_width.min_width = CSS_MIN_WIDTH_LENGTH;
			break;
		case CSS_NODE_PERCENTAGE:
			s->min_width.value.percent = atof(v->data);
			s->min_width.min_width = CSS_MIN_WIDTH_PERCENT;
			break;
		default:
			break;
	}
}

void parse_orphans(struct css_style * const s, const struct css_node * const v)
{
	if (v->next != 0)
		return;
	switch (v->type) {
		case CSS_NODE_IDENT:
			if (v->data_length == 7 &&
			    strncasecmp(v->data, "inherit", 7) == 0)
				s->orphans.orphans = CSS_ORPHANS_INHERIT;
			break;
		case CSS_NODE_NUMBER:
			s->orphans.value = atoi(v->data);
			s->orphans.orphans = CSS_ORPHANS_INTEGER;
			break;
		default:
			break;
	}
}

void parse_outline(struct css_style * const s, const struct css_node * v)
{
	css_outline_color_type c = CSS_OUTLINE_COLOR_INVERT;
	colour col = 0, col2;
	css_border_style b = CSS_BORDER_STYLE_NONE, b2;
	struct css_border_width w = { CSS_BORDER_WIDTH_LENGTH, { 2, CSS_UNIT_PX } };
	struct css_border_width w2;

	while (v) {
		switch (v->type) {
			case CSS_NODE_HASH:
			case CSS_NODE_FUNCTION:
			case CSS_NODE_IDENT:
				col2 = parse_colour(v);
				if (col2 != CSS_COLOR_NONE) {
					col = col2;
					c = CSS_OUTLINE_COLOR_COLOR;
					v = v->next;
					break;
				}
				if (v->type == CSS_NODE_HASH ||
				    v->type == CSS_NODE_FUNCTION)
					return;

				/* could be inherit */
				if (v->data_length == 7 &&
				   strncasecmp(v->data, "inherit", 7) == 0) {
					c = CSS_OUTLINE_COLOR_INHERIT;
					v = v->next;
					break;
				}

				b2 = css_border_style_parse(v->data, v->data_length);
				if (b2 != CSS_BORDER_STYLE_UNKNOWN) {
					b = b2;
					v = v->next;
					break;
				}

				/* fall through */
			case CSS_NODE_DIMENSION:
			case CSS_NODE_NUMBER:
				if (css_outline_width_parse(v, &w2)) {
					w = w2;
					v = v->next;
					break;
				}

				/* fall through */
			default:
				return;
		}
	}

	s->outline.color.color = c;
	s->outline.color.value = col;
	s->outline.width = w;
	s->outline.style = b;
}

void parse_outline_color(struct css_style * const s, const struct css_node * const v)
{
	colour c;

	if (v->next != 0)
		return;

	c = parse_colour(v);
	if (c == CSS_COLOR_NONE && v->type == CSS_NODE_IDENT) {
		if (v->data_length == 7 &&
		    strncasecmp(v->data, "inherit", 7) == 0)
			s->outline.color.color = CSS_OUTLINE_COLOR_INHERIT;
		else if (v->data_length == 6 &&
			strncasecmp(v->data, "invert", 6) == 0)
			s->outline.color.color = CSS_OUTLINE_COLOR_INVERT;
	}
	else {
		s->outline.color.value = c;
		s->outline.color.color = CSS_OUTLINE_COLOR_COLOR;
	}
}

void parse_outline_style(struct css_style * const s, const struct css_node * const v)
{
	css_border_style z;

	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_border_style_parse(v->data, v->data_length);
	if (z != CSS_BORDER_STYLE_UNKNOWN)
		s->outline.style = z;
}

void parse_outline_width(struct css_style * const s, const struct css_node * const v)
{
	struct css_border_width w;
	if (v->next != 0)
		return;
	if (!css_outline_width_parse(v, &w))
		return;
	s->outline.width = w;
}


bool css_outline_width_parse(const struct css_node * v, struct css_border_width * w)
{
	if (v->type == CSS_NODE_IDENT) {
		if (v->data_length == 7 &&
				strncasecmp(v->data, "inherit", 7) == 0) {
			w->width = CSS_BORDER_WIDTH_INHERIT;
			return true;
		} else if (v->data_length == 4 &&
				strncasecmp(v->data, "thin", 4) == 0) {
			w->width = CSS_BORDER_WIDTH_LENGTH;
			w->value.value = 1;
			w->value.unit = CSS_UNIT_PX;
			return true;
		} else if (v->data_length == 6 &&
				strncasecmp(v->data, "medium", 6) == 0) {
			w->width = CSS_BORDER_WIDTH_LENGTH;
			w->value.value = 2;
			w->value.unit = CSS_UNIT_PX;
			return true;
		} else if (v->data_length == 5 &&
				strncasecmp(v->data, "thick", 5) == 0) {
			w->width = CSS_BORDER_WIDTH_LENGTH;
			w->value.value = 4;
			w->value.unit = CSS_UNIT_PX;
			return true;
		}
	} else if ((v->type == CSS_NODE_DIMENSION ||
			v->type == CSS_NODE_NUMBER) &&
			parse_length(&w->value, v, true) == 0) {
		w->width = CSS_BORDER_WIDTH_LENGTH;
		return true;
	}

	return false;
}

void parse_overflow(struct css_style * const s, const struct css_node * const v)
{
	css_overflow z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_overflow_parse(v->data, v->data_length);
	if (z != CSS_OVERFLOW_UNKNOWN)
		s->overflow = z;
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
			strncasecmp(v->data, "inherit", 7) == 0) {
		s->padding[i].padding = CSS_PADDING_INHERIT;
	} else if (v->type == CSS_NODE_PERCENTAGE) {
		s->padding[i].padding = CSS_PADDING_PERCENT;
		s->padding[i].value.percent = atof(v->data);
	} else if ((v->type == CSS_NODE_DIMENSION || v->type == CSS_NODE_NUMBER) &&
			parse_length(&s->padding[i].value.length, v, true) == 0) {
		s->padding[i].padding = CSS_PADDING_LENGTH;
	}
}

void parse_page_break_after(struct css_style * const s, const struct css_node * v)
{
	css_page_break_after z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_page_break_after_parse(v->data, v->data_length);
	if (z != CSS_PAGE_BREAK_AFTER_UNKNOWN)
		s->page_break_after = z;
}

void parse_page_break_before(struct css_style * const s, const struct css_node * v)
{
	css_page_break_before z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_page_break_before_parse(v->data, v->data_length);
	if (z != CSS_PAGE_BREAK_BEFORE_UNKNOWN)
		s->page_break_before = z;
}

void parse_page_break_inside(struct css_style * const s, const struct css_node * v)
{
	css_page_break_inside z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_page_break_inside_parse(v->data, v->data_length);
	if (z != CSS_PAGE_BREAK_INSIDE_UNKNOWN)
		s->page_break_inside = z;
}

#define PARSE_POS(side, z) \
void parse_ ## side(struct css_style * const s,	\
		const struct css_node * const v)	\
{							\
	parse_pos(s, v, z);			\
}

PARSE_POS(top,	  TOP)
PARSE_POS(right,  RIGHT)
PARSE_POS(bottom, BOTTOM)
PARSE_POS(left,	  LEFT)

void parse_pos(struct css_style * const s, const struct css_node * v, unsigned int i)
{
	if (v->next != 0)
		return;

	switch (v->type) {
		case CSS_NODE_IDENT:
			if (v->data_length == 7 &&
			    strncasecmp(v->data, "inherit", 7) == 0)
				s->pos[i].pos = CSS_POS_INHERIT;
			else if (v->data_length == 4 &&
				 strncasecmp(v->data, "auto", 4) == 0)
				s->pos[i].pos = CSS_POS_AUTO;
			break;
		case CSS_NODE_DIMENSION:
		case CSS_NODE_NUMBER:
			if (parse_length(&s->pos[i].value.length, v, false) == 0)
				s->pos[i].pos = CSS_POS_LENGTH;
			break;
		case CSS_NODE_PERCENTAGE:
			s->pos[i].pos = CSS_POS_PERCENT;
			s->pos[i].value.percent = atof(v->data);
			break;
		default:
			break;
	}
}

void parse_position(struct css_style * const s, const struct css_node * v)
{
	css_position z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_position_parse(v->data, v->data_length);
	if (z != CSS_POSITION_UNKNOWN)
		s->position = z;
}

void parse_table_layout(struct css_style * const s, const struct css_node * v)
{
	css_table_layout z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_table_layout_parse(v->data, v->data_length);
	if (z != CSS_TABLE_LAYOUT_UNKNOWN)
		s->table_layout = z;
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
			parse_length(&s->text_indent.value.length, v, false) == 0) {
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

void parse_unicode_bidi(struct css_style * const s, const struct css_node * const v)
{
	css_unicode_bidi z;
	if (v->type != CSS_NODE_IDENT || v->next != 0)
		return;
	z = css_unicode_bidi_parse(v->data, v->data_length);
	if (z != CSS_UNICODE_BIDI_UNKNOWN)
		s->unicode_bidi = z;
}

void parse_vertical_align(struct css_style * const s, const struct css_node * v)
{
	if (v->next != 0)
		return;
	switch (v->type) {
		case CSS_NODE_IDENT:
			if (v->data_length == 7 &&
			    strncasecmp(v->data, "inherit", 7) == 0)
				s->vertical_align.type = CSS_VERTICAL_ALIGN_INHERIT;
			else if (v->data_length == 8 &&
				strncasecmp(v->data, "baseline", 8) == 0)
				s->vertical_align.type = CSS_VERTICAL_ALIGN_BASELINE;
			else if (v->data_length == 3 &&
				strncasecmp(v->data, "sub", 3) == 0)
				s->vertical_align.type = CSS_VERTICAL_ALIGN_SUB;
			else if (v->data_length == 5 &&
				strncasecmp(v->data, "super", 5) == 0)
				s->vertical_align.type = CSS_VERTICAL_ALIGN_SUPER;
			else if (v->data_length == 3 &&
				strncasecmp(v->data, "top", 3) == 0)
				s->vertical_align.type = CSS_VERTICAL_ALIGN_TOP;
			else if (v->data_length == 8 &&
				strncasecmp(v->data, "text-top", 8) == 0)
				s->vertical_align.type = CSS_VERTICAL_ALIGN_TEXT_TOP;
			else if (v->data_length == 6 &&
				strncasecmp(v->data, "middle", 6) == 0)
				s->vertical_align.type = CSS_VERTICAL_ALIGN_MIDDLE;
			else if (v->data_length == 6 &&
				strncasecmp(v->data, "bottom", 6) == 0)
				s->vertical_align.type = CSS_VERTICAL_ALIGN_BOTTOM;
			else if (v->data_length == 11 &&
				strncasecmp(v->data, "text-bottom", 11) == 0)
				s->vertical_align.type = CSS_VERTICAL_ALIGN_TEXT_BOTTOM;
			break;
		case CSS_NODE_DIMENSION:
		case CSS_NODE_NUMBER:
			if (parse_length(&s->vertical_align.value.length, v, false) == 0)
				s->vertical_align.type = CSS_VERTICAL_ALIGN_LENGTH;
			break;
		case CSS_NODE_PERCENTAGE:
			s->vertical_align.value.percent = atof(v->data);
			s->vertical_align.type = CSS_VERTICAL_ALIGN_PERCENT;
			break;
		default:
			break;
	}
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

void parse_widows(struct css_style * const s, const struct css_node * const v)
{
	if (v->next != 0)
		return;
	switch (v->type) {
		case CSS_NODE_IDENT:
			if (v->data_length == 7 &&
			    strncasecmp(v->data, "inherit", 7) == 0)
				s->widows.widows = CSS_WIDOWS_INHERIT;
			break;
		case CSS_NODE_NUMBER:
			s->widows.value = atoi(v->data);
			s->widows.widows = CSS_WIDOWS_INTEGER;
			break;
		default:
			break;
	}
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

void parse_word_spacing(struct css_style * const s, const struct css_node * v)
{
	if (v->next != 0)
		return;

	switch (v->type) {
		case CSS_NODE_IDENT:
			if (v->data_length == 7 &&
			    strncasecmp(v->data, "inherit", 7) == 0)
				s->word_spacing.word_spacing = CSS_WORD_SPACING_INHERIT;
			else if (v->data_length == 6 &&
				strncasecmp(v->data, "normal", 6) == 0)
				s->word_spacing.word_spacing = CSS_WORD_SPACING_NORMAL;
			break;
		case CSS_NODE_DIMENSION:
		case CSS_NODE_NUMBER:
			if (parse_length(&s->word_spacing.length, v, false) == 0)
				s->word_spacing.word_spacing = CSS_WORD_SPACING_LENGTH;
			break;
		default:
			break;
	}
}

void parse_z_index(struct css_style * const s, const struct css_node * const v)
{
	if (v->next != 0)
		return;
	switch (v->type) {
		case CSS_NODE_IDENT:
			if (v->data_length == 7 &&
			    strncasecmp(v->data, "inherit", 7) == 0)
				s->z_index.z_index = CSS_Z_INDEX_INHERIT;
			else if (v->data_length == 4 &&
				strncasecmp(v->data, "auto", 4) == 0)
				s->z_index.z_index = CSS_Z_INDEX_AUTO;
			break;
		case CSS_NODE_NUMBER:
			s->z_index.value = atoi(v->data);
			s->z_index.z_index = CSS_Z_INDEX_INTEGER;
			break;
		default:
			break;
	}
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

/** \} */

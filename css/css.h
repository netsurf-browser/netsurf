/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * CSS handling (interface).
 *
 * This module aims to implement CSS 2.1.
 *
 * CSS stylesheets are held in a struct ::content with type CONTENT_CSS.
 * Creation and parsing should be carried out via the content_* functions.
 *
 * Styles are stored in a struct ::css_style, which can be retrieved from a
 * content using css_get_style().
 *
 * css_parse_property_list() constructs a struct ::css_style from a CSS
 * property list, as found in HTML style attributes.
 */

#ifndef _NETSURF_CSS_CSS_H_
#define _NETSURF_CSS_CSS_H_

#include <stdbool.h>
#include "libxml/HTMLparser.h"
#include "css_enum.h"


typedef unsigned long colour;  /* 0xbbggrr */
#define TRANSPARENT 0x1000000
#define CSS_COLOR_INHERIT 0x2000000
#define CSS_COLOR_NONE 0x3000000
#define TOP 0
#define RIGHT 1
#define BOTTOM 2
#define LEFT 3

/** Representation of a CSS 2 length. */
struct css_length {
	float value;
	css_unit unit;
};

typedef enum {
	CSS_TEXT_DECORATION_NONE = 0x0,
	CSS_TEXT_DECORATION_INHERIT = 0x1,
	CSS_TEXT_DECORATION_UNDERLINE = 0x2,
	CSS_TEXT_DECORATION_BLINK = 0x4,
	CSS_TEXT_DECORATION_LINE_THROUGH = 0x8,
	CSS_TEXT_DECORATION_OVERLINE = 0x10,
	CSS_TEXT_DECORATION_UNKNOWN = 0x1000
} css_text_decoration;

typedef enum {
	CSS_BACKGROUND_IMAGE_NONE,
	CSS_BACKGROUND_IMAGE_INHERIT,
	CSS_BACKGROUND_IMAGE_URI
} css_background_image_type;

/** Part of struct css_style, for convenience. */
struct css_background_position {
	enum {
		CSS_BACKGROUND_POSITION_LENGTH,
		CSS_BACKGROUND_POSITION_PERCENT,
		CSS_BACKGROUND_POSITION_INHERIT
	} pos;
	union {
		float percent;
		struct css_length length;
	} value;
};

struct css_border_width {
	enum { CSS_BORDER_WIDTH_INHERIT,
	       CSS_BORDER_WIDTH_LENGTH } width;
	struct css_length value;
};

typedef enum {
	CSS_LIST_STYLE_IMAGE_INHERIT,
	CSS_LIST_STYLE_IMAGE_NONE,
	CSS_LIST_STYLE_IMAGE_URI
} css_list_style_image_type;

typedef enum {
	CSS_OUTLINE_COLOR_INHERIT,
	CSS_OUTLINE_COLOR_COLOR,
	CSS_OUTLINE_COLOR_INVERT
} css_outline_color_type;

typedef enum {
	CSS_VERTICAL_ALIGN_INHERIT,
	CSS_VERTICAL_ALIGN_BASELINE,
	CSS_VERTICAL_ALIGN_SUB,
	CSS_VERTICAL_ALIGN_SUPER,
	CSS_VERTICAL_ALIGN_TOP,
	CSS_VERTICAL_ALIGN_TEXT_TOP,
	CSS_VERTICAL_ALIGN_MIDDLE,
	CSS_VERTICAL_ALIGN_BOTTOM,
	CSS_VERTICAL_ALIGN_TEXT_BOTTOM,
	CSS_VERTICAL_ALIGN_LENGTH,
	CSS_VERTICAL_ALIGN_PERCENT
} css_vertical_align_type;


/** Representation of a complete CSS 2 style. */
struct css_style {
	/* background properties */
	css_background_attachment background_attachment;
	colour background_color;
	struct {
		css_background_image_type type;
		char *uri;
	} background_image;
	struct {
		struct css_background_position horz;
		struct css_background_position vert;
	} background_position;
	css_background_repeat background_repeat;

	/* borders */
	struct {
		colour color;
		struct css_border_width width;
		css_border_style style;
	} border[4];  /**< top, right, bottom, left */
	css_border_collapse border_collapse;
	struct {
		enum { CSS_BORDER_SPACING_INHERIT,
		       CSS_BORDER_SPACING_LENGTH } border_spacing;
		struct css_length horz;
		struct css_length vert;
	} border_spacing;

	css_caption_side caption_side;
	css_clear clear;

	struct {
		enum { CSS_CLIP_INHERIT,
		       CSS_CLIP_AUTO,
		       CSS_CLIP_RECT } clip;
		struct {
			enum { CSS_CLIP_RECT_AUTO,
			       CSS_CLIP_RECT_LENGTH } rect;
			struct css_length value;
		} rect[4]; /**< top, right, bottom, left */
	} clip;

	colour color;

	/** \todo content and counters */

	css_cursor cursor;
	css_direction direction;
	css_display display;
	css_empty_cells empty_cells;
	css_float float_;

	/* font properties */
	css_font_family font_family;
	struct {
		enum { CSS_FONT_SIZE_INHERIT,
		       CSS_FONT_SIZE_ABSOLUTE,
		       CSS_FONT_SIZE_LENGTH,
		       CSS_FONT_SIZE_PERCENT } size;
		union {
			struct css_length length;
			float absolute;
			float percent;
		} value;
	} font_size;
	css_font_style font_style;
	css_font_variant font_variant;
	css_font_weight font_weight;

	struct {
		enum { CSS_HEIGHT_INHERIT,
		       CSS_HEIGHT_AUTO,
		       CSS_HEIGHT_LENGTH } height;
		struct css_length length;
	} height;

	struct {
		enum { CSS_LETTER_SPACING_INHERIT,
		       CSS_LETTER_SPACING_NORMAL,
		       CSS_LETTER_SPACING_LENGTH } letter_spacing;
		struct css_length length;
	} letter_spacing;

	struct {
		enum { CSS_LINE_HEIGHT_INHERIT,
		       CSS_LINE_HEIGHT_ABSOLUTE,
		       CSS_LINE_HEIGHT_LENGTH,
		       CSS_LINE_HEIGHT_PERCENT } size;
		union {
			float absolute;
			struct css_length length;
			float percent;
		} value;
	} line_height;

	/* list properties */
	struct {
		css_list_style_image_type type;
		char *uri;
	} list_style_image;
	css_list_style_position list_style_position;
	css_list_style_type list_style_type;

	/* margins */
	struct {
		enum { CSS_MARGIN_INHERIT,
		       CSS_MARGIN_LENGTH,
		       CSS_MARGIN_PERCENT,
		       CSS_MARGIN_AUTO } margin;
		union {
			struct css_length length;
			float percent;
		} value;
	} margin[4];  /**< top, right, bottom, left */

	/* min/max width/height */
	struct {
		enum { CSS_MAX_HEIGHT_INHERIT,
		       CSS_MAX_HEIGHT_NONE,
		       CSS_MAX_HEIGHT_LENGTH,
		       CSS_MAX_HEIGHT_PERCENT } max_height;
		union {
			struct css_length length;
			float percent;
		} value;
	} max_height;
	struct {
		enum { CSS_MAX_WIDTH_INHERIT,
		       CSS_MAX_WIDTH_NONE,
		       CSS_MAX_WIDTH_LENGTH,
		       CSS_MAX_WIDTH_PERCENT } max_width;
		union {
			struct css_length length;
			float percent;
		} value;
	} max_width;
	struct {
		enum { CSS_MIN_HEIGHT_INHERIT,
		       CSS_MIN_HEIGHT_LENGTH,
		       CSS_MIN_HEIGHT_PERCENT } min_height;
		union {
			struct css_length length;
			float percent;
		} value;
	} min_height;
	struct {
		enum { CSS_MIN_WIDTH_INHERIT,
		       CSS_MIN_WIDTH_LENGTH,
		       CSS_MIN_WIDTH_PERCENT } min_width;
		union {
			struct css_length length;
			float percent;
		} value;
	} min_width;

	struct {
		enum { CSS_ORPHANS_INHERIT,
		       CSS_ORPHANS_INTEGER } orphans;
		int value;
	} orphans;

	struct {
		struct {
			css_outline_color_type color;
			colour value;
		} color;
		struct css_border_width width;
		css_border_style style;
	} outline;

	css_overflow overflow;

	/* padding */
	struct {
		enum { CSS_PADDING_INHERIT,
		       CSS_PADDING_LENGTH,
		       CSS_PADDING_PERCENT } padding;
		union {
			struct css_length length;
			float percent;
		} value;
	} padding[4];  /**< top, right, bottom, left */

	css_page_break_after page_break_after;
	css_page_break_before page_break_before;
	css_page_break_inside page_break_inside;

	struct {
		enum { CSS_POS_INHERIT,
		       CSS_POS_AUTO,
		       CSS_POS_PERCENT,
		       CSS_POS_LENGTH } pos;
		union {
			struct css_length length;
			float percent;
		} value;
	} pos[4]; /**< top, right, bottom, left */

	css_position position;

	/** \todo quotes */

	css_table_layout table_layout;

	/* text properties */
	css_text_align text_align;
	css_text_decoration text_decoration;
	struct {
		enum { CSS_TEXT_INDENT_INHERIT,
		       CSS_TEXT_INDENT_LENGTH,
		       CSS_TEXT_INDENT_PERCENT } size;
		union {
		       struct css_length length;
		       float percent;
		} value ;
	} text_indent;
	css_text_transform text_transform;

	css_unicode_bidi unicode_bidi;

	struct {
		css_vertical_align_type type;
		union {
			struct css_length length;
			float percent;
		} value;
	} vertical_align;

	css_visibility visibility;

	css_white_space white_space;

	struct {
		enum { CSS_WIDOWS_INHERIT,
		       CSS_WIDOWS_INTEGER } widows;
		int value;
	} widows;

	struct {
		enum { CSS_WIDTH_INHERIT,
		       CSS_WIDTH_AUTO,
		       CSS_WIDTH_LENGTH,
		       CSS_WIDTH_PERCENT } width;
		union {
			struct css_length length;
			float percent;
		} value;
	} width;

	struct {
		enum { CSS_WORD_SPACING_INHERIT,
		       CSS_WORD_SPACING_NORMAL,
		       CSS_WORD_SPACING_LENGTH } word_spacing;
		struct css_length length;
	} word_spacing;

	struct {
		enum { CSS_Z_INDEX_INHERIT,
		       CSS_Z_INDEX_AUTO,
		       CSS_Z_INDEX_INTEGER } z_index;
		int value;
	} z_index;
};

struct css_stylesheet;

/** Data specific to CONTENT_CSS. */
struct content_css_data {
	struct css_stylesheet *css;	/**< Opaque stylesheet data. */
	unsigned int import_count;	/**< Number of entries in import_url. */
	char **import_url;		/**< Imported stylesheet urls. */
	struct content **import_content; /**< Imported stylesheet contents. */
};


extern const struct css_style css_base_style;
extern const struct css_style css_empty_style;
extern const struct css_style css_blank_style;


#ifdef CSS_INTERNALS

/** Type of a css_selector. */
typedef enum {
	CSS_SELECTOR_ELEMENT,
	CSS_SELECTOR_ID,
	CSS_SELECTOR_CLASS,
	CSS_SELECTOR_ATTRIB,
	CSS_SELECTOR_ATTRIB_EQ,
	CSS_SELECTOR_ATTRIB_INC,
	CSS_SELECTOR_ATTRIB_DM,
	CSS_SELECTOR_ATTRIB_PRE,
	CSS_SELECTOR_ATTRIB_SUF,
	CSS_SELECTOR_ATTRIB_SUB,
	CSS_SELECTOR_PSEUDO,
} css_selector_type;

/** Relationship to combiner in a css_selector. */
typedef enum {
	CSS_COMB_NONE,
	CSS_COMB_ANCESTOR,
	CSS_COMB_PARENT,
	CSS_COMB_PRECEDED,
} css_combinator;

/** Representation of a CSS selector. */
struct css_selector {
	css_selector_type type;
	const char *data;
	unsigned int data_length;
	const char *data2;
	unsigned int data2_length;
	struct css_selector *detail;
	struct css_selector *combiner;
	struct css_selector *next;
	css_combinator comb;
	struct css_style *style;
	unsigned long specificity;
};

/** Type of a css_node. */
typedef enum {
	CSS_NODE_DECLARATION,
	CSS_NODE_IDENT,
	CSS_NODE_NUMBER,
	CSS_NODE_PERCENTAGE,
	CSS_NODE_DIMENSION,
	CSS_NODE_STRING,
	CSS_NODE_DELIM,
	CSS_NODE_URI,
	CSS_NODE_HASH,
	CSS_NODE_UNICODE_RANGE,
	CSS_NODE_INCLUDES,
	CSS_NODE_FUNCTION,
	CSS_NODE_DASHMATCH,
	CSS_NODE_PREFIX,
	CSS_NODE_SUFFIX,
	CSS_NODE_SUBSTR,
	CSS_NODE_COLON,
	CSS_NODE_COMMA,
	CSS_NODE_DOT,
	CSS_NODE_PLUS,
	CSS_NODE_GT,
	CSS_NODE_PAREN,
	CSS_NODE_BRAC,
} css_node_type;

/** A node in a CSS parse tree. */
struct css_node {
	css_node_type type;
	const char *data;
	unsigned int data_length;
	struct css_node *value;
	struct css_node *next;
	css_combinator comb;
	struct css_style *style;
	unsigned long specificity;
	struct content *stylesheet;
};


#define HASH_SIZE (47 + 1)

/** Representation of a CSS 2 style sheet. */
struct css_stylesheet {
	struct css_selector *rule[HASH_SIZE];
};

/** Parameters to and results from the CSS parser. */
struct css_parser_params {
	bool ruleset_only;
	struct content *stylesheet;
	struct css_node *declaration;
	bool syntax_error;
	bool memory_error;
};

/** Token type for the CSS parser. */
struct css_parser_token {
	const char *text;
	unsigned int length;
};

#endif


struct content;

bool css_convert(struct content *c, int width, int height);
void css_destroy(struct content *c);

#ifdef CSS_INTERNALS

struct css_node * css_new_node(struct content *stylesheet,
		css_node_type type,
		const char *data, unsigned int data_length);
void css_free_node(struct css_node *node);
struct css_selector * css_new_selector(css_selector_type type,
		const char *data, unsigned int data_length);
void css_free_selector(struct css_selector *node);
void css_atimport(struct content *c, struct css_node *node);
void css_add_ruleset(struct content *c,
		struct css_selector *selector,
		struct css_node *declaration);
void css_add_declarations(struct css_style *style,
		struct css_node *declaration);
unsigned int css_hash(const char *s, int length);

int css_tokenise(unsigned char **buffer, unsigned char *end,
		unsigned char **token_text);

void css_parser_Trace(FILE *TraceFILE, char *zTracePrompt);
void *css_parser_Alloc(void *(*mallocProc)(/*size_t*/ int));
void css_parser_Free(void *p, void (*freeProc)(void*));
void css_parser_(void *yyp, int yymajor, struct css_parser_token yyminor,
		struct css_parser_params *param);
const char *css_parser_TokenName(int tokenType);

#endif

void css_get_style(struct content *c, xmlNode *n, struct css_style * style);
void css_cascade(struct css_style * const style,
		const struct css_style * const apply);
void css_merge(struct css_style * const style,
		const struct css_style * const apply);
void css_parse_property_list(struct content *c, struct css_style * style,
		char * str);
colour named_colour(const char *name);
void css_dump_style(const struct css_style * const style);
void css_dump_stylesheet(const struct css_stylesheet * stylesheet);

float len(struct css_length * length, struct css_style * style);

#endif

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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

/*
 * structures and typedefs
 */

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

/** Representation of a complete CSS 2 style. */
struct css_style {
	colour background_color;

	struct {
		colour color;
		struct {
			enum { CSS_BORDER_WIDTH_INHERIT,
			       CSS_BORDER_WIDTH_LENGTH } width;
			struct css_length value;
		} width;
		css_border_style style;
	} border[4];  /**< top, right, bottom, left */

	css_clear clear;
	colour color;
	css_display display;
	css_float float_;

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

        css_font_family font_family;
	css_font_weight font_weight;
	css_font_style font_style;
	css_font_variant font_variant;

	struct {
		enum { CSS_HEIGHT_INHERIT,
		       CSS_HEIGHT_AUTO,
		       CSS_HEIGHT_LENGTH } height;
		struct css_length length;
	} height;

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

	struct {
		enum { CSS_PADDING_INHERIT,
		       CSS_PADDING_LENGTH,
		       CSS_PADDING_PERCENT } padding;
		union {
			struct css_length length;
			float percent;
		} value;
	} padding[4];  /**< top, right, bottom, left */

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

	css_visibility visibility;

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

	css_white_space white_space;
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

typedef enum {
	CSS_NODE_BLOCK,
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
	CSS_NODE_COLON,
	CSS_NODE_COMMA,
	CSS_NODE_DOT,
	CSS_NODE_PLUS,
	CSS_NODE_GT,
	CSS_NODE_PAREN,
	CSS_NODE_BRAC,
	CSS_NODE_SELECTOR,
	CSS_NODE_ID,
	CSS_NODE_CLASS,
	CSS_NODE_ATTRIB,
	CSS_NODE_ATTRIB_EQ,
	CSS_NODE_ATTRIB_INC,
	CSS_NODE_ATTRIB_DM,
	CSS_NODE_PSEUDO,
} css_node_type;

typedef enum {
	CSS_COMB_NONE,
	CSS_COMB_ANCESTOR,
	CSS_COMB_PARENT,
	CSS_COMB_PRECEDED,
} css_combinator;

struct css_node {
	css_node_type type;
	char *data;
	char *data2;
	struct css_node *left;
	struct css_node *right;
	struct css_node *next;
	css_combinator comb;
	struct css_style *style;
	unsigned long specificity;
};

#include "netsurf/css/scanner.h"

#define HASH_SIZE (47 + 1)

struct css_stylesheet {
	yyscan_t lexer;
	void *parser;
	struct css_node *rule[HASH_SIZE];
};

struct parse_params {
	int ruleset_only;
	struct content *stylesheet;
	struct css_node *declaration;
	bool syntax_error;
};

#endif

/*
 * interface
 */

struct content;

void css_create(struct content *c, const char *params[]);
int css_convert(struct content *c, unsigned int width, unsigned int height);
void css_revive(struct content *c, unsigned int width, unsigned int height);
void css_destroy(struct content *c);

#ifdef CSS_INTERNALS

struct css_node * css_new_node(css_node_type type, char *data,
		struct css_node *left, struct css_node *right);
void css_free_node(struct css_node *node);
char *css_unquote(char *s);
void css_atimport(struct content *c, struct css_node *node);
void css_add_ruleset(struct content *c,
		struct css_node *selector,
		struct css_node *declaration);
void css_add_declarations(struct css_style *style, struct css_node *declaration);
unsigned int css_hash(const char *s);

void css_parser_Trace(FILE *TraceFILE, char *zTracePrompt);
void *css_parser_Alloc(void *(*mallocProc)(/*size_t*/int));
void css_parser_Free(void *p, void (*freeProc)(void*));
void css_parser_(void *yyp, int yymajor, char* yyminor,
		struct parse_params *param);

#endif

void css_get_style(struct content *c, xmlNode *n, struct css_style * style);
void css_cascade(struct css_style * const style, const struct css_style * const apply);
void css_merge(struct css_style * const style, const struct css_style * const apply);
void css_parse_property_list(struct css_style * style, char * str);
colour named_colour(const char *name);
void css_dump_style(const struct css_style * const style);
void css_dump_stylesheet(const struct css_stylesheet * stylesheet);

float len(struct css_length * length, struct css_style * style);

#endif

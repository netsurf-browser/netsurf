/**
 * $Id: css.h,v 1.6 2003/04/13 12:50:10 bursa Exp $
 */

#ifndef _NETSURF_CSS_CSS_H_
#define _NETSURF_CSS_CSS_H_

#include "css_enum.h"

/**
 * structures and typedefs
 */

typedef unsigned long colour;  /* 0xbbggrr */
#define TRANSPARENT 0x1000000
#define CSS_COLOR_INHERIT 0x2000000

struct css_length {
	float value;
	css_unit unit;
};

struct css_style {
	colour background_color;
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

	css_font_weight font_weight;
	css_font_style font_style;

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

	css_text_align text_align;

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
};

struct css_stylesheet;

struct css_selector {
	const char *element;
	char *class;
	char *id;
};


extern const struct css_style css_base_style;
extern const struct css_style css_empty_style;
extern const struct css_style css_blank_style;


#ifdef CSS_INTERNALS

typedef enum {
	NODE_BLOCK,
	NODE_DECLARATION,
	NODE_IDENT,
	NODE_NUMBER,
	NODE_PERCENTAGE,
	NODE_DIMENSION,
	NODE_STRING,
	NODE_DELIM,
	NODE_URI,
	NODE_HASH,
	NODE_UNICODE_RANGE,
	NODE_INCLUDES,
	NODE_FUNCTION,
	NODE_DASHMATCH,
	NODE_COLON,
	NODE_COMMA,
	NODE_PLUS,
	NODE_GT,
	NODE_PAREN,
	NODE_BRAC,
	NODE_SELECTOR,
	NODE_ID,
	NODE_CLASS,
} node_type;

typedef enum {
	COMB_NONE,
	COMB_ANCESTOR,
	COMB_PARENT,
	COMB_PRECEDED,
} combinator;

struct node {
	node_type type;
	char *data;
	struct node *left;
	struct node *right;
	struct node *next;
	combinator comb;
	struct css_style *style;
};

#include "netsurf/css/scanner.h"

#define HASH_SIZE (47 + 1)

struct css_stylesheet {
	yyscan_t lexer;
	void *parser;
	struct node *rule[HASH_SIZE];
};

struct parse_params {
	int ruleset_only;
	struct content *stylesheet;
	struct node *declaration;
};

#endif

/**
 * interface
 */

#include "netsurf/content/content.h"

void css_create(struct content *c);
void css_process_data(struct content *c, char *data, unsigned long size);
int css_convert(struct content *c, unsigned int width, unsigned int height);
void css_revive(struct content *c, unsigned int width, unsigned int height);
void css_reformat(struct content *c, unsigned int width, unsigned int height);
void css_destroy(struct content *c);

#ifdef CSS_INTERNALS

struct node * css_new_node(node_type type, char *data,
		struct node *left, struct node *right);
void css_free_node(struct node *node);
void css_atimport(struct content *c, struct node *node);
void css_add_ruleset(struct content *c,
		struct node *selector,
		struct node *declaration);
void css_add_declarations(struct css_style *style, struct node *declaration);
unsigned int css_hash(const char *s);

void css_parser_Trace(FILE *TraceFILE, char *zTracePrompt);
void *css_parser_Alloc(void *(*mallocProc)(int));
void css_parser_Free(void *p, void (*freeProc)(void*));
void css_parser_(void *yyp, int yymajor, char* yyminor,
		struct parse_params *param);

#endif

void css_get_style(struct content *c, struct css_selector * selector,
			unsigned int selectors, struct css_style * style);
void css_cascade(struct css_style * const style, const struct css_style * const apply);
void css_merge(struct css_style * const style, const struct css_style * const apply);
void css_parse_property_list(struct css_style * style, char * str);
colour named_colour(const char *name);

#endif

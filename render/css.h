/**
 * $Id: css.h,v 1.3 2002/06/18 21:24:21 bursa Exp $
 */

#include "css_enum.h"

/**
 * structures and typedefs
 */

typedef unsigned long colour;  /* 0xrrggbb */
#define TRANSPARENT 0x1000000

struct css_length {
	float value;
	css_unit unit;
};

struct css_style {
	css_display display;
	css_float float_;

	struct {
		enum { CSS_FONT_SIZE_INHERIT,
		       CSS_FONT_SIZE_ABSOLUTE,
		       CSS_FONT_SIZE_LENGTH,
		       CSS_FONT_SIZE_PERCENT } size;
		union {
			float absolute;
			struct css_length length;
			float percent;
		} value;
	} font_size;

	struct {
		enum { CSS_HEIGHT_AUTO,
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
		enum { CSS_WIDTH_AUTO,
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
	const char * element;
	char * class;
	char * id;
};

extern const struct css_style css_base_style;
extern const struct css_style css_empty_style;

/**
 * interface
 */

struct css_stylesheet * css_new_stylesheet(void);
void css_get_style(struct css_stylesheet * stylesheet, struct css_selector * selector,
			unsigned int selectors, struct css_style * style);
void css_parse_stylesheet(struct css_stylesheet * stylesheet, char * str);
void css_dump_style(const struct css_style * const style);
void css_dump_stylesheet(const struct css_stylesheet * stylesheet);
void css_cascade(struct css_style * const style, const struct css_style * const apply);
void css_parse_property_list(struct css_style * style, char * str);


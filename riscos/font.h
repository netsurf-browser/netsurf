/**
 * $Id: font.h,v 1.1 2002/09/11 14:24:02 monkeyson Exp $
 */

#ifndef _NETSURF_RISCOS_FONT_H_
#define _NETSURF_RISCOS_FONT_H_

/**
 * structures and typedefs
 */

#include "netsurf/render/css.h"
#include "oslib/font.h"

struct font_set;
typedef unsigned int font_id;
struct font_split {
	unsigned long width;
	unsigned long height;
	const char * end;
};

/**
 * interface
 */

struct font_set * font_set_create(void);
font_id font_add(struct font_set * font_set, const char * name, unsigned int weight,
		unsigned int size);
void font_set_free(struct font_set * font_set);
struct font_split font_split(struct font_set * font_set, font_id id, const char * text,
		unsigned long width, int force);
unsigned long font_width(struct css_style * style, const char * text, unsigned int length);

font_f riscos_font_css_to_handle(struct css_style* style);

void font_position_in_string(const char* text, struct css_style* style, int length, int x, int* char_offset, int* pixel_offset);

#endif

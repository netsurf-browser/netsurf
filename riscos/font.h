/**
 * $Id: font.h,v 1.2 2002/09/26 21:38:33 bursa Exp $
 */

#ifndef _NETSURF_RISCOS_FONT_H_
#define _NETSURF_RISCOS_FONT_H_

/**
 * structures and typedefs
 */

#include "netsurf/render/css.h"
#include "oslib/font.h"

typedef unsigned int font_id;

#define FONT_FAMILIES 1
#define FONT_CHUNKS 3
#define FONT_BOLD 2
#define FONT_SLANTED 1

struct font_set;
struct font_data {
	font_f handle[FONT_CHUNKS];
	unsigned int size;
	struct font_data *next;
};

/**
 * interface
 */

unsigned long font_width(struct font_data *font, const char * text, unsigned int length);
void font_position_in_string(const char* text, struct font_data *font,
		int length, int x, int* char_offset, int* pixel_offset);

struct font_set *font_new_set();
struct font_data *font_open(struct font_set *set, struct css_style *style);
void font_free_set(struct font_set *set);
char *font_utf8_to_string(struct font_data *data, const char *s, unsigned int length);

#endif

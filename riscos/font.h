/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#ifndef _NETSURF_RISCOS_FONT_H_
#define _NETSURF_RISCOS_FONT_H_

/**
 * structures and typedefs
 */

#include "netsurf/css/css.h"
#include "oslib/font.h"

typedef unsigned int font_id;

#define FONT_FAMILIES 1
#define FONT_BOLD 2
#define FONT_SLANTED 1

/* a font_set is just a linked list of font_data for each face for now */
struct font_set {
	struct font_data *font[FONT_FAMILIES * 4];
};

struct font_data {
	font_f handle;
	unsigned int size;
	unsigned int space_width;
	struct font_data *next;
};

/**
 * interface
 */

unsigned long font_width(struct font_data *font, const char * text, unsigned int length);
void font_position_in_string(const char* text, struct font_data *font,
		unsigned int length, unsigned long x, int* char_offset, int* pixel_offset);

struct font_set *font_new_set(void);
struct font_data *font_open(struct font_set *set, struct css_style *style);
void font_free_set(struct font_set *set);
char * font_split(struct font_data *data, const char * text, unsigned int length,
		unsigned int width, unsigned int *used_width);

#endif

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#ifndef _NETSURF_RISCOS_FONT_H_
#define _NETSURF_RISCOS_FONT_H_

#include "netsurf/css/css.h"

struct font_data {
	int handle;
	unsigned int size;
	unsigned int space_width;
	struct font_data *next;
};

struct font_set *font_new_set(void);
struct font_data *font_open(struct font_set *set, struct css_style *style);
void font_free_set(struct font_set *set);
unsigned long font_width(struct font_data *font, const char * text, unsigned int length);
void font_position_in_string(const char* text, struct font_data *font,
		unsigned int length, unsigned long x, int* char_offset, int* pixel_offset);
char * font_split(struct font_data *data, const char * text, unsigned int length,
		unsigned int width, unsigned int *used_width);

#endif

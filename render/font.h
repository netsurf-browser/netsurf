/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
 */

#ifndef _NETSURF_RENDER_FONT_H_
#define _NETSURF_RENDER_FONT_H_

#include <stddef.h>
#include "netsurf/css/css.h"

typedef enum {
	FONTTYPE_UFONT,
	FONTTYPE_STANDARD_UTF8ENC,
	FONTTYPE_STANDARD_LATIN1
} fonttype_e;

struct font_data {
	int id;
	int handle;
	fonttype_e ftype;
	unsigned int size;
	unsigned int space_width;
	struct font_data *next;
};

struct font_set *nsfont_new_set(void);
struct font_data *nsfont_open(struct font_set *set, struct css_style *style);
void nsfont_free_set(struct font_set *set);
unsigned long nsfont_width(struct font_data *font, const char *text,
		size_t length);
void nsfont_position_in_string(struct font_data *font, const char *text,
		size_t length, unsigned long x, int *char_offset,
		int *pixel_offset);
char *nsfont_split(struct font_data *font, const char *text,
		size_t length,
		unsigned int width, unsigned int *used_width);
void nsfont_paint(struct font_data *font, const char *str,
		size_t length, int xpos, int ypos, void *trfm);
void nsfont_txtenum(struct font_data *font, const char *text,
		size_t length,
		unsigned int *width,
		const char **rofontname,
		const char **rotext,
		size_t *rolength,
		size_t *consumed);
void nsfont_fill_nametable(void);

#endif

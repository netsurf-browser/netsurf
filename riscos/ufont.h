/* ufont.h
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2000 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
 */

#ifndef UFONT_HEADER_INCLUDED
#define UFONT_HEADER_INCLUDED

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oslib/font.h"
#include "oslib/os.h"

typedef struct ufont_font *ufont_f;

extern os_error *xufont_find_font(char const *font_name,
		int xsize,
		int ysize,
		int xres,
		int yres,
		ufont_f *font,
		int *xresOutP,
		int *yresOutP);

extern os_error *xufont_lose_font(ufont_f font);

extern os_error *xufont_paint(ufont_f font,
		unsigned char const *string,
		font_string_flags flags,
		int xpos,
		int ypos,
		font_paint_block const *block,
		os_trfm const *trfm,
		int length);

extern os_error *xufont_scan_string(ufont_f font,
		unsigned char const *string,
		font_string_flags flags,
		int x,
		int y,
		font_scan_block const *block,
		os_trfm const *trfm,
		int length,
		unsigned char const **split_point,
		int *x_out,
		int *y_out,
		int *length_out);

extern os_error *xufont_txtenum(ufont_f font,
		unsigned char const *string,
		font_string_flags flags,
		size_t length,
		int *width,
		unsigned char const **rofontname,
		unsigned char const **rotext,
		size_t *rolength,
		size_t *consumed);

extern os_error *xufont_convert(ufont_f font,
		unsigned char const *string,
		size_t length,
		char **presult,
		size_t **ptable);

#endif

/**
 * $Id: font.c,v 1.1 2002/07/27 21:10:45 bursa Exp $
 */

#include <stdio.h>
#include "netsurf/render/css.h"
#include "netsurf/render/font.h"
#include "netsurf/render/utils.h"
#include "oslib/font.h"

/**
 * functions
 */

extern font_f font;

unsigned long font_width(struct css_style * style, const char * text, unsigned int length)
{
	font_scan_block block;
	os_error * error;

	if (length == 0) return 0;

	block.space.x = block.space.y = 0;
	block.letter.x = block.letter.y = 0;
	block.split_char = -1;

	error = xfont_scan_string(font, text,
			font_GIVEN_BLOCK | font_GIVEN_LENGTH | font_GIVEN_FONT | font_KERN | font_RETURN_BBOX,
			0x7fffffff, 0x7fffffff,
			&block,
			0, length,
			0, 0, 0, 0);
	if (error != 0) {
		fprintf(stderr, "%s\n", error->errmess);
		die("font_scan_string failed");
	}
	return block.bbox.x1 / 800;
}


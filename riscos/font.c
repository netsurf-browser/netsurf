/**
 * $Id: font.c,v 1.2 2002/09/11 14:24:02 monkeyson Exp $
 */

#include <stdio.h>
#include "netsurf/render/css.h"
#include "netsurf/render/font.h"
#include "netsurf/render/utils.h"
#include "netsurf/desktop/gui.h"
#include "oslib/font.h"

/**
 * functions
 */

/** it is rather inefficient calling this all the time **/
font_f riscos_font_css_to_handle(struct css_style* style)
{
	int width = 12 * 16;
	int height = 12 * 16;
	char font_name[255];

	if (style->font_size.size == CSS_FONT_SIZE_LENGTH)
		width = height = style->font_size.value.length.value * 16;

	strcpy(font_name, "Homerton.");
	if (style->font_weight == CSS_FONT_WEIGHT_BOLD)
		strcat(font_name, "Bold");
	else 
		strcat(font_name, "Medium");

	if (style->font_style == CSS_FONT_STYLE_ITALIC || style->font_style == CSS_FONT_STYLE_OBLIQUE)
		strcat(font_name, ".Oblique");

	return font_find_font(font_name, width, height, 0, 0, 0, 0);
}

unsigned long font_width(struct css_style * style, const char * text, unsigned int length)
{
	font_scan_block block;
	os_error * error;
	font_f font;

	if (length == 0)
		return 0;

	block.space.x = block.space.y = 0;
	block.letter.x = block.letter.y = 0;
	block.split_char = -1;

	font = riscos_font_css_to_handle(style);

	error = xfont_scan_string(font, (char*) text,
			font_GIVEN_BLOCK | font_GIVEN_LENGTH | font_GIVEN_FONT | font_KERN | font_RETURN_BBOX,
			0x7fffffff, 0x7fffffff,
			&block,
			0, length,
			0, 0, 0, 0);
	if (error != 0) {
		fprintf(stderr, "%s\n", error->errmess);
		die("font_scan_string failed");
	}

//	fprintf(stderr, "Stated length %d, strlen %d\n", (int)length, strlen(text));

	if (length < 0x7fffffff)
	{
		if (text[length - 1] == ' ')
//		{
			block.bbox.x1 += 4*800;
/*			int minx,miny,maxx,maxy;
			char space = ' ';
//			fprintf(stderr, "Space at the end!\n");
			error = xfont_char_bbox(font, space, 0, &minx, &miny, &maxx, &maxy);
			if (error != 0) {
				fprintf(stderr, "%s\n", error->errmess);
				die("font_char_bbox failed");
			}
			block.bbox.x1 += maxx;
		}
//		else
//			fprintf(stderr, "No space\n");*/
	}

	font_lose_font(font);

	return block.bbox.x1 / 800;
}

void font_position_in_string(const char* text, struct css_style* style, int length, int x, int* char_offset, int* pixel_offset)
{
  font_f font;
  font_scan_block block;
  char* split_point;
  int x_out, y_out, length_out;

  block.space.x = block.space.y = 0;
  block.letter.x = block.letter.y = 0;
  block.split_char = -1;

  font = riscos_font_css_to_handle(style);
  
  xfont_scan_string(font, (char*) text,
    font_GIVEN_BLOCK | font_GIVEN_LENGTH | 
    font_GIVEN_FONT | font_KERN | font_RETURN_CARET_POS, 
    ro_x_units(x) * 400,
    0x7fffffff,
    &block, 0, length,
    &split_point, &x_out, &y_out, &length_out);

  font_lose_font(font);

  *char_offset = (int)(split_point - text);
  *pixel_offset = browser_x_units(x_out / 400);

  return;
}


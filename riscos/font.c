/**
 * $Id: font.c,v 1.3 2002/09/26 21:38:33 bursa Exp $
 */

#include <assert.h>
#include <stdio.h>
#include "utf-8.h"
#include "netsurf/render/css.h"
#include "netsurf/riscos/font.h"
#include "netsurf/render/utils.h"
#include "netsurf/desktop/gui.h"
#include "oslib/font.h"

/**
 * RISC OS fonts are 8-bit, so Unicode is displayed by combining many
 * font manager fonts. Each font manager font must have 128 characters of
 * Unicode in order, starting at character 128.
 */

/**
 * font id = font family * 4 + bold * 2 + slanted
 * font family: 0 = sans-serif, 1 = serif, ...
 */

const char * const font_table[FONT_FAMILIES * 4][FONT_CHUNKS] = {
	/* sans-serif */
	{ "Homerton.Medium\\EU0000",
	"Homerton.Medium\\EU0080",
	"Homerton.Medium\\EU0100" },
	{ "Homerton.Medium.Oblique\\EU0000",
	"Homerton.Medium.Oblique\\EU0080",
	"Homerton.Medium.Oblique\\EU0100" },
	{ "Homerton.Bold\\EU0000",
	"Homerton.Bold\\EU0080",
	"Homerton.Bold\\EU0100" },
	{ "Homerton.Bold.Oblique\\EU0000",
	"Homerton.Bold.Oblique\\EU0080",
	"Homerton.Bold.Oblique\\EU0100" },
};

/* a font_set is just a linked list of font_data for each face for now */
struct font_set {
	struct font_data *font[FONT_FAMILIES * 4];
};

void font_close(struct font_data *data);

/**
 * functions
 */

unsigned long font_width(struct font_data *font, const char * text, unsigned int length)
{
	font_scan_block block;
	os_error * error;
	char *text2;

	assert(font != 0 && text != 0);

	if (length == 0)
		return 0;

	block.space.x = block.space.y = 0;
	block.letter.x = block.letter.y = 0;
	block.split_char = -1;

	text2 = font_utf8_to_string(font, text, length);
	error = xfont_scan_string(font->handle[0], text2,
			font_GIVEN_BLOCK | font_GIVEN_FONT | font_KERN | font_RETURN_BBOX,
			0x7fffffff, 0x7fffffff,
			&block,
			0, 0,
			0, 0, 0, 0);
	if (error != 0) {
		fprintf(stderr, "%s\n", error->errmess);
		die("font_scan_string failed");
	}

/* 	fprintf(stderr, "font_width: '%.*s' => '%s' => %i %i %i %i\n", length, text, text2, */
/* 			block.bbox.x0, block.bbox.y0, block.bbox.x1, block.bbox.y1); */
	free(text2);

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

	return block.bbox.x1 / 800;
}

void font_position_in_string(const char* text, struct font_data* font,
		int length, int x, int* char_offset, int* pixel_offset)
{
  font_scan_block block;
  char* split_point;
  int x_out, y_out, length_out;
  char *text2;

  assert(font != 0 && text != 0);

  block.space.x = block.space.y = 0;
  block.letter.x = block.letter.y = 0;
  block.split_char = -1;

  text2 = font_utf8_to_string(font, text, length);
  xfont_scan_string(font, text2,
    font_GIVEN_BLOCK | font_GIVEN_FONT | font_KERN | font_RETURN_CARET_POS,
    ro_x_units(x) * 400,
    0x7fffffff,
    &block, 0, 0,
    &split_point, &x_out, &y_out, &length_out);
  free(text2);

  *char_offset = (int)(split_point - text);
  *pixel_offset = browser_x_units(x_out / 400);

  return;
}


struct font_set *font_new_set()
{
	struct font_set *set = xcalloc(1, sizeof(*set));
	unsigned int i;

	for (i = 0; i < FONT_FAMILIES * 4; i++)
		set->font[i] = 0;

	return set;
}


struct font_data *font_open(struct font_set *set, struct css_style *style)
{
	struct font_data *data;
	unsigned int i;
	unsigned int size = 16 * 11;
	unsigned int f = 0;

	assert(set != 0);

	if (style->font_size.size == CSS_FONT_SIZE_LENGTH)
		size = style->font_size.value.length.value * 16;

	switch (style->font_weight) {
		case CSS_FONT_WEIGHT_BOLD:
		case CSS_FONT_WEIGHT_600:
		case CSS_FONT_WEIGHT_700:
		case CSS_FONT_WEIGHT_800:
		case CSS_FONT_WEIGHT_900:
			f += FONT_BOLD;
			break;
		default:
			break;
	}

	switch (style->font_style) {
		case CSS_FONT_STYLE_ITALIC:
		case CSS_FONT_STYLE_OBLIQUE:
			f += FONT_SLANTED;
			break;
		default:
			break;
	}

	for (data = set->font[f]; data != 0; data = data->next)
		if (data->size == size)
	        	return data;

	data = xcalloc(1, sizeof(*data));

	for (i = 0; i < FONT_CHUNKS; i++) {
		font_f handle = font_find_font(font_table[f][i], size, size, 0, 0, 0, 0);
		data->handle[i] = handle;
	}
	data->size = size;

	data->next = set->font[f];
	set->font[f] = data;

	return data;
}


void font_free_set(struct font_set *set)
{
	unsigned int i;
	struct font_data *data, *next;

	assert(set != 0);

	for (i = 0; i < FONT_FAMILIES * 4; i++) {
		for (data = set->font[i]; data != 0; data = next) {
			next = data->next;
			font_close(data);
		}
        }

	free(set);
}


void font_close(struct font_data *data)
{
	unsigned int i;

	for (i = 0; i < FONT_CHUNKS; i++)
		font_lose_font(data->handle[i]);

	free(data);
}

#define BUFFER_CHUNK 100

char *font_utf8_to_string(struct font_data *font, const char *s, unsigned int length)
{
	unsigned long buffer_len = BUFFER_CHUNK;
	char *d = xcalloc(buffer_len, sizeof(char));
	unsigned int chunk0 = 0, chunk1;
	unsigned int u, chars, i = 0, j = 0;

	assert(font != 0 && s != 0);

/* 	fprintf(stderr, "font_utf8_to_string: '%s'", s); */

	while (j < length && s[j] != 0) {
		u = sgetu8(s + j, &chars);
		j += chars;
		if (buffer_len < i + 5) {
			buffer_len += BUFFER_CHUNK;
			d = xrealloc(d, buffer_len * sizeof(char));
		}
		chunk1 = u / 0x80;
		if (FONT_CHUNKS <= chunk1 || (u < 0x20 || (0x80 <= u && u <= 0x9f))) {
			d[i++] = '?';
		} else {
			if (chunk0 != chunk1) {
				d[i++] = font_COMMAND_FONT;
				d[i++] = font->handle[chunk1];
				chunk0 = chunk1;
			}
			d[i++] = 0x80 + (u % 0x80);
		}
	}
	d[i] = 0;

/* 	fprintf(stderr, " => '%s'\n", d); */

	return d;
}


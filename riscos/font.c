/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
 */

/** \file
 * Font handling (RISC OS implementation).
 *
 * The Font Manager is used to handle and render fonts.
 */

#include <assert.h>
#include <stdio.h>
#include "oslib/font.h"
#include "oslib/os.h"
#include "netsurf/css/css.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/render/font.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/ufont.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#define FONT_FAMILIES 5 /* Number of families */
#define FONT_FACES 8    /* Number of faces */

/* Font Variants */
#define FONT_SMALLCAPS 4

/* Font Styles */
#define FONT_BOLD 2
#define FONT_SLANTED 1

/* Font families */
#define FONT_SANS_SERIF (0 * FONT_FACES)
#define FONT_SERIF      (1 * FONT_FACES)
#define FONT_MONOSPACE  (2 * FONT_FACES)
#define FONT_CURSIVE    (3 * FONT_FACES)
#define FONT_FANTASY    (4 * FONT_FACES)

/* a font_set is just a linked list of font_data for each face for now */
struct font_set {
	struct font_data *font[FONT_FAMILIES * FONT_FACES];
};

static os_error *nsfont_open_ufont(const char *fontNameP, const char *fbFontNameP, int size, int *handleP, bool *using_fb);
static os_error *nsfont_open_standard(const char *fontNameP, const char *fbFontNameP, int size, int *handleP, bool *using_fb);

/** Table of font names for UFont and an UTF-8 capable FontManager.
 *
 * font id = font family * 8 + smallcaps * 4 + bold * 2 + slanted
 *
 * font family: 0 = sans-serif, 1 = serif, 2 = monospace, 3 = cursive,
 * 4 = fantasy.
 * Font family 0 must be available as it is the replacement font when
 * the other font families can not be found.
 */
static const char * const ufont_table[FONT_FAMILIES * FONT_FACES] = {
	/* sans-serif */
/*0*/	"Homerton.Medium",
/*1*/	"Homerton.Medium.Oblique",
/*2*/	"Homerton.Bold",
/*3*/	"Homerton.Bold.Oblique",
	"Homerton.Medium.SmallCaps",
	"Homerton.Medium.Oblique.SmallCaps",
	"Homerton.Bold.SmallCaps",
	"Homerton.Bold.Oblique.SmallCaps",
	/* serif */
/*8*/	"Trinity.Medium",
/*9*/	"Trinity.Medium.Italic",
/*10*/	"Trinity.Bold",
/*11*/	"Trinity.Bold.Italic",
	"Trinity.Medium.SmallCaps",
	"Trinity.Medium.Italic.SmallCaps",
	"Trinity.Bold.SmallCaps",
	"Trinity.Bold.Italic.SmallCaps",
	/* monospace */
/*16*/	"Corpus.Medium",
/*17*/	"Corpus.Medium.Oblique",
/*18*/	"Corpus.Bold",
/*19*/	"Corpus.Bold.Oblique",
	"Corpus.Medium.SmallCaps",
	"Corpus.Medium.Oblique.SmallCaps",
	"Corpus.Bold.SmallCaps",
	"Corpus.Bold.Oblique.SmallCaps",
	/* cursive */
/*24*/	"Churchill.Medium",
/*25*/	"Churchill.Medium.Oblique",
/*26*/	"Churchill.Bold",
/*27*/	"Churchill.Bold.Oblique",
	"Churchill.Medium.SmallCaps",
	"Churchill.Medium.Oblique.SmallCaps",
	"Churchill.Bold.SmallCaps",
	"Churchill.Bold.Oblique.SmallCaps",
	/* fantasy */
/*32*/	"Sassoon.Primary",
/*33*/	"Sassoon.Primary.Oblique",
/*34*/	"Sassoon.Primary.Bold",
/*35*/	"Sassoon.Primary.Bold.Oblique",
	"Sassoon.Primary.SmallCaps",
	"Sassoon.Primary.Oblique.SmallCaps",
	"Sassoon.Primary.Bold.SmallCaps",
	"Sassoon.Primary.Bold.Oblique.SmallCaps",
};

/** Table of Latin1 encoded font names for a pre-UTF-8 capable FontManager.
 *
 * font id = font family * 8 + smallcaps * 4 + bold * 2 + slanted
 *
 * font family: 0 = sans-serif, 1 = serif, 2 = monospace, 3 = cursive,
 * 4 = fantasy.
 * Font family 0 must be available as it is the replacement font when
 * the other font families can not be found.
 */
static const char * const font_table[FONT_FAMILIES * FONT_FACES] = {
	/* sans-serif */
/*0*/	"Homerton.Medium\\ELatin1",
/*1*/	"Homerton.Medium.Oblique\\ELatin1",
/*2*/	"Homerton.Bold\\ELatin1",
/*3*/	"Homerton.Bold.Oblique\\ELatin1",
	"Homerton.Medium.SmallCaps\\ELatin1",
	"Homerton.Medium.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
	"Homerton.Bold.SmallCaps\\ELatin1",
	"Homerton.Bold.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
	/* serif */
/*8*/	"Trinity.Medium\\ELatin1",
/*9*/	"Trinity.Medium.Italic\\ELatin1",
/*10*/	"Trinity.Bold\\ELatin1",
/*11*/	"Trinity.Bold.Italic\\ELatin1",
	"Trinity.Medium.SmallCaps\\ELatin1",
	"Trinity.Medium.Italic.SmallCaps\\ELatin1",
	"Trinity.Bold.SmallCaps\\ELatin1",
	"Trinity.Bold.Italic.SmallCaps\\ELatin1",
	/* monospace */
/*16*/	"Corpus.Medium\\ELatin1",
/*17*/	"Corpus.Medium.Oblique\\ELatin1",
/*18*/	"Corpus.Bold\\ELatin1",
/*19*/	"Corpus.Bold.Oblique\\ELatin1",
	"Corpus.Medium.SmallCaps\\ELatin1",
	"Corpus.Medium.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
	"Corpus.Bold.SmallCaps\\ELatin1",
	"Corpus.Bold.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
	/* cursive */
/*24*/	"Churchill.Medium\\ELatin1",
/*25*/	"Churchill.Medium\\ELatin1\\M65536 0 13930 65536 0 0",
/*26*/	"Churchill.Bold\\ELatin1",
/*27*/	"Churchill.Bold\\ELatin1\\M65536 0 13930 65536 0 0",
	"Churchill.Medium.SmallCaps\\ELatin1",
	"Churchill.Medium.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
	"Churchill.Bold.SmallCaps\\ELatin1",
	"Churchill.Bold.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
	/* fantasy */
/*32*/	"Sassoon.Primary\\ELatin1",
/*33*/	"Sassoon.Primary\\ELatin1\\M65536 0 13930 65536 0 0",
/*34*/	"Sassoon.Primary.Bold\\ELatin1",
/*35*/	"Sassoon.Primary.Bold\\ELatin1\\M65536 0 13930 65536 0 0",
	"Sassoon.Primary.SmallCaps\\ELatin1",
	"Sassoon.Primary.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
	"Sassoon.Primary.Bold.SmallCaps\\ELatin1",
	"Sassoon.Primary.Bold.SmallCaps\\ELatin1\\M65536 0 13930 65536 0 0",
};

/**
 * Create an empty font_set.
 *
 * \return an opaque struct font_set.
 */
struct font_set *nsfont_new_set(void)
{
	struct font_set *set;
	unsigned int i;

	LOG(("nsfont_new_set()\n"));

	if ((set = malloc(sizeof(*set))) == NULL)
		return NULL;

	for (i = 0; i < FONT_FAMILIES * FONT_FACES; i++)
		set->font[i] = NULL;

	return set;
}

/**
 * Open a font for use based on a css_style.
 *
 * \param set a font_set, as returned by nsfont_new_set()
 * \param style a css_style which describes the font
 * \return a struct font_data, with an opaque font handle in handle
 *
 * The set is updated to include the font, if it was not present.
 */
struct font_data *nsfont_open(struct font_set *set, struct css_style *style)
{
	struct font_data *data;
	unsigned int size = option_font_size * 1.6;
	unsigned int f = 0;
	int fhandle;
	os_error *error;
	bool using_fb;

	assert(set != NULL);
	assert(style != NULL);

	if (style->font_size.size == CSS_FONT_SIZE_LENGTH)
		size = len(&style->font_size.value.length, style) *
				72.0 / 90.0 * 16;
	if (size < option_font_min_size * 1.6)
		size = option_font_min_size * 1.6;
	if (1600 < size)
		size = 1600;

	switch (style->font_family) {
		case CSS_FONT_FAMILY_SANS_SERIF:
			f += FONT_SANS_SERIF;
			break;
		case CSS_FONT_FAMILY_SERIF:
			f += FONT_SERIF;
			break;
		case CSS_FONT_FAMILY_MONOSPACE:
			f += FONT_MONOSPACE;
			break;
		case CSS_FONT_FAMILY_CURSIVE:
			f += FONT_CURSIVE;
			break;
		case CSS_FONT_FAMILY_FANTASY:
			f += FONT_FANTASY;
			break;
		default:
			break;
	}

	switch (style->font_variant) {
		case CSS_FONT_VARIANT_SMALL_CAPS:
			f += FONT_SMALLCAPS;
			break;
		default:
			break;
	}

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

	for (data = set->font[f]; data != NULL; data = data->next)
		if (data->size == size)
			return data;

	if ((data = malloc(sizeof(*data))) == NULL)
		return NULL;

	/* Strategy : first try the UFont font code with given font name
	 * or the default font name if the former fails.
	 * If this still fails, try the use the default RISC OS font open
	 * in UTF-8 encoding (again first with given font name, then with
	 * the default font name).
	 * If this still fails, we repeat the previous step but now using
	 * the Latin 1 encoding.
	 */
	if ((error = nsfont_open_ufont(ufont_table[f], ufont_table[f % 4], (int)size, &fhandle, &using_fb)) != NULL) {
		char fontName1[128], fontName2[128];
		/* Go for the UTF-8 encoding with standard FontManager */
		strcpy(fontName1, ufont_table[f]);
		strcat(fontName1, "\\EUTF8");
		strcpy(fontName2, ufont_table[f % 4]);
		strcat(fontName2, "\\EUTF8");
		if ((error = nsfont_open_standard(fontName1, fontName2, (int)size, &fhandle, &using_fb)) != NULL) {
			/* All UTF-8 font methods failed, only support Latin 1 */
			if ((error = nsfont_open_standard(font_table[f], font_table[f % 4], (int)size, &fhandle, &using_fb)) != NULL) {
				LOG(("(u)font_find_font failed : %s\n", error->errmess));
				die("(u)font_find_font failed");
			}
			data->ftype = FONTTYPE_STANDARD_LATIN1;
		} else
			data->ftype = FONTTYPE_STANDARD_UTF8ENC;
	} else
		data->ftype = FONTTYPE_UFONT;

	data->id = (using_fb) ? f % 4 : f;
	data->handle = fhandle;
	data->size = size;
	data->space_width = nsfont_width(data, " ", sizeof(" ")-1);

	data->next = set->font[f];
	set->font[f] = data;

	return data;
}


/**
 * Open font via UFont code.
 *
 * \param fontNameP UFont font name
 * \param fbFontNameP fallback UFont font name
 * \param size font size
 * \param handle returning UFont handle in case there isn't an error.
 * \param using_fb returning whether the fallback font was used or not.
 * \return error in case there was one.
 */
static os_error *nsfont_open_ufont(const char *fontNameP, const char *fbFontNameP, int size, int *handleP, bool *using_fb)
{
	os_error *errorP;
	*handleP = 0; *using_fb = false;
	if ((errorP = xufont_find_font(fontNameP, size, size, 0, 0, (ufont_f *)handleP, NULL, NULL)) == NULL)
		return NULL;
	LOG(("ufont_find_font(<%s>) failed <%s> (case 1)", fontNameP, errorP->errmess));
	/* If the fallback font is the same as the first font name, return */
	if (strcmp(fontNameP, fbFontNameP) == 0)
		return errorP;
	*using_fb = true;
	if ((errorP = xufont_find_font(fbFontNameP, size, size, 0, 0, (ufont_f *)handleP, NULL, NULL)) == NULL)
		return NULL;
	LOG(("ufont_find_font(<%s>) failed <%s> (case 2)", fbFontNameP, errorP->errmess));
	return errorP;
}


/**
 * Open font via standard FontManager.
 *
 * \param fontNameP RISC OS font name
 * \param fbFontNameP fallback RISC OS font name
 * \param size font size
 * \param handle RISC OS handle in case there isn't an error.
 * \param using_fb returning whether the fallback font was used or not.
 * \return error in case there was one.
 */
static os_error *nsfont_open_standard(const char *fontNameP, const char *fbFontNameP, int size, int *handleP, bool *using_fb)
{
	os_error *errorP;
	*handleP = 0; *using_fb = false;
	if ((errorP = xfont_find_font(fontNameP, size, size, 0, 0, (font_f *)handleP, NULL, NULL)) == NULL)
		return NULL;
	LOG(("font_find_font(<%s>) failed <%s> (case 1)", fontNameP, errorP->errmess));
	/* If the fallback font is the same as the first font name, return */
	if (strcmp(fontNameP, fbFontNameP) == 0)
		return errorP;
	*using_fb = true;
	if ((errorP = xfont_find_font(fbFontNameP, size, size, 0, 0, (font_f *)handleP, NULL, NULL)) == NULL)
		return NULL;
	LOG(("font_find_font(<%s>) failed <%s> (case 2)", fbFontNameP, errorP->errmess));
	return errorP;
}


/**
 * Frees all the fonts in a font_set.
 *
 * \param set a font_set as returned by nsfont_new_set()
 */
void nsfont_free_set(struct font_set *set)
{
	unsigned int i;

	LOG(("nsfont_free_set()\n"));
	assert(set != NULL);

	for (i = 0; i < FONT_FAMILIES * FONT_FACES; i++) {
		struct font_data *data, *next;
		for (data = set->font[i]; data != NULL; data = next) {
			os_error *error;
			next = data->next;
			switch (data->ftype) {
				case FONTTYPE_UFONT:
					error = xufont_lose_font((ufont_f)data->handle);
					break;
				case FONTTYPE_STANDARD_UTF8ENC:
				case FONTTYPE_STANDARD_LATIN1:
					error = xfont_lose_font((font_f)data->handle);
					break;
				default:
					assert(0);
					break;
			}
			if (error != NULL)
				LOG(("(u)font_lose_font() failed : 0x%x <%s>\n", error->errnum, error->errmess));
			free(data);
		}
	}

	free(set);
}


/**
 * Find the width of some text in a font.
 *
 * \param font a font_data, as returned by nsfont_open()
 * \param text string to measure
 * \param length length of text
 * \return width of text in pixels
 */
unsigned long nsfont_width(struct font_data *font, const char *text,
		size_t length)
{
	int width;
	os_error *error;

	assert(font != NULL && text != NULL);

	if (length == 0)
		return 0;

	switch (font->ftype) {
		case FONTTYPE_UFONT:
			error = xufont_scan_string((ufont_f)font->handle,
					text,
					font_GIVEN_FONT
						| font_KERN
						| font_GIVEN_LENGTH,
					0x7fffffff, 0x7fffffff,
					NULL,
					NULL, (int)length,
					NULL, &width, NULL, NULL);
			break;
		case FONTTYPE_STANDARD_UTF8ENC:
			error = xfont_scan_string((font_f)font->handle,
					text,
					font_GIVEN_FONT
						| font_KERN
						| font_GIVEN_LENGTH,
					0x7fffffff, 0x7fffffff,
					NULL,
					NULL, (int)length,
					NULL, &width, NULL, NULL);
			break;
		case FONTTYPE_STANDARD_LATIN1: {
			const char *loc_text = cnv_strn_local_enc(text, length, NULL);
			error = xfont_scan_string((font_f)font->handle,
					loc_text,
					font_GIVEN_FONT
						| font_KERN,
					0x7fffffff, 0x7fffffff,
					NULL,
					NULL, 0,
					NULL, &width, NULL, NULL);
			free((void *)loc_text);
			break;
		}
		default:
			assert(0);
			break;
	}
	if (error != NULL) {
		LOG(("(u)font_scan_string failed : %s\n", error->errmess));
		die("nsfont_width: (u)font_scan_string failed");
	}

	return width / 800;
}


/**
 * Find where in a string a x coordinate falls.
 *
 * For example, used to find where to position the caret in response to mouse
 * click.
 *
 * \param font a font_data, as returned by nsfont_open()
 * \param text a string
 * \param length length of text
 * \param x horizontal position in pixels
 * \param char_offset updated to give the offset in the string
 * \param pixel_offset updated to give the coordinate of the character in pixels
 */
void nsfont_position_in_string(struct font_data *font, const char *text,
		size_t length, unsigned long x,
		int *char_offset, int *pixel_offset)
{
	os_error *error;
	font_scan_block block;
	const char *split;
	int x_out;

	assert(font != NULL && text != NULL);

	block.space.x = block.space.y = block.letter.x = block.letter.y = 0;
	block.split_char = -1;

	switch (font->ftype) {
		case FONTTYPE_UFONT:
			error = xufont_scan_string((ufont_f)font->handle,
					text,
					font_GIVEN_BLOCK
						| font_GIVEN_FONT
						| font_KERN
						| font_RETURN_CARET_POS
						| font_GIVEN_LENGTH,
					x * 2 * 400, 0x7fffffff,
					&block, NULL, (int)length,
					&split, &x_out, NULL, NULL);
			break;
		case FONTTYPE_STANDARD_UTF8ENC:
			error = xfont_scan_string((font_f)font->handle,
					text,
					font_GIVEN_BLOCK
						| font_GIVEN_FONT
						| font_KERN
						| font_RETURN_CARET_POS
						| font_GIVEN_LENGTH,
					x * 2 * 400, 0x7fffffff,
					&block, NULL, (int)length,
					&split, &x_out, NULL, NULL);
			break;
		case FONTTYPE_STANDARD_LATIN1: {
			const ptrdiff_t *back_mapP;
			const char *loc_text = cnv_strn_local_enc(text, length, &back_mapP);
			error = xfont_scan_string((font_f)font->handle,
					loc_text,
					font_GIVEN_BLOCK
						| font_GIVEN_FONT
						| font_KERN
						| font_RETURN_CARET_POS,
					x * 2 * 400, 0x7fffffff,
					&block, NULL, 0,
					&split, &x_out, NULL, NULL);
			split = &text[back_mapP[split - loc_text]];
			free((void *)loc_text); free((void *)back_mapP);
			break;
		}
		default:
			assert(0);
			break;
	}
	if (error != NULL) {
		LOG(("(u)font_scan_string failed : %s\n", error->errmess));
		die("nsfont_position_in_string: (u)font_scan_string failed");
	}

	*char_offset = (int)(split - text);
	*pixel_offset = x_out / 800;
}


/**
 * Find where to split a string to fit in a width.
 *
 * For example, used when wrapping paragraphs.
 *
 * \param font a font_data, as returned by nsfont_open()
 * \param text string to split
 * \param length length of text
 * \param width available width
 * \param used_width updated to actual width used
 * \return pointer to character which does not fit
 */
char *nsfont_split(struct font_data *font, const char *text,
		size_t length, unsigned int width, unsigned int *used_width)
{
	os_error *error;
	font_scan_block block;
	const char *split;

	assert(font != NULL && text != NULL);

	block.space.x = block.space.y = block.letter.x = block.letter.y = 0;
	block.split_char = ' ';

	switch (font->ftype) {
		case FONTTYPE_UFONT:
			error = xufont_scan_string((ufont_f)font->handle,
					text,
					font_GIVEN_BLOCK
						| font_GIVEN_FONT
						| font_KERN
						| font_GIVEN_LENGTH,
					width * 2 * 400, 0x7fffffff,
					&block,
					NULL,
					(int)length,
					&split,
					used_width, NULL, NULL);
			break;
		case FONTTYPE_STANDARD_UTF8ENC:
			error = xfont_scan_string((font_f)font->handle,
					text,
					font_GIVEN_BLOCK
						| font_GIVEN_FONT
						| font_KERN
						| font_GIVEN_LENGTH,
					width * 2 * 400, 0x7fffffff,
					&block,
					NULL,
					(int)length,
					&split,
					used_width, NULL, NULL);
			break;
		case FONTTYPE_STANDARD_LATIN1: {
			const ptrdiff_t *back_mapP;
			const char *loc_text = cnv_strn_local_enc(text, length, &back_mapP);
			error = xfont_scan_string((font_f)font->handle,
					loc_text,
					font_GIVEN_BLOCK
						| font_GIVEN_FONT
						| font_KERN,
					width * 2 * 400, 0x7fffffff,
					&block,
					NULL,
					0,
					&split,
					used_width, NULL, NULL);
			split = &text[back_mapP[split - loc_text]];
			free((void *)loc_text); free((void *)back_mapP);
			break;
		}
		default:
			assert(0);
			break;
	}
	if (error != NULL) {
		LOG(("(u)font_scan_string failed : %s\n", error->errmess));
		die("nsfont_split: (u)font_scan_string failed");
	}

	assert(split == &text[length] || *split == ' ');

	*used_width = *used_width / 2 / 400;

	return split;
}


void nsfont_paint(struct font_data *data, const char *text,
		size_t length, int xpos, int ypos, void *trfm)
{
	os_error *error;
	unsigned int flags;
	const int var_input[3] = {136, 137, -1}; /* XOrig, YOrig, Terminator */
	int var_output[3];
	bool background_blending = option_background_blending;

	flags = font_OS_UNITS | font_GIVEN_FONT | font_KERN;
	if (trfm != NULL)
		flags |= font_GIVEN_TRFM;

	/* font background blending (RO3.7+) */
	if (ro_gui_current_redraw_gui)
		background_blending = ro_gui_current_redraw_gui->option.background_blending;
	if (background_blending) {
		int version;

		/* Font manager versions below 3.35 complain
		 * about this flag being set.
		 */
		error = xfont_cache_addr(&version, 0, 0);
		/**\todo should we do anything else on error? */
		if (!error && version >= 335)
			flags |= font_BLEND_FONT;
	}

	assert(data != NULL);
	assert(text != NULL);

	/* adjust by the origin */
	xos_read_vdu_variables((const os_vdu_var_list *)&var_input, (int *)&var_output);
	xpos += var_output[0];
	ypos += var_output[1];


	switch (data->ftype) {
		case FONTTYPE_UFONT:
			flags |= font_GIVEN_LENGTH;
			error = xufont_paint((ufont_f)data->handle, text,
					flags, xpos, ypos, NULL,
					trfm, length);
			break;
		case FONTTYPE_STANDARD_UTF8ENC:
			flags |= font_GIVEN_LENGTH;
			error = xfont_paint((font_f)data->handle, text,
					flags, xpos, ypos, NULL,
					trfm, length);
			break;
		case FONTTYPE_STANDARD_LATIN1: {
			const char *loc_text = cnv_strn_local_enc(text, length, NULL);
			error = xfont_paint((font_f)data->handle, loc_text,
					flags, xpos, ypos, NULL,
					trfm, 0);
			free((void *)loc_text);
			break;
		}
		default:
			assert(0);
			break;
	}
	if (error != NULL) {
		LOG(("(u)font_paint failed : %s\n", error->errmess));
		die("nsfont_paint: (u)font_paint failed");
	}
}


/**
 * Given a text line, return the number of bytes which can be set using
 * one RISC OS font and the bounding box fitting that part of the text
 * only.
 *
 * \param font a font_data, as returned by nsfont_open()
 * \param text string text.  Does not have to be NUL terminated.
 * \param length length in bytes of the text to consider.
 * \param width returned width of the text which can be set with one RISC OS font. If 0, then error happened or initial text length was 0.
 * \param rofontname returned name of the RISC OS font which can be used to set the text. If NULL, then error happened or initial text length was 0.
 * \param rotext returned string containing the characters in returned RISC OS font. Not necessary NUL terminated. free() after use.  If NULL, then error happened or initial text length was 0.
 * \param rolength length of return rotext string. If 0, then error happened or initial text length was 0.
 * \param consumed number of bytes of the given text which can be set with one RISC OS font. If 0, then error happened or initial text length was 0.
 */
void nsfont_txtenum(struct font_data *font, const char *text,
		size_t length,
		unsigned int *width,
		const char **rofontname,
		const char **rotext,
		size_t *rolength,
		size_t *consumed)
{
	assert(font != NULL && text != NULL && rofontname != NULL && rotext != NULL && rolength != NULL && consumed != NULL);

	*rotext = *rofontname = NULL;
	*consumed = *rolength = *width = 0;

	if (length == 0)
		return;

	switch (font->ftype) {
		case FONTTYPE_UFONT:
			(void)xufont_txtenum((ufont_f)font->handle,
					text,
					font_GIVEN_FONT
						| font_KERN
						| font_GIVEN_LENGTH,
					length,
					(int *)width,
					rofontname,
					rotext,
					rolength,
					consumed);
			*width /= 800;
			break;
		case FONTTYPE_STANDARD_UTF8ENC: {
			static char *fontname[128]; /** /todo: not nice */
			int rowidth;
			os_error *error;

			error = xfont_scan_string((font_f)font->handle,
					text,
					font_GIVEN_FONT
						| font_KERN
						| font_GIVEN_LENGTH,
					0x7fffffff, 0x7fffffff,
					NULL,
					NULL, (int)length,
					NULL, &rowidth, NULL, NULL);
			if (error != NULL)
				return;

			strcpy(fontname, ufont_table[font->id]);
			strcat(fontname, "\\EUTF8");
			if ((*rotext = strndup(text, length)) == NULL)
				return;
			*rolength = length;
			*rofontname = fontname;
			*consumed = length;
			*width = (unsigned int)rowidth / 800;
			break;
		}
		case FONTTYPE_STANDARD_LATIN1: {
			int rowidth;
			os_error *error;

			if ((*rotext = cnv_strn_local_enc(text, length, NULL)) == NULL)
				return;

			error = xfont_scan_string((font_f)font->handle,
					*rotext,
					font_GIVEN_FONT
						| font_KERN,
					0x7fffffff, 0x7fffffff,
					NULL,
					NULL, 0,
					NULL, &rowidth, NULL, NULL);
			if (error != NULL) {
				free((void *)*rotext); *rotext = NULL;
				return;
			}
			*rolength = strlen(*rotext);
			*rofontname = font_table[font->id];
			*consumed = length;
			*width = (unsigned int)rowidth / 800;
			break;
		}
		default:
			assert(0);
			break;
	}
}

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
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

#define FONT_MAX_NAME 128 /* max length of a font name */

#define FONT_FAMILIES 6 /* Number of families */
#define FONT_FACES 4	/* Number of faces per family */

/* Font Variants */
#define FONT_SMALLCAPS 4

/* Font Styles */
#define FONT_BOLD 2
#define FONT_SLANTED 1

/* Font families */
#define FONT_DEFAULT	(0 * FONT_FACES)
#define FONT_SANS_SERIF (1 * FONT_FACES)
#define FONT_SERIF	(2 * FONT_FACES)
#define FONT_MONOSPACE  (3 * FONT_FACES)
#define FONT_CURSIVE	(4 * FONT_FACES)
#define FONT_FANTASY	(5 * FONT_FACES)

/* a font_set is just a linked list of font_data for each face for now */
struct font_set {
	struct font_data *font[FONT_FAMILIES * FONT_FACES];
};

static os_error *nsfont_open_ufont(const char *fontNameP, const char *fbFontNameP, int size, int *handleP, bool *using_fb, bool log_errors);
static os_error *nsfont_open_standard(const char *fontNameP, const char *fbFontNameP, int size, int *handleP, bool *using_fb, bool log_errors);
static char *nsfont_create_font_name(char *base, int id);

/** Table of font names.
 *
 * font id = font family * 4 + bold * 2 + slanted
 *
 * font family: 1 = sans-serif, 2 = serif, 3 = monospace, 4 = cursive,
 * 5 = fantasy.
 * Font family 0 must be available as it is the replacement font when
 * the other font families can not be found.
 */
static char font_table[FONT_FAMILIES * FONT_FACES][FONT_MAX_NAME] = {
	/* default */			/* ---bs */
/*0*/	"Homerton.Medium",		/* 00000 */
/*1*/	"Homerton.Medium.Oblique",	/* 00001 */
/*2*/	"Homerton.Bold",		/* 00010 */
/*3*/	"Homerton.Bold.Oblique",	/* 00011 */
	/* sans-serif */
/*4*/	"Homerton.Medium",		/* 00100 */
/*5*/	"Homerton.Medium.Oblique",	/* 00101 */
/*6*/	"Homerton.Bold",		/* 00110 */
/*7*/	"Homerton.Bold.Oblique",	/* 00111 */
	/* serif */
/*8*/	"Trinity.Medium",		/* 01000 */
/*9*/	"Trinity.Medium.Italic",	/* 01001 */
/*10*/	"Trinity.Bold",			/* 01010 */
/*11*/	"Trinity.Bold.Italic",		/* 01011 */
	/* monospace */
/*12*/	"Corpus.Medium",		/* 01100 */
/*13*/	"Corpus.Medium.Oblique",	/* 01101 */
/*14*/	"Corpus.Bold",			/* 01110 */
/*15*/	"Corpus.Bold.Oblique",		/* 01111 */
	/* cursive */
/*16*/	"Churchill.Medium",		/* 10000 */
/*17*/	"Churchill.Medium.Italic",	/* 10001 */
/*18*/	"Churchill.Bold",		/* 10010 */
/*19*/	"Churchill.Bold.Italic",	/* 10011 */
	/* fantasy */
/*20*/	"Sassoon.Primary",		/* 10100 */
/*21*/	"Sassoon.Primary.Oblique",	/* 10101 */
/*22*/	"Sassoon.Primary.Bold",		/* 10110 */
/*23*/	"Sassoon.Primary.Bold.Oblique",	/* 10111 */
};

/**
 * Create an empty font_set.
 *
 * \return an opaque struct font_set, or NULL on memory exhaustion
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
		size = (int)(css_len2px(&style->font_size.value.length,
				style) * 72.0 / 90.0 * 16.);
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
	/** \todo (re)implement smallcaps */
/*	switch (style->font_variant) {
		case CSS_FONT_VARIANT_SMALL_CAPS:
			f += FONT_SMALLCAPS;
			break;
		default:
			break;
	}
*/

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
	if (!option_font_ufont || (error = nsfont_open_ufont(font_table[f], font_table[f % 4], (int)size, &fhandle, &using_fb, true)) != NULL) {
		char fontName1[FONT_MAX_NAME+10];
		char fontName2[FONT_MAX_NAME+10];
		/* Go for the UTF-8 encoding with standard FontManager */
		strcpy(fontName1, font_table[f]);
		strcat(fontName1, "\\EUTF8");
		strcpy(fontName2, font_table[f % 4]);
		strcat(fontName2, "\\EUTF8");

		if ((error = nsfont_open_standard(fontName1, fontName2, (int)size, &fhandle, &using_fb, true)) != NULL) {
			/* All UTF-8 font methods failed, only support Latin 1 */
			strcpy(fontName1, font_table[f]);
			strcat(fontName1, "\\ELatin1");
			strcpy(fontName2, font_table[f % 4]);
			strcat(fontName2, "\\ELatin1");

			if ((error = nsfont_open_standard(fontName1, fontName2, (int)size, &fhandle, &using_fb, true)) != NULL) {
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
static os_error *nsfont_open_ufont(const char *fontNameP, const char *fbFontNameP, int size, int *handleP, bool *using_fb, bool log_errors)
{
	os_error *errorP;
	*handleP = 0; *using_fb = false;
	if ((errorP = xufont_find_font(fontNameP, size, size, 0, 0, (ufont_f *)handleP, NULL, NULL)) == NULL)
		return NULL;
	if (log_errors)
		LOG(("ufont_find_font(<%s>) failed <%s> (case 1)", fontNameP, errorP->errmess));
	/* If the fallback font is the same as the first font name, return */
	if (strcmp(fontNameP, fbFontNameP) == 0)
		return errorP;
	*using_fb = true;
	if ((errorP = xufont_find_font(fbFontNameP, size, size, 0, 0, (ufont_f *)handleP, NULL, NULL)) == NULL)
		return NULL;
	if (log_errors)
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
static os_error *nsfont_open_standard(const char *fontNameP, const char *fbFontNameP, int size, int *handleP, bool *using_fb, bool log_errors)
{
	os_error *errorP;
	*handleP = 0; *using_fb = false;
	if ((errorP = xfont_find_font(fontNameP, size, size, 0, 0, (font_f *)handleP, NULL, NULL)) == NULL)
		return NULL;
	if (log_errors)
		LOG(("font_find_font(<%s>) failed <%s> (case 1)", fontNameP, errorP->errmess));
	/* If the fallback font is the same as the first font name, return */
	if (strcmp(fontNameP, fbFontNameP) == 0)
		return errorP;
	*using_fb = true;
	if ((errorP = xfont_find_font(fbFontNameP, size, size, 0, 0, (font_f *)handleP, NULL, NULL)) == NULL)
		return NULL;
	if (log_errors)
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
			if (!loc_text)
				return 0;

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
		LOG(("(u)font_scan_string failed : %s", error->errmess));
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
 * \return true on success, false on failure.
 */
bool nsfont_position_in_string(struct font_data *font, const char *text,
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
					(unsigned const char **)&split,
					&x_out, NULL, NULL);
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
					(char **)&split,
					&x_out, NULL, NULL);
			break;
		case FONTTYPE_STANDARD_LATIN1: {
			const ptrdiff_t *back_mapP;
			const char *loc_text = cnv_strn_local_enc(text, length, &back_mapP);
			if (!loc_text)
				return false;

			error = xfont_scan_string((font_f)font->handle,
					loc_text,
					font_GIVEN_BLOCK
						| font_GIVEN_FONT
						| font_KERN
						| font_RETURN_CARET_POS,
					x * 2 * 400, 0x7fffffff,
					&block, NULL, 0,
					(char **)&split,
					&x_out, NULL, NULL);
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
/*		die("nsfont_position_in_string: (u)font_scan_string failed");*/
		return false;
	}

	*char_offset = (int)(split - text);
	*pixel_offset = x_out / 800;

	return true;
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
					(unsigned const char **)&split,
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
					(char **)&split,
					used_width, NULL, NULL);
			break;
		case FONTTYPE_STANDARD_LATIN1: {
			const ptrdiff_t *back_mapP;
			const char *loc_text = cnv_strn_local_enc(text, length, &back_mapP);
			if (!loc_text)
				return NULL;

			error = xfont_scan_string((font_f)font->handle,
					loc_text,
					font_GIVEN_BLOCK
						| font_GIVEN_FONT
						| font_KERN,
					width * 2 * 400, 0x7fffffff,
					&block,
					NULL,
					0,
					(char **)&split,
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

	assert(split == &text[length] || *split == ' ' || *split == '\t');

	*used_width = *used_width / 2 / 400;

	return (char*)split;
}


bool nsfont_paint(struct font_data *data, const char *text,
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

	/* adjust by the origin
	 * (not if printing as the result is undefined)
	 */
	if (!print_active) {
		xos_read_vdu_variables((const os_vdu_var_list *)&var_input,
					(int *)&var_output);
		xpos += var_output[0];
		ypos += var_output[1];
	}


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
			if (!loc_text)
				return false;

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
		/*die("nsfont_paint: (u)font_paint failed");*/
		return false;
	}

	return true;
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
	static char *fontname[FONT_MAX_NAME]; /** \todo: not nice */

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
					(unsigned const char **)rofontname,
					(unsigned const char **)rotext,
					rolength,
					consumed);
			*width /= 800;
			break;
		case FONTTYPE_STANDARD_UTF8ENC: {
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

			strcpy(fontname, font_table[font->id]);
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
			strcpy(fontname, font_table[font->id]);
			strcat(fontname, "\\ELatin1");
			*rofontname = fontname;
			*consumed = length;
			*width = (unsigned int)rowidth / 800;
			break;
		}
		default:
			assert(0);
			break;
	}
}

/**
 * Fill in the font_table, based on the user's options
 *
 * \param force_rescan Indicate whether to rescan font names
 *		       and update options
 */
void nsfont_fill_nametable(bool force_rescan)
{
	int i;
	char *name = NULL, *created = NULL;

	for (i = 0; i != FONT_FAMILIES * FONT_FACES; i++) {
		/* read the relevant option string */
		switch (i) {
			/* default */
			case FONT_DEFAULT:
				name = option_font_default;
				break;
			case FONT_DEFAULT + FONT_SLANTED:
				name = option_font_default_italic;
				break;
			case FONT_DEFAULT + FONT_BOLD:
				name = option_font_default_bold;
				break;
			case FONT_DEFAULT + FONT_BOLD + FONT_SLANTED:
				name = option_font_default_bold_italic;
				break;
			/* sans */
			case FONT_SANS_SERIF:
				name = option_font_sans;
				break;
			case FONT_SANS_SERIF + FONT_SLANTED:
				name = option_font_sans_italic;
				break;
			case FONT_SANS_SERIF + FONT_BOLD:
				name = option_font_sans_bold;
				break;
			case FONT_SANS_SERIF + FONT_BOLD + FONT_SLANTED:
				name = option_font_sans_bold_italic;
				break;
			/* serif */
			case FONT_SERIF:
				name = option_font_serif;
				break;
			case FONT_SERIF + FONT_SLANTED:
				name = option_font_serif_italic;
				break;
			case FONT_SERIF + FONT_BOLD:
				name = option_font_serif_bold;
				break;
			case FONT_SERIF + FONT_BOLD + FONT_SLANTED:
				name = option_font_serif_bold_italic;
				break;
			/* mono */
			case FONT_MONOSPACE:
				name = option_font_mono;
				break;
			case FONT_MONOSPACE + FONT_SLANTED:
				name = option_font_mono_italic;
				break;
			case FONT_MONOSPACE + FONT_BOLD:
				name = option_font_mono_bold;
				break;
			case FONT_MONOSPACE + FONT_BOLD + FONT_SLANTED:
				name = option_font_mono_bold_italic;
				break;
			/* cursive */
			case FONT_CURSIVE:
				name = option_font_cursive;
				break;
			case FONT_CURSIVE + FONT_SLANTED:
				name = option_font_cursive_italic;
				break;
			case FONT_CURSIVE + FONT_BOLD:
				name = option_font_cursive_bold;
				break;
			case FONT_CURSIVE + FONT_BOLD + FONT_SLANTED:
				name = option_font_cursive_bold_italic;
				break;
			/* fantasy */
			case FONT_FANTASY:
				name = option_font_fantasy;
				break;
			case FONT_FANTASY + FONT_SLANTED:
				name = option_font_fantasy_italic;
				break;
			case FONT_FANTASY + FONT_BOLD:
				name = option_font_fantasy_bold;
				break;
			case FONT_FANTASY + FONT_BOLD + FONT_SLANTED:
				name = option_font_fantasy_bold_italic;
				break;
		}

		if ((!force_rescan || (force_rescan && i == ((i / FONT_FACES) * FONT_FACES))) && name && name[0] != '\0') {
			/* got a configured font name => use it */
			strncpy(font_table[i], name, FONT_MAX_NAME);
		}
		else {
			char *dot, *next_segment;

			/* no configured name => try to create one */

			/* get the base font for the family */
			name = strdup(font_table[(i/FONT_FACES)*FONT_FACES]);
			next_segment = name;

			do {
				dot = strchr(next_segment, '.');

				/* restore '.' */
				if (next_segment != name)
					*(next_segment-1) = '.';

				if (dot) {
					*dot = '\0';
					next_segment = dot+1;
				}

				created = nsfont_create_font_name(name, i);

			} while(!created && dot);

			/* now fill in the table entry */
			if (created) {
				strncpy(font_table[i], created,
					FONT_MAX_NAME);
				free(created);
			}

			free(name);

			/* don't modify options if not rescanning */
			if (!force_rescan)
				continue;

			/* write the relevant option string */
			switch (i) {
				/* default */
				case FONT_DEFAULT:
					if (option_font_default)
						free(option_font_default);
					option_font_default = strdup(font_table[i]);
					break;
				case FONT_DEFAULT + FONT_SLANTED:
					if (option_font_default_italic)
						free(option_font_default_italic);
					option_font_default_italic = strdup(font_table[i]);
					break;
				case FONT_DEFAULT + FONT_BOLD:
					if (option_font_default_bold)
						free(option_font_default_bold);
					option_font_default_bold = strdup(font_table[i]);
					break;
				case FONT_DEFAULT + FONT_BOLD + FONT_SLANTED:
					if (option_font_default_bold_italic)
						free(option_font_default_bold_italic);
					option_font_default_bold_italic = strdup(font_table[i]);
					break;
				/* sans */
				case FONT_SANS_SERIF:
					if (option_font_sans)
						free(option_font_sans);
					option_font_sans = strdup(font_table[i]);
					break;
				case FONT_SANS_SERIF + FONT_SLANTED:
					if (option_font_sans_italic)
						free(option_font_sans_italic);
					option_font_sans_italic = strdup(font_table[i]);
					break;
				case FONT_SANS_SERIF + FONT_BOLD:
					if (option_font_sans_bold)
						free(option_font_sans_bold);
					option_font_sans_bold = strdup(font_table[i]);
					break;
				case FONT_SANS_SERIF + FONT_BOLD + FONT_SLANTED:
					if (option_font_sans_bold_italic)
						free(option_font_sans_bold_italic);
					option_font_sans_bold_italic = strdup(font_table[i]);
					break;
				/* serif */
				case FONT_SERIF:
					if (option_font_serif)
						free(option_font_serif);
					option_font_serif = strdup(font_table[i]);
					break;
				case FONT_SERIF + FONT_SLANTED:
					if (option_font_serif_italic)
						free(option_font_serif_italic);
					option_font_serif_italic = strdup(font_table[i]);
					break;
				case FONT_SERIF + FONT_BOLD:
					if (option_font_serif_bold)
						free(option_font_serif_bold);
					option_font_serif_bold = strdup(font_table[i]);
					break;
				case FONT_SERIF + FONT_BOLD + FONT_SLANTED:
					if (option_font_serif_bold_italic)
						free(option_font_serif_bold_italic);
					option_font_serif_bold_italic = strdup(font_table[i]);
					break;
				/* mono */
				case FONT_MONOSPACE:
					if (option_font_mono)
						free(option_font_mono);
					option_font_mono = strdup(font_table[i]);
					break;
				case FONT_MONOSPACE + FONT_SLANTED:
					if (option_font_mono_italic)
						free(option_font_mono_italic);
					option_font_mono_italic = strdup(font_table[i]);
					break;
				case FONT_MONOSPACE + FONT_BOLD:
					if (option_font_mono_bold)
						free(option_font_mono_bold);
					option_font_mono_bold = strdup(font_table[i]);
					break;
				case FONT_MONOSPACE + FONT_BOLD + FONT_SLANTED:
					if (option_font_mono_bold_italic)
						free(option_font_mono_bold_italic);
					option_font_mono_bold_italic = strdup(font_table[i]);
					break;
				/* cursive */
				case FONT_CURSIVE:
					if (option_font_cursive)
						free(option_font_cursive);
					option_font_cursive = strdup(font_table[i]);;
					break;
				case FONT_CURSIVE + FONT_SLANTED:
					if (option_font_cursive_italic)
						free(option_font_cursive_italic);
					option_font_cursive_italic = strdup(font_table[i]);
					break;
				case FONT_CURSIVE + FONT_BOLD:
					if (option_font_cursive_bold)
						free(option_font_cursive_bold);
					option_font_cursive_bold = strdup(font_table[i]);
					break;
				case FONT_CURSIVE + FONT_BOLD + FONT_SLANTED:
					if (option_font_cursive_bold_italic)
						free(option_font_cursive_bold_italic);
					option_font_cursive_bold_italic = strdup(font_table[i]);
					break;
				/* fantasy */
				case FONT_FANTASY:
					if (option_font_fantasy)
						free(option_font_fantasy);
					option_font_fantasy = strdup(font_table[i]);
					break;
				case FONT_FANTASY + FONT_SLANTED:
					if (option_font_fantasy_italic)
						free(option_font_fantasy_italic);
					option_font_fantasy_italic = strdup(font_table[i]);
					break;
				case FONT_FANTASY + FONT_BOLD:
					if (option_font_fantasy_bold)
						free(option_font_fantasy_bold);
					option_font_fantasy_bold = strdup(font_table[i]);
					break;
				case FONT_FANTASY + FONT_BOLD + FONT_SLANTED:
					if (option_font_fantasy_bold_italic)
						free(option_font_fantasy_bold_italic);
					option_font_fantasy_bold_italic = strdup(font_table[i]);
					break;
			}
		}
	}
}

/* lookup table used by nsfont_create_font_name.
 * Italic entries *must* precede bold entries
 */
static const char *style_lookup[] = {
#define ITALIC_COUNT 3
	"Italic", "Oblique", "Slant",
#define BOLD_COUNT 5
	"Bold", "Demi", "ExtraBold", "Ultra", "Heavy"
};

/**
 * Create a valid font name, testing for presence on the system
 *
 * \param base The base name
 * \param id   The id of the font (entry into font_table)
 * \return The font name, or NULL on failure
 */
char *nsfont_create_font_name(char *base, int id)
{
	char *created, tempname[FONT_MAX_NAME+10];
	int bold, italic;
	os_error *error;
	int fhandle;
	bool using_fb, found = false;

	created = malloc(FONT_MAX_NAME);
	if (!created)
		return NULL;

	/* Font presence testing strategy is as-per nsfont_open */

	/* try bold-italic first */
	if ((id & FONT_BOLD) && (id & FONT_SLANTED)) {
		for (bold = 0; bold != BOLD_COUNT; bold++) {
			for (italic = 0; italic != ITALIC_COUNT; italic++) {
				snprintf(created, FONT_MAX_NAME,
					"%s.%s.%s", base,
					style_lookup[bold+ITALIC_COUNT],
					style_lookup[italic]);

				/* try ufont first */
				if (option_font_ufont && (error = nsfont_open_ufont(created, created, 160, &fhandle, &using_fb, false)) == NULL) {
					xufont_lose_font((ufont_f)fhandle);
					found = true;
					break;
				}

				/* then UTF8 encoding */
				strcpy(tempname, created);
				strcat(tempname, "\\EUTF8");
				if ((error = nsfont_open_standard(tempname, tempname, 160, &fhandle, &using_fb, false)) == NULL) {
					xfont_lose_font((font_f)fhandle);
					found = true;
					break;
				}

				/* then Latin1 */
				strcpy(tempname, created);
				strcat(tempname, "\\ELatin1");
				if ((error = nsfont_open_standard(tempname, tempname, 160, &fhandle, &using_fb, false)) == NULL) {
					xfont_lose_font((font_f)fhandle);
					found = true;
					break;
				}
			}
			if (found)
				break;
		}
	}
	/* bold */
	else if (id & FONT_BOLD) {
		for (bold = 0; bold != BOLD_COUNT; bold++) {
			snprintf(created, FONT_MAX_NAME, "%s.%s", base,
				style_lookup[bold+ITALIC_COUNT]);

			/* try ufont first */
			if (option_font_ufont && (error = nsfont_open_ufont(created, created, 160, &fhandle, &using_fb, false)) == NULL) {
				xufont_lose_font((ufont_f)fhandle);
				found = true;
				break;
			}

			/* then UTF8 encoding */
			strcpy(tempname, created);
			strcat(tempname, "\\EUTF8");
			if ((error = nsfont_open_standard(tempname, tempname, 160, &fhandle, &using_fb, false)) == NULL) {
				xfont_lose_font((font_f)fhandle);
				found = true;
				break;
			}

			/* then Latin1 */
			strcpy(tempname, created);
			strcat(tempname, "\\ELatin1");
			if ((error = nsfont_open_standard(tempname, tempname, 160, &fhandle, &using_fb, false)) == NULL) {
				xfont_lose_font((font_f)fhandle);
				found = true;
				break;
			}
		}
	}
	/* italic */
	else if (id & FONT_SLANTED) {
		for (italic = 0; italic != ITALIC_COUNT; italic++) {
			snprintf(created, FONT_MAX_NAME, "%s.%s", base,
				style_lookup[italic]);

			/* try ufont first */
			if (option_font_ufont && (error = nsfont_open_ufont(created, created, 160, &fhandle, &using_fb, false)) == NULL) {
				xufont_lose_font((ufont_f)fhandle);
				found = true;
				break;
			}

			/* then UTF8 encoding */
			strcpy(tempname, created);
			strcat(tempname, "\\EUTF8");
			if ((error = nsfont_open_standard(tempname, tempname, 160, &fhandle, &using_fb, false)) == NULL) {
				xfont_lose_font((font_f)fhandle);
				found = true;
				break;
			}

			/* then Latin1 */
			strcpy(tempname, created);
			strcat(tempname, "\\ELatin1");
			if ((error = nsfont_open_standard(tempname, tempname, 160, &fhandle, &using_fb, false)) == NULL) {
				xfont_lose_font((font_f)fhandle);
				found = true;
				break;
			}
		}
	}

	if (found)
		return created;

	free(created);
	return NULL;
}


/**
 * Reopens all font handles to the current screen resolution
 */
void nsfont_reopen_set(struct font_set *fonts) {
	os_error *error;
	char fontName1[FONT_MAX_NAME+10];
	char fontName2[FONT_MAX_NAME+10];
	struct font_data *f;
	bool using_fb;

	for (int i = 0; i < (FONT_FAMILIES * FONT_FACES); i++) {
		for (f = fonts->font[i]; f; f = f->next) {
		 	switch (f->ftype) {
		 		case FONTTYPE_UFONT:
		 			error = xufont_lose_font((ufont_f)f->handle);
		 			if (error) {
						LOG(("xufont_lose_font: 0x%x: %s",
							error->errnum, error->errmess));
					}
					error = nsfont_open_ufont(font_table[f->id], font_table[f->id % 4],
							(int)f->size, &f->handle, &using_fb, true);
		 			if (error) {
						LOG(("nsfont_open_ufont: 0x%x: %s",
							error->errnum, error->errmess));
					}
		 			break;
				case FONTTYPE_STANDARD_LATIN1:
		 			error = xfont_lose_font((font_f)f->handle);
		 			if (error) {
						LOG(("xfont_lose_font: 0x%x: %s",
							error->errnum, error->errmess));
					}
					strcpy(fontName1, font_table[f->id]);
					strcat(fontName1, "\\ELatin1");
					strcpy(fontName2, font_table[f->id % 4]);
					strcat(fontName2, "\\ELatin1");
					error = nsfont_open_standard(fontName1, fontName2, (int)f->size,
							&f->handle, &using_fb, true);
		 			if (error) {
						LOG(("nsfont_open_standard: 0x%x: %s",
							error->errnum, error->errmess));
					}
					break;
				case FONTTYPE_STANDARD_UTF8ENC:
		 			error = xfont_lose_font((font_f)f->handle);
		 			if (error) {
						LOG(("xfont_lose_font: 0x%x: %s",
							error->errnum, error->errmess));
					}
					strcpy(fontName1, font_table[f->id]);
					strcat(fontName1, "\\EUTF8");
					strcpy(fontName2, font_table[f->id % 4]);
					strcat(fontName2, "\\EUTF8");
					error = nsfont_open_standard(fontName1, fontName2, (int)f->size,
							&f->handle, &using_fb, true);
		 			if (error) {
						LOG(("nsfont_open_standard: 0x%x: %s",
							error->errnum, error->errmess));
					}
					break;
			}
			f->space_width = nsfont_width(f, " ", sizeof(" ")-1);
		}
	}
}

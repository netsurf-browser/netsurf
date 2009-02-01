/*
 * Copyright 2008 Adam Blokus <adamblokus@gmail.com>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

 /** \file
 * Target independent PDF plotting using Haru Free PDF Library.
 */

#include "utils/config.h"
#ifdef WITH_PDF_EXPORT

#include <stdlib.h>
#include <string.h>
#include "hpdf.h"

#include "desktop/options.h"
#include "desktop/plotters.h"
#include "desktop/print.h"
#include "desktop/printer.h"
#include "desktop/save_pdf/pdf_plotters.h"
#include "image/bitmap.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "utils/useragent.h"

#include "font_haru.h"

#define R(x) ((  (x) & 0x0000ff     )/256.0)
#define G(x) ((( (x) & 0x00ff00)>>8 )/256.0)
#define B(x) ((( (x) & 0xff0000)>>16)/256.0)

/*#define PDF_DEBUG*/

static bool pdf_plot_clg(colour c);
static bool pdf_plot_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed);
static bool pdf_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed);
static bool pdf_plot_polygon(int *p, unsigned int n, colour fill);
static bool pdf_plot_fill(int x0, int y0, int x1, int y1, colour c);
static bool pdf_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1);
static bool pdf_plot_text(int x, int y, const struct css_style *style,
		const char *text, size_t length, colour bg, colour c);
static bool pdf_plot_disc(int x, int y, int radius, colour c, bool filled);
static bool pdf_plot_arc(int x, int y, int radius, int angle1, int angle2,
    		colour c);
static bool pdf_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg, struct content *content);
static bool pdf_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y, struct content *content);
static bool pdf_plot_path(float *p, unsigned int n, colour fill, float width,
		colour c, float *transform);

static void pdf_set_solid(void);
static void pdf_set_dashed(void);
static void pdf_set_dotted(void);

static HPDF_Image pdf_extract_image(struct bitmap *bitmap, struct content *content);
static void apply_clip_and_mode(void);


static void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no,
		void *user_data);

#ifdef PDF_DEBUG
static void pdf_plot_grid(int x_dist,int y_dist,unsigned int colour);
#endif

/*PDF Plotter - current doc,page and font*/
static HPDF_Doc pdf_doc;
static HPDF_Page pdf_page;
static HPDF_Font pdf_font;

/*PDF Page size*/
static HPDF_REAL page_height, page_width;

/*Remeber if pdf_plot_clip was invoked for current page*/
static bool page_clipped;
static int last_clip_x0, last_clip_y0, last_clip_x1, last_clip_y1;

static bool in_text_mode, text_mode_request;

static struct print_settings* settings;

static const struct plotter_table pdf_plotters = {
	pdf_plot_clg,
	pdf_plot_rectangle,
	pdf_plot_line,
	pdf_plot_polygon,
	pdf_plot_fill,
	pdf_plot_clip,
	pdf_plot_text,
	pdf_plot_disc,
	pdf_plot_arc,
	pdf_plot_bitmap,
	pdf_plot_bitmap_tile,
	NULL,
	NULL,
	NULL,
	pdf_plot_path,
	false
};

const struct printer pdf_printer = {
	&pdf_plotters,
	pdf_begin,
	pdf_next_page,
	pdf_end
};

float pdf_scale;
static char *owner_pass;
static char *user_pass;

bool pdf_plot_clg(colour c)
{
	return true;
}

bool pdf_plot_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed)
{
#ifdef PDF_DEBUG
	LOG(("."));
#endif
	apply_clip_and_mode();

	HPDF_Page_SetLineWidth(pdf_page, line_width);

	if (dotted)
		pdf_set_dotted();
	else if (dashed)
		pdf_set_dashed();

	HPDF_Page_SetRGBStroke(pdf_page, R(c), G(c), B(c));
	HPDF_Page_Rectangle(pdf_page, x0, page_height - y0 + height, width, height);
	HPDF_Page_Stroke(pdf_page);

	if (dotted||dashed)
		pdf_set_solid();

	return true;
}

bool pdf_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed)
{
#ifdef PDF_DEBUG
	LOG(("."));
#endif

	apply_clip_and_mode();

	HPDF_Page_SetLineWidth(pdf_page, width);

	if (dotted)
		pdf_set_dotted();
	else if (dashed)
		pdf_set_dashed();

	HPDF_Page_SetRGBStroke(pdf_page, R(c), G(c), B(c));
	HPDF_Page_SetLineWidth(pdf_page, width);
	HPDF_Page_MoveTo(pdf_page, x0, page_height - y0);
	HPDF_Page_LineTo(pdf_page, x1, page_height - y1);
	HPDF_Page_Stroke(pdf_page);

	if (dotted||dashed)
		pdf_set_solid();

	return true;
}

bool pdf_plot_polygon(int *p, unsigned int n, colour fill)
{
	unsigned int i;
#ifdef PDF_DEBUG
	int pmaxx = p[0], pmaxy = p[1];
	int pminx = p[0], pminy = p[1];
	LOG(("."));
#endif
	if (n == 0)
		return true;

	apply_clip_and_mode();

	HPDF_Page_SetRGBFill(pdf_page, R(fill), G(fill), B(fill));
	HPDF_Page_MoveTo(pdf_page, p[0], page_height - p[1]);

	for (i = 1 ; i<n ; i++) {
		HPDF_Page_LineTo(pdf_page, p[i*2], page_height - p[i*2+1]);
#ifdef PDF_DEBUG
		pmaxx = max(pmaxx, p[i*2]);
		pmaxy = max(pmaxy, p[i*2+1]);
		pminx = min(pminx, p[i*2]);
		pminy = min(pminy, p[i*2+1]);
#endif
	}

#ifdef PDF_DEBUG
	LOG(("%d %d %d %d %f", pminx, pminy, pmaxx, pmaxy, page_height-pminy));
#endif

	HPDF_Page_LineTo(pdf_page, p[0], page_height - p[1]);
	HPDF_Page_Fill(pdf_page);

	return true;
}

bool pdf_plot_fill(int x0, int y0, int x1, int y1, colour c)
{
#ifdef PDF_DEBUG
	LOG(("%d %d %d %d %f %X", x0, y0, x1, y1, page_height-y0, c));
#endif

	/*Normalize boundaries of the area - to prevent overflows.
	  It is needed only in a few functions, where integers are subtracted.
	  When the whole browser window is meant min and max int values are used
	  what must be handled in paged output.
	*/
	x0 = min(max(x0, 0), page_width);
	y0 = min(max(y0, 0), page_height);
	x1 = min(max(x1, 0), page_width);
	y1 = min(max(y1, 0), page_height);

	apply_clip_and_mode();

	HPDF_Page_SetRGBFill(pdf_page, R(c), G(c), B(c));
	HPDF_Page_Rectangle(pdf_page, x0, page_height - y1, x1-x0, y1-y0);
	HPDF_Page_Fill(pdf_page);

	return true;
}

/**here the clip is only queried */
bool pdf_plot_clip(int clip_x0, int clip_y0, int clip_x1, int clip_y1)
{
#ifdef PDF_DEBUG
	LOG(("%d %d %d %d", clip_x0, clip_y0, clip_x1, clip_y1));
#endif

	/*Normalize cllipping area - to prevent overflows.
	  See comment in pdf_plot_fill.
	*/
	last_clip_x0 = min(max(clip_x0, 0), page_width);
	last_clip_y0 = min(max(clip_y0, 0), page_height);
	last_clip_x1 = min(max(clip_x1, 0), page_width);
	last_clip_y1 = min(max(clip_y1, 0), page_height);

	page_clipped = true;

	return true;
}

bool pdf_plot_text(int x, int y, const struct css_style *style,
		const char *text, size_t length, colour bg, colour c)
{
#ifdef PDF_DEBUG
	LOG((". %d %d %s", x, y, text));
#endif
	char *word;
	HPDF_REAL size;
	float text_bottom_position, descent;

	if (length == 0)
		return true;

	text_mode_request = true;
	apply_clip_and_mode();

	if (style->font_size.value.length.unit  == CSS_UNIT_PX)
		size = style->font_size.value.length.value;
	else
		size = css_len2pt(&style->font_size.value.length, style);

	/*this can be removed when export options get added for riscos*/
#ifdef riscos
	size *= DEFAULT_EXPORT_SCALE;
#else
	size *= pdf_scale;
#endif

	if (size <= 0)
		return true;

	if (size > HPDF_MAX_FONTSIZE)
		size = HPDF_MAX_FONTSIZE;

	haru_nsfont_apply_style(style, pdf_doc, pdf_page, &pdf_font);

	descent = size * (HPDF_Font_GetDescent(pdf_font) / 1000.0);
	text_bottom_position = page_height - y + descent;

	word = (char*) malloc( sizeof(char) * (length+1) );
	if (word == NULL)
		return false;

	memcpy(word, text, length);
	word[length] = '\0';

	HPDF_Page_SetRGBFill(pdf_page, R(c), G(c), B(c));

	HPDF_Page_SetFontAndSize (pdf_page, pdf_font, size);
	HPDF_Page_TextOut (pdf_page, x, page_height - y, word);

	free(word);

	return true;
}

bool pdf_plot_disc(int x, int y, int radius, colour c, bool filled)
{
#ifdef PDF_DEBUG
	LOG(("."));
#endif
	apply_clip_and_mode();

	if (filled)
		HPDF_Page_SetRGBFill(pdf_page, R(c), G(c), B(c));
	else
		HPDF_Page_SetRGBStroke(pdf_page, R(c), G(c), B(c));

	HPDF_Page_Circle(pdf_page, x, page_height-y, radius);

	if (filled)
		HPDF_Page_Fill(pdf_page);
	else
		HPDF_Page_Stroke(pdf_page);

	return true;
}

bool pdf_plot_arc(int x, int y, int radius, int angle1, int angle2, colour c)
{
#ifdef PDF_DEBUG
	LOG(("%d %d %d %d %d %X", x, y, radius, angle1, angle2, c));
#endif

	/*Normalize angles*/
	angle1 %= 360;
	angle2 %= 360;
	if (angle1 > angle2)
		angle1 -= 360;

	apply_clip_and_mode();

	HPDF_Page_SetRGBStroke(pdf_page, R(c), G(c), B(c));

	HPDF_Page_Arc(pdf_page, x, page_height-y, radius, angle1, angle2);

	HPDF_Page_Stroke(pdf_page);
	return true;
}

bool pdf_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg, struct content *content)
{
	HPDF_Image image;

#ifdef PDF_DEBUG
	LOG(("%d %d %d %d %X %X %X", x, y, width, height,
	     bitmap, bg, content));
#endif
 	if (width == 0 || height == 0)
 		return true;

	apply_clip_and_mode();

	image = pdf_extract_image(bitmap, content);

	if (!image)
		return false;

	HPDF_Page_DrawImage(pdf_page, image,
			x, page_height-y-height,
			width, height);
	return true;


}

bool pdf_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
  		bool repeat_x, bool repeat_y, struct content *content)
{
	HPDF_Image image;

#ifdef PDF_DEBUG
	LOG(("%d %d %d %d %X %X %X", x, y, width, height,
	     bitmap, bg, content));
#endif
 	if (width == 0 || height == 0)
 		return true;

	apply_clip_and_mode();

	image = pdf_extract_image(bitmap, content);
	if (!image)
		return false;
	else {
		/*The position of the next tile*/
		HPDF_REAL current_x, current_y ;
		HPDF_REAL max_width, max_height;

		max_width =  (repeat_x ? page_width : width);
		max_height = (repeat_y ? page_height: height);


		for (current_y=0; current_y < max_height; current_y += height)
			for (current_x=0; current_x < max_width; current_x += width)
				HPDF_Page_DrawImage(pdf_page, image,
						current_x + x,
      						page_height-current_y - y - height,
      						width, height);
	}

	return true;
}

HPDF_Image pdf_extract_image(struct bitmap *bitmap, struct content *content)
{
	HPDF_Image image = NULL;

	if (content) {
		/*Not sure if I don't have to check if downloading has been
		finished.
		Other way - lock pdf plotting while fetching a website
		*/
		switch(content->type){
			/*Handle "embeddable" types of images*/
			case CONTENT_JPEG:
 				image = HPDF_LoadJpegImageFromMem(pdf_doc,
 						(const HPDF_BYTE *)content->source_data,
 						content->source_size);
 				break;

			/*Disabled until HARU PNG support will be more stable.

			case CONTENT_PNG:
				image = HPDF_LoadPngImageFromMem(pdf_doc,
						(const HPDF_BYTE *)content->source_data,
						content->total_size);
				break;*/
			default:
				break;
		}
	}

	if (!image) {
		HPDF_Image smask;
		unsigned char *img_buffer, *rgb_buffer, *alpha_buffer;
		int img_width, img_height, img_rowstride;
		int i, j;

		/*Handle pixmaps*/
		img_buffer = bitmap_get_buffer(bitmap);
		img_width = bitmap_get_width(bitmap);
		img_height = bitmap_get_height(bitmap);
		img_rowstride = bitmap_get_rowstride(bitmap);

		rgb_buffer = (unsigned char *)malloc(3 * img_width * img_height);
		alpha_buffer = (unsigned char *)malloc(img_width * img_height);
		if (rgb_buffer == NULL || alpha_buffer == NULL) {
			LOG(("Not enough memory to create RGB buffer"));
			free(rgb_buffer);
			free(alpha_buffer);
			return NULL;
		}

		for (i = 0; i < img_height; i++)
			for (j = 0; j < img_width; j++) {
				rgb_buffer[((i * img_width) + j) * 3] =
				  img_buffer[(i * img_rowstride) + (j * 4)];

				rgb_buffer[(((i * img_width) + j) * 3) + 1] =
				  img_buffer[(i * img_rowstride) + (j * 4) + 1];

				rgb_buffer[(((i * img_width) + j) * 3) + 2] =
				  img_buffer[(i * img_rowstride) + (j * 4) + 2];

				alpha_buffer[(i * img_width)+j] =
				  img_buffer[(i * img_rowstride) + (j * 4) + 3];
			}

		smask = HPDF_LoadRawImageFromMem(pdf_doc, alpha_buffer,
				img_width, img_height,
     				HPDF_CS_DEVICE_GRAY, 8);

		image = HPDF_LoadRawImageFromMem(pdf_doc, rgb_buffer,
				img_width, img_height,
     				HPDF_CS_DEVICE_RGB, 8);

		if (HPDF_Image_AddSMask(image, smask) != HPDF_OK)
			image = NULL;

		free(rgb_buffer);
		free(alpha_buffer);
	}

	return image;
}

/**change the mode and clip only if it's necessary*/
static void apply_clip_and_mode()
{

	if (in_text_mode && (!text_mode_request || page_clipped)) {
		HPDF_Page_EndText(pdf_page);
		in_text_mode = false;
	}

	if (page_clipped) {

		HPDF_Page_GRestore(pdf_page);
		HPDF_Page_GSave(pdf_page);

		HPDF_Page_Rectangle(pdf_page, last_clip_x0,
				page_height - last_clip_y1,
				last_clip_x1 - last_clip_x0,
				last_clip_y1 - last_clip_y0);
		HPDF_Page_Clip(pdf_page);
		HPDF_Page_EndPath(pdf_page);

		page_clipped = false;
	}

	if (text_mode_request) {
		if (!in_text_mode) {
			HPDF_Page_BeginText(pdf_page);
			in_text_mode = true;
		}

		text_mode_request = false;
	}
}

static inline float transform_x(float *transform, float x, float y)
{
	return ((transform[0] * x) + (transform[2] * (-y) ) + transform[4]) * 2;
}

static inline float transform_y(float *transform, float x, float y)
{
	return page_height - (((transform[1] * x) +
			(transform[3] * (-y)) - transform[5]) * 2);
}

bool pdf_plot_path(float *p, unsigned int n, colour fill, float width,
		colour c, float *transform)
{
#ifdef PDF_DEBUG
	LOG(("."));
#endif
	unsigned int i;
	bool empty_path = true;

	if (n == 0)
		return true;

	if ((c == TRANSPARENT) && (fill == TRANSPARENT))
		return true;

	if (p[0] != PLOTTER_PATH_MOVE) {
		return false;
	}

	HPDF_Page_SetRGBFill(pdf_page, R(fill), G(fill), B(fill));
	HPDF_Page_SetRGBStroke(pdf_page, R(c), G(c), B(c));

	transform[0] = 0.1;
	transform[1] = 0;
	transform[2] = 0;
	transform[3] = -0.1;
	transform[4] = 0;
	transform[5] = 0;

	for (i = 0 ; i<n ; ) {
		if (p[i] == PLOTTER_PATH_MOVE) {
			HPDF_Page_MoveTo(pdf_page,
					transform_x(transform, p[i+1], p[i+2]),
					transform_y(transform, p[i+1], p[i+2]));
			i+= 3;
		} else if (p[i] == PLOTTER_PATH_CLOSE) {
			if (!empty_path)
				HPDF_Page_ClosePath(pdf_page);
			i++;
		} else if (p[i] == PLOTTER_PATH_LINE) {
			HPDF_Page_LineTo(pdf_page,
					transform_x(transform, p[i+1], p[i+2]),
					transform_y(transform, p[i+1], p[i+2]));
			i+=3;
			empty_path = false;
		} else if (p[i] == PLOTTER_PATH_BEZIER) {
			HPDF_Page_CurveTo(pdf_page,
					transform_x(transform, p[i+1], p[i+2]),
					transform_y(transform, p[i+1], p[i+2]),
					transform_x(transform, p[i+3], p[i+4]),
					transform_y(transform, p[i+3], p[i+4]),
					transform_x(transform, p[i+5], p[i+6]),
					transform_y(transform, p[i+5], p[i+6]));
			i += 7;
			empty_path = false;
		} else {
			LOG(("bad path command %f", p[i]));
			return false;
		}
	}

	if (empty_path) {
		HPDF_Page_EndPath(pdf_page);
		return true;
	}

	if (fill!=TRANSPARENT) {
		if (c != TRANSPARENT)
			HPDF_Page_FillStroke(pdf_page);
		else
			HPDF_Page_Fill(pdf_page);
	}
	else
		HPDF_Page_Stroke(pdf_page);

	return true;
}

void pdf_set_solid()
{
	HPDF_Page_SetDash(pdf_page, NULL, 0, 0);
}

void pdf_set_dashed()
{
	HPDF_UINT16 dash_ptn[] = {3};
	HPDF_Page_SetDash(pdf_page, dash_ptn, 1, 1);
}

void pdf_set_dotted()
{
	HPDF_UINT16 dash_ptn[] = {1};
	HPDF_Page_SetDash(pdf_page, dash_ptn, 1, 1);
}

/**
 * Begin pdf plotting - initialize a new document
 * \param path Output file path
 * \param pg_width page width
 * \param pg_height page height
 */
bool pdf_begin(struct print_settings *print_settings)
{
	if (pdf_doc != NULL)
		HPDF_Free(pdf_doc);
	pdf_doc = HPDF_New(error_handler, NULL);
	if (!pdf_doc) {
		LOG(("Error creating pdf_doc"));
		return false;
	}

	settings = print_settings;

	page_width = settings->page_width - settings->margins[MARGINLEFT] -
			settings->margins[MARGINRIGHT];

	page_height = settings->page_height - settings->margins[MARGINTOP];


	if (option_enable_PDF_compression)
		HPDF_SetCompressionMode(pdf_doc, HPDF_COMP_ALL); /*Compression on*/
	HPDF_SetInfoAttr(pdf_doc, HPDF_INFO_CREATOR, user_agent_string());

	pdf_font = NULL;
	pdf_page = NULL;

#ifdef PDF_DEBUG
	LOG(("pdf_begin finishes"));
#endif
	return true;
}


bool pdf_next_page(void)
{
#ifdef PDF_DEBUG
	if (pdf_page != NULL) {
		HPDF_Page_GRestore(pdf_page);
		if (page_clipped)
			HPDF_Page_GRestore(pdf_page);
		pdf_plot_grid(10, 10, 0xCCCCCC);
		pdf_plot_grid(100, 100, 0xCCCCFF);
	}
#endif
	pdf_page = HPDF_AddPage(pdf_doc);
	if (pdf_page == NULL)
		return false;

	HPDF_Page_SetWidth (pdf_page, settings->page_width);
	HPDF_Page_SetHeight(pdf_page, settings->page_height);

	HPDF_Page_Concat(pdf_page,1,0,0,1,settings->margins[MARGINLEFT],0);

	page_clipped = false;
	HPDF_Page_GSave(pdf_page);

	text_mode_request = false;
	in_text_mode = false;

#ifdef PDF_DEBUG
	LOG(("%f %f", page_width, page_height));
#endif

	return true;
}


void pdf_end(void)
{
#ifdef PDF_DEBUG
	LOG(("pdf_end begins"));
	if (pdf_page != NULL) {
		HPDF_Page_GRestore(pdf_page);
		if (page_clipped)
			HPDF_Page_GRestore(pdf_page);
		pdf_plot_grid(10, 10, 0xCCCCCC);
		pdf_plot_grid(100, 100, 0xCCCCFF);
	}
#endif

	/*Encryption on*/
	if (option_enable_PDF_password)
		PDF_Password(&owner_pass, &user_pass,
				(void *)settings->output);
	else
		save_pdf(settings->output);
#ifdef PDF_DEBUG
	LOG(("pdf_end finishes"));
#endif
}

/** saves the pdf optionally encrypting it before*/
void save_pdf(const char *path)
{
	bool success = false;

	if (option_enable_PDF_password && owner_pass != NULL ) {
		HPDF_SetPassword(pdf_doc, owner_pass, user_pass);
		HPDF_SetEncryptionMode(pdf_doc, HPDF_ENCRYPT_R3, 16);
		free(owner_pass);
		free(user_pass);
	}

	if (path != NULL) {
		if (HPDF_SaveToFile(pdf_doc, path) != HPDF_OK)
			remove(path);
		else
			success = true;
	}

	if (!success)
		warn_user("Unable to save PDF file.", 0);

	HPDF_Free(pdf_doc);
	pdf_doc = NULL;
}


/**
 * Haru error handler
 * for debugging purposes - it immediately exits the program on the first error,
 * as it would otherwise flood the user with all resulting complications,
 * covering the most important error source.
*/
static void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no,
		void *user_data)
{
	LOG(("ERROR:\n\terror_no=%x\n\tdetail_no=%d\n",	(HPDF_UINT)error_no,
			(HPDF_UINT)detail_no));
#ifdef PDF_DEBUG
	exit(1);
#endif
}

/**
 * This function plots a grid - used for debug purposes to check if all
 * elements' final coordinates are correct.
*/
#ifdef PDF_DEBUG
void pdf_plot_grid(int x_dist, int y_dist, unsigned int colour)
{
	int i;

	for (int i = x_dist ; i < page_width ; i += x_dist)
		pdf_plot_line(i, 0, i, page_height, 1, colour, false, false);

	for (int i = y_dist ; i < page_height ; i += x_dist)
		pdf_plot_line(0, i, page_width, i, 1, colour, false, false);

}
#endif

/**used to synch the text scale with the scale for the whole content*/
void pdf_set_scale(float s)
{
	pdf_scale = s;
}

#endif /* WITH_PDF_EXPORT */


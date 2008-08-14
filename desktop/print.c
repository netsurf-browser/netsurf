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
 * Output-in-pages implementation
*/

#include "utils/config.h"
#ifdef WITH_PDF_EXPORT

#include <string.h>

#include "desktop/options.h"
#include "desktop/print.h"
#include "desktop/printer.h"
#include "desktop/save_pdf/font_haru.h"

#include "content/content.h"

#include "gtk/options.h"

#include "utils/log.h"
#include "utils/talloc.h"

#include "render/loosen.h"
#include "render/box.h"

static struct content *print_init(struct content *, struct print_settings *);
static bool print_apply_settings(struct content *, struct print_settings *);

static float page_content_width, page_content_height;
static struct content *printed_content;
static float done_height;

bool html_redraw_printing = false;
int html_redraw_printing_border = 0;
int html_redraw_printing_top_cropped = 0;

/**
 * This function calls print setup, prints page after page until the whole
 * content is printed calls cleaning up afterwise.
 * \param content The content to be printed
 * \param printer The printer interface for the printer to be used
 * \param settings The settings for printing to use or NULL for DEFAULT
 * \return true if successful, false otherwise
*/
bool print_basic_run(struct content *content,
		const struct printer *printer,
		struct print_settings *settings)
{
	bool ret = true;
	
	if (settings == NULL)
		settings = print_make_settings(DEFAULT, NULL);

	if (!print_set_up(content, printer, settings, NULL))
		ret = false;
	
	while (ret && (done_height < printed_content->height) )
		ret = print_draw_next_page(printer, settings);

	print_cleanup(content, printer, settings);
	
	return ret;
}

/**
 * This function prepares the content to be printed. The current browser content
 * is duplicated and resized, printer initialization is called.
 * \param content The content to be printed
 * \param printer The printer interface for the printer to be used
 * \param settings The settings for printing to use
 * \param height updated to the height of the printed content
 * \return true if successful, false otherwise
*/
bool print_set_up(struct content *content,
		const struct printer *printer, struct print_settings *settings,
		double *height)
{
	printed_content = print_init(content, settings);
	
	if (!printed_content)
		return false;
	
	print_apply_settings(printed_content, settings);

	if (height)
		*height = printed_content->height;
	
	printer->print_begin(settings);

	done_height = 0;
	
	return true;	
}

/**
 * This function draws one page, beginning with the height offset of done_height
 * \param printer The printer interface for the printer to be used
 * \param settings The settings for printing to use
 * \return true if successful, false otherwise
 */
bool print_draw_next_page(const struct printer *printer,
			struct print_settings *settings)
{
	int clip_x1, clip_y1;
	
	plot = *(printer->plotter);
	html_redraw_printing_top_cropped = INT_MAX;
	
	clip_x1 = page_content_width * settings->scale;
	clip_y1 = page_content_height  * settings->scale;
	
	html_redraw_printing = true;
	html_redraw_printing_border = clip_y1;
	
	printer->print_next_page();
	if( !content_redraw(printed_content,
			0,
			-done_height,
			0,0,
			0,
			0,
			clip_x1,
			clip_y1,
			settings->scale, 0xffffff))
		return false;
	done_height += page_content_height -
			(html_redraw_printing_top_cropped != INT_MAX ?
			clip_y1 - html_redraw_printing_top_cropped : 0) / settings->scale;
	
	return true;
}

/**
 * The content passed to the function is duplicated with its boxes, font
 * measuring functions are being set.
 * \param content The content to be printed
 * \param settings The settings for printing to use
 * \return true if successful, false otherwise
 */
struct content *print_init(struct content *content,
		struct print_settings *settings)
{
	struct content* printed_content;
	struct content_user *user_sentinel;
	
	content_add_user(content, NULL, (intptr_t)print_init, 0);
	
	printed_content = talloc_memdup(content, content, sizeof *content);
	
	if (!printed_content)
		return NULL;
	
	printed_content->data.html.bw = 0;
	
	user_sentinel = talloc(printed_content, struct content_user);
	user_sentinel->callback = 0;
	user_sentinel->p1 = user_sentinel->p2 = 0;
	user_sentinel->next = 0;
	printed_content->user_list = user_sentinel;
	content_add_user(printed_content, NULL, (intptr_t)print_init, 0);
	
	printed_content->data.html.layout =
			box_duplicate_tree(content->data.html.layout,
					   printed_content);
	
	if (!printed_content->data.html.layout)
		return NULL;
	
	if (settings->font_func == NULL)
 		printed_content->data.html.font_func = &haru_nsfont;
	else
		printed_content->data.html.font_func = settings->font_func;
	
	return printed_content;
}

/**
 * The content is resized to fit page width. In case it is to wide, it is
 * loosened.
 * \param content The content to be printed
 * \param settings The settings for printing to use
 * \return true if successful, false otherwise
 */
bool print_apply_settings(struct content *content,
			  struct print_settings *settings)
{
	if (settings == NULL)
		return false;
	
	/*Apply settings - adjust page size etc*/

	page_content_width = (settings->page_width - settings->margins[MARGINLEFT] - 
			settings->margins[MARGINRIGHT]) / settings->scale;
	
	page_content_height = (settings->page_height - settings->margins[MARGINTOP] - 
			settings->margins[MARGINBOTTOM]) / settings->scale;
	
	content_reformat(content, page_content_width, 0);
	LOG(("New layout applied.New height = %d ; New width = %d ",
	     content->height, content->width));
	
	/*check if loosening is necessary and requested*/
	if (option_enable_loosening && content->width > page_content_width)
  		return loosen_document_layout(content, content->data.html.layout,
  				page_content_width, page_content_height);
			
	return true;	
}

/**
 * Memory allocated during printing is being freed here.
 * \param content The original content
 * \param printer The printer interface for the printer to be used
 * \return true if successful, false otherwise
 */
bool print_cleanup(struct content *content,
		const struct printer *printer,
		struct print_settings *settings)
{
	printer->print_end();
	
	html_redraw_printing = false;
	
	if (printed_content) {
		content_remove_user(printed_content, NULL, (intptr_t)print_init, 0);
		talloc_free(printed_content);
	}
	
	content_remove_user(content, NULL, (intptr_t)print_init, 0);
	
	free((void *)settings->output);
	free(settings);
	
	return true;
}

/**
 * Generates one of the predefined print settings sets.
 * \param configuration the requested configuration
 * \param filename the filename or NULL
 * \return print_settings in case if successful, NULL if unknown configuration \
 * 	or lack of memory.
 */
struct print_settings *print_make_settings(print_configuration configuration,
		const char *filename)
{
	struct print_settings *settings;
	char *path;
	struct css_length length;
	
	path = malloc(PATH_MAX * sizeof(char));
	if (path == NULL)
		return NULL;
	
	length.unit = CSS_UNIT_MM;
	
	switch (configuration){
		case DEFAULT:	
			settings = (struct print_settings*) 
					malloc(sizeof(struct print_settings) );
			
			if (settings == NULL)
				return NULL;
			
			settings->page_width  = DEFAULT_PAGE_WIDTH;
			settings->page_height = DEFAULT_PAGE_HEIGHT;
			settings->copies = DEFAULT_COPIES;

			settings->scale = DEFAULT_EXPORT_SCALE;
			
			length.value = DEFAULT_MARGIN_LEFT_MM;
			settings->margins[MARGINLEFT] = css_len2px(&length, 0);
			length.value = DEFAULT_MARGIN_RIGHT_MM;
			settings->margins[MARGINRIGHT] = css_len2px(&length, 0);
			length.value = DEFAULT_MARGIN_TOP_MM;
			settings->margins[MARGINTOP] = css_len2px(&length, 0);
			length.value = DEFAULT_MARGIN_BOTTOM_MM;
			settings->margins[MARGINBOTTOM] = css_len2px(&length, 0);
			
			settings->font_func = &haru_nsfont;
			
			break;
		/*use settings from the Export options tab*/
		case OPTIONS:
			settings = (struct print_settings*) 
					malloc(sizeof(struct print_settings) );
			
			if (settings == NULL)
				return NULL;
			
			settings->page_width  = DEFAULT_PAGE_WIDTH;
			settings->page_height = DEFAULT_PAGE_HEIGHT;
			settings->copies = DEFAULT_COPIES;
			
			settings->scale = (float)option_export_scale / 100;
			
			length.value = option_margin_left;
			settings->margins[MARGINLEFT] = css_len2px(&length, 0);
			length.value = option_margin_right;
			settings->margins[MARGINRIGHT] = css_len2px(&length, 0);
			length.value = option_margin_top;
			settings->margins[MARGINTOP] = css_len2px(&length, 0);
			length.value = option_margin_bottom;
			settings->margins[MARGINBOTTOM] = css_len2px(&length, 0);
			
			settings->font_func = &haru_nsfont;
			
			break;
		default:
			return NULL;
	}
	
	/*if no filename is specified use one without an extension*/
	if (filename == NULL) {
		/*TODO: the "/" is not platform independent*/
		strcpy(path, "/out");
	}
	else
		strcpy(path, filename);
	
	settings->output = path;	
	
	return settings;	
}

#endif /* WITH_PDF_EXPORT */

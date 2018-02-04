/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
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

/**
 * \file
 * implementation of content handling for PDF.
 *
 * This implementation uses the netsurf pdf library.
 */

#include <stdbool.h>
#include <stdlib.h>

#include <nspdf/document.h>
#include <nspdf/meta.h>
#include <nspdf/page.h>

#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "netsurf/plotters.h"
#include "netsurf/content.h"
#include "netsurf/browser_window.h"
#include "content/llcache.h"
#include "content/content_protected.h"

#include "pdf.h"

typedef struct pdf_content {
	struct content base;

	struct nspdf_doc *doc;

	unsigned int current_page;
	unsigned int page_count;
} pdf_content;

static nserror nspdf2nserr(nspdferror nspdferr)
{
	nserror res;
	switch (nspdferr) {
	case NSPDFERROR_OK:
		res = NSERROR_OK;
		break;

	case NSPDFERROR_NOMEM:
		res = NSERROR_NOMEM;
		break;

	default:
		res = NSERROR_UNKNOWN;
		break;
	}
	return res;
}

/**
 * Content create entry point.
 */
static nserror
pdf_create(const content_handler *handler,
	   lwc_string *imime_type,
	   const struct http_parameter *params,
	   llcache_handle *llcache,
	   const char *fallback_charset,
	   bool quirks,
	   struct content **c)
{
	struct pdf_content *pdfc;
	nserror res;
	nspdferror pdfres;

	pdfc = calloc(1, sizeof(struct pdf_content));
	if (pdfc == NULL) {
		return NSERROR_NOMEM;
	}

	res = content__init(&pdfc->base,
			    handler,
			    imime_type,
			    params,
			    llcache,
			    fallback_charset,
			    quirks);
	if (res != NSERROR_OK) {
		free(pdfc);
		return res;
	}

	pdfres = nspdf_document_create(&pdfc->doc);
	if (pdfres != NSPDFERROR_OK) {
		free(pdfc);
		return nspdf2nserr(res);
	}

	*c = (struct content *)pdfc;

	return NSERROR_OK;
}

/* exported interface documented in image_cache.h */
static void pdf_destroy(struct content *c)
{
	struct pdf_content *pdfc = (struct pdf_content *)c;
	nspdf_document_destroy(pdfc->doc);
}

static bool pdf_convert(struct content *c)
{
	struct pdf_content *pdfc = (struct pdf_content *)c;
	nspdferror pdfres;
	const uint8_t *content_data;
	unsigned long content_length;
	struct lwc_string_s *title;
	float page_width;
	float page_height;

	content_data = (const uint8_t *)content__get_source_data(c,
						&content_length);

	pdfres = nspdf_document_parse(pdfc->doc,
				      content_data,
				      content_length);
	if (pdfres != NSPDFERROR_OK) {
		content_broadcast_errorcode(c, NSERROR_INVALID);
		return false;
	}

	pdfres = nspdf_page_count(pdfc->doc, &pdfc->page_count);
	if (pdfres != NSPDFERROR_OK) {
		content_broadcast_errorcode(c, NSERROR_INVALID);
		return false;
	}

	pdfres = nspdf_get_title(pdfc->doc, &title);
	if (pdfres == NSPDFERROR_OK) {
		content__set_title(c, lwc_string_data(title));
	}

	/** \todo extract documents starting page number */
	pdfc->current_page = 16;

	pdfres = nspdf_get_page_dimensions(pdfc->doc,
					   pdfc->current_page,
					   &page_width,
					   &page_height);
	if (pdfres == NSPDFERROR_OK) {
		pdfc->base.width = page_width;
		pdfc->base.height = page_height;
	}

	content_set_ready(c);
	content_set_done(c);

	return true;
}

static nspdferror
pdf_path(const struct nspdf_style *style,
	 const float *path,
	 unsigned int path_length,
	 const float transform[6],
	 const void *ctxin)
{
	const struct redraw_context *ctx = ctxin;

	ctx->plot->path(ctx,
			(const struct plot_style_s *)style,
			path,
			path_length,
			style->stroke_width,
			transform);
	return NSPDFERROR_OK;
}

/* exported interface documented in image_cache.h */
static bool
pdf_redraw(struct content *c,
	   struct content_redraw_data *data,
	   const struct rect *clip,
	   const struct redraw_context *ctx)
{
	struct pdf_content *pdfc = (struct pdf_content *)c;
	nspdferror pdfres;
	struct nspdf_render_ctx render_ctx;

	NSLOG(netsurf, DEBUG,
	      "data x:%d y:%d w:%d h:%d\nclip %d %d %d %d\n",
	       data->x, data->y, data->width, data->height,
	       clip->x0, clip->y0, clip->x1, clip->y1);

	render_ctx.ctx = ctx;
	render_ctx.device_space[0] = 1; /* scale x */
	render_ctx.device_space[1] = 0;
	render_ctx.device_space[2] = 0;
	render_ctx.device_space[3] = -1; /* scale y */
	render_ctx.device_space[4] = 0; /* x offset */
	render_ctx.device_space[5] = data->height; /* y offset */
	render_ctx.path = pdf_path;

	pdfres = nspdf_page_render(pdfc->doc, pdfc->current_page, &render_ctx);

	return true;
}

/**
 * Clone content.
 */
static nserror pdf_clone(const struct content *old, struct content **newc)
{
	return NSERROR_NOMEM;
}

static content_type pdf_content_type(void)
{
	return CONTENT_PDF;
}

static void
pdf_change_page(struct pdf_content *pdfc,
		struct browser_window *bw,
		unsigned int page_number)
{
	float page_width;
	float page_height;
	nspdferror pdfres;

	/* ensure page stays in bounds */
	if (page_number >= pdfc->page_count) {
		return;
	}

	pdfc->current_page = page_number;

	pdfres = nspdf_get_page_dimensions(pdfc->doc,
					   pdfc->current_page,
					   &page_width,
					   &page_height);
	if (pdfres == NSPDFERROR_OK) {
		pdfc->base.width = page_width;
		pdfc->base.height = page_height;
		NSLOG(netsurf, DEBUG,
		      "page %d w:%f h:%f\n",
		       pdfc->current_page,
		       page_width,
		       page_height);
	}

	browser_window_update(bw, false);
}

static void
pdf_mouse_action(struct content *c,
		 struct browser_window *bw,
		 browser_mouse_state mouse,
		 int x, int y)
{
	struct pdf_content *pdfc = (struct pdf_content *)c;

	if (mouse & BROWSER_MOUSE_CLICK_1) {
		int bwwidth;
		int bwheight;
		browser_window_get_extents(bw, false, &bwwidth, &bwheight);

		if (x < (bwwidth / 2)) {
			pdf_change_page(pdfc, bw, pdfc->current_page - 1);
		} else {
			pdf_change_page(pdfc, bw, pdfc->current_page + 1);
		}
	}
}

static const content_handler nspdf_content_handler = {
	.create = pdf_create,
	.data_complete = pdf_convert,
	.destroy = pdf_destroy,
	.redraw = pdf_redraw,
	.mouse_action = pdf_mouse_action,
	.clone = pdf_clone,
	.type = pdf_content_type,
	.no_share = false,
};

static const char *nspdf_types[] = {
	"application/pdf",
	"application/x-pdf",
	"application/acrobat",
	"applications/vnd.pdf",
	"text/pdf",
	"text/x-pdf"
};

CONTENT_FACTORY_REGISTER_TYPES(nspdf, nspdf_types, nspdf_content_handler);

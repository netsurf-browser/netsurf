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

#include "utils/utils.h"
#include "content/llcache.h"
#include "content/content_protected.h"

#include "pdf.h"

typedef struct pdf_content {
	struct content base;

	struct nspdf_doc *doc;
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

	content_data = (const uint8_t *)content__get_source_data(c,
						&content_length);

	pdfres = nspdf_document_parse(pdfc->doc,
				      content_data,
				      content_length);
	if (pdfres != NSPDFERROR_OK) {
		content_broadcast_errorcode(c, NSERROR_INVALID);
		return false;
	}

	pdfres = nspdf_get_title(pdfc->doc, &title);
	if (pdfres == NSPDFERROR_OK) {
		content__set_title(c, lwc_string_data(title));
	}

	content_set_ready(c);
	content_set_done(c);

	return true;
}

/* exported interface documented in image_cache.h */
static bool
pdf_redraw(struct content *c,
	   struct content_redraw_data *data,
	   const struct rect *clip,
	   const struct redraw_context *ctx)
{
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


static const content_handler nspdf_content_handler = {
	.create = pdf_create,
	.data_complete = pdf_convert,
	.destroy = pdf_destroy,
	.redraw = pdf_redraw,
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

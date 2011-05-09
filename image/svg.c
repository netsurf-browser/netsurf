/*
 * Copyright 2007-2008 James Bursa <bursa@users.sourceforge.net>
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
 * Content for image/svg (implementation).
 */

#include "utils/config.h"
#ifdef WITH_NS_SVG

#include <assert.h>
#include <string.h>

#include <svgtiny.h>

#include "content/content_protected.h"
#include "css/css.h"
#include "desktop/plotters.h"
#include "image/svg.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/utils.h"

typedef struct svg_content {
	struct content base;

	struct svgtiny_diagram *diagram;
	bool done_parse;
} svg_content;

static nserror svg_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static nserror svg_create_svg_data(svg_content *c);
static bool svg_convert(struct content *c);
static void svg_destroy(struct content *c);
static void svg_reformat(struct content *c, int width, int height);
static bool svg_redraw(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour,
		bool repeat_x, bool repeat_y);
static nserror svg_clone(const struct content *old, struct content **newc);
static content_type svg_content_type(lwc_string *mime_type);

static const content_handler svg_content_handler = {
	svg_create,
	NULL,
	svg_convert,
	svg_reformat,
	svg_destroy,
	NULL,
	NULL,
	NULL,
	svg_redraw,
	NULL,
	NULL,
	svg_clone,
	NULL,
	svg_content_type,
	false
};

static const char *svg_types[] = {
	"image/svg",
	"image/svg+xml"
};

static lwc_string *svg_mime_types[NOF_ELEMENTS(svg_types)];

nserror svg_init(void)
{
	uint32_t i;
	lwc_error lerror;
	nserror error;

	for (i = 0; i < NOF_ELEMENTS(svg_mime_types); i++) {
		lerror = lwc_intern_string(svg_types[i],
				strlen(svg_types[i]),
				&svg_mime_types[i]);
		if (lerror != lwc_error_ok) {
			error = NSERROR_NOMEM;
			goto error;
		}

		error = content_factory_register_handler(svg_mime_types[i],
				&svg_content_handler);
		if (error != NSERROR_OK)
			goto error;
	}

	return NSERROR_OK;

error:
	svg_fini();

	return error;
}

void svg_fini(void)
{
	uint32_t i;

	for (i = 0; i < NOF_ELEMENTS(svg_mime_types); i++) {
		if (svg_mime_types[i] != NULL)
			lwc_string_unref(svg_mime_types[i]);
	}
}

/**
 * Create a CONTENT_SVG.
 */

nserror svg_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	svg_content *svg;
	nserror error;

	svg = talloc_zero(0, svg_content);
	if (svg == NULL)
		return NSERROR_NOMEM;

	error = content__init(&svg->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		talloc_free(svg);
		return error;
	}

	error = svg_create_svg_data(svg);
	if (error != NSERROR_OK) {
		talloc_free(svg);
		return error;
	}

	*c = (struct content *) svg;

	return NSERROR_OK;
}

nserror svg_create_svg_data(svg_content *c)
{
	union content_msg_data msg_data;

	c->diagram = svgtiny_create();
	if (c->diagram == NULL)
		goto no_memory;

	c->done_parse = false;

	return NSERROR_OK;

no_memory:
	msg_data.error = messages_get("NoMemory");
	content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
	return NSERROR_NOMEM;
}


/**
 * Convert a CONTENT_SVG for display.
 */

bool svg_convert(struct content *c)
{
	/*c->title = malloc(100);
	if (c->title)
		snprintf(c->title, 100, messages_get("svgTitle"),
				width, height, c->source_size);*/
	//c->size += ?;
	content_set_ready(c);
	content_set_done(c);
	/* Done: update status bar */
	content_set_status(c, "");

	return true;
}

/**
 * Reformat a CONTENT_SVG.
 */

void svg_reformat(struct content *c, int width, int height)
{
	svg_content *svg = (svg_content *) c;
	const char *source_data;
	unsigned long source_size;

	assert(svg->diagram);

	if (svg->done_parse == false) {
		source_data = content__get_source_data(c, &source_size);

		svgtiny_parse(svg->diagram, source_data, source_size,
				content__get_url(c), width, height);

		svg->done_parse = true;
	}

	c->width = svg->diagram->width;
	c->height = svg->diagram->height;
}


/**
 * Redraw a CONTENT_SVG.
 */

bool svg_redraw(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		float scale, colour background_colour,
		bool repeat_x, bool repeat_y)
{
	svg_content *svg = (svg_content *) c;
	float transform[6];
	struct svgtiny_diagram *diagram = svg->diagram;
	bool ok;
	int px, py;
	unsigned int i;
	plot_font_style_t fstyle = *plot_style_font;

	assert(diagram);

	transform[0] = (float) width / (float) c->width;
	transform[1] = 0;
	transform[2] = 0;
	transform[3] = (float) height / (float) c->height;
	transform[4] = x;
	transform[5] = y;

#define BGR(c) ((c) == svgtiny_TRANSPARENT ? NS_TRANSPARENT : ((svgtiny_RED((c))) | (svgtiny_GREEN((c)) << 8) | (svgtiny_BLUE((c)) << 16)))

	for (i = 0; i != diagram->shape_count; i++) {
		if (diagram->shape[i].path) {
			ok = plot.path(diagram->shape[i].path,
					diagram->shape[i].path_length,
					BGR(diagram->shape[i].fill),
					diagram->shape[i].stroke_width,
					BGR(diagram->shape[i].stroke),
					transform);
			if (!ok)
				return false;

		} else if (diagram->shape[i].text) {
			px = transform[0] * diagram->shape[i].text_x +
				transform[2] * diagram->shape[i].text_y +
				transform[4];
			py = transform[1] * diagram->shape[i].text_x +
				transform[3] * diagram->shape[i].text_y +
				transform[5];

			fstyle.background = 0xffffff;
			fstyle.foreground = 0x000000;
			fstyle.size = (8 * FONT_SIZE_SCALE) * scale;

			ok = plot.text(px, py,
					diagram->shape[i].text,
					strlen(diagram->shape[i].text),
					&fstyle);
			if (!ok)
				return false;
		}
        }

#undef BGR

	return true;
}


/**
 * Destroy a CONTENT_SVG and free all resources it owns.
 */

void svg_destroy(struct content *c)
{
	svg_content *svg = (svg_content *) c;

	if (svg->diagram != NULL)
		svgtiny_free(svg->diagram);
}


nserror svg_clone(const struct content *old, struct content **newc)
{
	svg_content *svg;
	nserror error;

	svg = talloc_zero(0, svg_content);
	if (svg == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &svg->base);
	if (error != NSERROR_OK) {
		content_destroy(&svg->base);
		return error;
	}

	/* Simply replay create/convert */
	error = svg_create_svg_data(svg);
	if (error != NSERROR_OK) {
		content_destroy(&svg->base);
		return error;
	}

	if (old->status == CONTENT_STATUS_READY ||
			old->status == CONTENT_STATUS_DONE) {
		if (svg_convert(&svg->base) == false) {
			content_destroy(&svg->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *) svg;

	return NSERROR_OK;
}

content_type svg_content_type(lwc_string *mime_type)
{
	return CONTENT_IMAGE;
}

#endif /* WITH_NS_SVG */

/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2007 Rob Kendrick <rjek@netsurf-browser.org>
 */

/** \file
 * Content handler for image/svg using librsvg (implementation).
 *
 * SVG files are rendered to a NetSurf bitmap by creating a Cairo rendering
 * surface (content_rsvg_data.cs) over the bitmap's data, creating a Cairo
 * drawing context using that surface, and then passing that drawing context
 * to librsvg which then uses Cairo calls to plot the graphic to the bitmap.
 * We store this in content->bitmap, and then use the usual bitmap plotter
 * function to render it for redraw requests.
 */

#include "utils/config.h"
#ifdef WITH_RSVG

#include <stdbool.h>
#include <sys/types.h>

#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>

#include "image/rsvg.h"
#include "content/content.h"
#include "desktop/plotters.h"
#include "image/bitmap.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "utils/messages.h"
#include "utils/talloc.h"

static inline void rsvg_argb_to_abgr(u_int32_t pixels[], int width, int height,
				int rowstride);
static void rsvg_size_func(gint *width, gint *height, gpointer user_data);

/** Callback called by librsvg when it has determined the graphic's natural
 * size.
 *
 * \param width		Pointer to graphic's natual width.  Change this to
 *			change what size we'd like librsvg to render it
 * \param height	Pointer to the graphic's natual height.  Change this to
 *			change what size we'd like librsvg to redner it
 * \param user_data	User-specific context, in this case it contains the
 *			pointer to the content object
 */
static void rsvg_size_func(gint *width, gint *height, gpointer user_data)
{
	struct content *c = (struct content *)user_data;
	LOG(("svg image's natural size is %d x %d", *width, *height));

	c->width = *width;
	c->height = *height;	
}

bool rsvg_create(struct content *c, const char *params[])
{
	struct content_rsvg_data *d = &c->data.rsvg;
	union content_msg_data msg_data;

	LOG(("!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));

	if ((d->rsvgh = rsvg_handle_new()) == NULL) {
		LOG(("rsvg_handle_new() returned NULL."));
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	
	rsvg_handle_set_size_callback(d->rsvgh, rsvg_size_func, c, NULL);
	
	return true;
}

/** Convert Cairo's ARGB output to NetSurf's favoured ABGR format.  It converts
 * the data in-place.
 *
 * \param pixels	Array of 32-bit values, in the form of ARGB.  This will
 *			be overwritten with new data in the form of ABGR.
 * \param width		Width of the bitmap
 * \param height	Height of the bitmap
 * \param rowstride	Number of bytes to skip after each row (this
 *			implementation requires this to be a multiple of 4.)
 */
static inline void rsvg_argb_to_abgr(u_int32_t pixels[], int width, int height,
				int rowstride)
{
	u_int32_t *p = &pixels[0];

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			u_int32_t e = p[x];
			u_int32_t s = (((e & 0xff) << 24) |
					((e & 0xff00) << 8) |
					((e & 0xff0000) >> 8) |
					((e & 0xff000000) >> 24));
			p[x] = (s >> 8) | (s << 24);
		}
		p += rowstride;
	}
}

bool rsvg_convert(struct content *c, int iwidth, int iheight)
{
	struct content_rsvg_data *d = &c->data.rsvg;
	union content_msg_data msg_data;
	GError *err = NULL;

	if (rsvg_handle_write(d->rsvgh, (guchar *)c->source_data, 
					(gsize)c->source_size, &err)
		== FALSE) {
		LOG(("rsvg_handle_write returned an error: %s", err->message));
		rsvg_handle_free(d->rsvgh);
		msg_data.error = err->message;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;	
	}
	
	if (rsvg_handle_close(d->rsvgh, &err) == FALSE) {
		LOG(("rsvg_handle_close returned an error: %s", err->message));
		rsvg_handle_free(d->rsvgh);
		msg_data.error = err->message;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	
	if ((d->bitmap = bitmap_create(c->width, c->height,
			BITMAP_NEW)) == NULL) {
		LOG(("Failed to create bitmap for rsvg render."));
		rsvg_handle_free(d->rsvgh);
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);		
		return false;		
	}
	
	if ((d->cs = cairo_image_surface_create_for_data(
			(unsigned char *)bitmap_get_buffer(d->bitmap),
			CAIRO_FORMAT_ARGB32,
			c->width, c->height,
			bitmap_get_rowstride(d->bitmap))) == NULL) {
		LOG(("Failed to create Cairo image surface for rsvg render."));
		bitmap_destroy(d->bitmap);
		rsvg_handle_free(d->rsvgh);
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	
	if ((d->ct = cairo_create(d->cs)) == NULL) {
		LOG(("Failed to create Cairo drawing context for rsvg render."));
		bitmap_destroy(d->bitmap);
		rsvg_handle_free(d->rsvgh);
		cairo_surface_destroy(d->cs);
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;		
	}

	rsvg_handle_render_cairo(d->rsvgh, d->ct);
	/* TODO: This currently segfaults */
//	rsvg_argb_to_abgr((u_int32_t *)bitmap_get_buffer(d->bitmap),
//				c->width, c->height,
//				bitmap_get_rowstride(d->bitmap));
				
	c->bitmap = d->bitmap;
	c->status = CONTENT_STATUS_DONE;
	
	return true;
}

bool rsvg_redraw(struct content *c, int x, int y, int width, int height,
			int clip_x0, int clip_y0, int clip_x1, int clip_y1,
			float scale, unsigned long background_colour)
{
	plot.bitmap(x, y, width, height, c->bitmap, background_colour);
	return true;
}

bool rsvg_redraw_tiled(struct content *c, int x, int y, int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour,
		bool repeat_x, bool repeat_y)
{
	return true;
}

void rsvg_destroy(struct content *c)
{
	struct content_rsvg_data *d = &c->data.rsvg;

	bitmap_destroy(d->bitmap);
	rsvg_handle_free(d->rsvgh);
	cairo_destroy(d->ct);
	cairo_surface_destroy(d->cs);

	return;
}

#endif /* WITH_RSVG */

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Content for image/mng, image/png, and image/jng (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include "libmng.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/options.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/image/mng.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_MNG

/*	We do not currently support any form of colour/gamma correction, nor do
	we support dynamic MNGs.
*/


static mng_bool nsmng_openstream(mng_handle mng);
static mng_bool nsmng_readdata(mng_handle mng, mng_ptr buffer, mng_uint32 size, mng_uint32 *bytesread);
static mng_bool nsmng_closestream(mng_handle mng);
static mng_bool nsmng_processheader(mng_handle mng, mng_uint32 width, mng_uint32 height);
static mng_ptr nsmng_getcanvasline(mng_handle mng, mng_uint32 line);
static mng_uint32 nsmng_gettickcount(mng_handle mng);
static mng_bool nsmng_refresh(mng_handle mng, mng_uint32 x, mng_uint32 y, mng_uint32 w, mng_uint32 h);
static mng_bool nsmng_settimer(mng_handle mng, mng_uint32 msecs);
static void nsmng_animate(void *p);
static bool nsmng_broadcast_error(struct content *c);
static mng_bool nsmng_errorproc(mng_handle mng, mng_int32 code,
	mng_int8 severity, mng_chunkid chunktype, mng_uint32 chunkseq,
	mng_int32 extra1, mng_int32 extra2, mng_pchar text);
#ifndef MNG_INTERNAL_MEMMNGMT
static mng_ptr nsmng_alloc(mng_size_t n);
static void nsmng_free(mng_ptr p, mng_size_t n);
#endif


bool nsmng_create(struct content *c, const char *params[]) {

	assert(c != NULL);
	assert(params != NULL);

	/*	Initialise the library
	*/
#ifdef MNG_INTERNAL_MEMMNGMT
	c->data.mng.handle = mng_initialize(c, MNG_NULL, MNG_NULL, MNG_NULL);
#else
	c->data.mng.handle = mng_initialize(c, nsmng_alloc, nsmng_free, MNG_NULL);
#endif
	if (c->data.mng.handle == MNG_NULL) {
		LOG(("Unable to initialise MNG library."));
		return nsmng_broadcast_error(c);
	}

	/*	We need to decode in suspension mode
	*/
	if (mng_set_suspensionmode(c->data.mng.handle, MNG_TRUE) != MNG_NOERROR) {
		LOG(("Unable to set suspension mode."));
		return nsmng_broadcast_error(c);
	}

	/*	We need to register our callbacks
	*/
	if (mng_setcb_openstream(c->data.mng.handle, nsmng_openstream) != MNG_NOERROR) {
		LOG(("Unable to set openstream callback."));
		return nsmng_broadcast_error(c);
	}
	if (mng_setcb_readdata(c->data.mng.handle, nsmng_readdata) != MNG_NOERROR) {
		LOG(("Unable to set readdata callback."));
		return nsmng_broadcast_error(c);
	}
	if (mng_setcb_closestream(c->data.mng.handle, nsmng_closestream) != MNG_NOERROR) {
		LOG(("Unable to set closestream callback."));
		return nsmng_broadcast_error(c);
	}
	if (mng_setcb_processheader(c->data.mng.handle, nsmng_processheader) != MNG_NOERROR) {
		LOG(("Unable to set processheader callback."));
		return nsmng_broadcast_error(c);
	}

	/*	Register our callbacks for displaying
	*/
	if (mng_setcb_getcanvasline(c->data.mng.handle, nsmng_getcanvasline) != MNG_NOERROR) {
		LOG(("Unable to set getcanvasline callback."));
		return nsmng_broadcast_error(c);
	}
	if (mng_setcb_refresh(c->data.mng.handle, nsmng_refresh) != MNG_NOERROR) {
		LOG(("Unable to set refresh callback."));
		return nsmng_broadcast_error(c);
	}
	if (mng_setcb_gettickcount(c->data.mng.handle, nsmng_gettickcount) != MNG_NOERROR) {
		LOG(("Unable to set gettickcount callback."));
		return nsmng_broadcast_error(c);
	}
	if (mng_setcb_settimer(c->data.mng.handle, nsmng_settimer) != MNG_NOERROR) {
		LOG(("Unable to set settimer callback."));
		return nsmng_broadcast_error(c);
	}

	/* register error handling function */
	if (mng_setcb_errorproc(c->data.mng.handle, nsmng_errorproc) != MNG_NOERROR) {
		LOG(("Unable to set errorproc"));
		return nsmng_broadcast_error(c);
	}

	/*	Initialise the reading
	*/
	c->data.mng.read_start = true;
	c->data.mng.read_resume = false;
	c->data.mng.read_size = 0;
	c->data.mng.waiting = false;

	c->data.mng.displayed = false;
	return true;
}


/*	START OF CALLBACKS REQUIRED FOR READING
*/


mng_bool nsmng_openstream(mng_handle mng) {
	assert(mng != NULL);
	return MNG_TRUE;
}

mng_bool nsmng_readdata(mng_handle mng, mng_ptr buffer, mng_uint32 size, mng_uint32 *bytesread) {
	struct content *c;

	assert(mng != NULL);
	assert(buffer != NULL);
	assert(bytesread != NULL);

	/*	Get our content back
	*/
	c = (struct content *)mng_get_userdata(mng);
	assert(c != NULL);

	/*	Copy any data we have (maximum of 'size')
	*/
	*bytesread = ((c->source_size - c->data.mng.read_size) < size) ?
			(c->source_size - c->data.mng.read_size) : size;

	LOG(("Read %d, processing %p", *bytesread, mng));

	if ((*bytesread) > 0) {
		memcpy(buffer, c->source_data + c->data.mng.read_size, *bytesread);
		c->data.mng.read_size += *bytesread;
	}

	/*	Return success
	*/
	return MNG_TRUE;
}

mng_bool nsmng_closestream(mng_handle mng) {
	assert(mng != NULL);
	return MNG_TRUE;
}

mng_bool nsmng_processheader(mng_handle mng, mng_uint32 width, mng_uint32 height) {
	struct content *c;
	union content_msg_data msg_data;

	assert(mng != NULL);

	/*	This function is called when the header has been read and we know
		the dimensions of the canvas.
	*/
	c = (struct content *)mng_get_userdata(mng);
	assert(c != NULL);

	LOG(("processing header (%p) %d, %d", c, width, height));

	c->bitmap = bitmap_create(width, height, BITMAP_NEW);
	if (!c->bitmap) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		LOG(("Insufficient memory to create canvas."));
		return MNG_FALSE;
	}

	/*	Initialise the content size
	*/
	c->width = width;
	c->height = height;

	/*	Set the canvas style
	*/
	if (mng_set_canvasstyle(mng, MNG_CANVAS_RGBA8) != MNG_NOERROR) {
		LOG(("Error setting canvas style."));
	}

	/*	Return success
	*/
	return MNG_TRUE;
}


/*	END OF CALLBACKS REQUIRED FOR READING
*/


bool nsmng_process_data(struct content *c, char *data, unsigned int size) {
	mng_retcode status;

	assert(c != NULL);
	assert(data != NULL);

	/*	We only need to do any processing if we're starting/resuming reading.
	*/
	if ((!c->data.mng.read_resume) && (!c->data.mng.read_start)) return true;

	/*	Try to start processing, or process some more data
	*/
	if (c->data.mng.read_start) {
		status = mng_read(c->data.mng.handle);
		c->data.mng.read_start = false;
	} else {
		status = mng_read_resume(c->data.mng.handle);
	}
	c->data.mng.read_resume = (status == MNG_NEEDMOREDATA);
	if ((status != MNG_NOERROR) && (status != MNG_NEEDMOREDATA)) {
		LOG(("Failed to start/continue reading (%i).", status));
		return nsmng_broadcast_error(c);
	}

	/*	Continue onwards
	*/
	return true;
}


bool nsmng_convert(struct content *c, int width, int height) {
	mng_retcode status;

	union content_msg_data msg_data;

	assert(c != NULL);

	LOG(("Converting %p '%s'", c, c->url));

	/* by this point, the png should have been parsed
	 * and the bitmap created, so ensure that's the case
	 */
	if (!c->bitmap)
		return nsmng_broadcast_error(c);

	/*	Set the title
	*/
	c->title = malloc(100);
	if (!c->title) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	if (c->type == CONTENT_MNG) {
		snprintf(c->title, 100, messages_get("MNGTitle"),
				c->width, c->height, c->source_size);
	} else if (c->type == CONTENT_PNG) {
		snprintf(c->title, 100, messages_get("PNGTitle"),
				c->width, c->height, c->source_size);
	} else {
		snprintf(c->title, 100, messages_get("JNGTitle"),
				c->width, c->height, c->source_size);
	}

	c->size += c->width * c->height * 4 + 100;
	c->status = CONTENT_STATUS_DONE;

	/* jmb: I'm really not sure that this should be here.
	 * The *_convert functions are for converting a content into a
	 * displayable format. They should not, however, do anything which
	 * could cause the content to be displayed; the content may have
	 * hidden visibility or be a fallback for an object; this
	 * information is not available here (nor is there any need for it
	 * to be).
	 * The specific issue here is that mng_display calls the display
	 * callbacks, which include nsmng_refresh. nsmng_refresh forces
	 * a content to be redrawn regardless of whether it should be
	 * displayed or not.
	 */
	/*	Start displaying
	*/
	status = mng_display(c->data.mng.handle);
	if ((status != MNG_NOERROR) && (status != MNG_NEEDTIMERWAIT)) {
		LOG(("Unable to start display (%i)", status));
		return nsmng_broadcast_error(c);
	}
	bitmap_modified(c->bitmap);

	/*	Optimise the plotting of JNG/PNGs
	*/
	c->data.mng.opaque_test_pending = (c->type == CONTENT_PNG) || (c->type == CONTENT_JNG);
	if (c->data.mng.opaque_test_pending)
		bitmap_set_opaque(c->bitmap, false);
	return true;
}


/*	START OF CALLBACKS REQUIRED FOR DISPLAYING
*/


mng_ptr nsmng_getcanvasline(mng_handle mng, mng_uint32 line) {
	struct content *c;

	assert(mng != NULL);

	/*	Get our content back
	*/
	c = (struct content *)mng_get_userdata(mng);
	assert(c != NULL);

	/*	Calculate the address
	*/
	return bitmap_get_buffer(c->bitmap) +
			bitmap_get_rowstride(c->bitmap) * line;
}


/**
 * Get the wall-clock time in milliseconds since some fixed time.
 */

mng_uint32 nsmng_gettickcount(mng_handle mng) {
	static bool start = true;
	static time_t t0;
	struct timeval tv;
	struct timezone tz;

	assert(mng != NULL);

	gettimeofday(&tv, &tz);
	if (start) {
		t0 = tv.tv_sec;
		start = false;
	}

	return (tv.tv_sec - t0) * 1000 + tv.tv_usec / 1000;
}


mng_bool nsmng_refresh(mng_handle mng, mng_uint32 x, mng_uint32 y, mng_uint32 w, mng_uint32 h) {
	union content_msg_data data;
	struct content *c;

	assert(mng != NULL);

	/*	Get our content back
	*/
	c = (struct content *)mng_get_userdata(mng);
	assert(c != NULL);

	/*	Set the minimum redraw area
	*/
	data.redraw.x = x;
	data.redraw.y = y;
	data.redraw.width = w;
	data.redraw.height = h;

	/*	Set the redraw area to the whole canvas to ensure that if we can redraw something
		to trigger animation later then we do
	*/
/*	data.redraw.x = 0;
	data.redraw.y = 0;
	data.redraw.width = c->width;
	data.redraw.height = c->height;
*/
	/*	Always redraw everything
	*/
	data.redraw.full_redraw = true;

	/*	Set the object characteristics
	*/
	data.redraw.object = c;
	data.redraw.object_x = 0;
	data.redraw.object_y = 0;
	data.redraw.object_width = c->width;
	data.redraw.object_height = c->height;

	/* Only attempt to force the redraw if we've been requested to
	 * display the image in the first place (i.e. nsmng_redraw has
	 * been called). This avoids the situation of forcibly redrawing
	 * an image that shouldn't be shown (e.g. if the image is a fallback
	 * for an object that can't be rendered)
	 */
	if (c->data.mng.displayed)
		content_broadcast(c, CONTENT_MSG_REDRAW, data);

	return MNG_TRUE;
}

mng_bool nsmng_settimer(mng_handle mng, mng_uint32 msecs) {
	struct content *c;

	assert(mng != NULL);

	/*	Get our content back
	*/
	c = (struct content *)mng_get_userdata(mng);
	assert(c != NULL);

	/*	Perform the scheduling
	*/
	schedule(msecs / 10, nsmng_animate, c);
	return MNG_TRUE;
}


/*	END OF CALLBACKS REQUIRED FOR DISPLAYING
*/


void nsmng_destroy(struct content *c) {

	assert (c != NULL);

	/*	Cleanup the MNG structure and release the canvas memory
	*/
	schedule_remove(nsmng_animate, c);
	mng_cleanup(&c->data.mng.handle);
	if (c->bitmap)
		bitmap_destroy(c->bitmap);
	free(c->title);
}


bool nsmng_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour)
{
	return nsmng_redraw_tiled(c, x, y, width, height,
			clip_x0, clip_y0, clip_x1, clip_y1,
			scale, background_colour,
			false, false);
}


bool nsmng_redraw_tiled(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour,
		bool repeat_x, bool repeat_y)
{
	bool ret;

	/* mark image as having been requested to display */
	c->data.mng.displayed = true;

	if ((c->bitmap) && (c->data.mng.opaque_test_pending)) {
		bitmap_set_opaque(c->bitmap, bitmap_test_opaque(c->bitmap));
		c->data.mng.opaque_test_pending = false;
	}

	ret = plot.bitmap_tile(x, y, width, height,
			c->bitmap, background_colour,
			repeat_x, repeat_y);

	/*	Check if we need to restart the animation
	*/
	if ((c->data.mng.waiting) && (option_animate_images))
		nsmng_animate(c);

	return ret;
}

/**
 * Animates to the next frame
 */
void nsmng_animate(void *p) {
 	struct content *c;

 	assert(p != NULL);

 	c = (struct content *)p;

 	/*	If we used the last animation we advance, if not we try again later
 	*/
 	if (c->user_list->next == NULL) {
 		c->data.mng.waiting = true;
 	} else {
 		c->data.mng.waiting = false;
 		mng_display_resume(c->data.mng.handle);
		c->data.mng.opaque_test_pending = true;
		if (c->bitmap)
			bitmap_modified(c->bitmap);
 	}
}



/**
 * Broadcasts an error message and returns false
 *
 * \param c the content to broadcast for
 * \return false
 */
bool nsmng_broadcast_error(struct content *c) {
	union content_msg_data msg_data;

	assert(c != NULL);

	if (c->type == CONTENT_MNG) {
		msg_data.error = messages_get("MNGError");
	} else if (c->type == CONTENT_PNG) {
		msg_data.error = messages_get("PNGError");
	} else {
		msg_data.error = messages_get("JNGError");
	}
	content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
	return false;

}

mng_bool nsmng_errorproc(mng_handle mng, mng_int32 code,
	mng_int8 severity,
    mng_chunkid chunktype, mng_uint32 chunkseq,
    mng_int32 extra1, mng_int32 extra2, mng_pchar text)
{
	struct content *c;
	char chunk[5];

	assert(mng != NULL);

	c = (struct content *)mng_get_userdata(mng);
	assert(c != NULL);

	chunk[0] = (char)((chunktype >> 24) & 0xFF);
	chunk[1] = (char)((chunktype >> 16) & 0xFF);
	chunk[2] = (char)((chunktype >>  8) & 0xFF);
	chunk[3] = (char)((chunktype      ) & 0xFF);
	chunk[4] = '\0';

	LOG(("error playing '%s' chunk %s (%d):", c->url, chunk, chunkseq));
	LOG(("code %d severity %d extra1 %d extra2 %d text:'%s'", code,
					severity, extra1, extra2, text));

    return (0);
}


#ifndef MNG_INTERNAL_MEMMNGMT

/**
 * Memory allocation callback for libmng.
 */

mng_ptr nsmng_alloc(mng_size_t n)
{
	return calloc(1, n);
}


/**
 * Memory free callback for libmng.
 */

void nsmng_free(mng_ptr p, mng_size_t n)
{
	free(p);
}

#endif

#endif

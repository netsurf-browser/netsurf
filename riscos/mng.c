/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <swis.h>
#include "libmng/libmng.h"
#include "oslib/colourtrans.h"
#include "oslib/os.h"
#include "oslib/osspriteop.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/mng.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/riscos/wimp.h"
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
static mng_bool nsmng_trace(mng_handle mng, mng_int32 iFunNr, mng_int32 iFuncseq, mng_pchar zFuncname);
static mng_bool nsmng_errorproc(mng_handle mng, mng_int32 code,
	mng_int8 severity, mng_chunkid chunktype, mng_uint32 chunkseq,
	mng_int32 extra1, mng_int32 extra2, mng_pchar text);

bool nsmng_create(struct content *c, const char *params[]) {

	/*	Initialise the library (libmng is compiled with MNG_INTERNAL_MEMMNGMT)
	*/
	c->data.mng.sprite_area = NULL;
	c->data.mng.handle = mng_initialize(c, MNG_NULL, MNG_NULL, MNG_NULL);
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
	return true;
}


/*	START OF CALLBACKS REQUIRED FOR READING
*/


mng_bool nsmng_openstream(mng_handle mng) {
	return MNG_TRUE;
}

mng_bool nsmng_readdata(mng_handle mng, mng_ptr buffer, mng_uint32 size, mng_uint32 *bytesread) {
	struct content *c;

	/*	Get our content back
	*/
	c = (struct content *)mng_get_userdata(mng);

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
	return MNG_TRUE;
}

mng_bool nsmng_processheader(mng_handle mng, mng_uint32 width, mng_uint32 height) {
	struct content *c;
	int sprite_size;
	osspriteop_area *sprite_area;
	osspriteop_header *sprite_header;
	union content_msg_data msg_data;

	/*	This function is called when the header has been read and we know
		the dimensions of the canvas.
	*/
	c = (struct content *)mng_get_userdata(mng);
	sprite_size = width * height * 4 + sizeof(osspriteop_header) + sizeof(osspriteop_area);
	c->data.mng.sprite_area = (osspriteop_area *)malloc(sprite_size);
	if (!(c->data.mng.sprite_area)) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		LOG(("Insufficient memory to create canvas."));
		return MNG_FALSE;
	}

	/*	Initialise the content size
	*/
	c->width = width;
	c->height = height;

	/*	Initialise the sprite area
	*/
	sprite_area = c->data.mng.sprite_area;
	sprite_area->size = sprite_size;
	sprite_area->sprite_count = 1;
	sprite_area->first = sizeof(osspriteop_area);
	sprite_area->used = sprite_size;

	/*	Initialise the sprite header
	*/
	sprite_header = (osspriteop_header *)(sprite_area + 1);
	sprite_header->size = sprite_size - sizeof(osspriteop_area);
	memset(sprite_header->name, 0x00, 12);
	strcpy(sprite_header->name, "mng");
	sprite_header->width = width - 1;
	sprite_header->height = height - 1;
	sprite_header->left_bit = 0;
	sprite_header->right_bit = 31;
	sprite_header->mask = sprite_header->image = sizeof(osspriteop_header);
	sprite_header->mode = (os_mode) 0x301680b5;

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

	LOG(("Converting"));

	/*	Set the title
	*/
	c->title = malloc(100);
	if (c->title) {
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
	}
	c->size += (c->width * c->height * 4) + sizeof(osspriteop_header) + sizeof(osspriteop_area) + 100;
	c->status = CONTENT_STATUS_DONE;


	/*	Start displaying
	*/
	status = mng_display(c->data.mng.handle);
	if ((status != MNG_NOERROR) && (status != MNG_NEEDTIMERWAIT)) {
		LOG(("Unable to start display (%i)", status));
		return nsmng_broadcast_error(c);
	}
	return true;
}


/*	START OF CALLBACKS REQUIRED FOR DISPLAYING
*/


mng_ptr nsmng_getcanvasline(mng_handle mng, mng_uint32 line) {
  	char *base;
	struct content *c;

	/*	Get our content back
	*/
	c = (struct content *)mng_get_userdata(mng);

	/*	Calculate the address
	*/
	base = ((char *) c->data.mng.sprite_area + c->data.mng.sprite_area->first);
	base += sizeof(osspriteop_header);
	return base + (c->width * 4) * line;
}


mng_uint32 nsmng_gettickcount(mng_handle mng) {
	os_t time;

	/*	Get the time in centiseconds and return in milliseconds
	*/
	xos_read_monotonic_time(&time);
	return (time * 10);
}

mng_bool nsmng_refresh(mng_handle mng, mng_uint32 x, mng_uint32 y, mng_uint32 w, mng_uint32 h) {
	union content_msg_data data;
	struct content *c;

	/*	Get our content back
	*/
	c = (struct content *)mng_get_userdata(mng);

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

	content_broadcast(c, CONTENT_MSG_REDRAW, data);
	return MNG_TRUE;
}

mng_bool nsmng_settimer(mng_handle mng, mng_uint32 msecs) {
	struct content *c;

	/*	Get our content back
	*/
	c = (struct content *)mng_get_userdata(mng);

	/*	Perform the scheduling
	*/
	schedule(msecs / 10, nsmng_animate, c);
	return MNG_TRUE;
}


/*	END OF CALLBACKS REQUIRED FOR DISPLAYING
*/


void nsmng_destroy(struct content *c) {
	/*	Cleanup the MNG structure and release the canvas memory
	*/
	schedule_remove(nsmng_animate, c);
	mng_cleanup(&c->data.mng.handle);
	if (c->data.mng.sprite_area) {
		free(c->data.mng.sprite_area);
		c->data.mng.sprite_area = NULL;
	}
	free(c->title);
}


bool nsmng_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale) {
	unsigned int tinct_options;
	unsigned int size;
	os_factors f;
	osspriteop_trans_tab *table;
	_kernel_oserror *e;
	os_error *error;

	/*	If we have a gui_window then we work from there, if not we use the global
		settings as we are drawing a thumbnail.
	*/
	if (ro_gui_current_redraw_gui) {
		tinct_options = (ro_gui_current_redraw_gui->option.filter_sprites?tinct_BILINEAR_FILTER:0) |
				(ro_gui_current_redraw_gui->option.dither_sprites?tinct_DITHER:0);
	} else {
		tinct_options = (option_filter_sprites?tinct_BILINEAR_FILTER:0) |
				(option_dither_sprites?tinct_DITHER:0);
	}

	/*	Tinct currently only handles 32bpp sprites that have an embedded alpha mask. Any
		sprites not matching the required specifications are ignored. See the Tinct
		documentation for further information.
	*/
	if (!print_active) {
		e = _swix(Tinct_PlotScaledAlpha, _INR(2,7),
			((char *) c->data.mng.sprite_area + c->data.mng.sprite_area->first),
			x, y - height,
			width, height,
			tinct_options);
		if (e) {
			LOG(("xtince_plotscaledalpha: 0x%x: %s", e->errnum, e->errmess));
			return false;
		}
	}
	else {
		error = xcolourtrans_generate_table_for_sprite(
			c->data.mng.sprite_area,
			(osspriteop_id)((char*)c->data.mng.sprite_area +
			c->data.mng.sprite_area->first),
			colourtrans_CURRENT_MODE,
			colourtrans_CURRENT_PALETTE,
			0, colourtrans_GIVEN_SPRITE, 0, 0, &size);
		if (error) {
			LOG(("xcolourtrans_generate_table_for_sprite: 0x%x: %s", error->errnum, error->errmess));
			return false;
		}

		table = calloc(size, sizeof(char));

		error = xcolourtrans_generate_table_for_sprite(
			c->data.mng.sprite_area,
			(osspriteop_id)((char*)c->data.mng.sprite_area +
			c->data.mng.sprite_area->first),
			colourtrans_CURRENT_MODE,
			colourtrans_CURRENT_PALETTE,
			table, colourtrans_GIVEN_SPRITE, 0, 0, 0);
		if (error) {
			LOG(("xcolourtrans_generate_table_for_sprite: 0x%x: %s", error->errnum, error->errmess));
			free(table);
			return false;
		}

		f.xmul = width;
		f.ymul = height;
		f.xdiv = c->width * 2;
		f.ydiv = c->height * 2;

		error = xosspriteop_put_sprite_scaled(osspriteop_PTR,
			c->data.mng.sprite_area,
			(osspriteop_id)((char*)c->data.mng.sprite_area +
			c->data.mng.sprite_area->first),
			x, (int)(y - height),
			osspriteop_USE_MASK | osspriteop_USE_PALETTE,
			&f, table);
		if (error) {
			LOG(("xosspriteop_put_sprite_scaled: 0x%x: %s", error->errnum, error->errmess));
			free(table);
			return false;
		}

		free(table);
	}
	/*	Check if we need to restart the animation
	*/
	if (c->data.mng.waiting) nsmng_animate(c);

	return true;
}

/**
 * Animates to the next frame
 */
void nsmng_animate(void *p) {
 	struct content *c = p;

 	/*	If we used the last animation we advance, if not we try again later
 	*/
 	if (c->user_list->next == NULL) {
 		c->data.mng.waiting = true;
 	} else {
 		c->data.mng.waiting = false;
 		mng_display_resume(c->data.mng.handle);
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

mng_bool nsmng_trace(mng_handle mng, mng_int32 iFunNr, mng_int32 iFuncseq, mng_pchar zFuncname)
{
	LOG(("In %s(%d,%d), processing: %p", zFuncname, iFunNr, iFuncseq, mng));
	return MNG_TRUE;
}

mng_bool nsmng_errorproc(mng_handle mng, mng_int32 code,
	mng_int8 severity,
    mng_chunkid chunktype, mng_uint32 chunkseq,
    mng_int32 extra1, mng_int32 extra2, mng_pchar text)
{
	struct content *c;
	char chunk[5];

	c = (struct content *)mng_get_userdata(mng);

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
#endif

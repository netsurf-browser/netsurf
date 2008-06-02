/*
 * Copyright 2008 Rob Kendrick <rjek@netsurf-browser.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "utils/log.h"
#include "image/gifread.h"
#include "gtk/gtk_throbber.h"
#include "gtk/gtk_bitmap.h"

struct nsgtk_throbber *nsgtk_throbber = NULL;

/**
 * Creates the throbber using a PNG for each frame.  The number of frames must
 * be at least two.  The first frame is the inactive frame, others are the
 * active frames.
 *
 * \param  frames  The number of frames.  Must be at least two.
 * \param  ...     Filenames of PNGs containing frames.
 * \return true on success.
 */
bool nsgtk_throbber_initialise_from_png(const int frames, ...)
{
	va_list filenames;
	GError *err = NULL;
	struct nsgtk_throbber *throb;		/**< structure we generate */
	bool errors_when_loading = false;	/**< true if a frame failed */
	
	if (frames < 2) {
		/* we need at least two frames - one for idle, one for active */
		LOG(("Insufficent number of frames in throbber animation!"));
		LOG(("(called with %d frames, where 2 is a minimum.)",
			frames));
		return false;
	}
	
	throb = malloc(sizeof(throb));
	throb->nframes = frames;
	throb->framedata = malloc(sizeof(GdkPixbuf *) * throb->nframes);
	
	va_start(filenames, frames);
	
	for (int i = 0; i < frames; i++) {
		const char *fn = va_arg(filenames, const char *);
		throb->framedata[i] = gdk_pixbuf_new_from_file(fn, &err);
		if (err != NULL) {
			LOG(("Error when loading %s: %s (%d)",
				fn, err->message, err->code));
			throb->framedata[i] = NULL;
			errors_when_loading = true;
		}
	}
	
	va_end(filenames);
	
	if (errors_when_loading == true) {
		for (int i = 0; i < frames; i++) {
			if (throb->framedata[i] != NULL)
				gdk_pixbuf_unref(throb->framedata[i]);
		}

		free(throb);
		
		return false;		
	}
	
	nsgtk_throbber = throb;
	
	return true;
}

/**
 * Creates the throbber using a single GIF, using the first frame as the
 * inactive throbber, and the others for the active animation.  The GIF must
 * therefor have at least two frames.
 *
 * \param  fn Filename of GIF to use.  It must have at least two frames.
 * \return true on success.
 */
bool nsgtk_throbber_initialise_from_gif(const char *fn)
{
	/* disect the GIF provided by filename in *fn into a series of
	 * GdkPixbuf for use later.
	 */
	struct gif_animation *gif;		/**< structure for gifread.c */
	struct nsgtk_throbber *throb;		/**< structure we generate */
	int i;

	FILE *fh = fopen(fn, "rb");

	if (fh == NULL) {
		LOG(("Unable to open throbber image '%s' for reading!", fn));
		return false;
	}

	gif = (struct gif_animation *)malloc(sizeof(struct gif_animation));
	throb = (struct nsgtk_throbber *)malloc(sizeof(struct nsgtk_throbber));

	/* discover the size of the data file. */
	fseek(fh, 0, SEEK_END);
	gif->buffer_size = ftell(fh);
	fseek(fh, 0, SEEK_SET);

	/* allocate a block of sufficient size, and load the data in. */
	gif->gif_data = (unsigned char *)malloc(gif->buffer_size);
	fread(gif->gif_data, gif->buffer_size, 1, fh);
	fclose(fh);

	/* set current position within GIF file to beginning, in order to
	 * signal to gifread that we're brand new.
	 */
	gif->buffer_position = 0;

	/* initialise the gif_animation structure. */
	switch (gif_initialise(gif))
	{
		case GIF_INSUFFICIENT_FRAME_DATA:
		case GIF_FRAME_DATA_ERROR:
		case GIF_INSUFFICIENT_DATA:
		case GIF_DATA_ERROR:
			LOG(("GIF image '%s' appears invalid!", fn));
			free(gif->gif_data);
			free(gif);
			free(throb);
			return false;
			break;
		case GIF_INSUFFICIENT_MEMORY:
			LOG(("Ran out of memory decoding GIF image '%s'!", fn));
			free(gif->gif_data);
			free(gif);
			free(throb);
			return false;
			break;
	}

	throb->nframes = gif->frame_count;

	if (throb->nframes < 2)
	{
		/* we need at least two frames - one for idle, one for active */
		LOG(("Insufficent number of frames in throbber image '%s'!",
			fn));
		LOG(("(GIF contains %d frames, where 2 is a minimum.)",
			throb->nframes));
		free(gif->gif_data);
		free(gif);
		free(throb);
		return false;
	}

	throb->framedata = (GdkPixbuf **)malloc(sizeof(GdkPixbuf *)
						* throb->nframes);

	/* decode each frame in turn, extracting the struct bitmap * for each,
	 * and put that in our array of frames.
	 */
	for (i = 0; i < throb->nframes; i++)
	{
		gif_decode_frame(gif, i);
		throb->framedata[i] = gdk_pixbuf_copy(
				gtk_bitmap_get_primary(gif->frame_image));
	}

	gif_finalise(gif);
	free(gif->gif_data);
	free(gif);

	/* debug code: save out each frame as a PNG to make sure decoding is
	 * working correctly.

	for (i = 0; i < throb->nframes; i++) {
		char fname[20];
		sprintf(fname, "frame%d.png", i);
		gdk_pixbuf_save(throb->framedata[i], fname, "png", NULL, NULL);
	}
  	*/

	nsgtk_throbber = throb;

	return true;
}

void nsgtk_throbber_finalise(void)
{
	int i;

	for (i = 0; i < nsgtk_throbber->nframes; i++)
		gdk_pixbuf_unref(nsgtk_throbber->framedata[i]);

	free(nsgtk_throbber->framedata);
	free(nsgtk_throbber);

	nsgtk_throbber = NULL;
}

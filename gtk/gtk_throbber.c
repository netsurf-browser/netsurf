/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include "utils/log.h"
#include "image/gifread.h"
#include "gtk/gtk_throbber.h"
#include "gtk/gtk_bitmap.h"

struct nsgtk_throbber *nsgtk_throbber = NULL;

bool nsgtk_throbber_initialise(const char *fn)
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

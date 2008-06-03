/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
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
extern "C" {
#include "utils/log.h"
#include "image/gifread.h"
}
#include "beos/beos_throbber.h"
#include "beos/beos_bitmap.h"

struct nsbeos_throbber *nsbeos_throbber = NULL;

bool nsbeos_throbber_initialise(const char *fn)
{
	/* disect the GIF provided by filename in *fn into a series of
	 * BBitmap for use later.
	 */
	struct gif_animation *gif;		/**< structure for gifread.c */
	struct nsbeos_throbber *throb;		/**< structure we generate */
	int i;

	FILE *fh = fopen(fn, "rb");

	if (fh == NULL) {
		LOG(("Unable to open throbber image '%s' for reading!", fn));
		return false;
	}

	gif = (struct gif_animation *)malloc(sizeof(struct gif_animation));
	throb = (struct nsbeos_throbber *)malloc(sizeof(struct nsbeos_throbber));

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

	throb->framedata = (BBitmap **)malloc(sizeof(BBitmap *)
						* throb->nframes);

	/* decode each frame in turn, extracting the struct bitmap * for each,
	 * and put that in our array of frames.
	 */
	for (i = 0; i < throb->nframes; i++)
	{
		gif_decode_frame(gif, i);
		throb->framedata[i] = new BBitmap(
				nsbeos_bitmap_get_primary(gif->frame_image));
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

	nsbeos_throbber = throb;

	return true;
}

void nsbeos_throbber_finalise(void)
{
	int i;

	for (i = 0; i < nsbeos_throbber->nframes; i++)
		delete nsbeos_throbber->framedata[i];

	free(nsbeos_throbber->framedata);
	free(nsbeos_throbber);

	nsbeos_throbber = NULL;
}

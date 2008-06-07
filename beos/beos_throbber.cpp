/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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
#include <string.h>
extern "C" {
#include "utils/log.h"
#include "image/gifread.h"
}
#include "beos/beos_throbber.h"
#include "beos/beos_bitmap.h"

#include <File.h>
#include <TranslationUtils.h>

struct nsbeos_throbber *nsbeos_throbber = NULL;

/**
 * Creates the throbber using a PNG for each frame.  The number of frames must
 * be at least two.  The first frame is the inactive frame, others are the
 * active frames.
 *
 * \param  frames  The number of frames.  Must be at least two.
 * \param  ...     Filenames of PNGs containing frames.
 * \return true on success.
 */
bool nsbeos_throbber_initialise_from_png(const int frames, ...)
{
	va_list filenames;
	status_t err;
	struct nsbeos_throbber *throb;		/**< structure we generate */
	bool errors_when_loading = false;	/**< true if a frame failed */
	
	if (frames < 2) {
		/* we need at least two frames - one for idle, one for active */
		LOG(("Insufficent number of frames in throbber animation!"));
		LOG(("(called with %d frames, where 2 is a minimum.)",
			frames));
		return false;
	}
	
	throb = (struct nsbeos_throbber *)malloc(sizeof(throb));
	throb->nframes = frames;
	throb->framedata = (BBitmap **)malloc(sizeof(BBitmap *) * throb->nframes);
	
	va_start(filenames, frames);
	
	for (int i = 0; i < frames; i++) {
		const char *fn = va_arg(filenames, const char *);
		BFile file(fn, B_READ_ONLY);
		throb->framedata[i] = NULL;
		err = file.InitCheck();
		if (err < B_OK) {
			LOG(("Error when loading %s: %s", fn, strerror(err)));
			errors_when_loading = true;
			continue;
		}
		throb->framedata[i] = BTranslationUtils::GetBitmap(&file);
		if (throb->framedata[i] == NULL) {
			LOG(("Error when loading %s: GetBitmap() returned NULL", fn));
			errors_when_loading = true;
		}
	}
	
	va_end(filenames);
	
	if (errors_when_loading == true) {
		for (int i = 0; i < frames; i++) {
			delete throb->framedata[i];
		}

		free(throb->framedata);
		free(throb);
		
		return false;		
	}
	
	nsbeos_throbber = throb;
	
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
bool nsbeos_throbber_initialise_from_gif(const char *fn)
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

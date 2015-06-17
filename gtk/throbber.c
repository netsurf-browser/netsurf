/*
 * Copyright 2008 Rob Kendrick <rjek@netsurf-browser.org>
 * Copyright 2008 Sean Fox <dyntryx@gmail.com>
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
#include <gtk/gtk.h>
#include <stdint.h>

#include "utils/log.h"

#include "gtk/resources.h"
#include "gtk/throbber.h"

struct nsgtk_throbber *nsgtk_throbber = NULL;

#define THROBBER_FRAMES 9
#define THROBBER_FMT "throbber/throbber%d.png"

/* exported interface documented in gtk/throbber.h */
nserror nsgtk_throbber_init(void)
{
	struct nsgtk_throbber *throb;		/**< structure we generate */
	int frame;
	char resname[] = THROBBER_FMT;
	nserror res = NSERROR_OK;

	throb = malloc(sizeof(*throb));
	if (throb == NULL) {
		return NSERROR_NOMEM;
	}

	throb->framedata = malloc(sizeof(GdkPixbuf *) * THROBBER_FRAMES);
	if (throb->framedata == NULL) {
		free(throb);
		return false;
	}

	for (frame = 0; frame < THROBBER_FRAMES; frame++) {
		snprintf(resname, sizeof(resname), THROBBER_FMT, frame);
		res = nsgdk_pixbuf_new_from_resname(resname,
						    throb->framedata + frame);
		if (res != NSERROR_OK) {
			break;
		}
		LOG("%s",resname);
	}

	if (frame < 1) {
		/* we need at least two frames - one for idle, one for active */
		LOG("Insufficent number of frames (%d) in throbber animation.", frame);
		res = NSERROR_INIT_FAILED;
	}

	throb->nframes = frame;
	nsgtk_throbber = throb;
	return res;


}


void nsgtk_throbber_finalise(void)
{
	int i;

	for (i = 0; i < nsgtk_throbber->nframes; i++)
		g_object_unref(nsgtk_throbber->framedata[i]);

	free(nsgtk_throbber->framedata);
	free(nsgtk_throbber);

	nsgtk_throbber = NULL;
}


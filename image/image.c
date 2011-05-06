/*
 * Copyright 2011 John-Mark Bell <jmb@netsurf-browser.org>
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

#include "image/image.h"

#include "image/bmp.h"
#include "image/gif.h"
#include "image/ico.h"
#include "image/jpeg.h"
#include "image/mng.h"
#include "image/nssprite.h"
#include "image/png.h"
#include "image/rsvg.h"
#include "image/svg.h"
#include "image/webp.h"

/**
 * Initialise image content handlers
 *
 * \return NSERROR_OK on success, appropriate error otherwise.
 */
nserror image_init(void)
{
	nserror error;

	error = nsbmp_init();
	if (error != NSERROR_OK)
		return error;

	error = nsgif_init();
	if (error != NSERROR_OK)
		return error;

	error = nsico_init();
	if (error != NSERROR_OK)
		return error;

	error = nsjpeg_init();
	if (error != NSERROR_OK)
		return error;

	/* Prefer libpng over libmng for pngs */
	error = nsmng_init();
	if (error != NSERROR_OK)
		return error;
	error = nspng_init();
	if (error != NSERROR_OK)
		return error;

	error = nssprite_init();
	if (error != NSERROR_OK)
		return error;

	/* Prefer rsvg over libsvgtiny for svgs */
	error = svg_init();
	if (error != NSERROR_OK)
		return error;
	error = nsrsvg_init();
	if (error != NSERROR_OK)
		return error;

	error = webp_init();
	if (error != NSERROR_OK)
		return error;

	return NSERROR_OK;
}

/**
 * Finalise image content handlers
 */
void image_fini(void)
{
	nsbmp_fini();
	nsgif_fini();
	nsico_fini();
	nsjpeg_fini();
	nsmng_fini();
	nssprite_fini();
	nspng_fini();
	nsrsvg_fini();
	svg_fini();
	webp_fini();
}


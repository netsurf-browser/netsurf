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
#include "oslib/osspriteop.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/mng.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_MNG

bool nsmng_create(struct content *c, const char *params[]) {
	return false;
}


bool nsmng_process_data(struct content *c, char *data, unsigned int size) {
	return true;
}


bool nsmng_convert(struct content *c, int width, int height) {
	return true;
}


void nsmng_destroy(struct content *c) {
}


void nsmng_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale) {
	unsigned int tinct_options;

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
/*	_swix(Tinct_PlotScaledAlpha, _IN(2) | _IN(3) | _IN(4) | _IN(5) | _IN(6) | _IN(7),
			((char *) c->data.mng.sprite_area + c->data.mng.sprite_area->first),
			x, y - height,
			width, height,
			tinct_options);
*/
}
#endif

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <stdbool.h>
#include <string.h>

#include "oslib/osfile.h"

#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/riscos/save_html.h"

void save_as_html(struct content *c, char *path) {

	if (c->type != CONTENT_HTML) {
		return;
	}
	
	xosfile_save_stamped(path, 0xfaf, (byte*)c->data.html.source,
				(byte*)c->data.html.source+c->data.html.length);
}

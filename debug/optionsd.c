/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#include "netsurf/desktop/options.h"
#include <stdio.h>
#include <string.h>
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

struct options OPTIONS;

void options_init(struct options* opt)
{
	opt->http = 0;
	opt->http_proxy = strdup("");
	opt->http_port = 8080;
	opt->use_mouse_gestures = 0;
	opt->allow_text_selection = 1;
	opt->use_riscos_elements = 1;
	opt->show_toolbar = 1;
	opt->show_print_preview = 0;
	opt->theme = strdup("Default");
}


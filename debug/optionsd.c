/**
 * $Id: optionsd.c,v 1.1 2003/06/21 13:18:00 bursa Exp $
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


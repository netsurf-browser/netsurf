/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/render/box.h"
#include "netsurf/render/html.h"
#include "netsurf/utils/log.h"

void html_add_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state)
{
	unsigned int i;
	for (i = 0; i != c->data.html.object_count; i++) {
		if (c->data.html.object[i].content == 0)
			continue;
               	content_add_instance(c->data.html.object[i].content,
				bw, c,
				c->data.html.object[i].box,
				c->data.html.object[i].box->object_params,
				&c->data.html.object[i].box->object_state);
	}
}


void html_reshape_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state)
{
	unsigned int i;
	for (i = 0; i != c->data.html.object_count; i++) {
		if (c->data.html.object[i].content == 0)
			continue;
	        content_reshape_instance(c->data.html.object[i].content,
				bw, c,
				c->data.html.object[i].box,
				c->data.html.object[i].box->object_params,
				&c->data.html.object[i].box->object_state);
	}
}

void html_remove_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state)
{
	unsigned int i;
	for (i = 0; i != c->data.html.object_count; i++) {
		if (c->data.html.object[i].content == 0)
			continue;
               	content_remove_instance(c->data.html.object[i].content,
				bw, c,
				c->data.html.object[i].box,
				c->data.html.object[i].box->object_params,
				&c->data.html.object[i].box->object_state);
	}
}

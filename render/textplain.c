/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

#include "libxml/HTMLparser.h"
#include "netsurf/content/content.h"
#include "netsurf/render/html.h"
#include "netsurf/render/textplain.h"


static const char header[] = "<html><body><pre>";
static const char footer[] = "</pre></body></html>";


bool textplain_create(struct content *c, const char *params[])
{
	if (!html_create(c, params))
		/* html_create() must have broadcast MSG_ERROR already, so we
		 * don't need to. */
		return false;
	htmlParseChunk(c->data.html.parser, header, sizeof(header) - 1, 0);
	return true;
}


bool textplain_convert(struct content *c, int width, int height)
{
	htmlParseChunk(c->data.html.parser, footer, sizeof(footer) - 1, 0);
	c->type = CONTENT_HTML;
	return html_convert(c, width, height);
}

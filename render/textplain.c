/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "libxml/HTMLparser.h"
#include "netsurf/render/html.h"
#include "netsurf/render/textplain.h"
#include "netsurf/utils/log.h"


static const char header[] = "<html><body><pre>";
static const char footer[] = "</pre></body></html>";


void textplain_create(struct content *c)
{
	html_create(c);
	htmlParseChunk(c->data.html.parser, header, sizeof(header) - 1, 0);
}


void textplain_process_data(struct content *c, char *data, unsigned long size)
{
	html_process_data(c, data, size);
}


int textplain_convert(struct content *c, unsigned int width, unsigned int height)
{
	htmlParseChunk(c->data.html.parser, footer, sizeof(footer) - 1, 0);
	c->type = CONTENT_HTML;
	return html_convert(c, width, height);
}


void textplain_revive(struct content *c, unsigned int width, unsigned int height)
{
	assert(0);
}


void textplain_reformat(struct content *c, unsigned int width, unsigned int height)
{
	assert(0);
}


void textplain_destroy(struct content *c)
{
	assert(0);
}

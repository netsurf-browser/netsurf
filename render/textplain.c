/**
 * $Id: textplain.c,v 1.1 2003/02/09 12:58:15 bursa Exp $
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "libxml/HTMLparser.h"
#include "netsurf/render/html.h"
#include "netsurf/render/textplain.h"


static const char header[] = "<html><body><pre>";
static const char footer[] = "</pre></body></html>";


void textplain_create(struct content *c)
{
	html_create(c);
	htmlParseChunk(c->data.html.parser, header, sizeof(header), 0);
}


void textplain_process_data(struct content *c, char *data, unsigned long size)
{
	html_process_data(c, data, size);
}


int textplain_convert(struct content *c, unsigned int width, unsigned int height)
{
	htmlParseChunk(c->data.html.parser, footer, sizeof(footer), 0);
	return html_convert(c, width, height);
}


void textplain_revive(struct content *c, unsigned int width, unsigned int height)
{
	html_revive(c, width, height);
}


void textplain_reformat(struct content *c, unsigned int width, unsigned int height)
{
	html_reformat(c, width, height);
}


void textplain_destroy(struct content *c)
{
	html_destroy(c);
}

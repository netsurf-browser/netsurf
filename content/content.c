/**
 * $Id: content.c,v 1.5 2003/04/05 21:38:06 bursa Exp $
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/render/html.h"
#include "netsurf/render/textplain.h"
#include "netsurf/riscos/jpeg.h"
#include "netsurf/utils/utils.h"


/* mime_map must be in sorted order by mime_type */
struct mime_entry {
	char mime_type[16];
	content_type type;
};
static const struct mime_entry mime_map[] = {
	{"image/jpeg", CONTENT_JPEG},
/*	{"image/png", CONTENT_PNG},*/
	{"text/css", CONTENT_CSS},
	{"text/html", CONTENT_HTML},
	{"text/plain", CONTENT_TEXTPLAIN},
};
#define MIME_MAP_COUNT (sizeof(mime_map) / sizeof(mime_map[0]))

/* handler_map must be ordered as enum content_type */
struct handler_entry {
	void (*create)(struct content *c);
	void (*process_data)(struct content *c, char *data, unsigned long size);
	int (*convert)(struct content *c, unsigned int width, unsigned int height);
	void (*revive)(struct content *c, unsigned int width, unsigned int height);
	void (*reformat)(struct content *c, unsigned int width, unsigned int height);
	void (*destroy)(struct content *c);
};
static const struct handler_entry handler_map[] = {
	{html_create, html_process_data, html_convert, html_revive, html_reformat, html_destroy},
	{textplain_create, textplain_process_data, textplain_convert,
		textplain_revive, textplain_reformat, textplain_destroy},
	{jpeg_create, jpeg_process_data, jpeg_convert, jpeg_revive, jpeg_reformat, jpeg_destroy},
	{css_create, css_process_data, css_convert, css_revive, css_reformat, css_destroy},
/*	{png_create, png_process_data, png_convert, png_revive, png_destroy},*/
};


/**
 * content_lookup -- look up mime type
 */

content_type content_lookup(const char *mime_type)
{
	struct mime_entry *m;
	m = bsearch(mime_type, mime_map, MIME_MAP_COUNT, sizeof(mime_map[0]),
			(int (*)(const void *, const void *)) strcmp);
	if (m == 0)
		return CONTENT_OTHER;
	return m->type;
}


/**
 * content_create -- create a content structure of the specified mime type
 */

struct content * content_create(content_type type, char *url)
{
	struct content *c;
	assert(type < CONTENT_OTHER);
	c = xcalloc(1, sizeof(struct content));
	c->url = xstrdup(url);
	c->type = type;
	c->status = CONTENT_LOADING;
	c->size = sizeof(struct content);
	handler_map[type].create(c);
	return c;
}


/**
 * content_process_data -- process a block source data
 */

void content_process_data(struct content *c, char *data, unsigned long size)
{
	assert(c != 0);
	assert(c->type < CONTENT_OTHER);
	assert(c->status == CONTENT_LOADING);
	handler_map[c->type].process_data(c, data, size);
}


/**
 * content_convert -- all data has arrived, complete the conversion
 */

int content_convert(struct content *c, unsigned long width, unsigned long height)
{
	assert(c != 0);
	assert(c->type < CONTENT_OTHER);
	assert(c->status == CONTENT_LOADING);
	if (handler_map[c->type].convert(c, width, height))
		return 1;
	c->status = CONTENT_READY;
	return 0;
}


/**
 * content_revive -- fix content that has been loaded from the cache
 *   eg. load dependencies, reformat to current width
 */

void content_revive(struct content *c, unsigned long width, unsigned long height)
{
	assert(c != 0);
	assert(c->type < CONTENT_OTHER);
	assert(c->status == CONTENT_READY);
	handler_map[c->type].revive(c, width, height);
}


/**
 * content_reformat -- reformat to new size
 */

void content_reformat(struct content *c, unsigned long width, unsigned long height)
{
	assert(c != 0);
	assert(c->type < CONTENT_OTHER);
	assert(c->status == CONTENT_READY);
	handler_map[c->type].reformat(c, width, height);
}


/**
 * content_destroy -- free content
 */

void content_destroy(struct content *c)
{
	assert(c != 0);
	assert(c->type < CONTENT_OTHER);
	handler_map[c->type].destroy(c);
	xfree(c);
}


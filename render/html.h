/**
 * $Id: html.h,v 1.3 2003/06/17 19:24:21 bursa Exp $
 */

#ifndef _NETSURF_RENDER_HTML_H_
#define _NETSURF_RENDER_HTML_H_

#include "netsurf/content/content.h"

void html_create(struct content *c);
void html_process_data(struct content *c, char *data, unsigned long size);
int html_convert(struct content *c, unsigned int width, unsigned int height);
void html_revive(struct content *c, unsigned int width, unsigned int height);
void html_reformat(struct content *c, unsigned int width, unsigned int height);
void html_destroy(struct content *c);
void html_fetch_object(struct content *c, char *url, struct box *box);

#endif

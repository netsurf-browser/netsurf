/**
 * $Id: textplain.h,v 1.1 2003/02/09 12:58:15 bursa Exp $
 */

#ifndef _NETSURF_RENDER_TEXTPLAIN_H_
#define _NETSURF_RENDER_TEXTPLAIN_H_

#include "netsurf/content/content.h"

void textplain_create(struct content *c);
void textplain_process_data(struct content *c, char *data, unsigned long size);
int textplain_convert(struct content *c, unsigned int width, unsigned int height);
void textplain_revive(struct content *c, unsigned int width, unsigned int height);
void textplain_reformat(struct content *c, unsigned int width, unsigned int height);
void textplain_destroy(struct content *c);

#endif

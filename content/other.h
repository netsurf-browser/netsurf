/**
 * $Id: other.h,v 1.1 2003/06/17 19:24:20 bursa Exp $
 */

#ifndef _NETSURF_RISCOS_OTHER_H_
#define _NETSURF_RISCOS_OTHER_H_

#include "netsurf/content/content.h"

void other_create(struct content *c);
void other_process_data(struct content *c, char *data, unsigned long size);
int other_convert(struct content *c, unsigned int width, unsigned int height);
void other_revive(struct content *c, unsigned int width, unsigned int height);
void other_reformat(struct content *c, unsigned int width, unsigned int height);
void other_destroy(struct content *c);

#endif

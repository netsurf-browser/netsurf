/**
 * $Id: jpeg.h,v 1.2 2003/05/10 11:13:34 bursa Exp $
 */

#ifndef _NETSURF_RISCOS_JPEG_H_
#define _NETSURF_RISCOS_JPEG_H_

#include "netsurf/content/content.h"

void jpeg_create(struct content *c);
void jpeg_process_data(struct content *c, char *data, unsigned long size);
int jpeg_convert(struct content *c, unsigned int width, unsigned int height);
void jpeg_revive(struct content *c, unsigned int width, unsigned int height);
void jpeg_reformat(struct content *c, unsigned int width, unsigned int height);
void jpeg_destroy(struct content *c);
void jpeg_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height);

#endif

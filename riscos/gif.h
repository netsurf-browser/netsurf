/**
 * $Id: gif.h,v 1.1 2003/06/05 13:24:28 philpem Exp $
 */

#ifndef _NETSURF_RISCOS_GIF_H_
#define _NETSURF_RISCOS_GIF_H_

#include "netsurf/content/content.h"

void nsgif_init(void);
void nsgif_create(struct content *c);
void nsgif_process_data(struct content *c, char *data, unsigned long size);
int nsgif_convert(struct content *c, unsigned int width, unsigned int height);
void nsgif_revive(struct content *c, unsigned int width, unsigned int height);
void nsgif_reformat(struct content *c, unsigned int width, unsigned int height);
void nsgif_destroy(struct content *c);
void nsgif_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height);
#endif

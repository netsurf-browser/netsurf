/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Philip Pemberton <philpem@users.sourceforge.net>
 */

#ifndef _NETSURF_RISCOS_GIF_H_
#define _NETSURF_RISCOS_GIF_H_

#include "oslib/osspriteop.h"

struct content;

struct content_gif_data {
	char *data;
	unsigned long length;
	unsigned long buffer_pos;
	osspriteop_area *sprite_area;
	char *sprite_image;
};

void nsgif_init(void);
void nsgif_create(struct content *c, const char *params[]);
void nsgif_process_data(struct content *c, char *data, unsigned long size);
int nsgif_convert(struct content *c, unsigned int width, unsigned int height);
void nsgif_revive(struct content *c, unsigned int width, unsigned int height);
void nsgif_reformat(struct content *c, unsigned int width, unsigned int height);
void nsgif_destroy(struct content *c);
void nsgif_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1);
#endif

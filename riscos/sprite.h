/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#ifndef _NETSURF_RISCOS_SPRITE_H_
#define _NETSURF_RISCOS_SPRITE_H_

#include "oslib/osspriteop.h"

struct content;

struct content_sprite_data {
	void *data;
	unsigned long length;
};

void sprite_init(void);
void sprite_create(struct content *c, const char *params[]);
void sprite_process_data(struct content *c, char *data, unsigned long size);
int sprite_convert(struct content *c, unsigned int width, unsigned int height);
void sprite_revive(struct content *c, unsigned int width, unsigned int height);
void sprite_reformat(struct content *c, unsigned int width, unsigned int height);
void sprite_destroy(struct content *c);
void sprite_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1);
#endif

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#ifndef _NETSURF_RISCOS_DRAW_H_
#define _NETSURF_RISCOS_DRAW_H_

struct content;

struct content_draw_data {
	void *data;
	unsigned long length;
	int x0, y0;
};

void draw_init(void);
void draw_create(struct content *c, const char *params[]);
void draw_process_data(struct content *c, char *data, unsigned long size);
int draw_convert(struct content *c, unsigned int width, unsigned int height);
void draw_revive(struct content *c, unsigned int width, unsigned int height);
void draw_reformat(struct content *c, unsigned int width, unsigned int height);
void draw_destroy(struct content *c);
void draw_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1);
#endif

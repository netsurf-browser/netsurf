/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#ifndef _NETSURF_RENDER_IMAGEMAP_H_
#define _NETSURF_RENDER_IMAGEMAP_H_

#include "libxml/HTMLtree.h"

struct content;

void imagemap_destroy(struct content *c);
void imagemap_dump(struct content *c);
bool imagemap_extract(xmlNode *node, struct content *c);
char *imagemap_get(struct content *c, const char *key, unsigned long x,
              unsigned long y, unsigned long click_x, unsigned long click_y);

#endif

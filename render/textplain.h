/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
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

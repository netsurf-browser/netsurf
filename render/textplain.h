/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

#ifndef _NETSURF_RENDER_TEXTPLAIN_H_
#define _NETSURF_RENDER_TEXTPLAIN_H_

struct content;

bool textplain_create(struct content *c, const char *params[]);
bool textplain_process_data(struct content *c, char *data,
		unsigned int size);
bool textplain_convert(struct content *c, int width, int height);

#endif

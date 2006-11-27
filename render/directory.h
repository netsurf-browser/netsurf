/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

/** \file
 * Content for directory listings (interface).
 *
 * These functions should in general be called via the content interface.
 */

#ifndef _NETSURF_RENDER_DIRECTORY_H_
#define _NETSURF_RENDER_DIRECTORY_H_

#include <stdbool.h>
#include "netsurf/content/content_type.h"


bool directory_create(struct content *c, const char *params[]);
bool directory_convert(struct content *c, int width, int height);

#endif

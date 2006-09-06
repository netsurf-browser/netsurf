/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
 */

#ifndef _NETSURF_RISCOS_QUERY_H
#define _NETSURF_RISCOS_QUERY_H
#include <stdbool.h>
#include "oslib/wimp.h"
#include "netsurf/utils/utils.h"

struct gui_query_window;

void ro_gui_query_init(void);
void ro_gui_query_window_bring_to_front(query_id id);

#endif

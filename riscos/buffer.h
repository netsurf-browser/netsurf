/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Screen buffering (interface).
 */

#ifndef _NETSURF_RISCOS_BUFFER_H_
#define _NETSURF_RISCOS_BUFFER_H_

#include "oslib/wimp.h"

void ro_gui_buffer_open(wimp_draw *redraw);
void ro_gui_buffer_close(void);

#endif

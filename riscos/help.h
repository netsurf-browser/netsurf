/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Interactive help (interface).
 */

#ifndef _NETSURF_RISCOS_HELP_H_
#define _NETSURF_RISCOS_HELP_H_

#include "oslib/wimp.h"

void ro_gui_interactive_help_request(wimp_message *message);
int ro_gui_interactive_help_available(void);

#endif

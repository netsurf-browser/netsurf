/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Interactive help (interface).
 */

#ifndef _NETSURF_RISCOS_HELP_H_
#define _NETSURF_RISCOS_HELP_H_

#include <stdbool.h>
#include "oslib/wimp.h"

void ro_gui_interactive_help_request(wimp_message *message);
bool ro_gui_interactive_help_available(void);
void ro_gui_interactive_help_start(void);

#endif

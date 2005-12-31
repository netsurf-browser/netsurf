/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * RISC OS option setting (interface).
 */


#ifndef _NETSURF_RISCOS_CONFIGURE_H_
#define _NETSURF_RISCOS_CONFIGURE_H_

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "oslib/os.h"
#include "oslib/wimp.h"

void ro_gui_configure_initialise(void);
void ro_gui_configure_show(void);
void ro_gui_configure_register(const char *window,
		bool (*initialise)(wimp_w w), void (*finalise)(wimp_w w));

#endif

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Automated RISC OS WIMP event handling (interface).
 */


#ifndef _NETSURF_RISCOS_OPTIONS_CONFIGURE_H_
#define _NETSURF_RISCOS_OPTIONS_CONFIGURE_H_

#include <stdbool.h>

bool ro_gui_options_cache_initialise(wimp_w w);
bool ro_gui_options_connection_initialise(wimp_w w);
bool ro_gui_options_content_initialise(wimp_w w);
bool ro_gui_options_fonts_initialise(wimp_w w);
bool ro_gui_options_home_initialise(wimp_w w);
bool ro_gui_options_image_initialise(wimp_w w);
void ro_gui_options_image_finalise(wimp_w w);
bool ro_gui_options_interface_initialise(wimp_w w);
bool ro_gui_options_language_initialise(wimp_w w);
bool ro_gui_options_memory_initialise(wimp_w w);
bool ro_gui_options_security_initialise(wimp_w w);
bool ro_gui_options_theme_initialise(wimp_w w);
void ro_gui_options_theme_finalise(wimp_w w);

#endif

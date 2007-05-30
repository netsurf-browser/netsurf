/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

/** \file
 * Progress bar (interface).
 */

#include <stdbool.h>
#include <oslib/osspriteop.h>
#include "oslib/wimp.h"

#ifndef _NETSURF_RISCOS_PROGRESS_BAR_H_
#define _NETSURF_RISCOS_PROGRESS_BAR_H_

struct progress_bar;

void ro_gui_progress_bar_init(osspriteop_area *icons);

struct progress_bar *ro_gui_progress_bar_create(void);
void ro_gui_progress_bar_destroy(struct progress_bar *pb);
void ro_gui_progress_bar_update(struct progress_bar *pb, int width, int height);

wimp_w ro_gui_progress_bar_get_window(struct progress_bar *pb);
void ro_gui_progress_bar_set_icon(struct progress_bar *pb, const char *icon);
void ro_gui_progress_bar_set_value(struct progress_bar *pb, unsigned int value);
unsigned int ro_gui_progress_bar_get_value(struct progress_bar *pb);
void ro_gui_progress_bar_set_range(struct progress_bar *pb, unsigned int range);
unsigned int ro_gui_progress_bar_get_range(struct progress_bar *pb);
#endif

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

/** \file
 * UTF8 status bar (interface).
 */

#include <stdbool.h>

#ifndef _NETSURF_RISCOS_STATUS_BAR_H_
#define _NETSURF_RISCOS_STATUS_BAR_H_

struct status_bar;

struct status_bar *ro_gui_status_bar_create(wimp_w parent, unsigned int width);
void ro_gui_status_bar_destroy(struct status_bar *sb);

wimp_w ro_gui_status_bar_get_window(struct status_bar *sb);
unsigned int ro_gui_status_bar_get_width(struct status_bar *sb);
void ro_gui_status_bar_resize(struct status_bar *sb);
void ro_gui_status_bar_set_visible(struct status_bar *pb, bool visible);
bool ro_gui_status_bar_get_visible(struct status_bar *pb);
void ro_gui_status_bar_set_text(struct status_bar *pb, const char *text);
void ro_gui_status_bar_set_progress_value(struct status_bar *sb,
		unsigned int value);
void ro_gui_status_bar_set_progress_range(struct status_bar *sb,
		unsigned int range);
void ro_gui_status_bar_set_progress_icon(struct status_bar *sb,
		const char *icon);
#endif

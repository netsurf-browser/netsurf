/*
 * Copyright 2006 Adrian Lees <adrianl@users.sourceforge.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Text selection import/export (interface).
 */

#ifndef _NETSURF_RISCOS_TEXTSELECTION_H_
#define _NETSURF_RISCOS_TEXTSELECTION_H_

#include "oslib/wimp.h"
#include "desktop/gui.h"


void ro_gui_selection_drag_end(struct gui_window *g, wimp_dragged *drag);
void ro_gui_selection_claim_entity(wimp_full_message_claim_entity *claim);
void ro_gui_selection_data_request(wimp_full_message_data_request *req);
bool ro_gui_save_clipboard(const char *path);

/* drag-and-drop, receiving */
void ro_gui_selection_dragging(wimp_message *message);
void ro_gui_selection_drag_reset(void);

/* drag-and-drop, sending */
void ro_gui_selection_send_dragging(wimp_pointer *pointer);
void ro_gui_selection_drag_claim(wimp_message *message);

#endif

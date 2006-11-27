/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Adrian Lees <adrianl@users.sourceforge.net>
 */

/** \file
 * Text selection import/export (interface).
 */

#ifndef _NETSURF_RISCOS_TEXTSELECTION_H_
#define _NETSURF_RISCOS_TEXTSELECTION_H_

#include "oslib/wimp.h"
#include "netsurf/desktop/gui.h"


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

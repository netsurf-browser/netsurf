/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Adrian Lees <adrianl@users.sourceforge.net>
 */

/** \file
 * File/object/selection saving (Interface).
 */

#ifndef _NETSURF_RISCOS_SAVE_H_
#define _NETSURF_RISCOS_SAVE_H_

#include "oslib/wimp.h"
#include "netsurf/desktop/gui.h"


wimp_w ro_gui_saveas_create(const char *template_name);
void ro_gui_saveas_quit(void);
void ro_gui_save_prepare(gui_save_type save_type, struct content *c);
void ro_gui_save_start_drag(wimp_pointer *pointer);
void ro_gui_drag_icon(int x, int y, const char *sprite);
void ro_gui_drag_box_cancel(void);
void ro_gui_save_drag_end(wimp_dragged *drag);
void ro_gui_send_datasave(gui_save_type save_type, wimp_full_message_data_xfer *message, wimp_t to);
void ro_gui_save_datasave_ack(wimp_message *message);
bool ro_gui_save_ok(wimp_w w);
void ro_gui_convert_save_path(char *dp, size_t len, const char *p);

#endif

/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

#ifndef _NETSURF_RISCOS_DIALOG_H_
#define _NETSURF_RISCOS_DIALOG_H_

#include <stdbool.h>
#include <stdlib.h>
#include <oslib/wimp.h>
#include "netsurf/riscos/theme.h"


#include "netsurf/riscos/gui.h"



void ro_gui_dialog_init(void);
wimp_w ro_gui_dialog_create(const char *template_name);
wimp_window * ro_gui_dialog_load_template(const char *template_name);

void ro_gui_dialog_open(wimp_w w);
void ro_gui_dialog_close(wimp_w close);

bool ro_gui_dialog_open_top(wimp_w w, struct toolbar *toolbar,
		int width, int height);
void ro_gui_dialog_open_at_pointer(wimp_w w);
void ro_gui_dialog_open_centre_parent(wimp_w parent, wimp_w w);

void ro_gui_dialog_open_persistent(wimp_w parent, wimp_w w, bool pointer);
void ro_gui_dialog_add_persistent(wimp_w parent, wimp_w w);
void ro_gui_dialog_close_persistent(wimp_w parent);




void ro_gui_dialog_click(wimp_pointer *pointer);
void ro_gui_dialog_prepare_zoom(struct gui_window *g);
void ro_gui_dialog_prepare_open_url(void);
void ro_gui_save_options(void);
void ro_gui_dialog_open_config(void);
void ro_gui_dialog_proxyauth_menu_selection(int item);
void ro_gui_dialog_image_menu_selection(int item);
void ro_gui_dialog_languages_menu_selection(const char *lang);
void ro_gui_dialog_font_menu_selection(int item);
#endif

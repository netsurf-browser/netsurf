/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Generic tree handling (interface).
 */

#ifndef _NETSURF_RISCOS_TREEVIEW_H_
#define _NETSURF_RISCOS_TREEVIEW_H_

#include <stdbool.h>
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "netsurf/desktop/tree.h"
#include "netsurf/image/bitmap.h"

#define TREE_TEXT_HEIGHT 40
#define TREE_SPRITE_WIDTH 40	/* text plus sprite entries only */

struct node_sprite {
	osspriteop_area *area;
	char name[12];
	char expanded_name[12];
};

bool ro_gui_tree_initialise(void);
void ro_gui_tree_redraw(wimp_draw *redraw);
bool ro_gui_tree_click(wimp_pointer *pointer, struct tree *tree);
void ro_gui_tree_menu_closed(struct tree *tree);
bool ro_gui_tree_toolbar_click(wimp_pointer* pointer);
void ro_gui_tree_stop_edit(struct tree *tree);
void ro_gui_tree_open(wimp_open *open);
void ro_gui_tree_show(struct tree *tree);
bool ro_gui_tree_keypress(wimp_key *key);
void ro_gui_tree_selection_drag_end(wimp_dragged *drag);
void ro_gui_tree_move_drag_end(wimp_dragged *drag);
void ro_gui_tree_launch_selected(struct tree *tree);
void ro_gui_tree_start_edit(struct tree *tree, struct node_element *element,
		wimp_pointer *pointer);
void ro_gui_tree_scroll_visible(struct tree *tree, struct node_element *element);
void ro_gui_tree_get_tree_coordinates(struct tree *tree, int x, int y,
		int *tree_x, int *tree_y);
int ro_gui_tree_help(int x, int y);
void ro_gui_tree_update_theme(struct tree *tree);

#endif

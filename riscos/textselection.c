/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

#include <string.h>
#include "oslib/wimp.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


void ro_gui_start_selection(wimp_pointer *pointer, wimp_window_state *state,
		struct gui_window *g)
{
	wimp_drag drag;

	gui_current_drag_type = GUI_DRAG_SELECTION;
	current_gui = g;

	drag.type = wimp_DRAG_USER_POINT;
	drag.initial.x0 = pointer->pos.x;
	drag.initial.y0 = pointer->pos.y;
	drag.initial.x1 = pointer->pos.x;
	drag.initial.y1 = pointer->pos.y;
	drag.bbox.x0 = state->visible.x0;
	drag.bbox.y0 = state->visible.y0;
	drag.bbox.x1 = state->visible.x1;
	drag.bbox.y1 = state->visible.y1;
	wimp_drag_box(&drag);
}


void ro_gui_selection_drag_end(wimp_dragged *drag)
{
/*     struct browser_action msg; */
    int final_x0, final_y0;
    wimp_window_state state;

    state.w = current_gui->window;
    wimp_get_window_state(&state);

    final_x0 = window_x_units(drag->final.x0, &state) / 2;
    final_y0 = window_y_units(drag->final.y0, &state) / 2;

/*    msg.data.mouse.x = final_x0;
    msg.data.mouse.y = final_y0;
    msg.type = act_ALTER_SELECTION;
    browser_window_action(current_gui->bw, &msg);*/

/*     if (box_position_eq(&(current_gui->bw->current_content->data.html.text_selection.start), */
/*                         &(current_gui->bw->current_content->data.html.text_selection.end))) */
    {
/*      msg.type = act_CLEAR_SELECTION;
      browser_window_action(current_gui->bw, &msg);*/
    }
/*     current_gui->bw->current_content->data.html.text_selection.altering = alter_UNKNOWN; */
}


void ro_gui_copy_selection(struct gui_window* g)
{
  {
//    if (g->bw->text_selection->selected == 1)
//    {
//    }
  }
}



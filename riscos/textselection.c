/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

#include "oslib/wimp.h"
#include "netsurf/riscos/gui.h"

struct ro_gui_drag_info current_drag;

static void ro_gui_drag_box(wimp_drag* drag, struct ro_gui_drag_info* drag_info);


void ro_gui_start_selection(wimp_pointer *pointer, wimp_window_state *state,
                            gui_window *g) {

  wimp_drag drag;
  struct ro_gui_drag_info drag_info;

  drag.type = wimp_DRAG_USER_POINT;
  drag.initial.x0 = pointer->pos.x;
  drag.initial.y0 = pointer->pos.y;
  drag.initial.x1 = pointer->pos.x;
  drag.initial.y1 = pointer->pos.y;
  drag.bbox.x0 = state->visible.x0;
  drag.bbox.y0 = state->visible.y0;
  drag.bbox.x1 = state->visible.x1;
  drag.bbox.y1 = state->visible.y1;
  drag_info.type = draginfo_BROWSER_TEXT_SELECTION;
  drag_info.data.selection.gui = g;
  ro_gui_drag_box(&drag, &drag_info);

}

void ro_gui_drag_box(wimp_drag* drag, struct ro_gui_drag_info* drag_info)
{
  wimp_drag_box(drag);

  if (drag_info != NULL)
    memcpy(&current_drag, drag_info, sizeof(struct ro_gui_drag_info));
  else
    current_drag.type = draginfo_NONE;
}

void ro_gui_drag_end(wimp_dragged* drag)
{
  if (current_drag.type == draginfo_BROWSER_TEXT_SELECTION)
  {
    struct browser_action msg;
    int final_x0, final_y0;
    wimp_window_state state;

    state.w = current_drag.data.selection.gui->data.browser.window;
    wimp_get_window_state(&state);

    final_x0 = browser_x_units(window_x_units(drag->final.x0, &state));
    final_y0 = browser_y_units(window_y_units(drag->final.y0, &state));

    msg.data.mouse.x = final_x0;
    msg.data.mouse.y = final_y0;
    msg.type = act_ALTER_SELECTION;
    browser_window_action(current_drag.data.selection.gui->data.browser.bw, &msg);

    if (box_position_eq(&(current_drag.data.selection.gui->data.browser.bw->current_content->data.html.text_selection.start),
                        &(current_drag.data.selection.gui->data.browser.bw->current_content->data.html.text_selection.end)))
    {
      msg.type = act_CLEAR_SELECTION;
      browser_window_action(current_drag.data.selection.gui->data.browser.bw, &msg);
    }
    current_drag.data.selection.gui->drag_status = drag_NONE;
    current_drag.data.selection.gui->data.browser.bw->current_content->data.html.text_selection.altering = alter_UNKNOWN;
  }

  current_drag.type = draginfo_NONE;
}

void ro_gui_copy_selection(gui_window* g)
{
  if (g->type == GUI_BROWSER_WINDOW)
  {
//    if (g->data.browser.bw->text_selection->selected == 1)
//    {
//    }
  }
}



/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <stdbool.h>

#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/frames.h"
#include "netsurf/riscos/gui.h"
/*#ifndef TEST
#  define NDEBUG
#endif*/
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#include "oslib/os.h"
#include "oslib/wimp.h"

void frame_add_instance_to_list(struct content *c, struct browser_window *parent, struct content *page, struct box *box, struct object_params *params, void **state, struct browser_window *bw, gui_window *g);
void frame_remove_instance_from_list(struct content *c);
struct frame_list *frame_get_instance_from_list(struct content *c);

void frame_add_instance(struct content *c, struct browser_window *parent,
                struct content *page, struct box *box,
                struct object_params *params, void **state)
{

  wimp_window w;
  struct browser_window *bw = NULL;
  os_error *e;
  gui_window *g = (gui_window*)xcalloc(1, sizeof(gui_window));

  bw = create_browser_window(parent->flags, parent->format_width,
                             parent->format_height, parent);

  w.visible.x0 = 346;
  w.visible.x1 = 370;
  w.visible.y0 = 664;
  w.visible.y1 = 610;
  w.xscroll = w.yscroll = 0;
  w.next = wimp_TOP;
  w.flags = wimp_WINDOW_NEW_FORMAT | wimp_WINDOW_MOVEABLE;
  w.title_fg = wimp_COLOUR_TRANSPARENT;
  w.title_bg = wimp_COLOUR_WHITE;
  w.work_fg = wimp_COLOUR_VERY_LIGHT_GREY;
  w.work_bg = wimp_COLOUR_RED;
  w.scroll_outer = wimp_COLOUR_DARK_GREY;
  w.scroll_inner = wimp_COLOUR_MID_LIGHT_GREY;
  w.highlight_bg = wimp_COLOUR_CREAM;
  w.extra_flags = 0;
  w.extent.x0 = 0;
  w.extent.y0 = -8192;
  w.extent.x1 = 8192;
  w.extent.y1 = 0;
  w.title_flags = wimp_ICON_HCENTRED | wimp_ICON_VCENTRED;
  w.work_flags = wimp_BUTTON_CLICK << wimp_ICON_BUTTON_TYPE_SHIFT;
  w.xmin = 1;
  w.ymin = 0;
  w.icon_count = 0;

  LOG(("Creating frame"));

  e = xwimp_create_window(&w, &g->window);
  if (e) {
    LOG(("%s", e->errmess));
    return;
  }

  g->type = GUI_BROWSER_WINDOW;
  g->data.browser.bw = bw;
  g->data.browser.toolbar = 0;
  g->redraw_safety = SAFE;
  g->data.browser.reformat_pending = false;
  g->data.browser.old_width = 0;

  bw->current_content = c;
  bw->window = g;

  LOG(("Adding to list"));

  frame_add_instance_to_list(c, parent, page, box, params, state, bw, g);

  LOG(("Done"));
}

void frame_remove_instance(struct content *c, struct browser_window *bw,
                struct content *page, struct box *box,
                struct object_params *params, void **state)
{
  struct frame_list *f;

  f = frame_get_instance_from_list(c);

  wimp_close_window(f->g->window);
  wimp_delete_window(f->g->window);
  frame_remove_instance_from_list(c);
}

void frame_reshape_instance(struct content *c, struct browser_window *bw,
                struct content *page, struct box *box,
                struct object_params *params, void **state)
{

  struct frame_list *f;
  unsigned long x, y;
  int x0, y1;
  os_box b;
  wimp_window_state s;

  LOG(("Reshaping frame"));

  f = frame_get_instance_from_list(c);
  if (f == NULL) {
    LOG(("Couldn't find frame"));
    return;
  }

  s.w = bw->window->window;
  wimp_get_window_state(&s);
  LOG(("ParentWindow: [(%d,%d),(%d,%d)]", s.visible.x0, s.visible.y0,
                                          s.visible.x1, s.visible.y1));

  x0 = s.visible.x0 - s.xscroll;
  y1 = s.visible.y1 - s.yscroll;

  LOG(("%d,%d", x0, y1));

  box_coords(box, &x, &y);
  b.x0 = x0 + ((int)x << 1);
  b.y1 = y1 - (((int)y << 1));
  b.x1 = (b.x0 + (box->width << 1));
  b.y0 = (b.y1 - (box->height << 1));

  /*if(b.x1 > (s.visible.x1-s.xscroll)) {
    b.x1 -= 16;
  }*/
  s.w = f->g->window;
  s.visible = b;

  LOG(("Opening frame window : [(%d,%d),(%d,%d)]",b.x0,b.y0,b.x1,b.y1));
  xwimp_open_window_nested((wimp_open*)&s, bw->window->window, 0);
}


static struct frame_list pl = {0, 0, 0, 0, 0, 0, 0, 0, &pl, &pl};
static struct frame_list *plist = &pl;

/**
 * Adds a plugin instance to the list of plugin instances.
 */
void frame_add_instance_to_list(struct content *c, struct browser_window *parent, struct content *page, struct box *box, struct object_params *params, void **state, struct browser_window *bw, gui_window *g) {

         struct frame_list *npl = xcalloc(1, sizeof(*npl));

         npl->c = c;
         npl->parent = parent;
         npl->page = page;
         npl->box = box;
         npl->params = params;
         npl->state = state;
         npl->bw = bw;
         npl->g = g;
         npl->prev = plist->prev;
         npl->next = plist;
         plist->prev->next = npl;
         plist->prev = npl;

         LOG(("Added Frame %p", npl));
}

/**
 * Removes a plugin instance from the list of plugin instances
 */
void frame_remove_instance_from_list(struct content *c) {

         struct frame_list *temp =
                frame_get_instance_from_list(c);
         if(temp != NULL) {

                 LOG(("Removed Frame %p", temp));
                 temp->prev->next = temp->next;
                 temp->next->prev = temp->prev;
                 xfree(temp);
         }
}

/**
 * Retrieves an instance of a plugin from the list of plugin instances
 * returns NULL if no instance is found
 */
struct frame_list *frame_get_instance_from_list(struct content *c) {

         struct frame_list *npl;

         for(npl = plist->next; (npl != plist)
                             && (npl->c != c);
             npl = npl->next)
                ;

         if(npl != plist)
                 return npl;

         return NULL;
}

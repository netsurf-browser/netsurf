/*
 * Copyright 2011 Daniel Silverstone <dsilvers@digital-scurf.org>
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

/* Browser-related callbacks */

#include <stdio.h>


#include "desktop/browser.h"
#include "desktop/gui.h"
#include "utils/ring.h"

#include "monkey/browser.h"

static uint32_t win_ctr = 0;

static struct gui_window *gw_ring = NULL;

struct gui_window *
monkey_find_window_by_num(uint32_t win_num)
{
  struct gui_window *ret = NULL;
  
  RING_ITERATE_START(struct gui_window, gw_ring, c_ring) {
    if (c_ring->win_num == win_num) {
      ret = c_ring;
      RING_ITERATE_STOP(gw_ring, c_ring);
    }
  } RING_ITERATE_END(gw_ring, c_ring);
  
  return ret;
}

struct gui_window *
monkey_find_window_by_content(hlcache_handle *content)
{
  struct gui_window *ret = NULL;
  
  RING_ITERATE_START(struct gui_window, gw_ring, c_ring) {
    if (c_ring->bw->current_content == content) {
      ret = c_ring;
      RING_ITERATE_STOP(gw_ring, c_ring);
    }
  } RING_ITERATE_END(gw_ring, c_ring);
  
  return ret;
}

void
monkey_window_process_reformats(void)
{
  RING_ITERATE_START(struct gui_window, gw_ring, c_ring) {
    if (c_ring == NULL)
      RING_ITERATE_STOP(gw_ring, c_ring);
    if (c_ring->bw->reformat_pending) {
      browser_window_reformat(c_ring->bw,
                              c_ring->width,
                              c_ring->height);
    }
  } RING_ITERATE_END(gw_ring, c_ring);
}


struct gui_window *
gui_create_browser_window(struct browser_window *bw,
                          struct browser_window *clone, bool new_tab)
{
  struct gui_window *ret = calloc(sizeof(*ret), 1);
  if (ret == NULL)
    return NULL;
  
  ret->win_num = win_ctr++;
  ret->bw = bw;
  
  ret->width = 800;
  ret->height = 600;
  
  fprintf(stdout, "BROWSER_WINDOW NEW WIN %u FOR %p CLONE %p NEWTAB %s\n",
          ret->win_num, bw, clone, new_tab ? "TRUE" : "FALSE");
  fprintf(stdout, "BROWSER_WINDOW SIZE WIN %u WIDTH %d HEIGHT %d\n",
          ret->win_num, ret->width, ret->height);
  
  RING_INSERT(gw_ring, ret);
  
  return ret;
}

struct browser_window *
gui_window_get_browser_window(struct gui_window *g)
{
  return g->bw;
}

void
gui_window_destroy(struct gui_window *g)
{
  fprintf(stdout, "BROWSER_WINDOW DESTROY WIN %u\n", g->win_num);
  RING_REMOVE(gw_ring, g);
  free(g);
}

void
gui_window_set_title(struct gui_window *g, const char *title)
{
  fprintf(stdout, "BROWSER_WINDOW TITLE WIN %u STR %s\n", g->win_num, title);
}

void
gui_window_redraw_window(struct gui_window *g)
{
  fprintf(stdout, "BROWSER_WINDOW REDRAW WIN %u\n", g->win_num);
}

void
gui_launch_url(const char *url)
{
  fprintf(stdout, "GENERIC LAUNCH URL %s\n", url);
}

void
gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
                          bool scaled)
{
  fprintf(stdout, "BROWSER_WINDOW GET_DIMENSIONS WIN %u WIDTH %d HEIGHT %d\n",
          g->win_num, g->width, g->height);
  *width = g->width;
  *height = g->height;
}

void
gui_window_new_content(struct gui_window *g)
{
  fprintf(stdout, "BROWSER_WINDOW NEW_CONTENT WIN %u\n", g->win_num);
}

void
gui_window_set_icon(struct gui_window *g, hlcache_handle *icon)
{
  fprintf(stdout, "BROWSER_WINDOW NEW_ICON WIN %u\n", g->win_num);
}

void
gui_window_start_throbber(struct gui_window *g)
{
  fprintf(stdout, "BROWSER_WINDOW START_THROBBER WIN %u\n", g->win_num);
}

void
gui_window_stop_throbber(struct gui_window *g)
{
  fprintf(stdout, "BROWSER_WINDOW STOP_THROBBER WIN %u\n", g->win_num);
}

void
gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
  g->scrollx = sx;
  g->scrolly = sy;
  fprintf(stdout, "BROWSER_WINDOW SET_SCROLL WIN %u X %d Y %d\n", g->win_num, sx, sy);
}

void
gui_window_update_box(struct gui_window *g,
                      const union content_msg_data *data)
{
  fprintf(stdout, "BROWSER_WINDOW UPDATE_BOX WIN %u X %d Y %d WIDTH %d HEIGHT %d\n",
          g->win_num, data->redraw.x, data->redraw.y,
          data->redraw.width, data->redraw.height);
  
}

void
gui_window_update_extent(struct gui_window *g)
{
  if (!g->bw->current_content)
    return;

  fprintf(stdout, "BROWSER_WINDOW UPDATE_EXTENT WIN %u WIDTH %d HEIGHT %d\n", 
          g->win_num,
          content_get_width(g->bw->current_content),
          content_get_height(g->bw->current_content));
}

void
gui_window_set_status(struct gui_window *g, const char *text)
{
  fprintf(stdout, "BROWSER_WINDOW SET_STATUS WIN %u STR %s\n", g->win_num, text);
}

void
gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
  const char *ptr_name = "UNKNOWN";
  
  switch (shape) {
  case GUI_POINTER_POINT:
    ptr_name = "POINT";
    break;
  case GUI_POINTER_CARET:
    ptr_name = "CARET";
    break;
  case GUI_POINTER_UP:
    ptr_name = "UP";
    break;
  case GUI_POINTER_DOWN:
    ptr_name = "DOWN";
    break;
  case GUI_POINTER_LEFT:
    ptr_name = "LEFT";
    break;
  case GUI_POINTER_RIGHT:
    ptr_name = "RIGHT";
    break;
  case GUI_POINTER_LD:
    ptr_name = "LD";
    break;
  case GUI_POINTER_RD:
    ptr_name = "RD";
    break;
  case GUI_POINTER_LU:
    ptr_name = "LU";
    break;
  case GUI_POINTER_RU:
    ptr_name = "RU";
    break;
  case GUI_POINTER_CROSS:
    ptr_name = "CROSS";
    break;
  case GUI_POINTER_MOVE:
    ptr_name = "MOVE";
    break;
  case GUI_POINTER_WAIT:
    ptr_name = "WAIT";
    break;
  case GUI_POINTER_HELP:
    ptr_name = "HELP";
    break;
  case GUI_POINTER_MENU:
    ptr_name = "MENU";
    break;
  case GUI_POINTER_PROGRESS:
    ptr_name = "PROGRESS";
    break;
  case GUI_POINTER_NO_DROP:
    ptr_name = "NO_DROP";
    break;
  case GUI_POINTER_NOT_ALLOWED:
    ptr_name = "NOT_ALLOWED";
    break;
  case GUI_POINTER_DEFAULT:
    ptr_name = "DEFAULT";
    break;
  default:
    break;
  }
  fprintf(stdout, "BROWSER_WINDOW SET_POINTER WIN %u POINTER %s\n", g->win_num, ptr_name);
}

void
gui_window_set_scale(struct gui_window *g, float scale)
{
  fprintf(stdout, "BROWSER_WINDOW SET_SCALE WIN %u SCALE %f\n", g->win_num, scale);
}

void
gui_window_set_url(struct gui_window *g, const char *url)
{
  fprintf(stdout, "BROWSER_WINDOW SET_URL WIN %u URL %s\n", g->win_num, url);
}

void
gui_drag_save_object(gui_save_type type, hlcache_handle *c,
                     struct gui_window *g)
{
  /* Ignore? */
}

bool
gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
  fprintf(stdout, "BROWSER_WINDOW GET_SCROLL WIN %u X %d Y %d\n",
          g->win_num, g->scrollx, g->scrolly);
  *sx = g->scrollx;
  *sy = g->scrolly;
  return true;
}

bool
gui_window_scroll_start(struct gui_window *g)
{
  fprintf(stdout, "BROWSER_WINDOW SCROLL_START WIN %u\n", g->win_num);
  g->scrollx = g->scrolly = 0;
  return true;
}

void
gui_window_position_frame(struct gui_window *g, int x0, int y0,
                          int x1, int y1)
{
  fprintf(stdout, "BROWSER_WINDOW POSITION_FRAME WIN %u X0 %d Y0 %d X1 %d Y1 %d\n",
          g->win_num, x0, y0, x1, y1);
}

bool
gui_window_frame_resize_start(struct gui_window *g)
{
  return true;
}

void
gui_window_set_search_ico(hlcache_handle *ico)
{
}

void
gui_window_scroll_visible(struct gui_window *g, int x0, int y0,
                          int x1, int y1)
{
  fprintf(stdout, "BROWSER_WINDOW SCROLL_VISIBLE WIN %u X0 %d Y0 %d X1 %d Y1 %d\n",
          g->win_num, x0, y0, x1, y1);
}

void
gui_drag_save_selection(struct selection *s, struct gui_window *g)
{
}

void
gui_start_selection(struct gui_window *g)
{
}

void
gui_clear_selection(struct gui_window *g)
{
}

void
gui_paste_from_clipboard(struct gui_window *g, int x, int y)
{
}

bool
gui_empty_clipboard(void)
{
  return true;
}

bool
gui_add_to_clipboard(const char *text, size_t length, bool space)
{
  return true;
}

bool
gui_commit_clipboard(void)
{
  return true;
}

bool
gui_copy_to_clipboard(struct selection *s)
{
  return true;
}

void
gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
  fprintf(stdout, "BROWSER_WINDOW PLACE_CARET WIN %u X %d Y %d HEIGHT %d\n",
          g->win_num, x, y, height);
}

void
gui_window_remove_caret(struct gui_window *g)
{
  fprintf(stdout, "BROWSER_WINDOW REMOVE_CARET WIN %u\n", g->win_num);
}

bool
gui_window_box_scroll_start(struct gui_window *g,
                            int x0, int y0, int x1, int y1)
{
  fprintf(stdout, "BROWSER_WINDOW SCROLL_START WIN %u X0 %d Y0 %d X1 %d Y1 %d\n",
          g->win_num, x0, y0, x1, y1);
  return false;
}

void
gui_create_form_select_menu(struct browser_window *bw,
                            struct form_control *control)
{
  fprintf(stdout, "BROWSER_WINDOW SELECT_MENU WIN %u\n",
          bw->window->win_num);
}

void
gui_window_save_link(struct gui_window *g, const char *url, 
                     const char *title)
{
  fprintf(stdout, "BROWSER_WINDOW SAVE_LINK WIN %u URL %s TITLE %s\n",
          g->win_num, url, title);
}

/**
 * $Id: browser.c,v 1.36 2003/05/10 11:13:34 bursa Exp $
 */

#include "netsurf/content/cache.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/riscos/font.h"
#include "netsurf/render/box.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"
#include "libxml/uri.h"
#include "libxml/debugXML.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>

static void browser_window_start_throbber(struct browser_window* bw);
static void browser_window_text_selection(struct browser_window* bw,
		unsigned long click_x, unsigned long click_y, int click_type);
static void browser_window_clear_text_selection(struct browser_window* bw);
static void browser_window_change_text_selection(struct browser_window* bw, struct box_position* new_start, struct box_position* new_end);
static int redraw_box_list(struct browser_window* bw, struct box* current,
		unsigned long x, unsigned long y, struct box_position* start,
		struct box_position* end, int* plot);
static void browser_window_redraw_boxes(struct browser_window* bw, struct box_position* start, struct box_position* end);
static void browser_window_follow_link(struct browser_window* bw,
		unsigned long click_x, unsigned long click_y, int click_type);
static void browser_window_callback(fetchcache_msg msg, struct content *c,
		void *p, const char *error);
static void clear_radio_gadgets(struct browser_window* bw, struct box* box, struct gui_gadget* group);
static void gui_redraw_gadget2(struct browser_window* bw, struct box* box, struct gui_gadget* g,
		unsigned long x, unsigned long y);
static void browser_window_gadget_select(struct browser_window* bw, struct gui_gadget* g, int item);
static int browser_window_gadget_click(struct browser_window* bw, unsigned long click_x, unsigned long click_y);


void browser_window_start_throbber(struct browser_window* bw)
{
  bw->throbbing = 1;
  gui_window_start_throbber(bw->window);
  return;
}

void browser_window_stop_throbber(struct browser_window* bw)
{
  bw->throbbing = 0;
  gui_window_stop_throbber(bw->window);
}


void browser_window_reformat(struct browser_window* bw)
{
  LOG(("bw = %p", bw));

  assert(bw != 0);
  if (bw->current_content == NULL)
    return;

  if (bw->current_content->title == 0)
    gui_window_set_title(bw->window, bw->url);
  else
    gui_window_set_title(bw->window, bw->current_content->title);
  gui_window_set_extent(bw->window, bw->current_content->width, bw->current_content->height);
  gui_window_set_scroll(bw->window, 0, 0);
  gui_window_redraw_window(bw->window);

  LOG(("done"));
}

/* create a new history item */
struct history* history_create(char* desc, char* url)
{
  struct history* h = xcalloc(1, sizeof(struct history));
  LOG(("desc = %s, url = %s", desc, url));
  h->description = 0;
  if (desc != 0)
	  h->description = xstrdup(desc);
  h->url = xstrdup(url);
  LOG(("return h = %p", h));
  return h;
}

void browser_window_back(struct browser_window* bw)
{
  if (bw->history != NULL)
  {
    if (bw->history->earlier != NULL)
    {
      bw->history = bw->history->earlier;
      browser_window_open_location_historical(bw, bw->history->url);
    }
  }
}

void browser_window_forward(struct browser_window* bw)
{
  if (bw->history != NULL)
  {
    if (bw->history->later != NULL)
    {
      bw->history = bw->history->later;
      browser_window_open_location_historical(bw, bw->history->url);
    }
  }
}

/* remember a new page after the current one. anything remembered after the
   current page is forgotten. */
void history_remember(struct history* current, char* desc, char* url)
{
  struct history* h;
  LOG(("current = %p, desc = %s, url = %s", current, desc, url));
  assert(current != NULL);

  /* forget later history items */
  h = current->later;
  while (h != NULL)
  {
    struct history* hh;
    hh = h;
    h = h->later;

    if (hh->description != NULL)
      xfree(hh->description);
    if (hh->url != NULL)
      xfree(hh->url);

    xfree(hh);
  }

  current->later = history_create(desc, url);
  current->later->earlier = current;

  LOG(("end"));
}


struct browser_window* create_browser_window(int flags, int width, int height)
{
  struct browser_window* bw;
  bw = (struct browser_window*) xcalloc(1, sizeof(struct browser_window));

  bw->flags = flags;
  bw->throbbing = 0;
  bw->format_width = width;
  bw->format_height = height;

  bw->scale.mult = 1;
  bw->scale.div = 1;

  bw->current_content = NULL;
  bw->history = NULL;

  bw->url = NULL;

  bw->window = create_gui_browser_window(bw);

  return bw;
}

void browser_window_set_status(struct browser_window* bw, const char* text)
{
  if (bw->window != NULL)
    gui_window_set_status(bw->window, text);
}

void browser_window_destroy(struct browser_window* bw)
{
  LOG(("bw = %p", bw));
  assert(bw != 0);

  if (bw->current_content != NULL)
    cache_free(bw->current_content);

  if (bw->history != NULL)
  {
    struct history* current;

    while (current->earlier != NULL)
      current = current->earlier;

    while (current != NULL)
    {
      struct history* hh;
      hh = current;
      current = current->later;

      if (hh->description != NULL)
        xfree(hh->description);
      if (hh->url != NULL)
        xfree(hh->url);

      xfree(hh);
    }
  }

  xfree(bw->url);

  gui_window_destroy(bw->window);

  xfree(bw);

  LOG(("end"));
}

void browser_window_open_location_historical(struct browser_window* bw, const char* url)
{
  LOG(("bw = %p, url = %s", bw, url));

  assert(bw != 0 && url != 0);

  browser_window_set_status(bw, "Opening page...");
  browser_window_start_throbber(bw);
  bw->time0 = clock();
  fetchcache(url, 0, browser_window_callback, bw,
		  gui_window_get_width(bw->window), 0,
		  (1 << CONTENT_HTML) | (1 << CONTENT_TEXTPLAIN) |
		  (1 << CONTENT_JPEG) | (1 << CONTENT_PNG));

  LOG(("end"));
}

void browser_window_open_location(struct browser_window* bw, const char* url0)
{
  char *url;
  LOG(("bw = %p, url0 = %s", bw, url0));
  assert(bw != 0 && url0 != 0);
  url = url_join(url0, bw->url);
  browser_window_open_location_historical(bw, url);
  if (bw->history == NULL)
    bw->history = history_create(NULL, url);
  else
  {
    history_remember(bw->history, NULL, url);
    bw->history = bw->history->later;
  }
  xfree(url);
  LOG(("end"));
}

void browser_window_callback(fetchcache_msg msg, struct content *c,
		void *p, const char *error)
{
  struct browser_window* bw = p;
  gui_safety previous_safety;
  char status[40];

  switch (msg)
  {
    case FETCHCACHE_OK:
      {
        struct gui_message gmsg;
        if (bw->url != 0)
          xfree(bw->url);
        bw->url = xstrdup(c->url);

        gmsg.type = msg_SET_URL;
        gmsg.data.set_url.url = bw->url;
        gui_window_message(bw->window, &gmsg);

        previous_safety = gui_window_set_redraw_safety(bw->window, UNSAFE);
        if (bw->current_content != NULL)
        {
          if (bw->current_content->type == CONTENT_HTML)
          {
            int gc;
            for (gc = 0; gc < bw->current_content->data.html.elements.numGadgets; gc++)
            {
              gui_remove_gadget(bw->current_content->data.html.elements.gadgets[gc]);
            }
          }
          cache_free(bw->current_content);
        }
        bw->current_content = c;
        browser_window_reformat(bw);
        gui_window_set_redraw_safety(bw->window, previous_safety);
	if (bw->current_content->status == CONTENT_DONE) {
          sprintf(status, "Page complete (%gs)", ((float) (clock() - bw->time0)) / CLOCKS_PER_SEC);
          browser_window_set_status(bw, status);
          browser_window_stop_throbber(bw);
	} else {
          browser_window_set_status(bw, bw->current_content->status_message);
	}
      }
      break;

    case FETCHCACHE_ERROR:
      browser_window_set_status(bw, error);
      browser_window_stop_throbber(bw);
      break;

    case FETCHCACHE_BADTYPE:
      sprintf(status, "Unknown type '%s'", error);
      browser_window_set_status(bw, status);
      browser_window_stop_throbber(bw);
      break;

    case FETCHCACHE_STATUS:
      browser_window_set_status(bw, error);
      break;

    default:
      assert(0);
  }
}

void clear_radio_gadgets(struct browser_window* bw, struct box* box, struct gui_gadget* group)
{
	struct box* c;
	if (box == NULL)
		return;
	if (box->gadget != 0)
	{
		if (box->gadget->type == GADGET_RADIO && box->gadget->name != 0 && box->gadget != group)
		{
			if (strcmp(box->gadget->name, group->name) == 0)
			{
				if (box->gadget->data.radio.selected)
				{
					box->gadget->data.radio.selected = 0;
					gui_redraw_gadget(bw, box->gadget);
				}
			}
		}
	}
  for (c = box->children; c != 0; c = c->next)
    if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
      clear_radio_gadgets(bw, c, group);

  for (c = box->float_children; c != 0; c = c->next_float)
      clear_radio_gadgets(bw, c, group);
}

void gui_redraw_gadget2(struct browser_window* bw, struct box* box, struct gui_gadget* g,
		unsigned long x, unsigned long y)
{
	struct box* c;

	if (box->gadget == g)
	{
  		gui_window_redraw(bw->window, x + box->x, y + box->y, x + box->x + box->width, y+box->y + box->height);
	}

  for (c = box->children; c != 0; c = c->next)
    if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
      gui_redraw_gadget2(bw, c, g, box->x + x, box->y + y);

  for (c = box->float_children; c != 0; c = c->next_float)
     gui_redraw_gadget2(bw, c, g, box->x + x, box->y + y);
}

void gui_redraw_gadget(struct browser_window* bw, struct gui_gadget* g)
{
	assert(bw->current_content->type == CONTENT_HTML);
	gui_redraw_gadget2(bw, bw->current_content->data.html.layout->children, g, 0, 0);
}

void browser_window_gadget_select(struct browser_window* bw, struct gui_gadget* g, int item)
{
	struct formoption* o;
	int count;

	count = 0;
	o = g->data.select.items;
	while (o != NULL)
	{
		if (g->data.select.multiple == 0)
			o->selected = 0;
		if (count == item)
			o->selected = !(o->selected);
		o = o->next;
		count++;
	}

	gui_redraw_gadget(bw, g);
}

int browser_window_gadget_click(struct browser_window* bw, unsigned long click_x, unsigned long click_y)
{
	struct box_selection* click_boxes;
	int found, plot_index;
	int i;

	found = 0;
	click_boxes = NULL;
	plot_index = 0;

	assert(bw->current_content->type == CONTENT_HTML);
	box_under_area(bw->current_content->data.html.layout->children,
			click_x, click_y, 0, 0, &click_boxes, &found, &plot_index);

	if (found == 0)
		return 0;

	for (i = found - 1; i >= 0; i--)
	{
		if (click_boxes[i].box->type == BOX_INLINE && click_boxes[i].box->gadget != 0)
		{
			struct gui_gadget* g = click_boxes[i].box->gadget;

			/* gadget clicked */
			switch (g->type)
			{
				case GADGET_SELECT:
					gui_gadget_combo(bw, g, click_x, click_y);
					break;
				case GADGET_CHECKBOX:
					g->data.checkbox.selected = !g->data.checkbox.selected;
					gui_redraw_gadget(bw, g);
					break;
				case GADGET_RADIO:
					clear_radio_gadgets(bw, bw->current_content->data.html.layout->children, g);
					g->data.radio.selected = -1;
					gui_redraw_gadget(bw, g);
					break;
				case GADGET_ACTIONBUTTON:
					g->data.actionbutt.pressed = -1;
					gui_redraw_gadget(bw, g);
					break;
				case GADGET_TEXTAREA:
					gui_edit_textarea(bw, g);
					break;
				case GADGET_TEXTBOX:
					gui_edit_textbox(bw, g);
					break;
				case GADGET_HIDDEN:
					break;
			}

			xfree(click_boxes);
			return 1;
		}
	}
	xfree(click_boxes);

	return 0;
}

int browser_window_action(struct browser_window* bw, struct browser_action* act)
{
  switch (act->type)
  {
    case act_MOUSE_AT:
     browser_window_follow_link(bw, act->data.mouse.x, act->data.mouse.y, 0);
      break;
    case act_MOUSE_CLICK:
      return browser_window_gadget_click(bw, act->data.mouse.x, act->data.mouse.y);
      break;
    case act_CLEAR_SELECTION:
     browser_window_text_selection(bw, act->data.mouse.x, act->data.mouse.y, 0);
      break;
    case act_START_NEW_SELECTION:
     browser_window_text_selection(bw, act->data.mouse.x, act->data.mouse.y, 1);
      break;
    case act_ALTER_SELECTION:
     browser_window_text_selection(bw, act->data.mouse.x, act->data.mouse.y, 2);
      break;
    case act_FOLLOW_LINK:
     browser_window_follow_link(bw, act->data.mouse.x, act->data.mouse.y, 1);
      break;
    case act_FOLLOW_LINK_NEW_WINDOW:
     browser_window_follow_link(bw, act->data.mouse.x, act->data.mouse.y, 2);
      break;
    case act_GADGET_SELECT:
      browser_window_gadget_select(bw, act->data.gadget_select.g, act->data.gadget_select.item);
    default:
      break;
  }
  return 0;
}

void box_under_area(struct box* box, unsigned long x, unsigned long y, unsigned long ox, unsigned long oy,
		struct box_selection** found, int* count, int* plot_index)
{
  struct box* c;

  if (box == NULL)
    return;

  *plot_index = *plot_index + 1;

  if (x >= box->x + ox && x <= box->x + ox + box->width &&
      y >= box->y + oy && y <= box->y + oy + box->height)
  {
    *found = xrealloc(*found, sizeof(struct box_selection) * (*count + 1));
    (*found)[*count].box = box;
    (*found)[*count].actual_x = box->x + ox;
    (*found)[*count].actual_y = box->y + oy;
    (*found)[*count].plot_index = *plot_index;
    *count = *count + 1;
  }

  for (c = box->children; c != 0; c = c->next)
    if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
      box_under_area(c, x, y, box->x + ox, box->y + oy, found, count, plot_index);

  for (c = box->float_children; c != 0; c = c->next_float)
    box_under_area(c, x, y, box->x + ox, box->y + oy, found, count, plot_index);

  return;
}

void browser_window_follow_link(struct browser_window* bw,
		unsigned long click_x, unsigned long click_y, int click_type)
{
  struct box_selection* click_boxes;
  int found, plot_index;
  int i;
  int done = 0;

  found = 0;
  click_boxes = NULL;
  plot_index = 0;

  if (bw->current_content->type != CONTENT_HTML)
    return;

  box_under_area(bw->current_content->data.html.layout->children,
                 click_x, click_y, 0, 0, &click_boxes, &found, &plot_index);

  if (found == 0)
    return;

  for (i = found - 1; i >= 0; i--)
  {
    if (click_boxes[i].box->href != NULL)
    {
      if (click_type == 1)
        browser_window_open_location(bw, (char*) click_boxes[i].box->href);
      else if (click_type == 2)
      {
        struct browser_window* bw_new;
        bw_new = create_browser_window(browser_TITLE | browser_TOOLBAR
          | browser_SCROLL_X_ALWAYS | browser_SCROLL_Y_ALWAYS, 640, 480);
        gui_window_show(bw_new->window);
        if (bw->url != NULL)
          bw_new->url = xstrdup(bw->url);
        browser_window_open_location(bw_new, (char*) click_boxes[i].box->href);
      }
      else if (click_type == 0)
      {
        browser_window_set_status(bw, (char*) click_boxes[i].box->href);
        done = 1;
      }
      i = -1;
    }
  }

  if (click_type == 0 && done == 0)
    browser_window_set_status(bw, "");

  free(click_boxes);

  return;
}

void browser_window_text_selection(struct browser_window* bw,
		unsigned long click_x, unsigned long click_y, int click_type)
{
  struct box_selection* click_boxes;
  int found, plot_index;
  int i;

  if (click_type == 0 /* click_CLEAR_SELECTION */ )
  {
    browser_window_clear_text_selection(bw);
    return;
  }

  found = 0;
  click_boxes = NULL;
  plot_index = 0;

  assert(bw->current_content->type == CONTENT_HTML);
  box_under_area(bw->current_content->data.html.layout->children,
                 click_x, click_y, 0, 0, &click_boxes, &found, &plot_index);

  if (found == 0)
    return;

  for (i = found - 1; i >= 0; i--)
  {
    if (click_boxes[i].box->type == BOX_INLINE)
    {
      struct box_position new_pos;
      struct box_position* start;
      struct box_position* end;
      int click_char_offset, click_pixel_offset;

      /* shortcuts */
      start = &(bw->current_content->data.html.text_selection.start);
      end = &(bw->current_content->data.html.text_selection.end);

      if (click_boxes[i].box->font != 0)
      {
      font_position_in_string(click_boxes[i].box->text,
          click_boxes[i].box->font, click_boxes[i].box->length,
          click_x - click_boxes[i].actual_x,
          &click_char_offset, &click_pixel_offset);
      }
      else
      {
        click_char_offset = 0;
	click_pixel_offset = 0;
      }

      new_pos.box = click_boxes[i].box;
      new_pos.actual_box_x = click_boxes[i].actual_x;
      new_pos.actual_box_y = click_boxes[i].actual_y;
      new_pos.plot_index = click_boxes[i].plot_index;
      new_pos.char_offset = click_char_offset;
      new_pos.pixel_offset = click_pixel_offset;

      if (click_type == 1 /* click_START_SELECTION */ )
      {
        /* update both start and end */
        browser_window_clear_text_selection(bw);
        bw->current_content->data.html.text_selection.altering = alter_UNKNOWN;
        bw->current_content->data.html.text_selection.selected = 1;
        memcpy(start, &new_pos, sizeof(struct box_position));
        memcpy(end, &new_pos, sizeof(struct box_position));
        i = -1;
      }
      else if (bw->current_content->data.html.text_selection.selected == 1 &&
               click_type == 2 /* click_ALTER_SELECTION */)
      {
        /* alter selection */

        if (bw->current_content->data.html.text_selection.altering
            != alter_UNKNOWN)
        {
          if (bw->current_content->data.html.text_selection.altering
              == alter_START)
          {
            if (box_position_gt(&new_pos,end))
            {
              bw->current_content->data.html.text_selection.altering
                = alter_END;
              browser_window_change_text_selection(bw, end, &new_pos);
            }
            else
              browser_window_change_text_selection(bw, &new_pos, end);
          }
          else
          {
            if (box_position_lt(&new_pos,start))
            {
              bw->current_content->data.html.text_selection.altering
                = alter_START;
              browser_window_change_text_selection(bw, &new_pos, start);
            }
            else
              browser_window_change_text_selection(bw, start, &new_pos);
          }
          i = -1;
        }
        else
        {
          /* work out whether the start or end is being dragged */

          int click_start_distance = 0;
          int click_end_distance = 0;

          int inside_block = 0;
          int before_start = 0;
          int after_end = 0;

          if (box_position_lt(&new_pos, start))
            before_start = 1;

          if (box_position_gt(&new_pos, end))
            after_end = 1;

          if (!box_position_lt(&new_pos, start)
              && !box_position_gt(&new_pos, end))
            inside_block = 1;

          if (inside_block == 1)
          {
            click_start_distance = box_position_distance(start, &new_pos);
            click_end_distance = box_position_distance(end, &new_pos);
          }

          if (before_start == 1
              || (after_end == 0 && inside_block == 1
                  && click_start_distance < click_end_distance))
          {
            /* alter the start position */
            bw->current_content->data.html.text_selection.altering
              = alter_START;
            browser_window_change_text_selection(bw, &new_pos, end);
            i = -1;
          }
          else if (after_end == 1
                   || (before_start == 0 && inside_block == 1
                       && click_start_distance >= click_end_distance))
          {
            /* alter the end position */
            bw->current_content->data.html.text_selection.altering = alter_END;
            browser_window_change_text_selection(bw, start, &new_pos);
            i = -1;
          }
        }
      }
    }
  }

  free(click_boxes);

  return;
}

void browser_window_clear_text_selection(struct browser_window* bw)
{
  struct box_position* old_start;
  struct box_position* old_end;

  assert(bw->current_content->type == CONTENT_HTML);
  old_start = &(bw->current_content->data.html.text_selection.start);
  old_end = &(bw->current_content->data.html.text_selection.end);

  if (bw->current_content->data.html.text_selection.selected == 1)
  {
    bw->current_content->data.html.text_selection.selected = 0;
    browser_window_redraw_boxes(bw, old_start, old_end);
  }

  bw->current_content->data.html.text_selection.altering = alter_UNKNOWN;
}

void browser_window_change_text_selection(struct browser_window* bw,
  struct box_position* new_start, struct box_position* new_end)
{
  struct box_position start;
  struct box_position end;

  assert(bw->current_content->type == CONTENT_HTML);
  memcpy(&start, &(bw->current_content->data.html.text_selection.start), sizeof(struct box_position));
  memcpy(&end, &(bw->current_content->data.html.text_selection.end), sizeof(struct box_position));

  if (!box_position_eq(new_start, &start))
  {
    if (box_position_lt(new_start, &start))
      browser_window_redraw_boxes(bw, new_start, &start);
    else
      browser_window_redraw_boxes(bw, &start, new_start);
    memcpy(&start, new_start, sizeof(struct box_position));
  }

  if (!box_position_eq(new_end, &end))
  {
    if (box_position_lt(new_end, &end))
      browser_window_redraw_boxes(bw, new_end, &end);
    else
      browser_window_redraw_boxes(bw, &end, new_end);
    memcpy(&end, new_end, sizeof(struct box_position));
  }

  memcpy(&(bw->current_content->data.html.text_selection.start), &start, sizeof(struct box_position));
  memcpy(&(bw->current_content->data.html.text_selection.end), &end, sizeof(struct box_position));

  bw->current_content->data.html.text_selection.selected = 1;
}


int box_position_lt(struct box_position* x, struct box_position* y)
{
  return (x->plot_index < y->plot_index ||
          (x->plot_index == y->plot_index && x->char_offset < y->char_offset));
}

int box_position_gt(struct box_position* x, struct box_position* y)
{
  return (x->plot_index > y->plot_index ||
          (x->plot_index == y->plot_index && x->char_offset > y->char_offset));
}

int box_position_eq(struct box_position* x, struct box_position* y)
{
  return (x->plot_index == y->plot_index && x->char_offset == y->char_offset);
}

int box_position_distance(struct box_position* x, struct box_position* y)
{
  int dx = (y->actual_box_x + y->pixel_offset)
           - (x->actual_box_x + x->pixel_offset);
  int dy = (y->actual_box_y + y->box->height / 2)
           - (x->actual_box_y + x->box->height / 2);
  return dx*dx + dy*dy;
}

unsigned long redraw_min_x = LONG_MAX;
unsigned long redraw_min_y = LONG_MAX;
unsigned long redraw_max_x = 0;
unsigned long redraw_max_y = 0;

int redraw_box_list(struct browser_window* bw, struct box* current,
		unsigned long x, unsigned long y, struct box_position* start,
		struct box_position* end, int* plot)
{

  struct box* c;

  if (current == start->box)
    *plot = 1;

  if (*plot >= 1 && current->type == BOX_INLINE)
  {
    unsigned long minx = x + current->x;
    unsigned long miny = y + current->y;
    unsigned long maxx = x + current->x + current->width;
    unsigned long maxy = y + current->y + current->height;

    if (minx < redraw_min_x)
      redraw_min_x = minx;
    if (miny < redraw_min_y)
      redraw_min_y = miny;
    if (maxx > redraw_max_x)
      redraw_max_x = maxx;
    if (maxy > redraw_max_y)
      redraw_max_y = maxy;

    *plot = 2;
  }

  if (current == end->box)
    return 1;

  for (c = current->children; c != 0; c = c->next)
    if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
      if (redraw_box_list(bw, c, x + current->x, y + current->y,
                          start, end, plot) == 1)
        return 1;

  for (c = current->float_children; c != 0; c = c->next_float)
    if (redraw_box_list(bw, c, x + current->x, y + current->y,
                        start, end, plot) == 1)
      return 1;

  return 0;
}

void browser_window_redraw_boxes(struct browser_window* bw, struct box_position* start, struct box_position* end)
{
  int plot = 0;

  assert(bw->current_content->type == CONTENT_HTML);
  if (box_position_eq(start, end))
    return;

  redraw_min_x = LONG_MAX;
  redraw_min_y = LONG_MAX;
  redraw_max_x = 0;
  redraw_max_y = 0;

  redraw_box_list(bw, bw->current_content->data.html.layout,
    0,0, start, end, &plot);

  if (plot == 2)
    gui_window_redraw(bw->window, redraw_min_x, redraw_min_y,
      redraw_max_x, redraw_max_y);
}



/**
 * $Id: browser.c,v 1.20 2003/01/06 23:53:39 bursa Exp $
 */

#include "netsurf/riscos/font.h"
#include "netsurf/render/box.h"
#include "netsurf/render/layout.h"
#include "netsurf/render/css.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/render/utils.h"
#include "netsurf/desktop/cache.h"
#include "netsurf/utils/log.h"
#include "libxml/uri.h"
#include "libxml/debugXML.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>

void browser_window_text_selection(struct browser_window* bw, int click_x, int click_y, int click_type);
void browser_window_clear_text_selection(struct browser_window* bw);
void browser_window_change_text_selection(struct browser_window* bw, struct box_position* new_start, struct box_position* new_end);
void browser_window_redraw_boxes(struct browser_window* bw, struct box_position* start, struct box_position* end);
void browser_window_follow_link(struct browser_window* bw,
  int click_x, int click_y, int click_type);

void box_under_area(struct box* box, int x, int y, int ox, int oy, struct box_selection** found, int* count, int* plot_index);

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

void content_destroy(struct content* c)
{
  if (c == NULL)
    return;

  switch (c->type)
  {
    case CONTENT_HTML:
      /* free other memory here */
//      xmlFreeParserCtxt(c->data.html.parser);
      LOG(("free parser"));
//      htmlFreeParserCtxt(c->data.html.parser);
      LOG(("free sheet"));
//      xfree(c->data.html.stylesheet);
      LOG(("free style"));
//      xfree(c->data.html.style);
      if (c->data.html.layout != NULL)
      {
        LOG(("box_free box"));
//        box_free(c->data.html.layout);
        LOG(("free box"));
//        xfree(c->data.html.layout);
      }
      LOG(("free font"));
      font_free_set(c->data.html.fonts);
      break;
    default:
      break;
  }

  c->main_fetch = fetch_cancel(c->main_fetch);
  xfree(c);

  return;
}

size_t content_html_receive_data(struct content* c, void* data, size_t size, size_t nmemb)
{
  size_t amount = nmemb;
  int offset = 0;
  size_t numInChunk = 2048 / size;  /* process in 2k chunks */

  if (numInChunk > nmemb)
    numInChunk = nmemb;
  else if (numInChunk <= (size_t)0)
    numInChunk = 1;

  while (amount > 0)
  {
    htmlParseChunk(c->data.html.parser, (char*)data + (offset * size), numInChunk, 0);
    offset += numInChunk;
    amount -= numInChunk;
    if (amount < numInChunk)
      numInChunk = amount;
    gui_multitask();
  }

  return size * nmemb;
}

void set_content_html(struct content* c)
{
  c->type = CONTENT_HTML;
  c->data.html.parser = htmlCreatePushParserCtxt(0, 0, "", 0, 0, XML_CHAR_ENCODING_8859_1);
  c->data.html.document = NULL;
  c->data.html.markup = NULL;
  c->data.html.layout = NULL;
  c->data.html.stylesheet = NULL;
  c->data.html.style = NULL;
  return;
}

void content_html_reformat(struct content* c, int width)
{
  char* file;
  struct css_selector* selector = xcalloc(1, sizeof(struct css_selector));

  LOG(("Starting stuff"));
  if (c->data.html.layout != NULL)
  {
    /* TODO: skip if width is unchanged */
    layout_document(c->data.html.layout->children, (unsigned long)width);
    return;
  }

  LOG(("Setting document to myDoc"));
  c->data.html.document = c->data.html.parser->myDoc;
  xmlDebugDumpDocument(stderr, c->data.html.parser->myDoc);

  /* skip to start of html */
  LOG(("Skipping to html"));
  if (c->data.html.document == NULL)
  {
    LOG(("There is no document!"));
    return;
  }
  for (c->data.html.markup = c->data.html.document->children;
       c->data.html.markup != 0 &&
         c->data.html.markup->type != XML_ELEMENT_NODE;
       c->data.html.markup = c->data.html.markup->next)
    ;

  if (c->data.html.markup == 0)
  {
    LOG(("No markup"));
    return;
  }
  if (strcmp((const char *) c->data.html.markup->name, "html"))
  {
    LOG(("Not html"));
    return;
  }

//  xfree(c->data.html.stylesheet);
//  xfree(c->data.html.style);

  LOG(("Loading CSS"));
  file = load("<NetSurf$Dir>.Resources.CSS");  /*!!! not portable! !!!*/
  c->data.html.stylesheet = css_new_stylesheet();
  LOG(("Parsing stylesheet"));
  css_parse_stylesheet(c->data.html.stylesheet, file);

  LOG(("Copying base style"));
  c->data.html.style = xcalloc(1, sizeof(struct css_style));
  memcpy(c->data.html.style, &css_base_style, sizeof(struct css_style));

  LOG(("Creating box"));
  c->data.html.layout = xcalloc(1, sizeof(struct box));
  c->data.html.layout->type = BOX_BLOCK;
  c->data.html.layout->node = c->data.html.markup;

  c->data.html.fonts = font_new_set();

  LOG(("XML to box"));
  xml_to_box(c->data.html.markup, c->data.html.style, c->data.html.stylesheet, &selector, 0, c->data.html.layout, 0, 0, c->data.html.fonts, 0, 0, 0, 0, &c->data.html.elements);
  box_dump(c->data.html.layout->children, 0);
  LOG(("Layout document"));
  layout_document(c->data.html.layout->children, (unsigned long)width);
  box_dump(c->data.html.layout->children, 0);

  /* can tidy up memory here? */

  return;
}

void browser_window_reformat(struct browser_window* bw)
{
  char status[100];
  clock_t time0, time1;

  LOG(("Entering..."));
  if (bw == NULL)
    return;
  if (bw->current_content == NULL)
    return;

  switch (bw->current_content->type)
  {
    case CONTENT_HTML:
      LOG(("HTML content."));
      browser_window_set_status(bw, "Formatting page...");
      time0 = clock();
      content_html_reformat(bw->current_content, gui_window_get_width(bw->window));
      time1 = clock();
      LOG(("Content reformatted"));
      if (bw->current_content->data.html.layout != NULL)
      {
        LOG(("Setting extent"));
        gui_window_set_extent(bw->window, bw->current_content->data.html.layout->children->width, bw->current_content->data.html.layout->children->height);
        LOG(("Setting scroll"));
        gui_window_set_scroll(bw->window, 0, 0);
        LOG(("Redraw window"));
        gui_window_redraw_window(bw->window);
        LOG(("Complete"));
        sprintf(status, "Format complete (%gs).", ((float) time1 - time0) / CLOCKS_PER_SEC);
        browser_window_set_status(bw, status);
      }
      else
      {
        LOG(("This isn't html"));
        browser_window_set_status(bw, "This is not HTML!");
        cache_free(bw->current_content);
        bw->current_content = NULL;
      }
      break;
    default:
        LOG(("Unknown content type"));
      break;
  }
}

/* create a new history item */
struct history* history_create(char* desc, char* url)
{
  struct history* h = xcalloc(1, sizeof(struct history));
  LOG(("desc = %s, url = %s", desc, url));
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
  bw->future_content = NULL;
  bw->history = NULL;

  bw->url = NULL;
  bw->title = xstrdup("NetSurf");

  bw->window = create_gui_browser_window(bw);

  return bw;
}

void browser_window_set_status(struct browser_window* bw, char* text)
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
  if (bw->future_content != NULL)
    cache_free(bw->future_content);

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
  xfree(bw->title);

  gui_window_destroy(bw->window);

  xfree(bw);

  LOG(("end"));
}

void browser_window_open_location_historical(struct browser_window* bw, char* url)
{
  LOG(("bw = %p, url = %s", bw, url));

  assert(bw != 0 && url != 0);

  if (bw->future_content != NULL)
    cache_free(bw->future_content);

  bw->future_content = cache_get(url);
  if (bw->future_content == 0)
  {
    /* not in cache: start fetch */
    struct fetch_request* req;

    LOG(("not in cache: starting fetch"));

    req = xcalloc(1, sizeof(struct fetch_request));
    req->type = REQUEST_FROM_BROWSER;
    req->requestor.browser = bw;

    bw->future_content = (struct content*) xcalloc(1, sizeof(struct content));
    bw->future_content->main_fetch = create_fetch(url, bw->url, 0, req);

    cache_put(url, bw->future_content, 1000);

    browser_window_start_throbber(bw);
  }
  else
  {
    /* in cache: reformat page and display */
    struct gui_message gmsg;
    gui_safety previous_safety;

    LOG(("in cache: reformatting"));

    browser_window_start_throbber(bw);

    /* TODO: factor out code shared with browser_window_message(), case msg_FETCH_FINISHED */
    if (url != bw->url)  /* reload <=> url == bw->url */
    {
      if (bw->url != NULL)
        xfree(bw->url);
      bw->url = xstrdup(url);
    }

    gmsg.type = msg_SET_URL;
    gmsg.data.set_url.url = bw->url;
    gui_window_message(bw->window, &gmsg);

    previous_safety = gui_window_set_redraw_safety(bw->window, UNSAFE);
    if (bw->current_content != NULL)
      cache_free(bw->current_content);
    bw->current_content = bw->future_content;
    bw->future_content = NULL;
    browser_window_reformat(bw);
    gui_window_set_redraw_safety(bw->window, previous_safety);
    browser_window_stop_throbber(bw);
  }

  LOG(("end"));
}

void browser_window_open_location(struct browser_window* bw, char* url)
{
  LOG(("bw = %p, url = %s", bw, url));
  assert(bw != 0 && url != 0);
  url = url_join(url, bw->url);
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

int browser_window_message(struct browser_window* bw, struct browser_message* msg)
{
  gui_safety previous_safety;

  switch (msg->type)
  {
    case msg_FETCH_SENDING:
      browser_window_set_status(bw, "Sending request...");
      break;

    case msg_FETCH_WAITING:
      browser_window_set_status(bw, "Waiting for reply...");
      break;

    case msg_FETCH_FETCH_INFO:
      browser_window_set_status(bw, "Request received...");
      if (msg->f == bw->future_content->main_fetch)
      {
        switch (msg->data.fetch_info.type)
        {
          case type_HTML:
            set_content_html(bw->future_content);
            break;
          default:
            browser_window_stop_throbber(bw);
            return 1;
        }
      }
      break;

    case msg_FETCH_DATA:
      browser_window_set_status(bw, "Data received...");
      if (msg->f == bw->future_content->main_fetch)
        content_html_receive_data(bw->future_content, msg->data.fetch_data.block, sizeof(char), msg->data.fetch_data.block_size);
      break;

    case msg_FETCH_ABORT:
      browser_window_set_status(bw, "Request failed.");
      if (msg->f == bw->future_content->main_fetch)
      {
        browser_window_stop_throbber(bw);
        bw->future_content->main_fetch = NULL;
        cache_free(bw->future_content);
        bw->future_content = NULL;
      }
      break;

    case msg_FETCH_FINISHED:
      browser_window_set_status(bw, "Request complete.");
      if (msg->f == bw->future_content->main_fetch)
      {
        struct gui_message gmsg;
        if (bw->future_content->main_fetch->location != NULL)
          xfree(bw->url);
        bw->url = xstrdup(bw->future_content->main_fetch->location);

        gmsg.type = msg_SET_URL;
        gmsg.data.set_url.url = bw->url;
        gui_window_message(bw->window, &gmsg);

        htmlParseChunk(bw->future_content->data.html.parser, "", 0, 1);
        bw->future_content->main_fetch = NULL;
        previous_safety = gui_window_set_redraw_safety(bw->window, UNSAFE);
        if (bw->current_content != NULL)
	{
	  int gc;
	  for (gc = 0; gc < bw->current_content->data.html.elements.numGadgets; gc++)
	  {
		  gui_remove_gadget(bw->current_content->data.html.elements.gadgets[gc]);
	  }
          cache_free(bw->current_content);
	}
        bw->current_content = bw->future_content;
        bw->future_content = NULL;
        browser_window_reformat(bw);
        gui_window_set_redraw_safety(bw->window, previous_safety);
        browser_window_stop_throbber(bw);
      }
      break;

    default:
      browser_window_set_status(bw, "???");
      break;
  }

  return 0;
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

void gui_redraw_gadget2(struct browser_window* bw, struct box* box, struct gui_gadget* g, int x, int y)
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

int browser_window_gadget_click(struct browser_window* bw, int click_x, int click_y)
{
	struct box_selection* click_boxes;
	int found, plot_index;
	int i;

	found = 0;
	click_boxes = NULL;
	plot_index = 0;

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
			}

			xfree(click_boxes);
			return 1;
		}
	}
	xfree(click_boxes);
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

void box_under_area(struct box* box, int x, int y, int ox, int oy,
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
  int click_x, int click_y, int click_type)
{
  struct box_selection* click_boxes;
  int found, plot_index;
  int i;
  int done = 0;

  found = 0;
  click_boxes = NULL;
  plot_index = 0;

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
          | browser_SCROLL_X_NONE | browser_SCROLL_Y_ALWAYS, 640, 480);
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
  int click_x, int click_y, int click_type)
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

int redraw_min_x = INT_MAX;
int redraw_min_y = INT_MAX;
int redraw_max_x = INT_MIN;
int redraw_max_y = INT_MIN;

int redraw_box_list(struct browser_window* bw, struct box* current,
    int x, int y, struct box_position* start, struct box_position* end,
    int* plot)
{

  struct box* c;

  if (current == start->box)
    *plot = 1;

  if (*plot >= 1 && current->type == BOX_INLINE)
  {
    int minx = x + current->x;
    int miny = y + current->y;
    int maxx = x + current->x + current->width;
    int maxy = y + current->y + current->height;

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

  if (box_position_eq(start, end))
    return;

  redraw_min_x = INT_MAX;
  redraw_min_y = INT_MAX;
  redraw_max_x = INT_MIN;
  redraw_max_y = INT_MIN;

  redraw_box_list(bw, bw->current_content->data.html.layout,
    0,0, start, end, &plot);

  if (plot == 2)
    gui_window_redraw(bw->window, redraw_min_x, redraw_min_y,
      redraw_max_x, redraw_max_y);
}

char *url_join(const char* new, const char* base)
{
  char* ret;
  int i;

  LOG(("new = %s, base = %s", new, base));

  if (base == 0)
  {
    /* no base, so make an absolute URL */
    ret = xcalloc(strlen(new) + 10, sizeof(char));

    /* check if a scheme is present */
    i = strspn(new, "abcdefghijklmnopqrstuvwxyz");
    if (new[i] == ':')
    {
      strcpy(ret, new);
      i += 3;
    }
    else
    {
      strcpy(ret, "http://");
      strcat(ret, new);
      i = 7;
    }

    /* make server name lower case */
    for (; ret[i] != 0 && ret[i] != '/'; i++)
      ret[i] = tolower(ret[i]);

    xmlNormalizeURIPath(ret + i);

    /* http://www.example.com -> http://www.example.com/ */
    if (ret[i] == 0)
    {
      ret[i] = '/';
      ret[i+1] = 0;
    }
  }
  else
  {
    /* relative url */
    ret = xmlBuildURI(new, base);
  }

  LOG(("ret = %s", ret));
  if (ret == NULL)
  {
    ret = xcalloc(strlen(new) + 10, sizeof(char));
    strcpy(ret, new);
  }
  return ret;
}

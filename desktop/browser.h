/**
 * $Id: browser.h,v 1.1 2002/09/11 14:24:02 monkeyson Exp $
 */

#ifndef _NETSURF_DESKTOP_BROWSER_H_
#define _NETSURF_DESKTOP_BROWSER_H_

#include "libxml/HTMLparser.h"
#include "netsurf/render/css.h"
#include "netsurf/render/box.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/fetch.h"

typedef int browser_window_flags;
#define browser_TOOLBAR             ((browser_window_flags) 1)
#define browser_TITLE               ((browser_window_flags) 2)
#define browser_SCROLL_X_NONE       ((browser_window_flags) 4)
#define browser_SCROLL_X_AUTO       ((browser_window_flags) 8)
#define browser_SCROLL_X_ALWAYS     ((browser_window_flags) 16)
#define browser_SCROLL_Y_NONE       ((browser_window_flags) 32)
#define browser_SCROLL_Y_AUTO       ((browser_window_flags) 64)
#define browser_SCROLL_Y_ALWAYS     ((browser_window_flags) 128)

typedef int action_buttons;
#define act_BUTTON_NORMAL        ((action_buttons) 4)
#define act_BUTTON_ALTERNATIVE   ((action_buttons) 1)
#define act_BUTTON_CONTEXT_MENU  ((action_buttons) 2)



struct box_position
{
  struct box* box;
  int actual_box_x;
  int actual_box_y;
  int plot_index;
  int pixel_offset;
  int char_offset;
};

struct content
{
  enum {CONTENT_UNKNOWN, CONTENT_HTML, CONTENT_IMAGE} type;

  union
  {
    struct
    {
      htmlParserCtxt* parser;
      xmlDoc* document;
      xmlNode* markup;
      struct box* layout;
      struct css_stylesheet* stylesheet;
      struct css_style* style;
      struct {
        struct box_position start;
        struct box_position end;
        enum {alter_UNKNOWN, alter_START, alter_END} altering;
        int selected; /* 0 = unselected, 1 = selected */
      } text_selection;
    } html;
  } data;
  struct fetch* main_fetch;
};


struct history
{
  struct history* earlier;
  struct history* later;
  char* description;
  char* url;
};

struct history* history_create(char* desc, char* url);
void history_remember(struct history* current, char* desc, char* url);


struct browser_window
{
  int format_width;
  int format_height;
  struct { int mult; int div; } scale;

  struct content* current_content;
  struct content* future_content;
  struct history* history;

  char* url;

  browser_window_flags flags;
  char* title;
  gui_window* window;
};


struct browser_message
{
  enum { msg_UNKNOWN,
         msg_FETCH_SENDING, msg_FETCH_WAITING, msg_FETCH_ABORT,
         msg_FETCH_FETCH_INFO, msg_FETCH_DATA, msg_FETCH_FINISHED
       } type;
  struct fetch* f;
  union {
    struct {
      enum { type_UNKNOWN, type_HTML } type; /* should be a MIME type ? */
      int total_size; /* -1 == unknown size */
    } fetch_info;
    struct {
      char* block;
      int block_size;
    } fetch_data;
  } data;
};


struct browser_action
{
  enum { act_UNKNOWN,
         act_MOUSE_AT, act_MOUSE_CLICK, act_START_NEW_SELECTION,
         act_ALTER_SELECTION, act_CLEAR_SELECTION,
         act_FOLLOW_LINK, act_FOLLOW_LINK_NEW_WINDOW
       } type;
  union {
    struct {
      int x;
      int y;
      action_buttons buttons;
    } mouse;
  } data;
};

/* public functions */

struct browser_window* create_browser_window(int flags, int width, int height);
void browser_window_destroy(struct browser_window* bw);
void browser_window_open_location(struct browser_window* bw, char* url);
int browser_window_message(struct browser_window* bw, struct browser_message* msg);
int browser_window_action(struct browser_window* bw, struct browser_action* act);
void browser_window_set_status(struct browser_window* bw, char* text);

int box_position_lt(struct box_position* x, struct box_position* y);
int box_position_gt(struct box_position* x, struct box_position* y);
int box_position_eq(struct box_position* x, struct box_position* y);
int box_position_distance(struct box_position* x, struct box_position* y);


#endif

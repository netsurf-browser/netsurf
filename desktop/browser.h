/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Browser window creation and manipulation (interface).
 */

#ifndef _NETSURF_DESKTOP_BROWSER_H_
#define _NETSURF_DESKTOP_BROWSER_H_

#include <stdbool.h>
#include <time.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/gui.h"

struct box;
struct history;
struct form_successful_control;

/** Browser window data. */
struct browser_window
{
	/** Page currently displayed, or 0. Must have status READY or DONE. */
	struct content *current_content;
	/** Instance state pointer for current_content. */
	void *current_content_state;
	/** Page being loaded, or 0. */
	struct content *loading_content;

	/** Window history structure. */
	struct history *history;

	/** Handler for keyboard input, or 0. */
	void (*caret_callback)(struct browser_window *bw, char key, void *p);
	/** User parameter for caret_callback. */
	void *caret_p;

	/** Platform specific window handle. */
	gui_window *window;

	/** Busy indicator is active. */
	bool throbbing;
	/** Add loading_content to the window history when it loads. */
	bool history_add;
	/** Start time of fetching loading_content. */
	clock_t time0;
};


struct browser_action
{
  enum { act_UNKNOWN,
         act_MOUSE_AT, act_MOUSE_CLICK, act_START_NEW_SELECTION,
         act_ALTER_SELECTION, act_CLEAR_SELECTION,
         act_FOLLOW_LINK, act_FOLLOW_LINK_NEW_WINDOW,
	 act_GADGET_SELECT
       } type;
  union {
    struct {
      unsigned long x;
      unsigned long y;
    } mouse;
    struct {
      struct form_control* g;
      int item;
    } gadget_select;
  } data;
};

struct box_selection
{
  struct content *content;
  struct box* box;
  int actual_x;
  int actual_y;
  int plot_index;
};


void browser_window_create(const char *url, struct browser_window *clone);
void browser_window_go(struct browser_window *bw, const char *url);
void browser_window_go_post(struct browser_window *bw, const char *url,
		char *post_urlenc,
		struct form_successful_control *post_multipart,
		bool history_add);
void browser_window_stop(struct browser_window *bw);
void browser_window_destroy(struct browser_window *bw);

int browser_window_action(struct browser_window* bw, struct browser_action* act);

void box_under_area(struct content *content, struct box* box, unsigned long x, unsigned long y, unsigned long ox, unsigned long oy,
		struct box_selection** found, int* count, int* plot_index);

int box_position_lt(struct box_position* x, struct box_position* y);
int box_position_gt(struct box_position* x, struct box_position* y);
int box_position_eq(struct box_position* x, struct box_position* y);
int box_position_distance(struct box_position* x, struct box_position* y);

void gui_redraw_gadget(struct browser_window* bw, struct form_control* g);

bool browser_window_key_press(struct browser_window *bw, char key);

/* In platform specific history.c. */
struct history *history_create(void);
void history_add(struct history *history, struct content *content);
void history_update(struct history *history, struct content *content);
void history_destroy(struct history *history);
void history_back(struct browser_window *bw, struct history *history);
void history_forward(struct browser_window *bw, struct history *history);

/* In platform specific about.c. */
struct content *about_create(const char *url,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, const char *error),
		void *p1, void *p2, unsigned long width, unsigned long height);

#endif

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

struct box;
struct content;
struct form_control;
struct form_successful_control;
struct gui_window;
struct history;

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
	void (*caret_callback)(struct browser_window *bw,
			unsigned int key, void *p);
	/** User parameter for caret_callback. */
	void *caret_p;

	/** Platform specific window data. */
	struct gui_window *window;

	/** Busy indicator is active. */
	bool throbbing;
	/** Add loading_content to the window history when it loads. */
	bool history_add;
	/** Start time of fetching loading_content. */
	clock_t time0;

	/** Fragment identifier for current_content */
	char *frag_id;
};


typedef enum {
	BROWSER_MOUSE_CLICK_1,
	BROWSER_MOUSE_CLICK_2,
	BROWSER_MOUSE_HOVER,
} browser_mouse_click;


void browser_window_create(const char *url, struct browser_window *clone);
void browser_window_go(struct browser_window *bw, const char *url);
void browser_window_go_post(struct browser_window *bw, const char *url,
		char *post_urlenc,
		struct form_successful_control *post_multipart,
		bool history_add);
void browser_window_stop(struct browser_window *bw);
void browser_window_reload(struct browser_window *bw, bool all);
void browser_window_destroy(struct browser_window *bw);

void browser_window_mouse_click(struct browser_window *bw,
		browser_mouse_click click, int x, int y);
bool browser_window_key_press(struct browser_window *bw, unsigned int key);
void browser_window_form_select(struct browser_window *bw,
		struct form_control *control, int item);

/* In platform specific hotlist.c. */
void hotlist_visited(struct content *content);

/* In platform specific history.c. */
struct history *history_create(void);
void history_add(struct history *history, struct content *content,
		char *frag_id);
void history_update(struct history *history, struct content *content);
void history_destroy(struct history *history);
void history_back(struct browser_window *bw, struct history *history);
void history_forward(struct browser_window *bw, struct history *history);
bool history_back_available(struct history *history);
bool history_forward_available(struct history *history);

#endif

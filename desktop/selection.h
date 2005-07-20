/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
 */

/** \file
  * Text selection within browser windows, platform-independent part (interface)
  */

#ifndef _NETSURF_DESKTOP_SELECTION_H_
#define _NETSURF_DESKTOP_SELECTION_H_

#include "netsurf/desktop/browser.h"
#include "netsurf/render/box.h"


typedef enum {
	DRAG_NONE,
	DRAG_START,
	DRAG_END
} seln_drag_state;


/* this structure should be treated as opaque outside selection.c
   (it's defined here to accelerate selection_defined(s) for reduced
   impact on redraw code) */

struct selection
{
	struct browser_window *bw;
	struct box *root;

	unsigned max_idx;  /* total bytes in text representation */

	unsigned start_idx;  /* offset in bytes within text representation */
	unsigned end_idx;

	bool defined;
	bool last_was_end;

	seln_drag_state drag_state;
};


typedef bool (*seln_traverse_handler)(struct box *b, int offset, size_t length, void *handle);


struct selection *selection_create(struct browser_window *bw);
void selection_destroy(struct selection *s);

void selection_init(struct selection *s, struct box *root);
void selection_reinit(struct selection *s, struct box *root);

/* bool selection_defined(struct selection *s); */
#define selection_defined(s) ((s)->defined)

/* bool selection_dragging(struct selection *s); */
#define selection_dragging(s) ((s)->drag_state != DRAG_NONE)


void selection_clear(struct selection *s, bool redraw);
void selection_select_all(struct selection *s);

void selection_set_start(struct selection *s, struct box *box, int idx);
void selection_set_end(struct selection *s, struct box *box, int idx);

struct box *selection_get_start(struct selection *s, int *pidx);
struct box *selection_get_end(struct selection *s, int *pidx);

bool selection_click(struct selection *s, struct box *box, browser_mouse_state mouse, int dx, int dy);
void selection_track(struct selection *s, struct box *box, browser_mouse_state mouse, int dx, int dy);

void selection_drag_end(struct selection *s, struct box *box,
		browser_mouse_state mouse, int dx, int dy);

bool selection_traverse(struct selection *s, seln_traverse_handler handler, void *handle);

bool selection_highlighted(struct selection *s, struct box *box,
		unsigned *start_idx, unsigned *end_idx);

bool selection_save_text(struct selection *s, const char *path);

#endif

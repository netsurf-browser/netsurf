/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/*
 * Frames are represented as a tree structure. eg:
 *
 *            index.html
 *                |
 *      --------------------
 *     |                   |
 *   nav.html          main.html
 *                         |
 *                --------------------
 *               |         |         |
 *            top.html  mid.html  end.html
 *
 * might represent something like:
 *
 *   ------------------------
 *  | nav.html |  top.html  |
 *  |          |------------|
 *  |          |  mid.html  |
 *  |          |------------|
 *  |          |  end.html  |
 *  -------------------------
 *
 * where the left frame is main.html with three sub frames (top, mid, end)
 * and the entire page is index.html with two sub frames (nav, main)
 */

#ifndef _NETSURF_RISCOS_FRAMES_H_
#define _NETSURF_RISCOS_FRAMES_H_

#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/render/box.h"
#include "netsurf/riscos/gui.h"

struct frame_list {

        struct frame *frame; /**< top most frame (ie root of tree) */
        struct browser_window *bw; /**< main window */
        struct frame_list *next; /**< next in list */
        struct frame_list *prev; /**< previous in list */
};

struct frame {

	struct browser_window *win; /**< window in which this frame appears */
	struct box *box; /**< box in parent window containing this frame */
	struct content *c; /**< content of this frame */
	char *name; /**< name of this frame */
	struct frame *parent; /**< parent frame */
	unsigned int no_children; /**< number of children this frame has */
	struct frame **children; /**< child frames */
};

void frameset_add_to_list(struct browser_window *bw, struct frame *frame);
void frameset_remove_from_list(struct browser_window *bw);
struct frame_list *frameset_get_from_list(struct browser_window *bw);
struct frame *frame_get_from_list(struct browser_window *bw, struct box *b,
                                  bool strict);

void add_frame_to_tree (struct browser_window *pbw, struct box *box,
                        struct browser_window *bw, struct content *c,
                        char *name);
struct frame *get_frame_from_tree(struct frame *root,
                                  struct browser_window *bw,
                                  struct box *b, bool strict);
void delete_tree(struct frame *root);
#endif

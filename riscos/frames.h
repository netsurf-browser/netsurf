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
 *
 * This is hung off the browser window structure.
 */

#ifndef _NETSURF_RISCOS_FRAMES_H_
#define _NETSURF_RISCOS_FRAMES_H_

#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/render/box.h"
#include "netsurf/riscos/gui.h"

#include "oslib/wimp.h"

#ifdef WITH_FRAMES

struct frame_list {

        struct content *c;
        struct browser_window *parent;
        struct content *page;
        struct box *box;
        struct object_params *params;
        void **state;
        struct browser_window *bw;
        gui_window *g;
        struct frame_list *next;
        struct frame_list *prev;
};

void frame_add_instance(struct content *c, struct browser_window *bw,
                struct content *page, struct box *box,
                struct object_params *params, void **state);
void frame_remove_instance(struct content *c, struct browser_window *bw,
                struct content *page, struct box *box,
                struct object_params *params, void **state);
void frame_reshape_instance(struct content *c, struct browser_window *bw,
                struct content *page, struct box *box,
                struct object_params *params, void **state);
#endif
#endif

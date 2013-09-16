/*
 * Copyright 2013 Ole Loots <ole@monochrom.net>
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

#include <inttypes.h>
#include <sys/types.h>
#include <string.h>


#include "assert.h"
#include "cflib.h"

#include "utils/nsoption.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/plotters.h"
#include "desktop/mouse.h"
#include "desktop/treeview.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "atari/gui.h"
#include "atari/plot/plot.h"
#include "atari/misc.h"
#include "atari/gemtk/gemtk.h"
#include "atari/treeview.h"


/**
 * Declare Core Window Callbacks:
 */

void atari_treeview_redraw_request(struct core_window *cw,
								const struct rect *r);
void atari_treeview_update_size(struct core_window *cw, int width, int height);
void atari_treeview_scroll_visible(struct core_window *cw,
								const struct rect *r);
void atari_treeview_get_window_dimensions(struct core_window *cw,
		int *width, int *height);
void atari_treeview_drag_status(struct core_window *cw,
		core_window_drag_status ds);


static struct core_window_callback_table cw_t = {
	.redraw_request = atari_treeview_redraw_request,
	.update_size = atari_treeview_update_size,
	.scroll_visible = atari_treeview_scroll_visible,
	.get_window_dimensions = atari_treeview_get_window_dimensions,
	.drag_status = atari_treeview_drag_status
};


struct atari_treeview_window {
	GUIWIN * window;
	bool disposing;
	bool redraw;
	GRECT rdw_area;
	POINT extent;
	POINT click;
	POINT startdrag;
	struct atari_treeview_callbacks *io;
};

typedef struct atari_treeview_window *ATARI_TREEVIEW;


/* native GUI event handlers: */
static void __CDECL on_mbutton_event(struct atari_treeview_window tvw,
									EVMULT_OUT *ev_out, short msg[8]);
static void __CDECL on_keybd_event(struct atari_treeview_window tvw,
									EVMULT_OUT *ev_out, short msg[8]);
static void __CDECL on_redraw_event(struct atari_treeview_window tvw,
									EVMULT_OUT *ev_out, short msg[8]);

/* GEMTK event sink: */
static short handle_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8])
{

}


struct atari_treeview_window *
atari_treeview_create(GUIWIN *win, struct atari_treeview_callbacks * callbacks,
					uint32_t flags)
{
/*
	init_func();

	sslcert_viewer_init(&cw_t, (struct core_window *)tree,
				ssl_current_session);
*/

	/* allocate the core_window struct: */
	struct atari_treeview_window * cw;

	cw = calloc(sizeof(struct atari_treeview_window), 1);
	if (cw == NULL) {
		LOG(("calloc failed"));
		warn_user(messages_get_errorcode(NSERROR_NOMEM), 0);
		return NULL;
	}

	cw->window = win;
	cw->io = callbacks;

	assert(cw->io);
	assert(cw->io->init);

	nserror err = cw->io->init(cw, &cw_t);
	if (err != NSERROR_OK) {
		free(cw);
		cw = NULL;
	}

	return(cw);
}

void atari_treeview_delete(struct atari_treeview_window * cw)
{
	assert(cw);
	assert(cw->io->fini);

	cw->io->fini(cw);

	free(cw);
}



/**
 * Core Window Callbacks:
 */


/**
 * Request a redraw of the window
 *
 * \param cw		the core window object
 * \param r		rectangle to redraw
 */
void atari_treeview_redraw_request(struct core_window *cw, const struct rect *r)
{
	GRECT area;

	RECT_TO_GRECT(r, &area)

	if (cw != NULL) {
		ATARI_TREEVIEW tv = (ATARI_TREEVIEW) cw;
		if( tv->redraw == false ){
			tv->redraw = true;
			tv->rdw_area.g_x = area.g_x;
			tv->rdw_area.g_y = area.g_y;
			tv->rdw_area.g_w = area.g_w;
			tv->rdw_area.g_h = area.g_h;
		} else {
			/* merge the redraw area to the new area.: */
			int newx1 = area.g_x+area.g_w;
			int newy1 = area.g_y+area.g_h;
			int oldx1 = tv->rdw_area.g_x + tv->rdw_area.g_w;
			int oldy1 = tv->rdw_area.g_y + tv->rdw_area.g_h;
			tv->rdw_area.g_x = MIN(tv->rdw_area.g_x, area.g_x);
			tv->rdw_area.g_y = MIN(tv->rdw_area.g_y, area.g_y);
			tv->rdw_area.g_w = ( oldx1 > newx1 ) ? oldx1 - tv->rdw_area.g_x : newx1 - tv->rdw_area.g_x;
			tv->rdw_area.g_h = ( oldy1 > newy1 ) ? oldy1 - tv->rdw_area.g_y : newy1 - tv->rdw_area.g_y;
		}
		// dbg_grect("atari_treeview_request_redraw", &tv->rdw_area);
	}
}

/**
 * Update the limits of the window
 *
 * \param cw		the core window object
 * \param width		the width in px, or negative if don't care
 * \param height	the height in px, or negative if don't care
 */
void atari_treeview_update_size(struct core_window *cw, int width, int height)
{

}


/**
 * Scroll the window to make area visible
 *
 * \param cw		the core window object
 * \param r		rectangle to make visible
 */
void atari_treeview_scroll_visible(struct core_window *cw, const struct rect *r)
{

}


/**
 * Get window viewport dimensions
 *
 * \param cw		the core window object
 * \param width		to be set to viewport width in px, if non NULL
 * \param height	to be set to viewport height in px, if non NULL
 */
void atari_treeview_get_window_dimensions(struct core_window *cw,
		int *width, int *height)
{

}


/**
 * Inform corewindow owner of drag status
 *
 * \param cw		the core window object
 * \param ds		the current drag status
 */
void atari_treeview_drag_status(struct core_window *cw,
		core_window_drag_status ds)
{

}


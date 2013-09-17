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

enum treeview_area_e {
	TREEVIEW_AREA_WORK = 0,
	TREEVIEW_AREA_TOOLBAR,
	TREEVIEW_AREA_CONTENT
};

/* native GUI event handlers: */
static void __CDECL on_mbutton_event(struct atari_treeview_window *tvw,
									EVMULT_OUT *ev_out, short msg[8]);
static void __CDECL on_keybd_event(struct atari_treeview_window *tvw,
									EVMULT_OUT *ev_out, short msg[8]);
static void __CDECL on_redraw_event(struct atari_treeview_window *tvw,
									EVMULT_OUT *ev_out, short msg[8]);


/**
 * Schedule a redraw of the treeview content
 *
 */
static void atari_treeview_redraw_grect_request(struct core_window *cw,
												GRECT *area)
{
	if (cw != NULL) {
		ATARI_TREEVIEW_PTR tv = (ATARI_TREEVIEW_PTR) cw;
		if( tv->redraw == false ){
			tv->redraw = true;
			tv->rdw_area.g_x = area->g_x;
			tv->rdw_area.g_y = area->g_y;
			tv->rdw_area.g_w = area->g_w;
			tv->rdw_area.g_h = area->g_h;
		} else {
			/* merge the redraw area to the new area.: */
			int newx1 = area->g_x+area->g_w;
			int newy1 = area->g_y+area->g_h;
			int oldx1 = tv->rdw_area.g_x + tv->rdw_area.g_w;
			int oldy1 = tv->rdw_area.g_y + tv->rdw_area.g_h;
			tv->rdw_area.g_x = MIN(tv->rdw_area.g_x, area->g_x);
			tv->rdw_area.g_y = MIN(tv->rdw_area.g_y, area->g_y);
			tv->rdw_area.g_w = ( oldx1 > newx1 ) ? oldx1 - tv->rdw_area.g_x : newx1 - tv->rdw_area.g_x;
			tv->rdw_area.g_h = ( oldy1 > newy1 ) ? oldy1 - tv->rdw_area.g_y : newy1 - tv->rdw_area.g_y;
		}
		// dbg_grect("atari_treeview_request_redraw", &tv->rdw_area);
	}
}


static void atari_treeview_get_grect(ATARI_TREEVIEW_PTR tptr, enum treeview_area_e mode,
									GRECT *dest)
{

	struct atari_treeview_window * tv = tptr;

	if (mode == TREEVIEW_AREA_CONTENT) {
		gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_CONTENT, dest);
	}
	else if (mode == TREEVIEW_AREA_TOOLBAR) {
		gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_TOOLBAR, dest);
	}
}


void atari_treeview_redraw(struct atari_treeview_window *tv)
{
	static FONT_PLOTTER vdi_txt_plotter = NULL;
	FONT_PLOTTER old_txt_plotter;

	VdiHdl plot_vdi_handle = 0;
	long atari_plot_flags = 0;

	/* TODO: do not use the global vdi handle for plot actions! */
	/* TODO: implement getter/setter for the vdi handle */

	if (tv != NULL) {
		if( tv->redraw && ((atari_plot_flags & PLOT_FLAG_OFFSCREEN) == 0) ) {

			plot_vdi_handle = plot_get_vdi_handle();
			long atari_plot_flags = plot_get_flags();
			short todo[4];
			GRECT work;
			short handle = gemtk_wm_get_handle(tv->window);
			struct gemtk_wm_scroll_info_s *slid;

/*
			if (vdi_txt_plotter == NULL) {
				int err = 0;
				VdiHdl vdih = plot_get_vdi_handle();
				vdi_txt_plotter = new_font_plotter(vdih, (char*)"vdi", PLOT_FLAG_TRANS,
													&err);
				if(err) {
					const char * desc = plot_err_str(err);
					die(("Unable to load vdi font plotter %s -> %s", "vdi", desc ));
				}
			}
*/
			gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_CONTENT, &work);
			slid = gemtk_wm_get_scroll_info(tv->window);

			struct redraw_context ctx = {
				.interactive = true,
				.background_images = true,
				.plot = &atari_plotters
			};
			plot_set_dimensions(work.g_x, work.g_y, work.g_w, work.g_h);
			if (plot_lock() == false)
				return;
/*
			if(vdi_txt_plotter != NULL){
				old_txt_plotter = plot_get_text_plotter();
				plot_set_text_plotter(vdi_txt_plotter);
			}
*/
			if( wind_get(handle, WF_FIRSTXYWH,
							&todo[0], &todo[1], &todo[2], &todo[3] )!=0 ) {
				while (todo[2] && todo[3]) {

					short pxy[4];
					pxy[0] = todo[0];
					pxy[1] = todo[1];
					pxy[2] = todo[0] + todo[2]-1;
					pxy[3] = todo[1] + todo[3]-1;
					vs_clip(plot_vdi_handle, 1, (short*)&pxy);

					/* convert screen to treeview coords: */
					todo[0] = todo[0] - work.g_x + slid->x_pos*slid->x_unit_px;
					todo[1] = todo[1] - work.g_y + slid->y_pos*slid->y_unit_px;
					if( todo[0] < 0 ){
						todo[2] = todo[2] + todo[0];
						todo[0] = 0;
					}
					if( todo[1] < 0 ){
						todo[3] = todo[3] + todo[1];
						todo[1] = 0;
					}

					if (rc_intersect((GRECT *)&tv->rdw_area,(GRECT *)&todo)) {
						tv->io->draw(tv, -(slid->x_pos*slid->x_unit_px),
										-(slid->y_pos*slid->y_unit_px),
							todo[0], todo[1], todo[2], todo[3], &ctx);
					}
					vs_clip(plot_vdi_handle, 0, (short*)&pxy);
					if (wind_get(handle, WF_NEXTXYWH,
							&todo[0], &todo[1], &todo[2], &todo[3])==0) {
						break;
					}
				}
			} else {
				/*
				plot_set_text_plotter(old_txt_plotter);
				*/
				plot_unlock();
				return;
			}
			/*
			plot_set_text_plotter(old_txt_plotter);
			*/
			plot_unlock();
			tv->redraw = false;
			tv->rdw_area.g_x = 65000;
			tv->rdw_area.g_y = 65000;
			tv->rdw_area.g_w = -1;
			tv->rdw_area.g_h = -1;
		} else {
			/* just copy stuff from the offscreen buffer */
		}
	}
}


/**
 * GEMTK event sink
 *
*/
static short handle_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8])
{
	ATARI_TREEVIEW_PTR tv = (ATARI_TREEVIEW_PTR) gemtk_wm_get_user_data(win);

    if( (ev_out->emo_events & MU_MESAG) != 0 ) {
        // handle message
        switch (msg[0]) {

        case WM_REDRAW:
			on_redraw_event(tv, ev_out, msg);
            break;

        default:
            break;
        }
    }
    if( (ev_out->emo_events & MU_KEYBD) != 0 ) {
        on_keybd_event(tv, ev_out, msg);
    }
    if( (ev_out->emo_events & MU_BUTTON) != 0 ) {
        LOG(("Treeview click at: %d,%d\n", ev_out->emo_mouse.p_x,
             ev_out->emo_mouse.p_y));
        on_mbutton_event(tv, ev_out, msg);
    }

	if (tv) {

	}
/*
    if(tv != NULL && tv->user_func != NULL){
		tv->user_func(win, ev_out, msg);
    }
*/
    return(0);
}


static void __CDECL on_keybd_event(ATARI_TREEVIEW_PTR tptr, EVMULT_OUT *ev_out,
									short msg[8])
{
	bool r=false;
	long kstate = 0;
	long kcode = 0;
	long ucs4;
	long ik;
	unsigned short nkc = 0;
	unsigned short nks = 0;
	unsigned char ascii;
	struct atari_treeview_window * tv = tptr;

	kstate = ev_out->emo_kmeta;
	kcode = ev_out->emo_kreturn;
	nkc= gem_to_norm( (short)kstate, (short)kcode );
	ascii = (nkc & 0xFF);
	ik = nkc_to_input_key(nkc, &ucs4);

	if (ik == 0) {
		if (ascii >= 9) {
			tv->io->keypress(tv, ucs4);
            //r = tree_keypress(tv->tree, ucs4);
		}
	} else {
		tv->io->keypress(tv, ik);
	}
}


static void __CDECL on_redraw_event(ATARI_TREEVIEW_PTR tptr, EVMULT_OUT *ev_out,
									short msg[8])
{
	GRECT work, clip;
	struct gemtk_wm_scroll_info_s *slid;
	struct atari_treeview_window * tv = tptr;

	if (tv == NULL)
		return;

	gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_CONTENT, &work);
	slid = gemtk_wm_get_scroll_info(tv->window);

	clip = work;
	if ( !rc_intersect( (GRECT*)&msg[4], &clip ) ) return;
	clip.g_x -= work.g_x;
	clip.g_y -= work.g_y;
	if( clip.g_x < 0 ) {
		clip.g_w = work.g_w + clip.g_x;
		clip.g_x = 0;
	}
	if( clip.g_y < 0 ) {
		clip.g_h = work.g_h + clip.g_y;
		clip.g_y = 0;
	}
	if( clip.g_h > 0 && clip.g_w > 0 ) {

		GRECT rdrw_area;

		rdrw_area.g_x = (slid->x_pos*slid->x_unit_px) + clip.g_x;
		rdrw_area.g_y =(slid->y_pos*slid->y_unit_px) + clip.g_y;
		rdrw_area.g_w = clip.g_w;
		rdrw_area.g_h = clip.g_h;

		atari_treeview_redraw_grect_request(tptr, &rdrw_area);
	}
}

static void __CDECL on_mbutton_event(ATARI_TREEVIEW_PTR tptr, EVMULT_OUT *ev_out,
									short msg[8])
{
	struct atari_treeview_window * tv = tptr;
	struct gemtk_wm_scroll_info_s *slid;
	GRECT work;
	short mx, my;
	int bms;
	bool ignore=false;
	short cur_rel_x, cur_rel_y, dummy, mbut;

	if(tv == NULL)
		return;

	gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_CONTENT, &work);
	slid = gemtk_wm_get_scroll_info(tv->window);
	mx = ev_out->emo_mouse.p_x;
	my = ev_out->emo_mouse.p_y;

	/* mouse click relative origin: */

	short origin_rel_x = (mx-work.g_x) +
							(slid->x_pos*slid->x_unit_px);
	short origin_rel_y = (my-work.g_y) +
							(slid->y_pos*slid->y_unit_px);

	/* Only pass on events in the content area: */
	if( origin_rel_x >= 0 && origin_rel_y >= 0
		&& mx < work.g_x + work.g_w
		&& my < work.g_y + work.g_h )
	{
		if (ev_out->emo_mclicks == 2) {
			tv->io->mouse_action(tv,
								BROWSER_MOUSE_CLICK_1|BROWSER_MOUSE_DOUBLE_CLICK,
								origin_rel_x, origin_rel_y);
			return;
		}

		graf_mkstate(&cur_rel_x, &cur_rel_y, &mbut, &dummy);
		/* check for click or hold: */
		if( (mbut&1) == 0 ){
			bms = BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_PRESS_1;
			if(ev_out->emo_mclicks == 2 ) {
				bms = BROWSER_MOUSE_DOUBLE_CLICK;
			}
			tv->io->mouse_action(tv, bms, origin_rel_x, origin_rel_y);
		} else {
			/* button still pressed */
			short prev_x = origin_rel_x;
			short prev_y = origin_rel_y;

			cur_rel_x = origin_rel_x;
			cur_rel_y = origin_rel_y;

			gem_set_cursor(&gem_cursors.hand);

			tv->startdrag.x = origin_rel_x;
			tv->startdrag.y = origin_rel_y;

			tv->io->mouse_action(tv, BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_DRAG_ON,
								cur_rel_x, cur_rel_y);
			do{
				if (abs(prev_x-cur_rel_x) > 5 || abs(prev_y-cur_rel_y) > 5) {
					tv->io->mouse_action(tv,
								BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_DRAG_ON,
								cur_rel_x, cur_rel_y);
					prev_x = cur_rel_x;
					prev_y = cur_rel_y;
				}

				if (tv->redraw) {
					// TODO: maybe GUI poll would fit better here?
					// 		 ... is gui_poll re-entrance save?
					atari_treeview_redraw(tv);
				}

				/* sample mouse button state: */
				graf_mkstate(&cur_rel_x, &cur_rel_y, &mbut, &dummy);
				cur_rel_x = (cur_rel_x-work.g_x)+(slid->x_pos*slid->x_unit_px);
				cur_rel_y = (cur_rel_y-work.g_y)+(slid->y_pos*slid->y_unit_px);
			} while( mbut & 1 );

			/* End drag: */
			tv->io->mouse_action(tv, BROWSER_MOUSE_HOVER, cur_rel_x, cur_rel_y);
			gem_set_cursor(&gem_cursors.arrow);
		}
	}
}


struct atari_treeview_window *
atari_treeview_create(GUIWIN *win, struct atari_treeview_callbacks * callbacks,
					uint32_t flags)
{

	/* allocate the core_window struct: */
	struct atari_treeview_window * cw;
	struct gemtk_wm_scroll_info_s *slid;

	cw = calloc(sizeof(struct atari_treeview_window), 1);
	if (cw == NULL) {
		LOG(("calloc failed"));
		warn_user(messages_get_errorcode(NSERROR_NOMEM), 0);
		return NULL;
	}

	/* Store the window ref inside the new treeview: */
	cw->window = win;
	cw->io = callbacks;

	// Setup gemtk event handler function:
	gemtk_wm_set_event_handler(win, handle_event);

	// bind window user data to treeview ref:
	gemtk_wm_set_user_data(win, (void*)cw);

	// Get acces to the gemtk scroll info struct:
	slid = gemtk_wm_get_scroll_info(cw->window);

	// Setup line and column height/width of the window,
	// each scroll takes the configured steps:
	slid->y_unit_px = 16;
	slid->x_unit_px = 16;

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

	atari_treeview_redraw_grect_request(cw, &area);
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


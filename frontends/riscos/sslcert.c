/*
 * Copyright 2006 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2016 Vincent Sanders <vince@netsurf-browser.org>
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

/**
 * \file
 * Implementation of RISC OS certificate verification UI.
 */

#include <oslib/wimp.h>

#include "utils/log.h"
#include "netsurf/plotters.h"
#include "desktop/sslcert_viewer.h"

#include "riscos/dialog.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/wimputils.h"
#include "riscos/gui.h"
#include "riscos/toolbar.h"
#include "riscos/corewindow.h"
#include "riscos/sslcert.h"

/* widget ID */
#define ICON_SSL_PANE 1
#define ICON_SSL_REJECT 3
#define ICON_SSL_ACCEPT 4

/**
 * RISC OS certificate viewer context.
 */
struct ro_cert_window {
	struct ro_corewindow core;

	/** certificate view window handle */
	wimp_w wh;

	/** SSL certificate viewer context data */
	struct sslcert_session_data *ssl_data;

};

/** riscos dialog template for certificate viewer window. */
static wimp_window *dialog_cert_template;

/** riscos template for certificate tree pane. */
static wimp_window *cert_tree_template;


/**
 * Handle closing of the RISC OS certificate verification dialog
 *
 * Deleting wimp windows, freeing up the core window and ssl data block.
 *
 * \param certw The context associated with the dialogue.
 */
static void ro_gui_cert_release_window(struct ro_cert_window *certw)
{
	os_error *error;

	ro_gui_wimp_event_finalise(certw->wh);

	sslcert_viewer_fini(certw->ssl_data);

	ro_corewindow_fini(&certw->core);

	error = xwimp_delete_window(certw->wh);
	if (error) {
		NSLOG(netsurf, INFO, "xwimp_delete_window: 0x%x:%s",
		      error->errnum, error->errmess);
	}

	error = xwimp_delete_window(certw->core.wh);
	if (error) {
		NSLOG(netsurf, INFO, "xwimp_delete_window: 0x%x:%s",
		      error->errnum, error->errmess);
	}

	free(certw);
}

/**
 * Handle acceptance of certificate via event callback.
 *
 * \param pointer The wimp pointer event.
 */
static void ro_gui_cert_accept(wimp_pointer *pointer)
{
	struct ro_cert_window *certw;
	certw = (struct ro_cert_window *)ro_gui_wimp_event_get_user_data(pointer->w);

	sslcert_viewer_accept(certw->ssl_data);
	ro_gui_dialog_close(certw->wh);
	ro_gui_cert_release_window(certw);
}


/**
 * Handle rejection of certificate via event callback.
 *
 * \param pointer The wimp pointer block.
 */
static void ro_gui_cert_reject(wimp_pointer *pointer)
{
	struct ro_cert_window *certw;
	certw = (struct ro_cert_window *)ro_gui_wimp_event_get_user_data(pointer->w);

	sslcert_viewer_reject(certw->ssl_data);
	ro_gui_dialog_close(certw->wh);
	ro_gui_cert_release_window(certw);
}


/**
 * Callback to handle the closure of the SSL dialogue by other means.
 *
 * \param w The window handle being closed.
 */
static void ro_gui_cert_close_window(wimp_w w)
{
	struct ro_cert_window *certw;
	certw = (struct ro_cert_window *)ro_gui_wimp_event_get_user_data(w);

	ro_gui_cert_release_window(certw);
}


/**
 * Attach tree window as a pane to ssl window.
 *
 * Nest the tree window inside the pane window.  To do this, we:
 * - Get the current pane extent,
 * - Get the parent window position and the location of the pane-
 *   locating icon inside it,
 * - Set the visible area of the pane to suit,
 * - Check that the pane extents are OK for this visible area, and
 *   increase them if necessary,
 * - Before finally opening the pane as a nested part of the parent.
 *
 */
static nserror cert_attach_pane(wimp_w parent, wimp_w pane)
{
	os_error *error;
	wimp_window_state wstate;
	wimp_window_info winfo;
	wimp_icon_state	istate;
	bool set_extent;

	winfo.w = pane;
	error = xwimp_get_window_info_header_only(&winfo);
	if (error) {
		NSLOG(netsurf, INFO, "xwimp_get_window_info: 0x%x: %s",
		      error->errnum, error->errmess);
		return NSERROR_INIT_FAILED;
	}

	wstate.w = parent;
	error = xwimp_get_window_state(&wstate);
	if (error) {
		NSLOG(netsurf, INFO, "xwimp_get_window_state: 0x%x: %s",
		      error->errnum, error->errmess);
		return NSERROR_INIT_FAILED;
	}

	istate.w = parent;
	istate.i = ICON_SSL_PANE;
	error = xwimp_get_icon_state(&istate);
	if (error) {
		NSLOG(netsurf, INFO, "xwimp_get_icon_state: 0x%x: %s",
		      error->errnum, error->errmess);
		return NSERROR_INIT_FAILED;
	}

	wstate.w = pane;
	wstate.visible.x1 = wstate.visible.x0 + istate.icon.extent.x1 - 20 - ro_get_vscroll_width(pane);
	wstate.visible.x0 += istate.icon.extent.x0 + 20;
	wstate.visible.y0 = wstate.visible.y1 + istate.icon.extent.y0 + 20 + ro_get_hscroll_height(pane);
	wstate.visible.y1 += istate.icon.extent.y1 - 32;

	set_extent = false;

	if ((winfo.extent.x1 - winfo.extent.x0) <
	    (wstate.visible.x1 - wstate.visible.x0)) {
		winfo.extent.x0 = 0;
		winfo.extent.x1 = wstate.visible.x1 - wstate.visible.x0;
		set_extent = true;
	}
	if ((winfo.extent.y1 - winfo.extent.y0) <
	    (wstate.visible.y1 - wstate.visible.y0)) {
		winfo.extent.y1 = 0;
		winfo.extent.x1 = wstate.visible.y0 - wstate.visible.y1;
		set_extent = true;
	}

	if (set_extent) {
		error = xwimp_set_extent(pane, &(winfo.extent));
		if (error) {
			NSLOG(netsurf, INFO, "xwimp_set_extent: 0x%x: %s",
			      error->errnum, error->errmess);
			return NSERROR_INIT_FAILED;
		}
	}

	error = xwimp_open_window_nested(
		PTR_WIMP_OPEN(&wstate),
		parent,
		wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT << wimp_CHILD_XORIGIN_SHIFT |
		wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT << wimp_CHILD_YORIGIN_SHIFT |
		wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT << wimp_CHILD_LS_EDGE_SHIFT |
		wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT << wimp_CHILD_RS_EDGE_SHIFT);
	if (error) {
		NSLOG(netsurf, INFO, "xwimp_open_window_nested: 0x%x: %s",
		      error->errnum, error->errmess);
		return NSERROR_INIT_FAILED;
	}

	return NSERROR_OK;
}


/**
 * Callback to draw on drawable area of ro certificate viewer window.
 *
 * \param ro_cw The riscos core window structure.
 * \param originx The risc os plotter x origin.
 * \param originy The risc os plotter y origin.
 * \param r The rectangle of the window that needs updating.
 * \return NSERROR_OK on success otherwise apropriate error code
 */
static nserror
cert_draw(struct ro_corewindow *ro_cw, int originx, int originy, struct rect *r)
{
	struct ro_cert_window *certw;
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &ro_plotters
	};

	certw = (struct ro_cert_window *)ro_cw;

	ro_plot_origin_x = originx;
	ro_plot_origin_y = originy;
	no_font_blending = true;
	sslcert_viewer_redraw(certw->ssl_data, 0, 0, r, &ctx);
	no_font_blending = false;

	return NSERROR_OK;
}


/**
 * callback for keypress on ro certificate viewer window
 *
 * \param ro_cw The ro core window structure.
 * \param nskey The netsurf key code.
 * \return NSERROR_OK if key processed,
 *         NSERROR_NOT_IMPLEMENTED if key not processed
 *         otherwise apropriate error code
 */
static nserror cert_key(struct ro_corewindow *ro_cw, uint32_t nskey)
{
	struct ro_cert_window *certw;
	certw = (struct ro_cert_window *)ro_cw;

	if (sslcert_viewer_keypress(certw->ssl_data, nskey)) {
		return NSERROR_OK;
	}
	return NSERROR_NOT_IMPLEMENTED;
}


/**
 * callback for mouse event on ro certificate viewer window
 *
 * \param ro_cw The ro core window structure.
 * \param mouse_state mouse state
 * \param x location of event
 * \param y location of event
 * \return NSERROR_OK on sucess otherwise apropriate error code.
 */
static nserror
cert_mouse(struct ro_corewindow *ro_cw,
	   browser_mouse_state mouse_state,
	   int x, int y)
{
	struct ro_cert_window *certw;
	certw = (struct ro_cert_window *)ro_cw;

	sslcert_viewer_mouse_action(certw->ssl_data, mouse_state, x, y);

	return NSERROR_OK;
}

/* exported interface documented in riscos/sslcert.h */
nserror
gui_cert_verify(nsurl *url,
		const struct ssl_cert_info *certs,
		unsigned long num,
		nserror (*cb)(bool proceed, void *pw),
		void *cbpw)
{
	os_error *error;
	struct ro_cert_window *ncwin; /* new certificate window */
	nserror res;

	ncwin = malloc(sizeof(struct ro_cert_window));
	if (ncwin == NULL) {
		return NSERROR_NOMEM;
	}

	/* initialise certificate viewing interface */
	res = sslcert_viewer_create_session_data(num, url, cb, cbpw, certs,
						 &ncwin->ssl_data);
	if (res != NSERROR_OK) {
		free(ncwin);
		return res;
	}

	/* Create the SSL window */
	error = xwimp_create_window(dialog_cert_template, &ncwin->wh);
	if (error) {
		NSLOG(netsurf, INFO, "xwimp_create_window: 0x%x: %s",
		      error->errnum, error->errmess);
		free(ncwin);
		return NSERROR_INIT_FAILED;
	}

	/* create ssl viewer pane window */
	error = xwimp_create_window(cert_tree_template, &ncwin->core.wh);
	if (error) {
		NSLOG(netsurf, INFO, "xwimp_create_window: 0x%x: %s",
		      error->errnum, error->errmess);
		free(ncwin);
		return NSERROR_INIT_FAILED;
	}

	/* setup callbacks */
	ncwin->core.draw = cert_draw;
	ncwin->core.key = cert_key;
	ncwin->core.mouse = cert_mouse;

	/* initialise core window */
	res = ro_corewindow_init(&ncwin->core, NULL, NULL, 0, NULL);
	if (res != NSERROR_OK) {
		free(ncwin);
		return res;
	}

	res = sslcert_viewer_init(ncwin->core.cb_table,
				  (struct core_window *)ncwin,
				  ncwin->ssl_data);
	if (res != NSERROR_OK) {
		free(ncwin);
		return res;
	}

	/* Set up the certificate window event handling.
	 *
	 * (The action buttons are registered as button events, not OK and
	 * Cancel, as both need to carry out actions.)
	 */
	ro_gui_wimp_event_set_user_data(ncwin->wh, ncwin);
	ro_gui_wimp_event_register_close_window(ncwin->wh,
						ro_gui_cert_close_window);
	ro_gui_wimp_event_register_button(ncwin->wh,
					  ICON_SSL_REJECT,
					  ro_gui_cert_reject);
	ro_gui_wimp_event_register_button(ncwin->wh,
					  ICON_SSL_ACCEPT,
					  ro_gui_cert_accept);

	ro_gui_dialog_open_persistent(NULL, ncwin->wh, false);

	res = cert_attach_pane(ncwin->wh, ncwin->core.wh);
	if (res != NSERROR_OK) {
		ro_gui_cert_release_window(ncwin);
	}

	return res;
}


/* exported interface documented in riscos/sslcert.h */
void ro_gui_cert_initialise(void)
{
	/* Load template for the SSL certificate window */
	dialog_cert_template = ro_gui_dialog_load_template("sslcert");

	/* load template for ssl treeview pane and adjust the window flags. */
	cert_tree_template = ro_gui_dialog_load_template("tree");

	cert_tree_template->flags &= ~(wimp_WINDOW_MOVEABLE |
				       wimp_WINDOW_BACK_ICON |
				       wimp_WINDOW_CLOSE_ICON |
				       wimp_WINDOW_TITLE_ICON |
				       wimp_WINDOW_SIZE_ICON |
				       wimp_WINDOW_TOGGLE_ICON);
}

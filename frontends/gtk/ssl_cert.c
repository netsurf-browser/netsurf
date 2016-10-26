/*
 * Copyright 2015 Vincent Sanders <vince@netsurf-browser.org>
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
 * Implementation of gtk certificate viewing using gtk core windows.
 */

#include <stdint.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#include "utils/log.h"
#include "netsurf/keypress.h"
#include "netsurf/plotters.h"
#include "desktop/sslcert_viewer.h"

#include "gtk/plotters.h"
#include "gtk/scaffolding.h"
#include "gtk/resources.h"
#include "gtk/ssl_cert.h"
#include "gtk/corewindow.h"


/**
 * GTK certificate viewing window context
 */
struct nsgtk_crtvrfy_window {
	/** GTK core window context */
	struct nsgtk_corewindow core;
	/** GTK builder for window */
	GtkBuilder *builder;
	/** GTK dialog window being shown */
	GtkDialog *dlg;
	/** SSL certificate viewer context data */
	struct sslcert_session_data *ssl_data;
};

/**
 * destroy a previously created certificate view
 */
static nserror nsgtk_crtvrfy_destroy(struct nsgtk_crtvrfy_window *crtvrfy_win)
{
	nserror res;

	res = sslcert_viewer_fini(crtvrfy_win->ssl_data);
	if (res == NSERROR_OK) {
		res = nsgtk_corewindow_fini(&crtvrfy_win->core);
		gtk_widget_destroy(GTK_WIDGET(crtvrfy_win->dlg));
		g_object_unref(G_OBJECT(crtvrfy_win->builder));
		free(crtvrfy_win);
	}
	return res;
}

static void
nsgtk_crtvrfy_accept(GtkButton *w, gpointer data)
{
	struct nsgtk_crtvrfy_window *crtvrfy_win;
	crtvrfy_win = (struct nsgtk_crtvrfy_window *)data;

	sslcert_viewer_accept(crtvrfy_win->ssl_data);

	nsgtk_crtvrfy_destroy(crtvrfy_win);
}

static void
nsgtk_crtvrfy_reject(GtkWidget *w, gpointer data)
{
	struct nsgtk_crtvrfy_window *crtvrfy_win;
	crtvrfy_win = (struct nsgtk_crtvrfy_window *)data;

	sslcert_viewer_reject(crtvrfy_win->ssl_data);

	nsgtk_crtvrfy_destroy(crtvrfy_win);
}

static gboolean
nsgtk_crtvrfy_delete_event(GtkWidget *w, GdkEvent  *event, gpointer data)
{
	nsgtk_crtvrfy_reject(w, data);
	return FALSE;
}

/**
 * callback for mouse action for certificate verify on core window
 *
 * \param nsgtk_cw The nsgtk core window structure.
 * \param mouse_state netsurf mouse state on event
 * \param x location of event
 * \param y location of event
 * \return NSERROR_OK on success otherwise appropriate error code
 */
static nserror
nsgtk_crtvrfy_mouse(struct nsgtk_corewindow *nsgtk_cw,
		    browser_mouse_state mouse_state,
		    int x, int y)
{
	struct nsgtk_crtvrfy_window *crtvrfy_win;
	/* technically degenerate container of */
	crtvrfy_win = (struct nsgtk_crtvrfy_window *)nsgtk_cw;

	sslcert_viewer_mouse_action(crtvrfy_win->ssl_data, mouse_state, x, y);

	return NSERROR_OK;
}

/**
 * callback for keypress for certificate verify on core window
 *
 * \param nsgtk_cw The nsgtk core window structure.
 * \param nskey The netsurf key code
 * \return NSERROR_OK on success otherwise appropriate error code
 */
static nserror
nsgtk_crtvrfy_key(struct nsgtk_corewindow *nsgtk_cw, uint32_t nskey)
{
	struct nsgtk_crtvrfy_window *crtvrfy_win;

	/* technically degenerate container of */
	crtvrfy_win = (struct nsgtk_crtvrfy_window *)nsgtk_cw;

	if (sslcert_viewer_keypress(crtvrfy_win->ssl_data, nskey)) {
		return NSERROR_OK;
	}
	return NSERROR_NOT_IMPLEMENTED;
}

/**
 * callback on draw event for certificate verify on core window
 *
 * \param nsgtk_cw The nsgtk core window structure.
 * \param r The rectangle of the window that needs updating.
 * \return NSERROR_OK on success otherwise appropriate error code
 */
static nserror
nsgtk_crtvrfy_draw(struct nsgtk_corewindow *nsgtk_cw, struct rect *r)
{
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &nsgtk_plotters
	};
	struct nsgtk_crtvrfy_window *crtvrfy_win;

	/* technically degenerate container of */
	crtvrfy_win = (struct nsgtk_crtvrfy_window *)nsgtk_cw;

	sslcert_viewer_redraw(crtvrfy_win->ssl_data, 0, 0, r, &ctx);

	return NSERROR_OK;
}

/* exported interface documented in gtk/ssl_cert.h */
nserror gtk_cert_verify(struct nsurl *url,
			const struct ssl_cert_info *certs,
			unsigned long num,
			nserror (*cb)(bool proceed, void *pw),
			void *cbpw)
{
	struct nsgtk_crtvrfy_window *ncwin;
	nserror res;

	ncwin = malloc(sizeof(struct nsgtk_crtvrfy_window));
	if (ncwin == NULL) {
		return NSERROR_NOMEM;
	}

	res = nsgtk_builder_new_from_resname("ssl", &ncwin->builder);
	if (res != NSERROR_OK) {
		LOG("SSL UI builder init failed");
		free(ncwin);
		return res;
	}

	gtk_builder_connect_signals(ncwin->builder, NULL);

	ncwin->dlg = GTK_DIALOG(gtk_builder_get_object(ncwin->builder,
						       "wndSSLProblem"));

	/* set parent for transient dialog */
	gtk_window_set_transient_for(GTK_WINDOW(ncwin->dlg),
		     nsgtk_scaffolding_window(nsgtk_current_scaffolding()));

	ncwin->core.scrolled = GTK_SCROLLED_WINDOW(
		gtk_builder_get_object(ncwin->builder, "SSLScrolled"));

	ncwin->core.drawing_area = GTK_DRAWING_AREA(
		gtk_builder_get_object(ncwin->builder, "SSLDrawingArea"));

	/* make the delete event call our destructor */
	g_signal_connect(G_OBJECT(ncwin->dlg),
			 "delete_event",
			 G_CALLBACK(nsgtk_crtvrfy_delete_event),
			 ncwin);

	/* accept button */
	g_signal_connect(G_OBJECT(gtk_builder_get_object(ncwin->builder,
							 "sslaccept")),
			 "clicked",
			 G_CALLBACK(nsgtk_crtvrfy_accept),
			 ncwin);

	/* reject button */
	g_signal_connect(G_OBJECT(gtk_builder_get_object(ncwin->builder,
							 "sslreject")),
			 "clicked",
			 G_CALLBACK(nsgtk_crtvrfy_reject),
			 ncwin);

	/* initialise GTK core window */
	ncwin->core.draw = nsgtk_crtvrfy_draw;
	ncwin->core.key = nsgtk_crtvrfy_key;
	ncwin->core.mouse = nsgtk_crtvrfy_mouse;

	res = nsgtk_corewindow_init(&ncwin->core);
	if (res != NSERROR_OK) {
		g_object_unref(G_OBJECT(ncwin->dlg));
		free(ncwin);
		return res;
	}

	/* initialise certificate viewing interface */
	res = sslcert_viewer_create_session_data(num, url, cb, cbpw, certs,
					   &ncwin->ssl_data);
	if (res != NSERROR_OK) {
		g_object_unref(G_OBJECT(ncwin->dlg));
		free(ncwin);
		return res;
	}

	res = sslcert_viewer_init(ncwin->core.cb_table,
				  (struct core_window *)ncwin,
				  ncwin->ssl_data);
	if (res != NSERROR_OK) {
		g_object_unref(G_OBJECT(ncwin->dlg));
		free(ncwin);
		return res;
	}

	gtk_widget_show(GTK_WIDGET(ncwin->dlg));

	return NSERROR_OK;
}

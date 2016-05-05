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

#include <stdlib.h>
#include <gtk/gtk.h>

#include "utils/log.h"
#include "utils/nsurl.h"
#include "desktop/tree.h"
#include "desktop/sslcert_viewer.h"

#include "gtk/treeview.h"
#include "gtk/scaffolding.h"
#include "gtk/resources.h"
#include "gtk/ssl_cert.h"


static void nsgtk_ssl_accept(GtkButton *w, gpointer data)
{
	void **session = data;
	GtkBuilder *x = session[0];
	struct nsgtk_treeview *wnd = session[1];
	struct sslcert_session_data *ssl_data = session[2];

	sslcert_viewer_accept(ssl_data);

	nsgtk_treeview_destroy(wnd);
	g_object_unref(G_OBJECT(x));
	free(session);
}

static void nsgtk_ssl_reject(GtkWidget *w, gpointer data)
{
	void **session = data;
	GtkBuilder *x = session[0];
	struct nsgtk_treeview *wnd = session[1];
	struct sslcert_session_data *ssl_data = session[2];

	sslcert_viewer_reject(ssl_data);

	nsgtk_treeview_destroy(wnd);
	g_object_unref(G_OBJECT(x));
	free(session);
}

static gboolean nsgtk_ssl_delete_event(GtkWidget *w, GdkEvent  *event, gpointer data)
{
	nsgtk_ssl_reject(w, data);
	return FALSE;
}

void gtk_cert_verify(nsurl *url, const struct ssl_cert_info *certs,
		unsigned long num, nserror (*cb)(bool proceed, void *pw),
		void *cbpw)
{
	static struct nsgtk_treeview *ssl_window;
	struct sslcert_session_data *data;
	GtkButton *accept, *reject;
	void **session;
	GtkDialog *dlg;
	GtkScrolledWindow *scrolled;
	GtkDrawingArea *drawing_area;
	GtkBuilder *builder;
	GtkWindow *gtk_parent;
	nserror res;

	/* state while dlg is open */
	session = calloc(sizeof(void *), 3);
	if (session == NULL) {
		return;
	}

	res = nsgtk_builder_new_from_resname("ssl", &builder);
	if (res != NSERROR_OK) {
		LOG("SSL UI builder init failed");
		free(session);
		cb(false, cbpw);
		return;
	}

	gtk_builder_connect_signals(builder, NULL);

	sslcert_viewer_create_session_data(num, url, cb, cbpw, certs, &data);
	ssl_current_session = data;

	dlg = GTK_DIALOG(gtk_builder_get_object(builder, "wndSSLProblem"));

	/* set parent for transient dialog */
	gtk_parent = nsgtk_scaffolding_window(nsgtk_current_scaffolding());
	gtk_window_set_transient_for(GTK_WINDOW(dlg), gtk_parent);

	scrolled = GTK_SCROLLED_WINDOW(gtk_builder_get_object(builder, "SSLScrolled"));
	drawing_area = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "SSLDrawingArea"));

	ssl_window = nsgtk_treeview_create(TREE_SSLCERT, GTK_WINDOW(dlg), scrolled,
			drawing_area);

	if (ssl_window == NULL) {
		free(session);
		g_object_unref(G_OBJECT(dlg));
		return;
	}

	accept = GTK_BUTTON(gtk_builder_get_object(builder, "sslaccept"));
	reject = GTK_BUTTON(gtk_builder_get_object(builder, "sslreject"));

	session[0] = builder;
	session[1] = ssl_window;
	session[2] = data;

#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))

	CONNECT(accept, "clicked", nsgtk_ssl_accept, session);
	CONNECT(reject, "clicked", nsgtk_ssl_reject, session);
 	CONNECT(dlg, "delete_event", G_CALLBACK(nsgtk_ssl_delete_event),
			(gpointer)session);

	gtk_widget_show(GTK_WIDGET(dlg));
}

/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include "netsurf/utils/log.h"
#include "netsurf/gtk/gtk_gui.h"
#include "netsurf/content/content.h"
#include "netsurf/content/urldb.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/401login.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

struct session_401 {
	char *url;				/**< URL being fetched */
	char *host;				/**< Host for user display */
	char *realm;				/**< Authentication realm */
	struct browser_window *bw;		/**< Browser window handle */
	GladeXML *x;				/**< Our glade windows */
	GtkWindow *wnd;				/**< The login window itself */
	GtkEntry *user;				/**< Widget with username */
	GtkEntry *pass;				/**< Widget with password */
};

static void create_login_window(struct browser_window *bw, const char *host,
                const char *realm, const char *fetchurl);
static void destroy_login_window(struct session_401 *session);
static void nsgtk_login_next(GtkWidget *w, gpointer data);
static void nsgtk_login_ok_clicked(GtkButton *w, gpointer data);
static void nsgtk_login_cancel_clicked(GtkButton *w, gpointer data);

void gui_401login_open(struct browser_window *bw, struct content *c,
		const char *realm)
{
	char *host;
	url_func_result res;

	res = url_host(c->url, &host);
	assert(res == URL_FUNC_OK);

	create_login_window(bw, host, realm, c->url);

	free(host);
}

void create_login_window(struct browser_window *bw, const char *host,
		const char *realm, const char *fetchurl)
{
	struct session_401 *session;

	/* create a new instance of the login window, and get handles to all
	 * the widgets we're interested in.
	 */

	GladeXML *x = glade_xml_new(glade_file_location, NULL, NULL);
	GtkWindow *wnd = GTK_WINDOW(glade_xml_get_widget(x, "wndLogin"));
	GtkLabel *lhost, *lrealm;
	GtkEntry *euser, *epass;
	GtkButton *bok, *bcan;

	lhost = GTK_LABEL(glade_xml_get_widget(x, "labelLoginHost"));
	lrealm = GTK_LABEL(glade_xml_get_widget(x, "labelLoginRealm"));
	euser = GTK_ENTRY(glade_xml_get_widget(x, "entryLoginUser"));
	epass = GTK_ENTRY(glade_xml_get_widget(x, "entryLoginPass"));
	bok = GTK_BUTTON(glade_xml_get_widget(x, "buttonLoginOK"));
	bcan = GTK_BUTTON(glade_xml_get_widget(x, "buttonLoginCan"));

	/* create and fill in our session structure */

	session = calloc(1, sizeof(struct session_401));
	session->url = strdup(fetchurl);
	session->host = strdup(host);
	session->realm = strdup(realm ? realm : "Secure Area");
	session->bw = bw;
	session->x = x;
	session->wnd = wnd;
	session->user = euser;
	session->pass = epass;

	/* fill in our new login window */

	gtk_label_set_text(GTK_LABEL(lhost), host);
	gtk_label_set_text(lrealm, realm);
	gtk_entry_set_text(euser, "");
	gtk_entry_set_text(epass, "");

	/* attach signal handlers to the Login and Cancel buttons in our new
	 * window to call functions in this file to process the login
	 */
	g_signal_connect(G_OBJECT(bok), "clicked",
			G_CALLBACK(nsgtk_login_ok_clicked), (gpointer)session);
	g_signal_connect(G_OBJECT(bcan), "clicked",
			G_CALLBACK(nsgtk_login_cancel_clicked),
			(gpointer)session);

	/* attach signal handlers to the entry boxes such that pressing
	 * enter in one progresses the focus onto the next widget.
	 */

	g_signal_connect(G_OBJECT(euser), "activate",
			G_CALLBACK(nsgtk_login_next), (gpointer)epass);
	g_signal_connect(G_OBJECT(epass), "activate",
			G_CALLBACK(nsgtk_login_next), (gpointer)bok);

	/* make sure the username entry box currently has the focus */
	gtk_widget_grab_focus(GTK_WIDGET(euser));

	/* finally, show the window */
	gtk_widget_show(GTK_WIDGET(wnd));
}

void destroy_login_window(struct session_401 *session)
{
	free(session->url);
	free(session->host);
	free(session->realm);
	gtk_widget_destroy(GTK_WIDGET(session->wnd));
	g_object_unref(G_OBJECT(session->x));
	free(session);
}

void nsgtk_login_next(GtkWidget *w, gpointer data)
{
	gtk_widget_grab_focus(GTK_WIDGET(data));
}

void nsgtk_login_ok_clicked(GtkButton *w, gpointer data)
{
	/* close the window and destroy it, having continued the fetch
	 * assoicated with it.
	 */

  	struct session_401 *session = (struct session_401 *)data;
	const gchar *user = gtk_entry_get_text(session->user);
	const gchar *pass = gtk_entry_get_text(session->pass);
	char *auth;

	auth = malloc(strlen(user) + strlen(pass) + 2);
	sprintf(auth, "%s:%s", user, pass);
	urldb_set_auth_details(session->url, session->realm, auth);
	free(auth);

	browser_window_go(session->bw, session->url, 0, true);

	destroy_login_window(session);
}

void nsgtk_login_cancel_clicked(GtkButton *w, gpointer data)
{
	/* just close and destroy the window */
	destroy_login_window((struct session_401 *)data);
}

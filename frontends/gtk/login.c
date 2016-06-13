/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
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
#include <string.h>
#include <gtk/gtk.h>

#include "utils/log.h"
#include "utils/nsurl.h"
#include "netsurf/url_db.h"

#include "gtk/resources.h"
#include "gtk/login.h"

/** login window session data */
struct session_401 {
	nsurl *url;				/**< URL being fetched */
	lwc_string *host;			/**< Host for user display */
	char *realm;				/**< Authentication realm */
	nserror (*cb)(bool proceed, void *pw);	/**< Continuation callback */
	void *cbpw;				/**< Continuation data */
	GtkBuilder *x;				/**< Our builder windows */
	GtkWindow *wnd;				/**< The login window itself */
	GtkEntry *user;				/**< Widget with username */
	GtkEntry *pass;				/**< Widget with password */
};

/**
 * Destroy login window and free all associated resources
 *
 * \param session The login window session to destroy.
 */
static void destroy_login_window(struct session_401 *session)
{
	nsurl_unref(session->url);
	lwc_string_unref(session->host);
	free(session->realm);
	gtk_widget_destroy(GTK_WIDGET(session->wnd));
	g_object_unref(G_OBJECT(session->x));
	free(session);
}


/**
 * process next signal in entry widgets.
 *
 * \param w current widget
 * \param data next widget 
 */
static void nsgtk_login_next(GtkWidget *w, gpointer data)
{
	gtk_widget_grab_focus(GTK_WIDGET(data));
}


/**
 * handler called when navigation is continued
 *
 * \param w current widget
 * \param data login window session
 */
static void nsgtk_login_ok_clicked(GtkButton *w, gpointer data)
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

	session->cb(true, session->cbpw);

	destroy_login_window(session);
}


/**
 * handler called when navigation is cancelled
 *
 * \param w widget
 * \param data login window session
 */
static void nsgtk_login_cancel_clicked(GtkButton *w, gpointer data)
{
	struct session_401 *session = (struct session_401 *) data;

	session->cb(false, session->cbpw);

	/* close and destroy the window */
	destroy_login_window(session);
}


/**
 * create a new instance of the login window
 *
 * creates login window and handles to all the widgets we're
 * interested in.
 *
 * \param url The url causing the login.
 * \param host the host being logged into
 * \param realm realmm the login relates to
 * \param cb callback when complete
 * \param cbpw data to pass to callback
 * \return NSERROR_OK on sucessful window creation or error code on faliure.
 */
static nserror
create_login_window(nsurl *url,
		    lwc_string *host,
		    const char *realm,
		    nserror (*cb)(bool proceed, void *pw),
		    void *cbpw)
{
	struct session_401 *session;
	GtkWindow *wnd;
	GtkLabel *lhost, *lrealm;
	GtkEntry *euser, *epass;
	GtkButton *bok, *bcan;
	GtkBuilder* builder;
	nserror res;

	session = calloc(1, sizeof(struct session_401));
	if (session == NULL) {
		return NSERROR_NOMEM;
	}

	res = nsgtk_builder_new_from_resname("login", &builder);
	if (res != NSERROR_OK) {
		free(session);
		return res;
	}

	gtk_builder_connect_signals(builder, NULL);

	wnd = GTK_WINDOW(gtk_builder_get_object(builder, "wndLogin"));
	lhost = GTK_LABEL(gtk_builder_get_object(builder, "labelLoginHost"));
	lrealm = GTK_LABEL(gtk_builder_get_object(builder, "labelLoginRealm"));
	euser = GTK_ENTRY(gtk_builder_get_object(builder, "entryLoginUser"));
	epass = GTK_ENTRY(gtk_builder_get_object(builder, "entryLoginPass"));
	bok = GTK_BUTTON(gtk_builder_get_object(builder, "buttonLoginOK"));
	bcan = GTK_BUTTON(gtk_builder_get_object(builder, "buttonLoginCan"));

	/* create and fill in our session structure */
	session->url = nsurl_ref(url);
	session->host = lwc_string_ref(host);
	session->realm = strdup(realm ? realm : "Secure Area");
	session->cb = cb;
	session->cbpw = cbpw;
	session->x = builder;
	session->wnd = wnd;
	session->user = euser;
	session->pass = epass;

	/* fill in our new login window */

	gtk_label_set_text(GTK_LABEL(lhost), lwc_string_data(host));
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

	return NSERROR_OK;
}


/* exported function documented in gtk/login.h */
void gui_401login_open(nsurl *url,
		       const char *realm,
		       nserror (*cb)(bool proceed, void *pw),
		       void *cbpw)
{
	lwc_string *host;
	nserror res;

	host = nsurl_get_component(url, NSURL_HOST);
	assert(host != NULL);

	res = create_login_window(url, host, realm, cb, cbpw);
	if (res != NSERROR_OK) {
		LOG("Login init failed");

		/* creating login failed so cancel navigation */
		cb(false, cbpw);
	}

	lwc_string_unref(host);
}

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
#include "utils/messages.h"
#include "netsurf/url_db.h"

#include "gtk/resources.h"
#include "gtk/login.h"

/** login window session data */
struct session_401 {
	nserror (*cb)(const char *username,
		      const char *password,
		      void *pw);		/**< Continuation callback */
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
	const gchar *user;
	const gchar *pass;

	user = gtk_entry_get_text(session->user);
	pass = gtk_entry_get_text(session->pass);

	session->cb(user, pass, session->cbpw);

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

	session->cb(NULL, NULL, session->cbpw);

	/* close and destroy the window */
	destroy_login_window(session);
}


/**
 * generate the description of the login request
 */
static nserror
get_login_description(struct nsurl *url,
		      const char *realm,
		      const char *username,
		      const char *password,
		      char **out_str)
{
	char *url_s;
	size_t url_l;
	nserror res;
	char *str = NULL;
	int slen;
	const char *key;

	res = nsurl_get(url, NSURL_SCHEME | NSURL_HOST, &url_s, &url_l);
	if (res != NSERROR_OK) {
		return res;
	}

	if ((*username == 0) && (*password == 0)) {
		key = "LoginDescription";
	} else {
		key = "LoginAgain";
	}

	str = messages_get_buff(key, url_s, realm);
	NSLOG(netsurf, INFO,
	      "key:%s url:%s realm:%s str:%s", key, url_s, realm, str);

	if (strcmp(key, str) != 0) {
		*out_str = str;
	} else {
		/* no message so fallback */
		const char *fmt = "The site %s is requesting your username and password. The realm is \"%s\"";
		slen = snprintf(str, 0, fmt, url_s, realm) + 1;
		str = malloc(slen);
		if (str == NULL) {
			res = NSERROR_NOMEM;
		} else {
			snprintf(str, slen, fmt, url_s, realm);
			*out_str = str;
		}
	}

	free(url_s);

	return res;
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
		    const char *username,
		    const char *password,
		    nserror (*cb)(const char *username,
				  const char *password,
				  void *pw),
		    void *cbpw)
{
	nserror res;
	struct session_401 *session;
	GtkWindow *wnd;
	GtkLabel *ldesc;
	GtkEntry *euser, *epass;
	GtkButton *bok, *bcan;
	GtkBuilder *builder;
	char *description = NULL;

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

	wnd = GTK_WINDOW(gtk_builder_get_object(builder, "LoginDialog"));
	ldesc = GTK_LABEL(gtk_builder_get_object(builder, "LoginDescription"));
	euser = GTK_ENTRY(gtk_builder_get_object(builder, "LoginUsername"));
	epass = GTK_ENTRY(gtk_builder_get_object(builder, "LoginPassword"));
	bok = GTK_BUTTON(gtk_builder_get_object(builder, "LoginOK"));
	bcan = GTK_BUTTON(gtk_builder_get_object(builder, "LoginCancel"));

	/* create and fill in our session structure */
	session->cb = cb;
	session->cbpw = cbpw;
	session->x = builder;
	session->wnd = wnd;
	session->user = euser;
	session->pass = epass;

	/* fill in our new login window */
	res = get_login_description(url, realm, username, password, &description);
	if (res == NSERROR_OK) {
		gtk_label_set_text(GTK_LABEL(ldesc), description);
		free(description);
	}
	gtk_entry_set_text(euser, username);
	gtk_entry_set_text(epass, password);

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
nserror gui_401login_open(nsurl *url, const char *realm,
		const char *username, const char *password,
		nserror (*cb)(const char *username,
				const char *password,
				void *pw),
		void *cbpw)
{
	lwc_string *host;
	nserror res;

	host = nsurl_get_component(url, NSURL_HOST);
	assert(host != NULL);

	res = create_login_window(url, host, realm, username, password,
			cb, cbpw);
	if (res != NSERROR_OK) {
		NSLOG(netsurf, INFO, "Login init failed");

		return res;
	}

	lwc_string_unref(host);

	return NSERROR_OK;
}

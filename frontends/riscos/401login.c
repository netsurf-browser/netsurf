/*
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
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

#include "utils/config.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <oslib/wimp.h>

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsurl.h"
#include "netsurf/url_db.h"

#include "riscos/gui.h"
#include "riscos/dialog.h"
#include "riscos/wimp_event.h"

#define ICON_401LOGIN_LOGIN 0
#define ICON_401LOGIN_CANCEL 1
#define ICON_401LOGIN_HOST 2
#define ICON_401LOGIN_REALM 3
#define ICON_401LOGIN_USERNAME 4
#define ICON_401LOGIN_PASSWORD 5

static void ro_gui_401login_close(wimp_w w);
static bool ro_gui_401login_apply(wimp_w w);
static nserror ro_gui_401login_open(nsurl *url, lwc_string *host,
		const char *realm,
		const char *username, const char *password,
		nserror (*cb)(const char *username,
				const char *password,
				void *pw),
		void *cbpw);

static wimp_window *dialog_401_template;

struct session_401 {
	lwc_string *host;		/**< Host for user display */
	char *realm;			/**< Authentication realm */
	char uname[256];		/**< Buffer for username */
	nsurl *url;			/**< URL being fetched */
	char pwd[256];			/**< Buffer for password */
	nserror (*cb)(const char *username,
			const char *password,
			void *pw);	/**< Continuation callback */
	void *cbpw;			/**< Continuation callback data */
};


/**
 * Load the 401 login window template.
 */

void ro_gui_401login_init(void)
{
	dialog_401_template = ro_gui_dialog_load_template("login");
}


/**
 * Open the login dialog
 */
nserror gui_401login_open(nsurl *url, const char *realm,
		const char *username, const char *password,
		nserror (*cb)(const char *username,
				const char *password,
				void *pw),
		void *cbpw)
{
	nserror err;
	lwc_string *host = nsurl_get_component(url, NSURL_HOST);
	assert(host != NULL);

	err = ro_gui_401login_open(url, host, realm, username, password,
			cb, cbpw);
	lwc_string_unref(host);

	return err;
}


/**
 * Open a 401 login window.
 */

nserror ro_gui_401login_open(nsurl *url, lwc_string *host, const char *realm,
		const char *username, const char *password,
		nserror (*cb)(const char *username,
				const char *password,
				void *pw),
		void *cbpw)
{
	struct session_401 *session;
	size_t len;
	wimp_w w;

	assert(host != NULL);
	assert(username != NULL);
	assert(password != NULL);

	session = calloc(1, sizeof(struct session_401));
	if (!session) {
		ro_warn_user("NoMemory", 0);
		return NSERROR_NOMEM;
	}

	if (realm == NULL)
		realm = "Secure Area";

	session->url = nsurl_ref(url);
	session->host = lwc_string_ref(host);
	session->realm = strdup(realm);
	session->cb = cb;
	session->cbpw = cbpw;

	len = strlen(username);
	assert(len < sizeof(session->uname));
	memcpy(session->uname, username, len + 1);

	len = strlen(password);
	assert(len < sizeof(session->pwd));
	memcpy(session->pwd, password, len + 1);

	if (!session->realm) {
		nsurl_unref(session->url);
		lwc_string_unref(session->host);
		free(session);
		ro_warn_user("NoMemory", 0);
		return NSERROR_NOMEM;
	}

	/* fill in download window icons */
	dialog_401_template->icons[ICON_401LOGIN_HOST].data.
			indirected_text.text =
					(char *)lwc_string_data(session->host);
	dialog_401_template->icons[ICON_401LOGIN_HOST].data.
			indirected_text.size =
					lwc_string_length(session->host) + 1;
	dialog_401_template->icons[ICON_401LOGIN_REALM].data.
			indirected_text.text = session->realm;
	dialog_401_template->icons[ICON_401LOGIN_REALM].data.
			indirected_text.size = strlen(session->realm) + 1;
	dialog_401_template->icons[ICON_401LOGIN_USERNAME].data.
			indirected_text.text = session->uname;
	dialog_401_template->icons[ICON_401LOGIN_USERNAME].data.
			indirected_text.size = sizeof(session->uname);
	dialog_401_template->icons[ICON_401LOGIN_PASSWORD].data.
			indirected_text.text = session->pwd;
	dialog_401_template->icons[ICON_401LOGIN_PASSWORD].data.
			indirected_text.size = sizeof(session->pwd);

	/* create and open the window */
	w = wimp_create_window(dialog_401_template);

	ro_gui_wimp_event_register_text_field(w, ICON_401LOGIN_USERNAME);
	ro_gui_wimp_event_register_text_field(w, ICON_401LOGIN_PASSWORD);
	ro_gui_wimp_event_register_cancel(w, ICON_401LOGIN_CANCEL);
	ro_gui_wimp_event_register_ok(w, ICON_401LOGIN_LOGIN,
			ro_gui_401login_apply);
	ro_gui_wimp_event_register_close_window(w, ro_gui_401login_close);
	ro_gui_wimp_event_set_user_data(w, session);

	ro_gui_dialog_open_persistent(NULL, w, false);

	return NSERROR_OK;
}

/**
 * Handle closing of login dialog
 */
void ro_gui_401login_close(wimp_w w)
{
	os_error *error;
	struct session_401 *session;

	session = (struct session_401 *)ro_gui_wimp_event_get_user_data(w);

	assert(session);

	/* If ok didn't happen, send failure response */
	if (session->cb != NULL)
		session->cb(NULL, NULL, session->cbpw);

	nsurl_unref(session->url);
	lwc_string_unref(session->host);
	free(session->realm);
	free(session);

	error = xwimp_delete_window(w);
	if (error) {
		NSLOG(netsurf, INFO, "xwimp_delete_window: 0x%x:%s",
		      error->errnum, error->errmess);
		ro_warn_user("WimpError", error->errmess);
	}
	ro_gui_wimp_event_finalise(w);
}


/* Login Clicked -> create a new fetch request, specifying uname & pwd
 *                  CURLOPT_USERPWD takes a string "username:password"
 */
bool ro_gui_401login_apply(wimp_w w)
{
	struct session_401 *session;

	session = (struct session_401 *)ro_gui_wimp_event_get_user_data(w);

	assert(session);

	session->cb(session->uname, session->pwd, session->cbpw);

	/* Flag that we sent response by invalidating callback details */
	session->cb = NULL;
	session->cbpw = NULL;

	return true;
}


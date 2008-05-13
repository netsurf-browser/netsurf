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

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "oslib/wimp.h"
#include "utils/config.h"
#include "content/content.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/401login.h"
#include "desktop/gui.h"
#include "riscos/dialog.h"
#include "riscos/wimp_event.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"

#ifdef WITH_AUTH

#define ICON_401LOGIN_LOGIN 0
#define ICON_401LOGIN_CANCEL 1
#define ICON_401LOGIN_HOST 2
#define ICON_401LOGIN_REALM 3
#define ICON_401LOGIN_USERNAME 4
#define ICON_401LOGIN_PASSWORD 5

static void ro_gui_401login_close(wimp_w w);
static bool ro_gui_401login_apply(wimp_w w);
static void ro_gui_401login_open(struct browser_window *bw, const char *host,
		const char *realm, const char *fetchurl);


static wimp_window *dialog_401_template;

struct session_401 {
	char *host;			/**< Host for user display */
	char *realm;			/**< Authentication realm */
	char uname[256];		/**< Buffer for username */
	char *url;			/**< URL being fetched */
	char pwd[256];			/**< Buffer for password */
	struct browser_window *bwin;	/**< Browser window handle */
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
void gui_401login_open(struct browser_window *bw, struct content *c,
		const char *realm)
{
	char *murl, *host;
	url_func_result res;

	murl = c->url;
	res = url_host(murl, &host);
	assert(res == URL_FUNC_OK);

	ro_gui_401login_open(bw, host, realm, murl);

	free(host);
}


/**
 * Open a 401 login window.
 */

void ro_gui_401login_open(struct browser_window *bw, const char *host,
		const char *realm, const char *fetchurl)
{
	struct session_401 *session;
	wimp_w w;

	session = calloc(1, sizeof(struct session_401));
	if (!session) {
		warn_user("NoMemory", 0);
		return;
	}

	session->url = strdup(fetchurl);
	if (!session->url) {
		free(session);
		warn_user("NoMemory", 0);
		return;
	}
	session->uname[0] = '\0';
	session->pwd[0] = '\0';
	session->host = strdup(host);
	session->realm = strdup(realm ? realm : "Secure Area");
	session->bwin = bw;
	if ((!session->host) || (!session->realm)) {
		free(session->host);
		free(session->realm);
		free(session);
		warn_user("NoMemory", 0);
		return;
	}

	/* fill in download window icons */
	dialog_401_template->icons[ICON_401LOGIN_HOST].data.indirected_text.text =
		session->host;
	dialog_401_template->icons[ICON_401LOGIN_HOST].data.indirected_text.size =
		strlen(host) + 1;
	dialog_401_template->icons[ICON_401LOGIN_REALM].data.indirected_text.text =
		session->realm;
	dialog_401_template->icons[ICON_401LOGIN_REALM].data.indirected_text.size =
		strlen(realm) + 1;
	dialog_401_template->icons[ICON_401LOGIN_USERNAME].data.indirected_text.text =
		session->uname;
	dialog_401_template->icons[ICON_401LOGIN_USERNAME].data.indirected_text.size =
		256;
	dialog_401_template->icons[ICON_401LOGIN_PASSWORD].data.indirected_text.text =
		session->pwd;
	dialog_401_template->icons[ICON_401LOGIN_PASSWORD].data.indirected_text.size =
		256;

	/* create and open the window */
	w = wimp_create_window(dialog_401_template);

	ro_gui_wimp_event_register_text_field(w, ICON_401LOGIN_USERNAME);
	ro_gui_wimp_event_register_text_field(w, ICON_401LOGIN_PASSWORD);
	ro_gui_wimp_event_register_cancel(w, ICON_401LOGIN_CANCEL);
	ro_gui_wimp_event_register_ok(w, ICON_401LOGIN_LOGIN,
			ro_gui_401login_apply);
	ro_gui_wimp_event_register_close_window(w, ro_gui_401login_close);
	ro_gui_wimp_event_set_user_data(w, session);

	ro_gui_dialog_open_persistent(bw->window->window, w, false);
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

	free(session->host);
	free(session->realm);
	free(session->url);
	free(session);

	error = xwimp_delete_window(w);
	if (error) {
		LOG(("xwimp_delete_window: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	ro_gui_wimp_event_finalise(w);
}


/* Login Clicked -> create a new fetch request, specifying uname & pwd
 *                  CURLOPT_USERPWD takes a string "username:password"
 */
bool ro_gui_401login_apply(wimp_w w)
{
	struct session_401 *session;
	char *auth;

	session = (struct session_401 *)ro_gui_wimp_event_get_user_data(w);

	assert(session);

	auth = malloc(strlen(session->uname) + strlen(session->pwd) + 2);
	if (!auth) {
		LOG(("calloc failed"));
		warn_user("NoMemory", 0);
		return false;
	}

	sprintf(auth, "%s:%s", session->uname, session->pwd);

	urldb_set_auth_details(session->url, session->realm, auth);

	free(auth);

	browser_window_go(session->bwin, session->url, 0, true);
	return true;
}

#endif

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "oslib/wimp.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/401login.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_AUTH

static void get_unamepwd(void);

static wimp_window *dialog_401_template;
extern wimp_w dialog_401li;

static char uname[256];
static char *url;
static char pwd[256];
static struct browser_window *bwin;


/**
 * Load the 401 login window template.
 */

void ro_gui_401login_init(void)
{
	dialog_401_template = ro_gui_dialog_load_template("login");
}


void gui_401login_open(struct browser_window *bw, struct content *c, char *realm)
{
	char *murl, *host;
	url_func_result res;

	murl = c->url;
	res = url_host(murl, &host);
	assert(res == URL_FUNC_OK);
	bwin = bw;

	ro_gui_401login_open(bw->window->window, host, realm, murl);

	xfree(host);
}


/**
 * Open a 401 login window.
 */

void ro_gui_401login_open(wimp_w parent, char *host, char* realm, char *fetchurl)
{
	url = xstrdup(fetchurl);
	uname[0] = pwd[0] = 0;

	/* fill in download window icons */
	dialog_401_template->icons[ICON_401LOGIN_HOST].data.indirected_text.text =
		xstrdup(host);
	dialog_401_template->icons[ICON_401LOGIN_HOST].data.indirected_text.size =
		strlen(host) + 1;
	dialog_401_template->icons[ICON_401LOGIN_REALM].data.indirected_text.text =
		xstrdup(realm);
	dialog_401_template->icons[ICON_401LOGIN_REALM].data.indirected_text.size =
		strlen(realm) + 1;
	dialog_401_template->icons[ICON_401LOGIN_USERNAME].data.indirected_text.text =
		uname;
	dialog_401_template->icons[ICON_401LOGIN_USERNAME].data.indirected_text.size =
		256;
	dialog_401_template->icons[ICON_401LOGIN_PASSWORD].data.indirected_text.text =
		pwd;
	dialog_401_template->icons[ICON_401LOGIN_PASSWORD].data.indirected_text.size =
		256;

	/* create and open the window */
	dialog_401li = wimp_create_window(dialog_401_template);
	ro_gui_dialog_open_persistant(parent, dialog_401li, false);
}

bool ro_gui_401login_keypress(wimp_key *key)
{
	switch (key->c) {
		case wimp_KEY_RETURN:
			get_unamepwd();
			ro_gui_dialog_close(dialog_401li);
			browser_window_go(bwin, url, 0);
			return true;
	}

	return false;
}

/* Login Clicked -> create a new fetch request, specifying uname & pwd
 *                  CURLOPT_USERPWD takes a string "username:password"
 */
void ro_gui_401login_click(wimp_pointer *pointer)
{
	if (pointer->buttons == wimp_CLICK_MENU)
		return;

	switch (pointer->i) {
		case ICON_401LOGIN_LOGIN:
			get_unamepwd();
			ro_gui_dialog_close(dialog_401li);
			browser_window_go(bwin, url, 0);
			break;
		case ICON_401LOGIN_CANCEL:
			ro_gui_dialog_close(dialog_401li);
			break;
	}
}

void get_unamepwd(void)
{
	char *lidets = calloc(strlen(uname)+strlen(pwd)+2, sizeof(char));
	if (!lidets) {
	  	LOG(("Insufficient memory for calloc"));
	  	warn_user("NoMemory", 0);
		return;
	}

	sprintf(lidets, "%s:%s", uname, pwd);

	login_list_add(url, lidets);
}

#endif

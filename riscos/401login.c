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
#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/401login.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

static void get_unamepwd(void);

static wimp_window *dialog_401_template;
extern wimp_w dialog_401li;

struct login LOGIN;

static char *uname;
static char* url;
static char *pwd;
static struct browser_window *bwin;

/**
 * Load the 401 login window template.
 */

void ro_gui_401login_init(void)
{
	char name[] = "dialog_401li";
	int context, window_size, data_size;
	char *data;

	/* find required buffer sizes */
	context = wimp_load_template(wimp_GET_SIZE, 0, 0, wimp_NO_FONTS,
			name, 0, &window_size, &data_size);
	assert(context != 0);

	dialog_401_template = xcalloc((unsigned int) window_size, 1);
	data = xcalloc((unsigned int) data_size, 1);

	/* load */
	wimp_load_template(dialog_401_template, data, data + data_size,
			wimp_NO_FONTS, name, 0, 0, 0);
}

void gui_401login_open(struct browser_window *bw, struct content *c, char *realm) {

  char *murl, *host;

  murl = c->url;
  host = get_host_from_url(murl);
  assert(host);
  bwin = bw;

  ro_gui_401login_open(host, realm, murl);

  xfree(host);
}


/**
 * Open a 401 login window.
 */

void ro_gui_401login_open(char *host, char* realm, char *fetchurl)
{
	url = xstrdup(fetchurl);
	uname = xcalloc(1, 256);
	pwd = xcalloc(1, 256);
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
	ro_gui_dialog_open(dialog_401li);
	wimp_set_caret_position(dialog_401li, ICON_401LOGIN_USERNAME,
			-1, -1, -1, 0);
}

bool ro_gui_401login_keypress(wimp_key *key) {

  switch (key->c) {
    case wimp_KEY_RETURN:
            get_unamepwd();
            ro_gui_dialog_close(dialog_401li);
            browser_window_open_location(bwin, url);
            return true;
    case wimp_KEY_ESCAPE:
            ro_gui_dialog_close(dialog_401li);
            break;
    default: break;
  }

  return false;
}

/* Login Clicked -> create a new fetch request, specifying uname & pwd
 *                  CURLOPT_USERPWD takes a string "username:password"
 */
void ro_gui_401login_click(wimp_pointer *pointer) {

  if (pointer->buttons == wimp_CLICK_MENU) return;

  switch (pointer->i) {
    case ICON_401LOGIN_LOGIN:
            get_unamepwd();
            ro_gui_dialog_close(dialog_401li);
            browser_window_open_location(bwin, url);
            break;
    case ICON_401LOGIN_CANCEL:
            ro_gui_dialog_close(dialog_401li);
            break;
    default: break;
  }
}

void get_unamepwd() {

  char *lidets = xcalloc(strlen(uname)+strlen(pwd)+2, sizeof(char));

  sprintf(lidets, "%s:%s", uname, pwd);

  login_list_add(url, lidets);
}

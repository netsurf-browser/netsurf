/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <string.h>
#include "oslib/wimp.h"
#include "netsurf/desktop/401login.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

static void get_unamepwd(void);
static void do_thing(void);

static wimp_window *dialog_401;
extern wimp_w dialog_401li;

struct login LOGIN;

static char *uname;
static char* url;
static char *pwd;

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

	dialog_401 = xcalloc((unsigned int) window_size, 1);
	data = xcalloc((unsigned int) data_size, 1);

	/* load */
	wimp_load_template(dialog_401, data, data + data_size,
			wimp_NO_FONTS, name, 0, 0, 0);
}

/**
 * Open a 401 login window.
 */

void ro_gui_401login_open(char *host, char* realm, char *fetchurl)
{
	url = xstrdup(fetchurl);
	uname = xstrdup("");
	pwd = xstrdup("");

	/* fill in download window icons */
	dialog_401->icons[ICON_401LOGIN_HOST].data.indirected_text.text =
		host;
	dialog_401->icons[ICON_401LOGIN_HOST].data.indirected_text.size =
		strlen(host) + 1;
	dialog_401->icons[ICON_401LOGIN_REALM].data.indirected_text.text =
		realm;
	dialog_401->icons[ICON_401LOGIN_REALM].data.indirected_text.size =
		strlen(realm) + 1;
	dialog_401->icons[ICON_401LOGIN_USERNAME].data.indirected_text.text =
		uname;
	dialog_401->icons[ICON_401LOGIN_PASSWORD].data.indirected_text.text =
		pwd;

	/* create and open the window */
	dialog_401li = wimp_create_window(dialog_401);
	ro_gui_dialog_open(dialog_401li);
	wimp_set_caret_position(dialog_401li, ICON_401LOGIN_USERNAME,0,0,0,0);
}

/* Login Clicked -> create a new fetch request, specifying uname & pwd
 *                  CURLOPT_USERPWD takes a string "username:password"
 */
void ro_gui_401login_click(wimp_pointer *pointer) {

  if (pointer->buttons == wimp_CLICK_MENU) return;

  switch (pointer->i) {
    case ICON_401LOGIN_LOGIN:
            if (pointer->buttons == wimp_CLICK_SELECT) {
                    LOG(("here"));
                    get_unamepwd();
                    ro_gui_dialog_close(dialog_401li);
                    do_thing();
                    LOGIN.string = 0; /* TODO: keep the details until we
                                       *       access a new site */
            }
            else
                    ro_gui_dialog_close(dialog_401li);
            break;
    case ICON_401LOGIN_CANCEL:
            if (pointer->buttons == wimp_CLICK_SELECT)
                    ro_gui_dialog_close(dialog_401li);
            else {
                    get_unamepwd();
                    ro_gui_dialog_close(dialog_401li);
                    do_thing();
                    LOGIN.string = 0; /* TODO: keep the details until we
                                       *       access a new site */
            }
            break;
    default: break;
  }
}

void get_unamepwd() {

  LOGIN.string = xcalloc(strlen(uname)+strlen(pwd)+2, sizeof(char));

  sprintf(LOGIN.string, "%s:%s", uname, pwd);
  LOG(("%s", LOGIN.string));
}

void do_thing() {

  struct gui_window *gw;

  /* TODO: fix this. For now we just open the page in the
   *                 first window in the list. */
  for (gw=window_list; gw!=NULL; gw=gw->next) {
    if (gw->type == GUI_BROWSER_WINDOW /*&&
        (strcasecmp(gw->url, url)==0 ||
         strcasecmp(gw->data.browser.bw->url, url)==0)*/)
      break;
  }
  if (gw != NULL)
    browser_window_open_location_historical(gw->data.browser.bw, url);
}

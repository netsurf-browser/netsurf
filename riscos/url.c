/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Shamelessly hacked from Rob Jackson's URI handler (see uri.c)
 */

#include <stdio.h>
#include <string.h>
#include "oslib/inetsuite.h"
#include "oslib/wimp.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/url.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

void ro_url_message_received(wimp_message* message)
{
  char* uri_requested = NULL;
  struct browser_window* bw;
  inetsuite_message_open_url *url_message = (inetsuite_message_open_url*)&message->data;

  if (strlen(url_message->url) > 0) {
    uri_requested = xstrdup(url_message->url);
    LOG(("%s", url_message->url));
  }
  else {

    /* TODO - handle indirected message data */
    return;
#if 0
    if (url_message->indirect.url.offset == 0 ||
        url_message->indirect.url.offset > 256) {
       /* pointer to shared memory */
       LOG(("shared: %x", url_message->indirect.url.pointer));
       uri_requested = xstrdup(url_message->indirect.url.pointer);
    }
    else { /* offset into message block */
       LOG(("in message"));
       uri_requested = xstrdup((char*)&url_message[url_message->indirect.url.offset]);
    }
    LOG(("%s", uri_requested));
#endif
  }

  if ( (strspn(uri_requested, "http://") != strlen("http://")) &&
       (strspn(uri_requested, "https://") != strlen("https://")) &&
       (strspn(uri_requested, "file:/")  != strlen("file:/")) ) {
            xfree(uri_requested);
            return;
  }

  /* send ack */
  message->your_ref = message->my_ref;
  xwimp_send_message(wimp_USER_MESSAGE_ACKNOWLEDGE, message,
                     message->sender);

  /* create new browser window */
  bw = create_browser_window(browser_TITLE | browser_TOOLBAR
          | browser_SCROLL_X_ALWAYS | browser_SCROLL_Y_ALWAYS, 640, 480, NULL);

  gui_window_show(bw->window);
  browser_window_open_location(bw, uri_requested);

  wimp_set_caret_position(bw->window->data.browser.toolbar,
               ICON_TOOLBAR_URL,
               0,0,-1, (int) strlen(bw->window->url) - 1);

  xfree(uri_requested);

  return;
}

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

/* Define this to allow posting of data to an URL */
#undef ALLOW_POST

static char *read_string_value(os_string_value string, char *msg);

void ro_url_message_received(wimp_message* message)
{
  char* uri_requested = NULL;
#ifdef ALLOW_POST
  char* filename = NULL, *mimetype = NULL;
  bool post=false;
#endif
  struct browser_window* bw;
  inetsuite_message_open_url *url_message = (inetsuite_message_open_url*)&message->data;

  /* If the url_message->indirect.tag is non-zero,
   * then the message data is contained within the message block.
   */
  if (url_message->indirect.tag != 0) {
    uri_requested = xstrdup(url_message->url);
    LOG(("%s", url_message->url));
  }
  else {
    /* Get URL */
    if (read_string_value(url_message->indirect.url,
                          (char*)url_message) != 0) {
      uri_requested = xstrdup(read_string_value(url_message->indirect.url,
                                                (char*)url_message));
    }
    else {
      return;
    }
    LOG(("%s", uri_requested));

#ifdef ALLOW_POST
    /* Get filename */
    if (read_string_value(url_message->indirect.body_file,
                          (char*)url_message) != 0) {
      filename = xstrdup(read_string_value(url_message->indirect.body_file,
                                           (char*)url_message));
    }
    /* We ignore the target window. Just open a new window. */
    /* Get mimetype */
    if (url_message->indirect.flags & inetsuite_USE_MIME_TYPE) {
      if (read_string_value(url_message->indirect.body_mimetype,
                            (char*)url_message) != 0) {
        mimetype = xstrdup(read_string_value(url_message->indirect.body_mimetype,
                                             (char*)url_message));
      }
      else {
        mimetype = xstrdup("application/x-www-form-urlencoded");
      }
    }
    else {
      mimetype = xstrdup("application/x-www-form-urlencoded");
    }

    /* Indicate a post request */
    if (filename && message->size > 28)
      post = true;
#endif
  }

  if ( (strspn(uri_requested, "http://") != strlen("http://")) &&
       (strspn(uri_requested, "https://") != strlen("https://")) &&
       (strspn(uri_requested, "file:/")  != strlen("file:/")) ) {
#ifdef ALLOW_POST
            xfree(filename);
            xfree(mimetype);
#endif
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

#ifdef ALLOW_POST
  if (post) {
    /* TODO - create urlencoded data from file contents.
     *        Delete the file when finished with it.
     */
    browser_window_open_location_historical(bw, uri_requested, /*data*/0, 0);
  }
  else
#endif
    browser_window_open_location(bw, uri_requested);

  wimp_set_caret_position(bw->window->data.browser.toolbar,
               ICON_TOOLBAR_URL,
               0,0,-1, (int) strlen(bw->window->url) - 1);

#ifdef ALLOW_POST
  xfree(filename);
  xfree(mimetype);
#endif
  xfree(uri_requested);

  return;
}

char *read_string_value(os_string_value string, char *msg) {

        if(string.offset == 0)  return NULL;
        if(string.offset > 256) return string.pointer;
        return &msg[string.offset];
}

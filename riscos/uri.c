/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Rob Jackson <jacko@xms.ms>
 */

#include <stdio.h>
#include <string.h>
#include "oslib/uri.h"
#include "oslib/wimp.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

void ro_uri_message_received(uri_full_message_process*);


void ro_uri_message_received(uri_full_message_process* uri_message)
{
  uri_h uri_handle;
  char* uri_requested;
  char* temp;

  struct browser_window* bw;
  int uri_length;
  int i;

  uri_handle = uri_message->handle;

  LOG(("URI message... %s, handle = %d", uri_message->uri,
                                         (int)uri_message->handle));

  if ( (strspn(uri_message->uri, "http://") != strlen("http://")) &&
       (strspn(uri_message->uri, "https://") != strlen("https://")) &&
       (strspn(uri_message->uri, "file:/")  != strlen("file:/")) )
            return;

  else LOG(("URI message deemed relevant"));

  uri_message->your_ref = uri_message->my_ref;
  uri_message->action = message_URI_PROCESS_ACK;

  xwimp_send_message(wimp_USER_MESSAGE_ACKNOWLEDGE,
                    (wimp_message*)uri_message,
                    uri_message->sender);

  xuri_request_uri(0, 0, 0, uri_handle, &uri_length);
  uri_requested = xcalloc(uri_length, sizeof(char));

  if (uri_requested == NULL)
     return;

  xuri_request_uri(0, uri_requested, uri_length, uri_handle, NULL);

  /* Kludge for file:/ URLs (changes them into file:/// URLs) */
  if( (strncasecmp(uri_requested, "file:/", 6) == 0) &&
      (strncasecmp(uri_requested, "file://", 7) != 0) ) {

        temp = xcalloc(strlen(uri_requested) + 5, sizeof(char));
        strcpy(temp, "file:///");
        for(i=6; i!=strlen(uri_requested); i++) {

          temp[i+2] = uri_message->uri[i];
        }
        xfree(uri_requested);
        uri_requested = strdup(temp);
        xfree(temp);
  }

  bw = create_browser_window(browser_TITLE | browser_TOOLBAR
          | browser_SCROLL_X_ALWAYS | browser_SCROLL_Y_ALWAYS, 640, 480, NULL);

  gui_window_show(bw->window);
  browser_window_open_location(bw, uri_requested);

  wimp_set_caret_position(bw->window->data.browser.toolbar,
               ICON_TOOLBAR_URL,
               0,0,-1, (int) strlen(bw->window->url) - 1);


  xfree(uri_requested);
}

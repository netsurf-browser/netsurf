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
#include "netsurf/utils/config.h"
#include "netsurf/content/fetch.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/uri.h"
#include "netsurf/riscos/url_protocol.h"
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
  struct browser_window* bw;
#endif
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

  if (!fetch_can_fetch(uri_requested)) {
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
  browser_window_create(uri_requested);

#if 0
  if (post) {
    /* TODO - create urlencoded data from file contents.
     *        Delete the file when finished with it.
     */
    browser_window_open_location_historical(bw, uri_requested, /*data*/0, 0);
  }
#endif

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

bool ro_url_broadcast(char *url) {

	inetsuite_full_message_open_url_direct message;
	os_error *e;
	int len = ((strlen(url)+1)>235) ? 235 : strlen(url)+1;

	message.size = ((20+len+3) & ~3);
	message.your_ref = 0;
	message.action = message_INET_SUITE_OPEN_URL;

	*message.url = 0;
	strncat(message.url, url, 235);
	message.url[len-1] = 0;
	e = xwimp_send_message(wimp_USER_MESSAGE_RECORDED,
				(wimp_message*)&message, 0);
	if (e) {
		return false;
	}

	return true;
}

bool ro_url_load(char *url) {

	char url_buf[512];
	char *colon;
	os_error *e;

	colon = strchr(url, ':');
	if (!colon) return false;

	strcpy(url_buf, "Alias$URLOpen_");
	strncat(url_buf, url, colon-url);
	if (!getenv(url_buf)) return false;

	strcat(url_buf, " ");
	strncat(url_buf, url, 512-strlen(url_buf)-1);

	e = xwimp_start_task(url_buf+5, 0);

	if (e) {
		return false;
	}

	return true;
}

void ro_url_bounce(wimp_message *message) {

	inetsuite_message_open_url *url_message = (inetsuite_message_open_url*)&message->data;

	/* ant broadcast bounced -> try uri broadcast / load */
	ro_uri_launch(url_message->url);
}

/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Rob Jackson <jacko@xms.ms>
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/uri.h"
#include "oslib/wimp.h"
#include "utils/config.h"
#include "content/fetch.h"
#include "desktop/browser.h"
#include "riscos/theme.h"
#include "desktop/gui.h"
#include "riscos/gui.h"
#include "riscos/url_protocol.h"
#include "utils/log.h"
#include "utils/utils.h"

#ifdef WITH_URI

void ro_uri_message_received(uri_full_message_process*);
bool ro_uri_launch(char *uri);
void ro_uri_bounce(uri_full_message_return_result*);


void ro_uri_message_received(uri_full_message_process* uri_message)
{
	uri_h uri_handle;
	char* uri_requested;
	int uri_length;

	uri_handle = uri_message->handle;

	if (!fetch_can_fetch(uri_message->uri)) return;

	uri_message->your_ref = uri_message->my_ref;
	uri_message->action = message_URI_PROCESS_ACK;

	xwimp_send_message(wimp_USER_MESSAGE, (wimp_message*)uri_message,
		uri_message->sender);

	xuri_request_uri(0, 0, 0, uri_handle, &uri_length);
	uri_requested = calloc((unsigned int)uri_length, sizeof(char));

	if (uri_requested == NULL)
		return;

	xuri_request_uri(0, uri_requested, uri_length, uri_handle, NULL);

	browser_window_create(uri_requested, NULL, 0, true);

	free(uri_requested);
}

bool ro_uri_launch(char *uri)
{
	uri_h uri_handle;
	wimp_t handle_task;
	uri_dispatch_flags returned;
	os_error *e;

	e = xuri_dispatch(uri_DISPATCH_INFORM_CALLER, uri, task_handle,
			&returned, &handle_task, &uri_handle);

	if (e || returned & 1) {
		return false;
	}

	return true;
}

void ro_uri_bounce(uri_full_message_return_result *message)
{
	char uri_buf[512];
	os_error *e;

	if ((message->flags & 1) == 0) return;

	e = xuri_request_uri(0, uri_buf, sizeof uri_buf, message->handle, 0);

	if (e) {
		LOG(("xuri_request_uri: %d: %s", e->errnum, e->errmess));
		return;
	}

	ro_url_load(uri_buf);

	return;
}
#endif

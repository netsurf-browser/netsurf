/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2003 Rob Jackson <jacko@xms.ms>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * ANT URL launching protocol (implementation).
 *
 * See http://www.vigay.com/inet/inet_url.html
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "oslib/inetsuite.h"
#include "oslib/wimp.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/fetch.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/uri.h"
#include "netsurf/riscos/url_protocol.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_URL

/**
 * Handle a Message_InetSuiteOpenURL.
 */

void ro_url_message_received(wimp_message *message)
{
	char *url;
	int i;
	inetsuite_message_open_url *url_message =
			(inetsuite_message_open_url*) &message->data;
	os_error *error;

	/* If the url_message->indirect.tag is non-zero,
	 * then the message data is contained within the message block.
	 */
	if (url_message->indirect.tag != 0) {
		url = strndup(url_message->url, 236);
		if (!url) {
			warn_user("NoMemory", 0);
			return;
		}
		/* terminate at first control character */
		for (i = 0; !iscntrl(url[i]); i++)
			;
		url[i] = 0;

	} else {
		if (!url_message->indirect.url.offset) {
			LOG(("no URL in message"));
			return;
		}
		if (28 < message->size &&
				url_message->indirect.body_file.offset) {
			LOG(("POST for URL message not implemented"));
			return;
		}
		if (url_message->indirect.url.offset < 28 ||
				236 <= url_message->indirect.url.offset) {
			LOG(("external pointers in URL message unimplemented"));
			/* these messages have never been seen in the wild,
			 * and there is the problem of invalid addresses which
			 * would cause an abort */
			return;
		}

		url = strndup((char *) url_message +
				url_message->indirect.url.offset,
				236 - url_message->indirect.url.offset);
		if (!url) {
			warn_user("NoMemory", 0);
			return;
		}
		for (i = 0; !iscntrl(url[i]); i++)
			;
		url[i] = 0;
	}

	if (!fetch_can_fetch(url)) {
		free(url);
		return;
	}

	/* send ack */
	message->your_ref = message->my_ref;
	error = xwimp_send_message(wimp_USER_MESSAGE_ACKNOWLEDGE, message,
			message->sender);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	/* create new browser window */
	browser_window_create(url, 0);

	free(url);
}


/**
 * Broadcast an ANT URL message.
 */

void ro_url_broadcast(const char *url)
{
	inetsuite_full_message_open_url_direct message;
	os_error *error;
	int len = strlen(url) + 1;

	if (236 < len)
		len = 236;

	message.size = ((20+len+3) & ~3);
	message.your_ref = 0;
	message.action = message_INET_SUITE_OPEN_URL;
	strncpy(message.url, url, 235);
	message.url[235] = 0;
	error = xwimp_send_message(wimp_USER_MESSAGE_RECORDED,
			(wimp_message *) &message, 0);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Launch a program to handle an URL, using the ANT protocol
 * Alias$URLOpen_ system.
 */

void ro_url_load(const char *url)
{
	char *command;
	char *colon;
	os_error *error;

	colon = strchr(url, ':');
	if (!colon) {
		LOG(("invalid url '%s'", url));
		return;
	}

	command = malloc(40 + (colon - url) + strlen(url));
	if (!command) {
		warn_user("NoMemory", 0);
		return;
	}

	sprintf(command, "Alias$URLOpen_%.*s", colon - url, url);
	if (!getenv(command)) {
		free(command);
		return;
	}

	sprintf(command, "URLOpen_%.*s %s", colon - url, url, url);

	error = xwimp_start_task(command, 0);
	if (error) {
		LOG(("xwimp_start_task: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	free(command);
}


/**
 * Handle a bounced Message_InetSuiteOpenURL.
 */

void ro_url_bounce(wimp_message *message)
{
	inetsuite_message_open_url *url_message =
			(inetsuite_message_open_url*) &message->data;

	/* ant broadcast bounced -> try uri broadcast / load */
	ro_uri_launch(url_message->url);
}

#endif

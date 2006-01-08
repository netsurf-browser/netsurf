/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

/** \file
 * Automated RISC OS message routing (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include "oslib/os.h"
#include "oslib/wimp.h"
#include "netsurf/riscos/message.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


struct active_message {
	unsigned int message_code;
	int id;
	void (*callback)(wimp_event_no event, wimp_message *message);
	struct active_message *next;
	struct active_message *previous;
};
struct active_message *current_messages = NULL;


/**
 * Sends a message and registers a return route for a bounce.
 *
 * \param event		the message event type
 * \param message	the message to register a route back for
 * \param task		the task to send a message to, or 0 for broadcast
 * \param callback	the code to call on a bounce
 * \return true on success, false otherwise
 */
bool ro_message_send_message(wimp_event_no event, wimp_message *message,
		wimp_t task,
		void (*callback)(wimp_event_no event, wimp_message *message)) {
	os_error *error;

	/* send a message */
	error = xwimp_send_message(event, message, task);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	
	/* register the default bounce handler */
	if (callback) {
		assert(event == wimp_USER_MESSAGE_RECORDED);
		return ro_message_register_handler(message, message->action,
				callback);
	}
	return true;
}



/**
 * Registers a return route for a message.
 *
 * This function must be called after wimp_send_message so that a
 * valid value is present in the my_ref field.
 *
 * \param message	the message to register a route back for
 * \param messge_code	the message action code to route
 * \param callback	the code to call for a matched action
 * \return true on success, false on memory exhaustion
 */
bool ro_message_register_handler(wimp_message *message,
		unsigned int message_code,
		void (*callback)(wimp_event_no event, wimp_message *message)) {
	struct active_message *add;

	assert(message);
	assert(callback);

	add = (struct active_message *)malloc(sizeof(*add));
	if (!add)
		return false;
	add->message_code = message_code;
	add->id = message->my_ref;
	add->callback = callback;
	add->next = current_messages;
	add->previous = NULL;
	current_messages = add;
	return true;
}


/**
 * Attempts to route a message.
 *
 * \param message	the message to attempt to route
 * \return true if message was routed, false otherwise
 */
bool ro_message_handle_message(wimp_event_no event, wimp_message *message) {
	struct active_message *test;
	struct active_message *next;
	bool handled = true;
	int ref;

	assert(message);

	/* we can't work without a reference */
	ref = message->my_ref;
	if (ref == 0)
		return false;

	/* handle the message */
	for (test = current_messages; test; test = test->next) {
		if ((ref == test->id) &&
				(message->action == test->message_code)) {
			handled = true;
			if (test->callback)
				test->callback(event, message);
			break;
		}
	}

	/* remove all handlers for this id */
	next = current_messages;
	while ((test = next)) {
	  	next = test->next;
		if (ref == test->id) {
			if (test->previous)
				test->previous->next = test->next;
			if (test->next)
				test->next->previous = test->previous;
			if (current_messages == test)
				current_messages = test->next;
		  	free(test);
		}
	}
	return handled;
}

/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

/** \file
 * Automated RISC OS message routing (interface).
 */


#ifndef _NETSURF_RISCOS_MESSAGE_H_
#define _NETSURF_RISCOS_MESSAGE_H_

#include "oslib/wimp.h"

bool ro_message_send_message(wimp_event_no event, wimp_message *message,
		wimp_t task, void (*callback)(wimp_message *message));
bool ro_message_send_message_to_window(wimp_event_no event, wimp_message *message,
		wimp_w to_w, wimp_i to_i, void (*callback)(wimp_message *message),
		wimp_t *to_t);
bool ro_message_register_handler(wimp_message *message,
		unsigned int message_code,
		void (*callback)(wimp_message *message));
bool ro_message_register_route(unsigned int message_code,
		void (*callback)(wimp_message *message));
bool ro_message_handle_message(wimp_event_no event, wimp_message *message);

#endif

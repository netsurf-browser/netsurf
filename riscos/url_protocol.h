/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * ANT URL launching protocol (interface).
 */

#ifndef _NETSURF_RISCOS_URL_H_
#define _NETSURF_RISCOS_URL_H_

#include "oslib/wimp.h"

void ro_url_message_received(wimp_message *message);
void ro_url_broadcast(const char *url);
void ro_url_load(const char *url);
void ro_url_bounce(wimp_message *message);

#endif

/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Rob Jackson <jacko@xms.ms>
 */

#ifndef _NETSURF_RISCOS_URI_H_
#define _NETSURF_RISCOS_URI_H_

#include "oslib/wimp.h"

void ro_uri_message_received(wimp_message *message);
bool ro_uri_launch(char *uri);
void ro_uri_bounce(wimp_message *message);

#endif

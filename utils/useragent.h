/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2007 Daniel Silverstone <dsilvers@digital-scurf.org>
 * Copyright 2007 Rob Kendrick <rjek@netsurf-browser.org>
 */

#ifndef _NETSURF_UTILS_USERAGENT_H_
#define _NETSURF_UTILS_USERAGENT_H_

/** Retrieve the core user agent for this release.
 *
 * The string returned can be relied upon to exist for the duration of
 * the execution of the program. There is no need to copy it.
 */
const char * user_agent_string(void);

#endif

/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2007 Daniel Silverstone <dsilvers@digital-scurf.org>
 * Copyright 2007 Rob Kendrick <rjek@netsurf-browser.org>
 */

#include <sys/utsname.h>
#include <stdio.h>

#include "useragent.h"
#include "desktop/netsurf.h"
#include "utils/log.h"

static const char *core_user_agent_string = NULL;

#define NETSURF_UA_FORMAT_STRING "NetSurf/%d.%d (%s; %s)"

/**
 * Prepare core_user_agent_string with a string suitable for use as a
 * user agent in HTTP requests.
 */
static void
build_user_agent(void)
{
  struct utsname un;
  const char *sysname = "Unknown";
  const char *machine = "Unknown";
  int len;
  
  if (uname(&un) == 0) {
    sysname = un.sysname;
    machine = un.machine;
  }
  
  len = snprintf(NULL, 0, NETSURF_UA_FORMAT_STRING,
                 netsurf_version_major,
                 netsurf_version_minor,
                 un.sysname,
                 un.machine);
  core_user_agent_string = malloc(len + 1);
  snprintf(core_user_agent_string, len + 1,
           NETSURF_UA_FORMAT_STRING,
           netsurf_version_major,
           netsurf_version_minor,
           un.sysname,
           un.machine);

  LOG(("Built user agent \"%s\"", core_user_agent_string));
}

/* This is a function so that later we can override it trivially */
const char *
user_agent_string(void)
{
  if (core_user_agent_string == NULL)
    build_user_agent();
  return core_user_agent_string;
}

/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#include "netsurf/desktop/options.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/content/cache.h"
#include "netsurf/content/fetch.h"
#include "netsurf/utils/log.h"
#include <stdlib.h>

int netsurf_quit = 0;

static void netsurf_init(int argc, char** argv);
static void netsurf_exit(void);


void netsurf_poll(void)
{
  gui_poll();
  fetch_poll();
}


void netsurf_init(int argc, char** argv)
{
  stdout = stderr;
  options_init(&OPTIONS);
  options_read(&OPTIONS, NULL);
  gui_init(argc, argv);
  fetch_init();
  cache_init();
  nspng_init();
  nsgif_init();
}


void netsurf_exit(void)
{
  cache_quit();
  fetch_quit();
}


int main(int argc, char** argv)
{
  netsurf_init(argc, argv);

  while (netsurf_quit == 0)
    netsurf_poll();

  LOG(("Netsurf quit!"));
  netsurf_exit();

  return 0;
}



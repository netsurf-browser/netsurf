/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/cache.h"
#include "netsurf/content/fetch.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/desktop/options.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

bool netsurf_quit = false;

static void netsurf_init(int argc, char** argv);
static void netsurf_poll(void);
static void netsurf_exit(void);


/**
 * Gui NetSurf main().
 */

int main(int argc, char** argv)
{
  netsurf_init(argc, argv);

  while (!netsurf_quit)
    netsurf_poll();

  netsurf_exit();

  return EXIT_SUCCESS;
}


/**
 * Initialise components used by gui NetSurf.
 */

void netsurf_init(int argc, char** argv)
{
  stdout = stderr;
  gui_init(argc, argv);
  setlocale(LC_ALL, "");
  fetch_init();
  cache_init();
  fetchcache_init();
#ifdef WITH_PNG
  nspng_init();
#endif
#ifdef WITH_GIF
  nsgif_init();
#endif
  url_init();
  save_complete_init();
}


/**
 * Poll components which require it.
 */

void netsurf_poll(void)
{
  gui_poll(fetch_active);
  fetch_poll();
}


/**
 * Clean up components used by gui NetSurf.
 */

void netsurf_exit(void)
{
  cache_quit();
  fetch_quit();
  gui_quit();
}

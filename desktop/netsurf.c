/**
 * $Id: netsurf.c,v 1.4 2002/11/03 09:39:53 bursa Exp $
 */

#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/fetch.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/cache.h"
#include <stdlib.h>

int netsurf_quit = 0;
gui_window* netsurf_gui_windows = NULL;
struct fetch* netsurf_fetches = NULL;


void netsurf_poll(void)
{
  gui_poll();
  netsurf_fetches = fetch_poll(netsurf_fetches);
}


void netsurf_init(int argc, char** argv)
{
  stdout = stderr;
  gui_init(argc, argv);
  cache_init();
}


void netsurf_exit(void)
{
  cache_quit();
}


int main(int argc, char** argv)
{
  netsurf_init(argc, argv);

  while (netsurf_quit == 0)
    netsurf_poll();

  fprintf(stderr, "Netsurf quit!\n");
  netsurf_exit();

  return 0;
}


void Log(char* func, char* msg)
{
#ifdef NETSURF_DUMP
  FILE* logfile = NULL;
  logfile = fopen("logfile","a");
  if (logfile == NULL)
    die("can't open logfile");
  fprintf(logfile, "%s: %s\n", func, msg);
  fclose(logfile);
#endif
}

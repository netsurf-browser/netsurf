/**
 * $Id: netsurf.h,v 1.1 2002/09/11 14:24:02 monkeyson Exp $
 */

#ifndef _NETSURF_DESKTOP_NETSURF_H_
#define _NETSURF_DESKTOP_NETSURF_H_

#include "netsurf/desktop/fetch.h"
#include "netsurf/desktop/browser.h"

extern struct fetch* netsurf_fetches;
extern gui_window* netsurf_gui_windows;

extern int netsurf_quit;

void netsurf_poll(void);
void Log(char* func, char* msg);

#endif

#ifndef _NETSURF_DESKTOP_NETSURF_H_
#define _NETSURF_DESKTOP_NETSURF_H_

#include "netsurf/desktop/fetch.h"
#include "netsurf/desktop/browser.h"

extern struct fetch* netsurf_fetches;
extern gui_window* netsurf_gui_windows;

extern int netsurf_quit;

void netsurf_poll(void);
void Log(char* func, char* msg);

#endif

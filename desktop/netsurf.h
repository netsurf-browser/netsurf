/**
 * $Id: netsurf.h,v 1.2 2003/02/09 12:58:15 bursa Exp $
 */

#ifndef _NETSURF_DESKTOP_NETSURF_H_
#define _NETSURF_DESKTOP_NETSURF_H_

#include "netsurf/desktop/browser.h"

extern gui_window* netsurf_gui_windows;
extern int netsurf_quit;

void netsurf_poll(void);

#endif


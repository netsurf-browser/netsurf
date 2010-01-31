/*
* Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
*
* This file is part of NetSurf, http://www.netsurf-browser.org/
*
* NetSurf is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2 of the License.
*
* NetSurf is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _NETSURF_WINDOWS_LOCALHISTORY_H_
#define _NETSURF_WINDOWS_LOCALHISTORY_H_

#include "desktop/browser.h"

struct nsws_localhistory;

void nsws_localhistory_init(struct gui_window *);
void nsws_localhistory_up(struct gui_window *);
void nsws_localhistory_close(struct gui_window *);

#endif

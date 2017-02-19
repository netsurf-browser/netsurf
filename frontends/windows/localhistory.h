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

#ifndef NETSURF_WINDOWS_LOCALHISTORY_H
#define NETSURF_WINDOWS_LOCALHISTORY_H

struct nsws_localhistory;

/**
 * Close win32 localhistory window.
 *
 * \param gw The win32 gui window to close local history for.
 */
void nsws_localhistory_close(struct gui_window *gw);

/**
 * creates localhistory window
 *
 * \param gw The win32 gui window to create a local history for.
 */
struct nsws_localhistory *nsws_window_create_localhistory(struct gui_window *gw);

/**
 * Create the win32 window class
 *
 * \param hinstance The application instance to create the window class under
 * \return NSERROR_OK on success else error code.
 */
nserror nsws_create_localhistory_class(HINSTANCE hinstance);

#endif

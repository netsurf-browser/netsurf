/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_LOGIN_H
#define AMIGA_LOGIN_H

#include <stdbool.h>

struct gui_login_window;

nserror gui_401login_open(nsurl *url, const char *realm,
		const char *username, const char *password,
		nserror (*cb)(const char *username,
				const char *password,
				void *pw),
		void *cbpw);
#endif


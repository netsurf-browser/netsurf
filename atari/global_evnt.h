/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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

#ifndef NS_ATARI_GLOBAL_EVNT_H
#define NS_ATARI_GLOBAL_EVNT_H

#include <stdbool.h>

struct s_keybd_evnt_data
{
	char ascii;
} keybd_evnt_data;

struct s_evnt_data
{
	bool ignore;
	union {
		struct s_keybd_evnt_data keybd;
	} u;
};

struct s_evnt_data evnt_data;

/*
	Global event handlers
*/

void bind_global_events( void );
void unbind_global_events( void );


#endif

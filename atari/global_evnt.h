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

struct s_accelerator
{
	char ascii;	/* either ascii or */
	long keycode; /* normalised keycode is valid  */
	short mod;  /* shift / ctrl etc */
};

typedef void __CDECL (*menu_evnt_func)(WINDOW * win, int item, int title, void * data);
struct s_menu_item_evnt {
	short title; /* to which menu this item belongs */
	short rid; /* resource ID */
	const char * nsid; /* Netsurf message ID */
	menu_evnt_func menu_func; /* click handler */
	struct s_accelerator accel; /* accelerator info */
	char * menustr;
};

/*
	Global & Menu event handlers
*/

void bind_global_events( void );
void unbind_global_events( void );
void main_menu_update( void );


#endif

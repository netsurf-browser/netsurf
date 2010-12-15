/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2010 Michael Drake <tlsa@netsurf-browser.org>
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

/** \file
 * Core mouse state.
 */

#ifndef _NETSURF_DESKTOP_MOUSE_H_
#define _NETSURF_DESKTOP_MOUSE_H_

/* Mouse state.	1 is    primary mouse button (e.g. Select on RISC OS).
 *		2 is  secondary mouse button (e.g. Adjust on RISC OS). */
typedef enum {
	BROWSER_MOUSE_PRESS_1   = 1,	/* button 1 pressed */
	BROWSER_MOUSE_PRESS_2   = 2,	/* button 2 pressed */

					/* note: click meaning is different for
					 * different front ends. On RISC OS, it
					 * is standard to act on press, so a
					 * click is fired at the same time as a
					 * mouse button is pressed. With GTK, it
					 * is standard to act on release, so a
					 * click is fired when the mouse button
					 * is released, if the operation wasn't
					 * a drag. */
	BROWSER_MOUSE_CLICK_1   = 4,	/* button 1 clicked. */
	BROWSER_MOUSE_CLICK_2   = 8,	/* button 2 clicked. */

	BROWSER_MOUSE_DOUBLE_CLICK = 16, /* button 1 double clicked */

	BROWSER_MOUSE_DRAG_1    = 32,	/* start of button 1 drag operation */
	BROWSER_MOUSE_DRAG_2    = 64,	/* start of button 2 drag operation */

	BROWSER_MOUSE_DRAG_ON   = 128,	/* a drag operation was started and
					 * a mouse button is still pressed */

	BROWSER_MOUSE_HOLDING_1 = 256,	/* while button 1 drag is in progress */
	BROWSER_MOUSE_HOLDING_2 = 512,	/* while button 2 drag is in progress */


	BROWSER_MOUSE_MOD_1     = 1024,	/* primary modifier key pressed
					 * (eg. Shift) */
	BROWSER_MOUSE_MOD_2     = 2048,	/* secondary modifier key pressed
					 * (eg. Ctrl) */
	BROWSER_MOUSE_MOD_3     = 4096	/* secondary modifier key pressed
					 * (eg. Alt) */
} browser_mouse_state;

void browser_mouse_state_dump(browser_mouse_state mouse);

#endif

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

#ifndef NS_ATARI_GUI_H_
#define NS_ATARI_GUI_H_

#include <windom.h>

struct point_s {
	int x;
	int y;
};

typedef struct point_s POINT;

#define MFORM_EX_FLAG_USERFORM 0x01
#define MFORM_EX_FLAG_REDRAW_REQUIRED 0x02

struct mform_ex_s
{
	unsigned char flags;
	int number;
	OBJECT * tree;
};

typedef struct mform_ex_s MFORM_EX;

struct s_gem_cursors {
	MFORM_EX hand;
	MFORM_EX ibeam;
	MFORM_EX cross;
	MFORM_EX sizeall;
	MFORM_EX sizewe;
	MFORM_EX sizens;
	MFORM_EX sizenesw;
	MFORM_EX sizenwse;
	MFORM_EX wait;
	MFORM_EX appstarting;
	MFORM_EX nodrop;
	MFORM_EX deny;
	MFORM_EX help;
	MFORM_EX menu;
	MFORM_EX arrow;
} gem_cursors;

enum focus_element_type {
	WIDGET_NONE=0,
	URL_WIDGET,
	BROWSER
};


struct s_focus_info
{
	enum focus_element_type type;
	void * element;
};

/* defines for data attached to components: */
#define CDT_OBJECT	            0x004f424aUL
#define CDT_OWNER 	            0x03UL
#define CDT_ICON 	            0x04UL
#define CDT_ICON_TYPE	        0x05UL
#define CDT_ICON_TYPE_NONE      0x00UL
#define CDT_ICON_TYPE_OBJECT    0x01UL
#define CDT_ICON_TYPE_BITMAP    0x02UL


struct gui_window;
struct s_browser;
struct s_statusbar;
struct s_toolbar;

typedef struct s_toolbar * CMP_TOOLBAR;
typedef struct s_statusbar * CMP_STATUSBAR;
typedef struct s_browser * CMP_BROWSER;

/*
	This is the "main" window. It can consist of several components
	and also holds information shared by several frames within
	the window.
*/
struct s_gui_win_root
{
	WINDOW * handle;
	CMP_TOOLBAR toolbar;
	CMP_STATUSBAR statusbar;
	COMPONENT * cmproot;
	MFORM_EX cursor;
	struct s_focus_info focus;
	float scale;
	bool throbbing;
	char * title;
	/* current size of window on screen: */
	GRECT loc;
};

/*
	This is the part of the gui which is known by netsurf core.
	You must implement it. Altough, you are free how to do it.
	Each of the browser "viewports" managed by netsurf are bound
	to this structure.
*/
struct gui_window {
	struct s_gui_win_root * root;
	CMP_BROWSER browser;
    /* icon to be drawn when iconified, or NULL for default resource. */
	struct bitmap * icon;
	struct gui_window *next, *prev;
};

extern struct gui_window *window_list;

#endif

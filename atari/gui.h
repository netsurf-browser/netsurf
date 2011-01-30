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

#define WM_FORCE_MOVE 1024 + 3

struct point_s {
	int x;
	int y;
};

struct bbox_s {
	int x0;
	int y0;
	int x1;
	int y1;
};

typedef struct bbox_s BBOX;
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

/* maybe its better to have an linked
	list of redraw areas, so that there
	is no such an overhead when 2 (or more)
	SMALL rectangles far away from each other
	need an redraw!
	Currently these rects get summarized into
	one big redraw area!
*/
struct s_browser_redrw_info
{
	BBOX area;		/* this is an box which describes the area to redraw */
								/* from netsurfs point of view */
	bool required;
};

struct s_scroll_info
{
	POINT requested;
	POINT current;
	bool required;
};

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


struct s_gui_input_state {
	short mbut;
	short mkstat;
	short mx;
	short my;
} prev_inp_state;


#define TB_BUTTON_WIDTH 32
#define TB_BUTTON_HEIGHT 21 /* includes 1px 3d effect */
#define TOOLBAR_HEIGHT 25
#define URLBOX_HEIGHT 21
#define STATUSBAR_HEIGHT 16
#define MOVER_WH 16
#define THROBBER_WIDTH 32


/* defines for data attached to components: */
#define CDT_OBJECT		0x004f424aUL
#define CDT_OWNER 		0x03UL
#define CDT_ICON 			0x04UL
#define CDT_ICON_TYPE	0x05UL

/*
 URL Widget Block size: size of memory block to allocated
 when input takes more memory than currently allocated:
*/
#define URL_WIDGET_BSIZE 64
#define URL_WIDGET_MAX_MEM 60000
struct s_url_widget
{
	short selection_len;	/* len & direction of selection */
	short caret_pos;	 	/* cursor pos */
	short char_size;	 	/* size of one character (width & hight) */
	short scrollx;  	 	/* current scroll position */
	bool redraw;		 	/* widget is only redrawn when this flag is set */
	char * text;			/* dynamicall allocated URL string*/
	unsigned short allocated;
	unsigned short used; 	/* memory used by URL (strlen + 1) */
	COMPONENT * comp;
};

struct s_throbber_widget
{
	COMPONENT * comp;
	short index;
	short max_index;
	bool running;
};

struct gui_window;

struct s_tb_button
{
    short rsc_id;
	void (*cb_click)(struct gui_window * gw);
    COMPONENT * comp;
};

struct s_toolbar
{
	COMPONENT * comp;
	struct gui_window * owner;
	struct s_url_widget url;
	struct s_throbber_widget throbber;
	GRECT btdim; /* size & location of buttons */
	struct s_tb_button * buttons;
	int btcnt;
	/* buttons are defined in toolbar.c */
};

struct s_statusbar
{
	COMPONENT * comp;
	char text[STATUSBAR_MAX_SLEN+1];
	size_t textlen;
	bool attached;
};

struct s_caret
{
	GRECT requested;
	GRECT current;
	bool redraw;
};

struct s_browser
{
	int type;
	COMPONENT * comp;
	WINDOW * compwin;
	struct browser_window * bw;
	struct s_scroll_info scroll;
	struct s_browser_redrw_info redraw; 
	struct s_caret caret;
	bool attached;
};

typedef struct s_toolbar * CMP_TOOLBAR;
typedef struct s_statusbar * CMP_STATUSBAR;
typedef struct s_browser * CMP_BROWSER;

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
	GRECT loc;	/* current size of window on screen */
};

struct gui_window {
	struct s_gui_win_root * root;
	CMP_BROWSER browser;
	struct gui_window * parent;
	struct gui_window *next, *prev;
};


extern struct gui_window *window_list;

/* scroll a window */
void gem_window_scroll(struct browser_window * , int x, int y);

#define MOUSE_IS_DRAGGING() (mouse_hold_start[0] || mouse_hold_start[1])


#endif

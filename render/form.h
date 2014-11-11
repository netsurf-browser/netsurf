/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net> 
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
 * Form handling functions (interface).
 */

#ifndef _NETSURF_RENDER_FORM_H_
#define _NETSURF_RENDER_FORM_H_

#include <stdbool.h>

struct box;
struct form_control;
struct form_option;
struct form_select_menu;
struct form;
struct html_content;
struct dom_string;
struct content;
struct nsurl;
struct fetch_multipart_data;
struct redraw_context;
struct browser_window;

enum browser_mouse_state;



/** Type of a struct form_control. */
typedef enum {
	GADGET_HIDDEN,
	GADGET_TEXTBOX,
	GADGET_RADIO,
	GADGET_CHECKBOX,
	GADGET_SELECT,
	GADGET_TEXTAREA,
	GADGET_IMAGE,
	GADGET_PASSWORD,
	GADGET_SUBMIT,
	GADGET_RESET,
	GADGET_FILE,
	GADGET_BUTTON
} form_control_type;

/** Data for textarea */
struct form_textarea_data {
	struct form_control *gadget;
};

/** Option in a select. */
struct form_option {
	void *node;			/**< Corresponding DOM node */
	bool selected;
	bool initial_selected;
	char *value;
	char *text; /**< NUL terminated. */
	struct form_option* next;
};

struct image_input_coords {
	int x;
	int y;
};

/** Form control. */
struct form_control {
	void *node;			/**< Corresponding DOM node */
	struct html_content *html;	/**< HTML content containing control */

	form_control_type type;		/**< Type of control */

	struct form *form;		/**< Containing form */

	char *name;			/**< Control name */
	char *value;			/**< Current value of control */
	char *initial_value;		/**< Initial value of control */
	bool disabled;			/**< Whether control is disabled */

	struct box *box;		/**< Box for control */

	unsigned int length;		/**< Number of characters in control */
	unsigned int maxlength;		/**< Maximum characters permitted */

	bool selected;			/**< Whether control is selected */

	union {
		struct {
			int mx, my;
		} image;
		struct {
			int num_items;
			struct form_option *items, *last_item;
			bool multiple;
			int num_selected;
			/** Currently selected item, if num_selected == 1. */
			struct form_option *current;
			struct form_select_menu *menu;
		} select;
		struct {
			struct textarea *ta;
			struct dom_string *initial;
			struct form_textarea_data data;
		} text;			/**< input type=text or textarea */
	} data;

	struct form_control *prev;      /**< Previous control in this form */
	struct form_control *next;	/**< Next control in this form. */
};


/**
 * Process a selection from a form select menu.
 *
 * \param  control  form control with menu
 * \param  item	    index of item selected from the menu
 */
nserror form_select_process_selection(struct form_control *control, int item);

#endif

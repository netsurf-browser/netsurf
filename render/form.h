/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Form handling functions (interface).
 */

#ifndef _NETSURF_RENDER_FORM_H_
#define _NETSURF_RENDER_FORM_H_

#include <stdbool.h>
#include "netsurf/utils/config.h"

struct box;
struct form_control;
struct form_option;

/** HTML form. */
struct form {
	char *action;				/**< Url to submit to. */
	enum {
		method_GET,		/**< GET, always url encoded. */
		method_POST_URLENC,	/**< POST, url encoded. */
		method_POST_MULTIPART	/**< POST, multipart/form-data. */
	} method;				/**< Method and enctype. */
	struct form_control *controls;		/**< Linked list of controls. */
	struct form_control *last_control;	/**< Last control in list. */
};

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
	GADGET_FILE
} form_control_type;

/** Form control. */
struct form_control {
	form_control_type type;
	char *name;
	char *value;
	char *initial_value;
	bool disabled;
	struct form *form;
	struct box *box;
	struct box *caret_inline_container;
	struct box *caret_text_box;
	unsigned int caret_box_offset, caret_form_offset;
	unsigned int length;
	int caret_pixel_offset;
	unsigned int maxlength;
	bool selected;
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
		} select;
	} data;
	struct form_control *prev;      /**< Previous control in this form */
	struct form_control *next;	/**< Next control in this form. */
};

/** Option in a select. */
struct form_option {
	bool selected;
	bool initial_selected;
	char* value;
	char* text; /**< NUL terminated. */
	struct form_option* next;
};

/** Successful control, as defined by HTML 4.01 17.13. */
struct form_successful_control {
        bool file;                              /**< It's a file */
	char *name;				/**< Control name. */
	char *value;				/**< Current value. */
	struct form_successful_control *next;	/**< Next in linked list. */
};

struct form_control *form_new_control(form_control_type type);
void form_add_control(struct form *form, struct form_control *control);
void form_free_control(struct form_control *control);
struct form_successful_control *form_successful_controls(struct form *form,
		struct form_control *submit_button);
char *form_url_encode(struct form_successful_control *control);
void form_free_successful(struct form_successful_control *control);

#endif

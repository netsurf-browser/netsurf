/*
 * Copyright 2014 Vincent Sanders <vince@netsurf-browser.org>
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

/**
 * \file
 * Interface to form handling functions internal to render.
 */

#ifndef _NETSURF_RENDER_FORM_INTERNAL_H_
#define _NETSURF_RENDER_FORM_INTERNAL_H_

#include "render/form.h"

/** Form submit method. */
typedef enum {
	method_GET,		/**< GET, always url encoded. */
	method_POST_URLENC,	/**< POST, url encoded. */
	method_POST_MULTIPART	/**< POST, multipart/form-data. */
} form_method;

/** HTML form. */
struct form {
	void *node;			/**< Corresponding DOM node */

	char *action;			/**< Absolute URL to submit to. */
	char *target;			/**< Target to submit to. */
	form_method method;		/**< Method and enctype. */
	char *accept_charsets;		/**< Charset to submit form in */
	char *document_charset;		/**< Charset of document containing form */
	struct form_control *controls;	/**< Linked list of controls. */
	struct form_control *last_control;	/**< Last control in list. */

	struct form *prev;		/**< Previous form in doc. */
};

/**
 * Called by the select menu when it wants an area to be redrawn. The
 * coordinates are menu origin relative.
 *
 * \param client_data	data which was passed to form_open_select_menu
 * \param x		X coordinate of redraw rectangle
 * \param y		Y coordinate of redraw rectangle
 * \param width		width of redraw rectangle
 * \param height	height of redraw rectangle
 */
typedef void(*select_menu_redraw_callback)(void *client_data,
		int x, int y, int width, int height);

/**
 * Create a struct form.
 *
 * \param  node    DOM node associated with form
 * \param  action  URL to submit form to, or NULL for default
 * \param  target  Target frame of form, or NULL for default
 * \param  method  method and enctype
 * \param  charset acceptable encodings for form submission, or NULL
 * \param  doc_charset  encoding of containing document, or NULL
 * \param  html  HTML content containing form
 * \return  a new structure, or NULL on memory exhaustion
 */
struct form *form_new(void *node, const char *action, const char *target,
		form_method method, const char *charset,
		const char *doc_charset);
void form_free(struct form *form);

/**
 * Create a struct form_control.
 *
 * \param  node  Associated DOM node
 * \param  type  control type
 * \return  a new structure, or NULL on memory exhaustion
 */
struct form_control *form_new_control(void *node, form_control_type type);

void form_add_control(struct form *form, struct form_control *control);
void form_free_control(struct form_control *control);
bool form_add_option(struct form_control *control, char *value, char *text,
		     bool selected, void *node);
bool form_successful_controls(struct form *form,
		struct form_control *submit_button,
		struct fetch_multipart_data **successful_controls);
bool form_successful_controls_dom(struct form *form,
		struct form_control *submit_button,
		struct fetch_multipart_data **successful_controls);

bool form_open_select_menu(void *client_data,
		struct form_control *control,
		select_menu_redraw_callback redraw_callback,
  		struct content *c);
void form_select_menu_callback(void *client_data,
		int x, int y, int width, int height);
void form_free_select_menu(struct form_control *control);
bool form_redraw_select_menu(struct form_control *control, int x, int y,
		float scale, const struct rect *clip,
		const struct redraw_context *ctx);
bool form_clip_inside_select_menu(struct form_control *control, float scale,
		const struct rect *clip);
const char *form_select_mouse_action(struct form_control *control,
		enum browser_mouse_state mouse, int x, int y);
void form_select_mouse_drag_end(struct form_control *control,
		enum browser_mouse_state mouse, int x, int y);
void form_select_get_dimensions(struct form_control *control,
		int *width, int *height);
void form_submit(struct nsurl *page_url, struct browser_window *target,
		struct form *form, struct form_control *submit_button);
void form_radio_set(struct form_control *radio);

void form_gadget_update_value(struct form_control *control, char *value);

#endif
